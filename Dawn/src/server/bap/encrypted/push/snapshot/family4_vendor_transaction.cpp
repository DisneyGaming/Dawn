#include "internal.h"
#include "../queuez/queuez_update_frame.h"
#include "../../queuez/queuez_state_validation.h"
#include "../../../../../middleware/datagen/definitions.h"
#include "../../../../../middleware/datagen/family4/character/character_encoder.h"
#include "../../../../../middleware/datagen/family4/character/layout.h"
#include "../../../../../middleware/datagen/family4/account/account_encoder.h"
#include "../../../../../middleware/datagen/family4/account/layout.h"
#include <algorithm>

namespace dawn::server::bap::encrypted::queuez {
bool stage_vendor_transaction(const SessionState& before,const state::vendors::Pending& pending,VendorTransaction& out) noexcept {
    out={};if(!pending.data) {return false;}const auto& p=*pending.data;
    if(p.character>=p.before.characterCount || p.before.primarySoid!=before.family4RootSoid
        || !stage_equipment_swap(before,p.before.characters[p.character].soid,out)) {return false;}
    constexpr auto instance=middleware::datagen::kItemInstanceObjectId;
    for(const auto removed:p.removed) {
        auto& state=out.after;std::size_t index=state.family4ResidentCount;
        for(std::size_t i=0;i<state.family4ResidentCount;++i) if(state.family4Residents[i].objectSoid==removed
            && state.family4Residents[i].definitionId==instance) {index=i;break;}
        if(index==state.family4ResidentCount) {return false;}
        for(auto i=index+1;i<state.family4ResidentCount;++i) {state.family4Residents[i-1]=state.family4Residents[i];}
        state.family4Residents[--state.family4ResidentCount]={};
    }
    for(const auto changed:p.changed) {
        auto& state=out.after;bool exists{};
        for(std::size_t i=0;i<state.family4ResidentCount;++i) if(state.family4Residents[i].objectSoid==changed) {
            if(state.family4Residents[i].definitionId!=instance) {return false;}exists=true;break;
        }
        if(!exists) {
            if(state.family4ResidentCount==state.family4Residents.size()) {return false;}
            state.family4Residents[state.family4ResidentCount++]={changed,instance};
        }
    }
    return valid(out.after);
}
}
namespace dawn::server::bap::encrypted::push {
bool append_vendor_transaction_notification(Scratch& scratch,const queuez::VendorTransaction& update,
    const state::vendors::Pending& pending,std::span<const std::byte,state::kAesKeySize> key,
    std::span<const std::byte,state::kBapNonceSize> nonce,std::span<std::byte> response,std::size_t& written) noexcept {
    namespace f4=middleware::datagen::family4;
    if(!pending.data) {return false;}const auto& p=*pending.data;
    auto account=std::make_unique<state::AccountState>();snapshot::Resolved selected{};
    if(!state::vendors::preview(pending,*account) || !snapshot::resolve(*account,p.character,selected)
        || update.characterSoid!=account->characters[p.character].soid) {return false;}
    snapshot::Prepared prepared{};const auto reservation=snapshot::reserve_prior(scratch,prepared);
    if(reservation.rawWriteOffset>scratch.plaintext.size()) {return false;}
    auto raw=std::span(scratch.plaintext).subspan(reservation.rawWriteOffset);
    if(raw.size()<f4::account::layout::kObjectSize || p.changed.size()+p.removed.size()+2>prepared.objects.size()) {return false;}
    prepared.rawClearSize=(std::max)(reservation.rawClearSize,reservation.rawWriteOffset+f4::account::layout::kObjectSize);
    std::size_t extent=reservation.compressedWriteOffset,count{};
    const auto fail=[&]() {snapshot::clear_after(scratch,reservation);return false;};
    constexpr auto instanceId=middleware::datagen::kItemInstanceObjectId;
    for(const auto soid:p.removed) {prepared.objects[count++]={instanceId,soid,middleware::queuez::Encoding::oodle,{}};}
    for(const auto soid:p.changed) {
        f4::instance::ResolvedInstance instance{};bool found{};
        for(std::size_t i=0;i<selected.loadout.itemCount;++i) if(selected.loadout.items[i].instance.instanceSoid==soid) {
            instance=selected.loadout.items[i].instance;found=true;break;
        }
        if(!found) for(std::size_t i=0;i<account->profileItemCount;++i) if(account->profileItems[i].instanceSoid==soid) {
            found=snapshot::resolve_profile_item_instance(account->profileItems[i],instance);break;
        }
        auto bytes=raw.first(f4::instance::layout::kObjectSize);
        if(!found || !f4::instance::encode(instance,bytes)
            || !snapshot::append_object(scratch,bytes,instanceId,soid,prepared.objects[count++],extent)) {return fail();}
    }
    auto bytes=raw.first(f4::character::layout::kObjectSize);
    if(!f4::character::encode(account->characters[p.character],selected.loadout,selected.lightEvaluation,bytes,account.get())) {return fail();}
    auto& changes=reinterpret_cast<f4::character::layout::Object*>(bytes.data())->inventoryChanges;
    std::uint16_t changed{};
    for(std::size_t i=0;i<selected.loadout.itemCount;++i) {
        const auto& item=selected.loadout.items[i];
        if(std::find(p.changed.begin(),p.changed.end(),item.instance.instanceSoid)==p.changed.end()) {continue;}
        bool acquired=true;
        for(std::size_t j=0;j<p.before.characters[p.character].inventory.count;++j) {
            const auto& before=p.before.characters[p.character].inventory.values[j];
            if(before.instanceSoid==item.instance.instanceSoid) {
                acquired=item.quantity>before.quantity;
                // Recovery preserves quantity and SOID; the new carried location
                // and mutation serial identify the acquisition instead.
                for(std::size_t k=0;before.postmaster && k<account->characters[p.character].inventory.count;++k) {
                    const auto& after=account->characters[p.character].inventory.values[k];
                    if(after.instanceSoid==before.instanceSoid) {acquired|=!after.postmaster;break;}
                }
                break;
            }
        }
        if(!acquired) {continue;}
        if(changed==changes.records.size()) {return fail();}
        auto& record=changes.records[changed];record.sequence=changed++;record.mutationSerial=item.mutationSerial;record.kind=1;
    }
    changes.writeSlot=changes.nextSequence=changed;
    if(!snapshot::append_object(scratch,bytes,update.characterDefinitionId,update.characterSoid,prepared.objects[count++],extent)) {return fail();}
    bytes=raw.first(f4::account::layout::kObjectSize);
    if(!f4::account::encode(*account,bytes)) {return fail();}
    auto& profile=reinterpret_cast<f4::account::layout::Object*>(bytes.data())->profileInventoryChanges;changed=0;
    for(std::size_t i=0;i<account->profileItemCount;++i) {
        const auto& item=account->profileItems[i];bool acquired=item.quantity>0;
        for(std::size_t j=0;j<p.before.profileItemCount;++j) if(p.before.profileItems[j].definitionHash==item.definitionHash) {
            acquired=item.quantity>p.before.profileItems[j].quantity;break;
        }
        if(!acquired) {continue;}if(changed==profile.records.size()) {return fail();}
        auto& record=profile.records[changed];record.sequence=changed++;record.mutationSerial=item.mutationSerial;record.kind=1;
    }
    profile.writeSlot=profile.nextSequence=changed;
    if(!snapshot::append_object(scratch,bytes,middleware::datagen::kAccountObjectId,account->primarySoid,prepared.objects[count++],extent)) {return fail();}
    prepared.compressedClearSize=(std::max)(reservation.compressedClearSize,extent);
    prepared.family={4,update.after.family4RootSoid,update.after.family4Version,0,std::span(prepared.objects).first(count)};
    return queuez_frame::append(scratch,prepared.family,prepared.rawClearSize,prepared.compressedClearSize,key,nonce,response,written);
}
}
