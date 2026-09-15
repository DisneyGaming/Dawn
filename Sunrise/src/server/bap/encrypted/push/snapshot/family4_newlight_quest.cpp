#include <algorithm>
#include "internal.h"
#include "../queuez/queuez_update_frame.h"
#include "../../queuez/queuez_state_validation.h"
#include "../../../../../middleware/datagen/definitions.h"
#include "../../../../../middleware/datagen/family4/character/character_encoder.h"
#include "../../../../../middleware/datagen/family4/character/layout.h"
#include "../../../../../middleware/datagen/family4/account/account_encoder.h"
#include "../../../../../middleware/datagen/family4/account/layout.h"
#include "../../../../../middleware/secure_channel/runtime.h"

namespace sunrise::server::bap::encrypted::queuez {
bool stage_newlight_quest(const SessionState& before,const state::PendingNewlightQuest& p,NewlightQuest& out) noexcept {
    out={};EquipmentSwap base{};
    if(!p.prepared || p.rewardCount>p.rewardInstances.size() || p.removedCount>p.removedInstances.size() || p.accountSoid!=before.family4RootSoid
        || !stage_equipment_swap(before,p.characterSoid,base)
        || !middleware::datagen::object_id(kAccountFamilyType,middleware::datagen::kAccountSlot,out.accountDefinitionId)
        || !middleware::datagen::object_id(kAccountFamilyType,middleware::datagen::kItemInstanceSlot,out.itemInstanceDefinitionId)) {return false;}
    for(std::size_t i=0;i<p.rewardCount;++i) {
        if(!p.rewardInstances[i] || p.rewardInstances[i]==p.questInstanceSoid) {return false;}
        for(std::size_t j=0;j<i;++j) if(p.rewardInstances[i]==p.rewardInstances[j]) {return false;}
    }
    bool pursuit=false,account=false;
    for(std::size_t i=0;i<before.family4ResidentCount;++i) {
        const auto& row=before.family4Residents[i];
        for(std::size_t j=0;j<p.rewardCount;++j) if(row.objectSoid==p.rewardInstances[j]) {return false;}
        if(row.objectSoid==p.questInstanceSoid) {pursuit=row.definitionId==out.itemInstanceDefinitionId;}
        if(row.objectSoid==p.accountSoid) {account=row.definitionId==out.accountDefinitionId;}
    }
    if(!pursuit || !account) {return false;}
    static_cast<EquipmentSwap&>(out)=base;
    for(std::size_t n=0;n<p.removedCount;++n) {
        auto& after=out.after;auto at=after.family4ResidentCount;
        for(std::size_t i=0;i<after.family4ResidentCount;++i)
            if(after.family4Residents[i].objectSoid==p.removedInstances[n]
                && after.family4Residents[i].definitionId==out.itemInstanceDefinitionId) {at=i;break;}
        if(at==after.family4ResidentCount) {return false;}
        for(auto i=at+1;i<after.family4ResidentCount;++i) {after.family4Residents[i-1]=after.family4Residents[i];}
        after.family4Residents[--after.family4ResidentCount]={};
    }
    for(std::size_t i=0;i<p.rewardCount;++i) {
        if(out.after.family4ResidentCount>=out.after.family4Residents.size()) {return false;}
        out.after.family4Residents[out.after.family4ResidentCount++]={p.rewardInstances[i],out.itemInstanceDefinitionId};
    }
    return valid(out.after);
}
}

namespace sunrise::server::bap::encrypted::push {
bool append_newlight_quest_notification(Scratch& scratch,const queuez::NewlightQuest& update,
    const state::PendingNewlightQuest& p,std::span<const std::byte,state::kAesKeySize> key,
    std::span<const std::byte,state::kBapNonceSize> nonce,std::span<std::byte> response,std::size_t& written) noexcept {
    namespace f4=middleware::datagen::family4;
    state::AccountState account{};snapshot::Resolved selected{};
    if(update.characterSoid!=p.characterSoid || !state::preview_newlight_quest(p,account)
        || !snapshot::resolve(account,p.characterIndex,selected)
        || selected.characterObjectId!=update.characterDefinitionId
        || selected.itemInstanceObjectId!=update.itemInstanceDefinitionId
        || update.accountDefinitionId!=middleware::datagen::kAccountObjectId) {return false;}
    snapshot::Prepared prepared{};
    const auto reservation=snapshot::reserve_prior(scratch,prepared);
    if(reservation.rawWriteOffset>scratch.plaintext.size()) {return false;}
    auto raw=std::span(scratch.plaintext).subspan(reservation.rawWriteOffset);
    const auto rawSize=p.profileChanged?f4::account::layout::kObjectSize:f4::character::layout::kObjectSize;
    if(raw.size()<rawSize) {return false;}
    auto bytes=raw.first(f4::character::layout::kObjectSize);
    if(!f4::character::encode(account.characters[p.characterIndex],selected.loadout,selected.lightEvaluation,bytes,&account)) {return false;}
    auto& character=*reinterpret_cast<f4::character::layout::Object*>(bytes.data());
    f4::loadout::ResolvedInstances instances{};
    for(std::size_t i=0;i<selected.loadout.itemCount;++i) {
        const auto& item=selected.loadout.items[i];
        const bool pursuit=item.instance.instanceSoid==p.questInstanceSoid;
        const bool reward=std::find(p.rewardInstances.begin(),p.rewardInstances.begin()+p.rewardCount,item.instance.instanceSoid)
            !=p.rewardInstances.begin()+p.rewardCount;
        if(!pursuit && !reward) {continue;}
        if(instances.itemCount>=3 || (pursuit && item.equipped)) {return false;}
        const auto index=instances.itemCount++;
        instances.items[index]={item.equipmentSlot,item.instance};
        auto& changed=character.inventoryChanges.records[index];
        changed.sequence=static_cast<std::uint16_t>(index);changed.mutationSerial=item.mutationSerial;changed.kind=1;
    }
    const std::size_t expected=p.rewardCount+1;
    if(instances.itemCount!=expected) {return false;}
    character.inventoryChanges.writeSlot=static_cast<std::uint16_t>(expected);
    character.inventoryChanges.nextSequence=static_cast<std::uint16_t>(expected);
    prepared.rawClearSize=(std::max)(reservation.rawClearSize,reservation.rawWriteOffset+rawSize);
    std::size_t extent=reservation.compressedWriteOffset,cursor{};
    // Instances precede the referring character in the wire, even though its compressed
    // body is prepared first so the same raw buffer can be reused for each instance.
    if(!snapshot::append_object(scratch,bytes,update.characterDefinitionId,p.characterSoid,prepared.objects[expected],extent)
        || !snapshot::append_items(scratch,raw,update.itemInstanceDefinitionId,instances,0,prepared,cursor,extent)
        || cursor!=expected) {snapshot::clear_after(scratch,reservation);return false;}
    auto count=expected+1;
    if(p.profileChanged) {
        auto accountBytes=raw.first(f4::account::layout::kObjectSize);
        if(!f4::account::encode(account,accountBytes)) {snapshot::clear_after(scratch,reservation);return false;}
        auto& changes=reinterpret_cast<f4::account::layout::Object*>(accountBytes.data())->profileInventoryChanges;
        std::uint16_t changed{};
        for(std::size_t i=0;i<p.afterProfileCount;++i) {
            if(p.afterProfile[i].mutationSerial==p.beforeProfile[i].mutationSerial) {continue;}
            if(changed>=changes.records.size()) {snapshot::clear_after(scratch,reservation);return false;}
            auto& record=changes.records[changed];record.sequence=changed++;
            record.mutationSerial=p.afterProfile[i].mutationSerial;record.kind=1;
        }
        changes.writeSlot=changes.nextSequence=changed;
        if(!changed || !snapshot::append_object(scratch,accountBytes,update.accountDefinitionId,p.accountSoid,
            prepared.objects[count++],extent)) {snapshot::clear_after(scratch,reservation);return false;}
    }
    for(std::size_t i=0;i<p.removedCount;++i) {
        if(count>=prepared.objects.size()) {snapshot::clear_after(scratch,reservation);return false;}
        prepared.objects[count++]={update.itemInstanceDefinitionId,p.removedInstances[i],middleware::queuez::Encoding::oodle,{}};
    }
    prepared.compressedClearSize=(std::max)(reservation.compressedClearSize,extent);
    prepared.family={4,update.after.family4RootSoid,update.after.family4Version,0,std::span(prepared.objects).first(count)};
    return queuez_frame::append(scratch,prepared.family,prepared.rawClearSize,prepared.compressedClearSize,key,nonce,response,written);
}
bool append_newlight_appearance(Scratch& scratch,queuez::SessionState& after,const state::PendingNewlightQuest& p,
    std::span<const std::byte,state::kAesKeySize> key,std::array<std::byte,state::kBapNonceSize>& nonce,
    std::span<std::byte> response,std::size_t& written) noexcept {
    if(!p.equipmentChanged) {return true;}
    // Update the same resident appearance keys. Recreating them tears down ship bindings.
    if(after.family0Active) {
        queuez::CharacterAppearanceRefresh refresh{};snapshot::Prepared prepared{};
        if(!queuez::stage_character_appearance_refresh(after,p.characterSoid,refresh)
            || !snapshot::prepare_character_appearance_refresh(scratch,refresh,p.afterCharacter,p.characterIndex,10,false,prepared)
            || !queuez_frame::append(scratch,prepared.family,prepared.rawClearSize,prepared.compressedClearSize,key,nonce,response,written)) {return false;}
        middleware::secure_channel::advance_nonce(nonce);after=refresh.after;
    }
    if(after.family3Active) {
        queuez::RosterAppearanceRefresh refresh{};snapshot::Prepared prepared{};
        if(!queuez::stage_roster_appearance_refresh(after,p.characterSoid,true,refresh)
            || !snapshot::prepare_roster_appearance_refresh(scratch,refresh,p.afterCharacter,p.characterIndex,prepared)
            || !queuez_frame::append(scratch,prepared.family,prepared.rawClearSize,prepared.compressedClearSize,key,nonce,response,written)) {return false;}
        middleware::secure_channel::advance_nonce(nonce);after=refresh.after;
    }
    return true;
}
}
