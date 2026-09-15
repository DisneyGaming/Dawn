#include <Windows.h>
#include <limits>
#include <memory>
#include <cwchar>
#include <algorithm>
#include "../../../build_data/runtime.h"
#include "quest_runtime.h"
#include "../../../vendors/persistence.h"
#include "../../../account/inventory/placement.h"
#include "../../../runtime/state_account_transaction_helpers.h"
#include "../../../runtime/storage/internal.h"
#include "../../../../core/filesystem/path.h"

namespace sunrise::state {
namespace q=activity::newlight::launchpad::quest;
namespace detail=runtime::detail;
namespace {
struct ProgressRow {std::uint64_t character{},pursuit{};std::uint32_t step{},reserved{};};
struct Progress {
    std::uint32_t magic{0x514C4E53U},version{1};std::uint64_t account{};
    std::array<ProgressRow,kCharacterCapacity> rows{};std::uint64_t checksum{};
};
static_assert(std::is_trivially_copyable_v<Progress>);
core::path::Buffer progressPath;
Progress progress;
std::uint64_t checksum(const Progress& p) noexcept {
    auto h=UINT64_C(14695981039346656037);const auto* bytes=reinterpret_cast<const unsigned char*>(&p);
    for(std::size_t i=0;i<offsetof(Progress,checksum);++i) {h=(h^bytes[i])*UINT64_C(1099511628211);}
    return h;
}
bool persist(const PendingNewlightQuest& p) noexcept {
    if(!progressPath.length) {return true;} // Pure offline fixtures have no module or disk path.
    auto next=progress;
    if(next.account!=p.accountSoid) {return false;}
    std::size_t at=next.rows.size();
    for(std::size_t i=0;i<next.rows.size();++i) if(next.rows[i].character==p.characterSoid) {at=i;break;}
    if(at==next.rows.size()) for(std::size_t i=0;i<next.rows.size();++i) if(!next.rows[i].character) {at=i;break;}
    if(at==next.rows.size()) {return false;}
    const auto& old=next.rows[at];
    if(old.character && (old.pursuit!=p.questInstanceSoid || old.step>p.expectedStep)) {return false;}
    next.rows[at]={p.characterSoid,p.questInstanceSoid,static_cast<std::uint32_t>(p.expectedStep)+1,0};next.checksum=checksum(next);
    auto temporary=progressPath;static unsigned sequence{};wchar_t suffix[64]{};
    std::swprintf(suffix,std::size(suffix),L".%08X.%08X.%08X.tmp",GetCurrentProcessId(),GetCurrentThreadId(),++sequence);
    if(!core::path::append(temporary,suffix)) {return false;}
    const auto file=CreateFileW(temporary.chars.data(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {return false;}
    DWORD written{};bool ok=WriteFile(file,&next,sizeof next,&written,nullptr) && written==sizeof next && FlushFileBuffers(file);
    CloseHandle(file);
    if(ok) {ok=MoveFileExW(temporary.chars.data(),progressPath.chars.data(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}
    if(!ok) {DeleteFileW(temporary.chars.data());return false;}
    progress=next;return true;
}
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
        const auto& inventory=checked.afterCharacter.inventory;
        const bool recovery=std::any_of(inventory.values.begin(),inventory.values.begin()+inventory.count,
            [](const auto& item){return item.postmaster;});
        ok=vendors::persistence::active() || recovery?vendors::persistence::save(*candidate,recovery):persist(checked);
        if(ok) {g_state.account=*candidate;}
    }
    ReleaseSRWLockExclusive(&g_stateLock);p={};return ok;
}
bool prepare_newlight_start(AccountState& account) noexcept {
    if(!account::valid(account)) {return false;}
    auto candidate=account;
    for(std::size_t ci=0;ci<candidate.characterCount;++ci) {
        auto& character=candidate.characters[ci];if(q::step(character)!=0) {continue;}
        for(const auto slot:q::kEscapeSlots) {character.equipment.slots[static_cast<std::size_t>(slot)].reset();}
        std::size_t kept{};
        for(std::size_t i=0;i<character.inventory.count;++i) {
            const auto item=character.inventory.values[i];build_data::items::Definition definition{};
            build_data::inventory::buckets::Descriptor bucket{};
            if(!build_data::find_item_definition_hash(item.definitionHash,definition)
                || !build_data::find_inventory_bucket_descriptor(definition.bucketId,bucket)) {return false;}
            // Native equipment rows 10/11 are ship/Sparrow; row 9 is a weapon.
            if(bucket.equipmentSlot==10 || bucket.equipmentSlot==11) {continue;}
            character.inventory.values[kept++]=item;
        }
        std::fill(character.inventory.values.begin()+kept,character.inventory.values.end(),account::inventory::Item{});
        character.inventory.count=kept;
    }
    if(!account::valid(candidate)) {return false;}
    account=candidate;return true;
}
bool restore_newlight_quests(void* module,AccountState& account) noexcept {
    progressPath={};progress={};progress.account=account.primarySoid;
    if(!module) {return true;}
    wchar_t name[80]{};
    std::swprintf(name,std::size(name),L"\\Sunrise\\newlight-progress-%016llX.bin",static_cast<unsigned long long>(account.primarySoid));
    if(!core::path::module_directory(module,progressPath) || !core::path::append(progressPath,name)) {return false;}
    const auto file=CreateFileW(progressPath.chars.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {return GetLastError()==ERROR_FILE_NOT_FOUND;}
    Progress saved{};DWORD size{};LARGE_INTEGER length{};
    const bool read=GetFileSizeEx(file,&length) && length.QuadPart==sizeof saved
        && ReadFile(file,&saved,sizeof saved,&size,nullptr) && size==sizeof saved;
    CloseHandle(file);
    if(!read || saved.magic!=progress.magic || saved.version!=1 || saved.account!=account.primarySoid
        || checksum(saved)!=saved.checksum) {return false;}
    auto candidate=account;
    for(std::size_t r=0;r<saved.rows.size();++r) {
        const auto& row=saved.rows[r];if(!row.character) {continue;}
        if(!row.pursuit || !row.step || row.step>5 || row.reserved) {return false;}
        for(std::size_t i=0;i<r;++i) if(saved.rows[i].character==row.character) {return false;}
        for(std::size_t ci=0;ci<candidate.characterCount;++ci) {
            auto& character=candidate.characters[ci];if(character.soid!=row.character) {continue;}
            auto current=q::step(character);
            // An abandoned/replaced Pursuit or a later authored step remains authoritative.
            if(current<0 || static_cast<std::uint32_t>(current)>=row.step) {continue;}
            bool same=false;for(std::size_t i=0;i<character.inventory.count;++i)
                if(character.inventory.values[i].instanceSoid==row.pursuit
                    && character.inventory.values[i].definitionHash==q::kSteps[current]) {same=true;}
            if(!same) {continue;}
            for(std::size_t i=0;i<candidate.characterCount;++i) {candidate.characters[i].selected=i==ci;}
            while(static_cast<std::uint32_t>(current)<row.step) {
                PendingNewlightQuest next{};
                if(!build(candidate,static_cast<std::uint8_t>(current),next)) {return false;}
                character=next.afterCharacter;
                if(next.profileChanged) {candidate.profileItems=next.afterProfile;candidate.profileItemCount=next.afterProfileCount;}
                ++current;
            }
            for(std::size_t i=0;i<candidate.characterCount;++i) {candidate.characters[i].selected=account.characters[i].selected;}
        }
    }
    if(!account::valid(candidate)) {return false;}
    progress=saved;account=candidate;return true;
}
}
