#include <Windows.h>
#include <limits>
#include <memory>
#include <algorithm>
#include "../../../build_data/runtime.h"
#include "quest_runtime.h"
#include "../../../persistence/persistence.h"
#include "../../../account/inventory/placement.h"
#include "../../../runtime/state_account_transaction_helpers.h"
#include "../../../runtime/storage/internal.h"

namespace sunrise::state {
namespace q=activity::newlight::launchpad::quest;
namespace detail=runtime::detail;
namespace {
bool credit(AccountState& account,q::Currency currency) noexcept {
    namespace bd=build_data;
    bd::items::Definition definition{};bd::items::details::Definition detail{};
    bd::inventory::buckets::Descriptor bucket{};
    if(!bd::find_item_definition_hash(currency.hash,definition)
        || !bd::find_configured_item_detail(definition.definitionIndex,detail)
        || detail.definitionHash!=currency.hash || detail.bucketId!=definition.bucketId
        || detail.instancedDefinitionState!=bd::items::details::InstancedDefinitionState::stackable
        || detail.maxStackSize<=0 || !bd::find_inventory_bucket_descriptor(detail.bucketId,bucket)
        || bucket.arraySelector!=bd::inventory::buckets::ArraySelector::profile
        || bd::is_profile_action_source(definition.definitionIndex,definition.bucketId)) {return false;}
    auto at=account.profileItemCount;std::int32_t serial{};
    for(std::size_t i=0;i<account.profileItemCount;++i) {
        const auto& item=account.profileItems[i];serial=(std::max)(serial,item.mutationSerial);
        if(item.definitionHash!=currency.hash) {continue;}
        if(at!=account.profileItemCount || item.instanceSoid || item.quantity<=0 || item.quantity>detail.maxStackSize) {return false;}
        at=i;
    }
    const bool append=at==account.profileItemCount;
    const auto quantity=append?0:account.profileItems[at].quantity;
    const auto amount=(std::min)(currency.quantity,detail.maxStackSize-quantity);
    if(!amount) {return true;}
    if(at>=account.profileItems.size() || serial==INT32_MAX) {return false;}
    account.profileItems[at]={0,currency.hash,quantity+amount,serial+1};
    if(append) {++account.profileItemCount;}return true;
}
bool award(AccountState& account,std::size_t ci,std::uint32_t hash,
           std::optional<account::inventory::EquipmentSlot> equip,PendingNewlightQuest& out) noexcept {
    auto& character=account.characters[ci];
    for(const auto& item:character.equipment.slots) if(item && item->definitionHash==hash) {return true;}
    for(std::size_t i=0;i<character.inventory.count;++i)
        if(character.inventory.values[i].definitionHash==hash && character.inventory.values[i].quantity>0) {return true;}
    if(out.rewardCount>=out.rewardInstances.size()) {return false;}
    account::inventory::Item item{};
    if(!detail::next_item_instance_soid(account,item.instanceSoid)) {return false;}
    item.definitionHash=hash;item.quantity=1;item.level=equip?0:detail::acquisition_level(character);
    item.mutationSerial=static_cast<std::int32_t>(character.nextInventorySerial++);
    item.sockets.policy=account::inventory::SocketPolicy::nativeDefaults;
    if(equip && !character.equipment.slots[static_cast<std::size_t>(*equip)]) {
        character.equipment.slots[static_cast<std::size_t>(*equip)]=item;out.equipmentChanged=true;
    } else {
        std::uint64_t removed{};
        if(!account::inventory::insert(character,item,removed)) {return false;}
        if(removed) {
            if(out.removedCount>=out.removedInstances.size()) {return false;}
            out.removedInstances[out.removedCount++]=removed;
        }
    }
    out.rewardInstances[out.rewardCount++]=item.instanceSoid;return true;
}
bool build(const AccountState& account,std::uint8_t expected,PendingNewlightQuest& out) noexcept {
    out={};
    if(expected>=5 || !account::valid(account) || !detail::valid_profile_inventory(account)) {return false;}
    std::size_t ci=account.characterCount;
    for(std::size_t i=0;i<account.characterCount;++i) if(account.characters[i].selected) {ci=i;break;}
    if(ci==account.characterCount) {return false;}
    const auto& before=account.characters[ci];
    if(q::step(before)!=expected || before.nextInventorySerial>INT32_MAX-3) {return false;}
    auto candidate=account;auto& after=candidate.characters[ci];
    for(std::size_t i=0;i<after.inventory.count;++i) {
        auto& item=after.inventory.values[i];
        if(item.postmaster || item.definitionHash!=q::kSteps[expected]) {continue;}
        if(!item.instanceSoid || item.quantity!=1) {return false;}
        out.questInstanceSoid=item.instanceSoid;item.definitionHash=q::kSteps[expected+1];
        item.mutationSerial=static_cast<std::int32_t>(after.nextInventorySerial++);
        item.sockets={};item.sockets.policy=account::inventory::SocketPolicy::nativeDefaults;
    }
    if(!out.questInstanceSoid) {return false;}
    if(expected==0) {
        // Persist the character's launch/completion flags with the quest and
        // rewards, before the Tower handoff. Abandoning the Pursuit cannot undo it.
        if(!q::record_escape(after)) {return false;}
        for(std::size_t i=0;i<q::kEscapeRewards.size();++i)
            if(!award(candidate,ci,q::kEscapeRewards[i],q::kEscapeSlots[i],out)) {return false;}
        for(const auto currency:q::kEscapeCurrencies) if(!credit(candidate,currency)) {return false;}
    } else if(expected>=2 && !award(candidate,ci,q::kIntroductions[expected-2].reward,{},out)) {return false;}
    middleware::datagen::family4::loadout::ResolvedLoadout resolved{};
    if(!account::valid(candidate) || !detail::valid_profile_inventory(candidate)
        || !middleware::datagen::family4::loadout::resolve(candidate,ci,resolved)) {return false;}
    out.beforeCharacter=before;out.afterCharacter=after;out.accountSoid=account.primarySoid;
    out.characterSoid=before.soid;out.characterIndex=ci;out.expectedStep=expected;
    out.beforeProfile=account.profileItems;out.beforeProfileCount=account.profileItemCount;
    out.afterProfile=candidate.profileItems;out.afterProfileCount=candidate.profileItemCount;
    out.profileChanged=!detail::same_profile_views(out.beforeProfile,out.beforeProfileCount,out.afterProfile,out.afterProfileCount);
    out.prepared=true;return true;
}
bool matches(const AccountState& current,const PendingNewlightQuest& p,PendingNewlightQuest& checked) noexcept {
    return p.prepared && current.primarySoid==p.accountSoid && p.characterIndex<current.characterCount
        && p.characterSoid==p.beforeCharacter.soid
        && detail::same_character(current.characters[p.characterIndex],p.beforeCharacter)
        && (!p.profileChanged || detail::same_profile_inventory(current,p.beforeProfile,p.beforeProfileCount))
        && build(current,p.expectedStep,checked) && checked.characterIndex==p.characterIndex
        && checked.questInstanceSoid==p.questInstanceSoid && checked.rewardInstances==p.rewardInstances
        && checked.rewardCount==p.rewardCount && checked.equipmentChanged==p.equipmentChanged
        && checked.removedCount==p.removedCount && checked.removedInstances==p.removedInstances
        && checked.profileChanged==p.profileChanged
        && (!p.profileChanged || detail::same_profile_views(checked.afterProfile,checked.afterProfileCount,p.afterProfile,p.afterProfileCount))
        && detail::same_character(checked.afterCharacter,p.afterCharacter);
}
}
bool prepare_newlight_quest(std::uint8_t expected,PendingNewlightQuest& p) noexcept {
    return build(account_snapshot(),expected,p);
}
bool preview_newlight_quest(const PendingNewlightQuest& p,AccountState& after) noexcept {
    after={};auto current=account_snapshot();PendingNewlightQuest checked{};
    if(!matches(current,p,checked)) {return false;}
    current.characters[p.characterIndex]=checked.afterCharacter;
    if(checked.profileChanged) {current.profileItems=checked.afterProfile;current.profileItemCount=checked.afterProfileCount;}
    after=current;return true;
}
bool commit_newlight_quest(PendingNewlightQuest& p) noexcept {
    using namespace runtime::storage;
    AcquireSRWLockExclusive(&g_stateLock);PendingNewlightQuest checked{};
    bool ok=matches(g_state.account,p,checked);
    if(ok) {
        auto candidate=std::make_unique<AccountState>(g_state.account);
        candidate->characters[p.characterIndex]=checked.afterCharacter;
        if(checked.profileChanged) {candidate->profileItems=checked.afterProfile;candidate->profileItemCount=checked.afterProfileCount;}
        // Inventory, currencies and the Pursuit step share the existing SQLite transaction.
        ok=persistence::commit_account(g_state.account,*candidate);
        if(ok) {g_state.account=*candidate;}
    }
    ReleaseSRWLockExclusive(&g_stateLock);p={};return ok;
}
bool prepare_newlight_start(AccountState& account) noexcept {
    if(!account::valid(account)) {return false;}
    auto candidate=account;
    constexpr account::inventory::EquipmentSlot kUnequippedAtResurrection[]{
        account::inventory::EquipmentSlot::kinetic,
        account::inventory::EquipmentSlot::energy,
        account::inventory::EquipmentSlot::heavy,
        account::inventory::EquipmentSlot::vehicle,
        account::inventory::EquipmentSlot::ship,
    };
    for(std::size_t ci=0;ci<candidate.characterCount;++ci) {
        auto& character=candidate.characters[ci];if(q::step(character)!=0 || q::escaped(character)) {continue;}
        // Character selection still needs the Guardian's authored armor, Ghost,
        // subclass and identity rows. New Light begins without weapons or travel
        // gear; the mission grants those through its native pickups and rewards.
        for(const auto slot:kUnequippedAtResurrection) {
            character.equipment.slots[static_cast<std::size_t>(slot)].reset();
        }
        std::size_t kept{};
        for(std::size_t i=0;i<character.inventory.count;++i) {
            const auto item=character.inventory.values[i];
            // Keep only the Pursuit that opts this Guardian into Launchpad. The
            // authored test inventory must not leak weapons or completed gear
            // into the resurrection sequence.
            if(!item.postmaster && item.definitionHash==q::kEscapeCosmodrome) {
                character.inventory.values[kept++]=item;
            }
        }
        std::fill(character.inventory.values.begin()+kept,character.inventory.values.end(),account::inventory::Item{});
        character.inventory.count=kept;
    }
    if(!account::valid(candidate)) {return false;}
    account=candidate;return true;
}
}
