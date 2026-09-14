#include <Windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "../src/core/logging/log.h"
#include "../src/core/settings/settings.h"
#include "../src/state/persistence/persistence.h"
#include "../src/state/runtime/state.h"
#include "../src/state/activity/progress/mission_progress.h"
#include "../src/state/activity/nightfall/completion_reward.h"
#include "../vendor/sqlite/sqlite3.h"

#define CHECK(expression) do { if (!(expression)) { std::fprintf(stderr,"CHECK failed at line %d: %s\n",__LINE__,#expression); std::abort(); } } while (false)

namespace sunrise::core::log {
void write(Channel,Level,std::string_view) noexcept {}
void early(std::string_view) noexcept {}
Settings defaults() noexcept { return {}; }
}

namespace {
using namespace sunrise::state;

AccountState make_account() {
    AccountState value{};
    value.primarySoid=0xF100000000000001ULL;
    value.characterCount=2;
    auto& character=value.characters[0];
    character.soid=0xF100000000000002ULL;character.race=CharacterRace::awoken;
    character.gender=CharacterGender::female;character.characterClass=CharacterClass::hunter;
    character.level=20;character.accepted=true;character.previewAvailable=true;
    character.appearanceValue=2.5F;character.lastOrbitedDestination=0xF1234567U;
    character.inventory.count=1;character.nextInventorySerial=12;
    auto& item=character.inventory.values[0];item.instanceSoid=0x4000000000000100ULL;
    item.definitionHash=0xF2345678U;item.level=200;item.quantity=1;item.mutationSerial=11;
    item.flags=account::inventory::kLockedItemFlag;item.sockets.policy=account::inventory::SocketPolicy::authored;
    item.sockets.plugCount=2;item.sockets.plugs[0]=0xF3456789U;
    value.profileItemCount=1;value.profileItems[0].definitionHash=0xF456789AU;
    value.profileItems[0].quantity=100;value.profileItems[0].mutationSerial=4;
    auto& settings=value.settings;settings.configured=true;settings.keyBindings.configured=true;
    settings.controls.mouseLookSensitivity=1;settings.controls.adsSensitivityModifier=1.0F;
    settings.audio.migrationVersion=account::settings::kCompletedAudioMigrationVersion;
    settings.display.calibrationPrimary=10000.0F;
    value.characters[1].soid=0xF100000000000003ULL;
    value.characters[1].characterClass=CharacterClass::warlock;
    return value;
}

std::filesystem::path database_path() {
    wchar_t path[32768]{};const DWORD length=GetModuleFileNameW(nullptr,path,32768);CHECK(length>0&&length<32768);
    return std::filesystem::path(path).parent_path()/L"Sunrise"/L"player-state.db";
}

void remove_database() {
    const auto path=database_path();
    CHECK(path.parent_path().filename()==L"Sunrise");
    CHECK(path.parent_path().parent_path().filename()==L"persistence");
    std::error_code ignored;std::filesystem::remove(path,ignored);
    std::filesystem::remove(path.wstring()+L"-wal",ignored);std::filesystem::remove(path.wstring()+L"-shm",ignored);
}

void run_fresh_process_verifier() {
    wchar_t executable[32768]{};CHECK(GetModuleFileNameW(nullptr,executable,32768)>0);
    std::wstring command=L"\"";command+=executable;command+=L"\" --verify";
    STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION process{};
    CHECK(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=FALSE);
    CHECK(WaitForSingleObject(process.hProcess,30000)==WAIT_OBJECT_0);DWORD exitCode=1;
    CHECK(GetExitCodeProcess(process.hProcess,&exitCode)!=FALSE);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    CHECK(exitCode==0);
}
}

int main(int argc,char**) {
    using namespace sunrise::state;
    namespace durable=sunrise::state::persistence;
    std::printf("sizeof(AccountState)=%zu sizeof(State)=%zu sizeof(ScopedTable)=%zu "
                "sizeof(Family5State)=%zu sizeof(ActivityDefaults)=%zu\n",
                sizeof(AccountState),sizeof(State),sizeof(unlocks::ScopedTable),
                sizeof(Family5State),sizeof(activity::defaults::ActivityDefaults));
    // Exercise the shipped JSON fixture through the production parser before handing its exact
    // authored account/unlock/investment structures to the first-import path.
    const auto fixturePath=std::filesystem::path(__FILE__).parent_path().parent_path()
        /L"resources"/L"default_settings.json";
    std::ifstream fixture(fixturePath,std::ios::binary);CHECK(fixture.good());
    const std::string fixtureJson{std::istreambuf_iterator<char>{fixture},
                                  std::istreambuf_iterator<char>{}};
    sunrise::core::settings::Settings fixtureSettings{};
    CHECK(sunrise::core::settings::parse(fixtureJson,fixtureSettings));
    CHECK(fixtureSettings.initialAccount.primarySoid==0x9EAA300100100100ULL);
    CHECK(fixtureSettings.initialAccount.characterCount==3);
    CHECK(fixtureSettings.initialAccount.characters[0].soid==0x9EAA300100100101ULL);
    CHECK(std::count(fixtureSettings.initialUnlocks.accountFlags.begin(),
                     fixtureSettings.initialUnlocks.accountFlags.end(),
                     unlocks::kFlagSet)!=0);
    CHECK(fixtureSettings.initialFamily5.flagCount!=0);

    const AccountState legacy=make_account();CHECK(account::valid(legacy));
    unlocks::Table legacyUnlocks{};legacyUnlocks.accountFlags[120]=unlocks::kFlagSet;
    legacyUnlocks.characterObjectValues[33]=77;legacyUnlocks.characterProgressions[8][2]=19;
    Family5State family{};family.flagCount=1;family.flags[0]={91,2};family.valueCount=1;family.values[0]={17,900};
    AccountState loaded{};unlocks::ScopedTable unlocks{};Family5State loadedFamily{};
    if(argc>1) {
        CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
        CHECK(loaded.profileItems[0].quantity==100&&loaded.characters[0].inventory.values[0].flags==0);
        bool found=false;durable::MissionRecord mission{};
        CHECK(durable::load_mission(loaded.characters[0].soid,0xF9876543U,found,mission));
        CHECK(found&&mission.checkpointSliceSet==41);durable::shutdown();return 0;
    }
    CHECK(account::valid(fixtureSettings.initialAccount));
    remove_database();
    CHECK(durable::initialize(GetModuleHandleW(nullptr),fixtureSettings.initialAccount,
        fixtureSettings.initialUnlocks,fixtureSettings.initialFamily5,loaded,unlocks,loadedFamily));
    CHECK(loaded==fixtureSettings.initialAccount);
    CHECK(unlocks==unlocks::expand(fixtureSettings.initialUnlocks,
                                   fixtureSettings.initialAccount));
    CHECK(loadedFamily==fixtureSettings.initialFamily5);
    durable::shutdown();
    remove_database();
    CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    CHECK(loaded.primarySoid==legacy.primarySoid&&loaded.characters[0].inventory.values[0].flags==1);
    CHECK(unlocks.accountFlags[120]==2&&unlocks.characters[0].objectValues[33]==77);
    CHECK(unlocks.characters[0].progressions[8][2]==19&&loadedFamily.values[0].value==900);
    CHECK(unlocks.characters[1].objectValues[33]==77);

    namespace mission_progress=sunrise::state::activity::progress;
    mission_progress::reset();
    for(int index=0;index<40;++index) {
        const std::string package="test_mission_"+std::to_string(index);
        CHECK(mission_progress::observe(loaded.characters[0].soid,package,index+1,
            0xF0000000U+static_cast<std::uint32_t>(index),index,index,false));
    }
    CHECK(mission_progress::observe(loaded.characters[0].soid,"test_mission_complete",90,
        0xF1230000U,50,9,true));
    CHECK(mission_progress::observe(loaded.characters[0].soid,"test_mission_complete",90,
        0xF1230001U,51,1,false));
    bool observerFound=false;durable::MissionRecord observerRecord{};
    CHECK(durable::load_mission(loaded.characters[0].soid,
        mission_progress::mission_key("test_mission_complete"),observerFound,observerRecord));
    CHECK(observerFound&&observerRecord.completed);

    // A failed durable write remains retryable and is never mistaken for a cached success.
    sqlite3* blockedWriter{};CHECK(sqlite3_open16(database_path().c_str(),&blockedWriter)==SQLITE_OK);
    CHECK(sqlite3_exec(blockedWriter,"BEGIN IMMEDIATE",nullptr,nullptr,nullptr)==SQLITE_OK);
    CHECK(!mission_progress::observe(loaded.characters[0].soid,"test_mission_retry",91,
        0xF1230002U,52,2,false));
    CHECK(sqlite3_exec(blockedWriter,"ROLLBACK",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_close(blockedWriter);
    CHECK(mission_progress::observe(loaded.characters[0].soid,"test_mission_retry",91,
        0xF1230002U,52,2,false));

    CHECK(durable::store_flag(durable::Scope::characterObject,loaded.characters[0].soid,99,2));
    durable::MissionRecord mission{};mission.characterSoid=loaded.characters[0].soid;
    mission.missionHash=0xF9876543U;mission.checkpointHash=0xF8765432U;mission.checkpointSliceSet=41;
    mission.activityIndex=7;mission.progress=3;mission.completed=true;mission.updatedUtc=1000;
    CHECK(durable::store_mission(mission));
    AccountState after=loaded;after.characters[0].inventory.values[0].flags=0;
    CHECK(durable::commit_account(loaded,after));loaded=after;
    std::uint64_t next{};CHECK(durable::next_item_instance_soid(loaded,next));
    CHECK(next>0x4000000000000100ULL);
    durable::shutdown();run_fresh_process_verifier();
    CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));

    // Closing a competing writer with an uncommitted WAL transaction must recover the prior
    // durable image on a genuinely fresh persistence open.
    sqlite3* rival{};CHECK(sqlite3_open16(database_path().c_str(),&rival)==SQLITE_OK);
    CHECK(sqlite3_exec(rival,"BEGIN IMMEDIATE;UPDATE profile_items SET quantity=777 WHERE position=0",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_close(rival);durable::shutdown();
    CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    CHECK(loaded.profileItems[0].quantity==100);

    // A second connection advances the revision. This writer must reject its stale snapshot and
    // roll every table replacement back.
    CHECK(sqlite3_open16(database_path().c_str(),&rival)==SQLITE_OK);
    CHECK(sqlite3_exec(rival,"UPDATE metadata SET value=value+1 WHERE key='account_revision'",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_close(rival);
    AccountState rejected=loaded;rejected.profileItems[0].quantity=999;
    CHECK(!durable::commit_account(loaded,rejected));
    durable::shutdown();

    AccountState changedLegacy=legacy;changedLegacy.profileItems[0].quantity=1;
    unlocks::Table changedUnlocks{};Family5State changedFamily{};
    CHECK(durable::initialize(GetModuleHandleW(nullptr),changedLegacy,changedUnlocks,changedFamily,loaded,unlocks,loadedFamily));
    CHECK(loaded.profileItems[0].quantity==100&&loaded.characters[0].inventory.values[0].flags==0);
    bool found=false;std::uint8_t flag{};CHECK(durable::load_flag(durable::Scope::characterObject,loaded.characters[0].soid,99,found,flag));
    CHECK(found&&flag==2);
    CHECK(durable::load_flag(durable::Scope::characterObject,loaded.characters[1].soid,99,found,flag));
    CHECK(!found);
    durable::MissionRecord restored{};CHECK(durable::load_mission(loaded.characters[0].soid,mission.missionHash,found,restored));
    CHECK(found&&restored.completed&&restored.checkpointSliceSet==41&&restored.activityIndex==7);

    // Identity rebases may overlap the previous namespace (P->C0, C0->C1). Every scoped
    // row must move exactly once rather than being remapped again by a later update.
    CHECK(durable::store_flag(durable::Scope::characterObject,loaded.characters[1].soid,99,1));
    const std::uint64_t oldPrimary=loaded.primarySoid;
    const std::uint64_t oldCharacter0=loaded.characters[0].soid;
    const std::uint64_t oldCharacter1=loaded.characters[1].soid;
    AccountState rebased=loaded;
    rebased.primarySoid=oldCharacter0;
    rebased.characters[0].soid=oldCharacter1;
    rebased.characters[1].soid=oldCharacter1+1;
    CHECK(durable::commit_account(loaded,rebased));loaded=rebased;
    CHECK(durable::load_flag(durable::Scope::account,loaded.primarySoid,120,found,flag));
    CHECK(found&&flag==unlocks::kFlagSet);
    CHECK(durable::load_flag(durable::Scope::characterObject,loaded.characters[0].soid,99,found,flag));
    CHECK(found&&flag==2);
    CHECK(durable::load_flag(durable::Scope::characterObject,loaded.characters[1].soid,99,found,flag));
    CHECK(found&&flag==1);
    CHECK(!durable::load_flag(durable::Scope::account,oldPrimary,120,found,flag));
    CHECK(durable::load_mission(loaded.characters[0].soid,mission.missionHash,found,restored));
    CHECK(found&&restored.completed);

    // Completion is an earned-history bit: a replay checkpoint must never clear it.
    restored.completed=false;restored.progress=1;restored.updatedUtc=1001;
    CHECK(durable::store_mission(restored));
    CHECK(durable::load_mission(loaded.characters[0].soid,mission.missionHash,found,restored));
    CHECK(found&&restored.completed);

    // A terminal creates its debt and completion together. Exact callbacks deduplicate inside one
    // process epoch, while the same process-local counters after a restart name a legitimate replay.
    durable::MissionRecord rewardedMission{};
    rewardedMission.characterSoid=loaded.characters[0].soid;
    rewardedMission.missionHash=0xF9876500U;rewardedMission.activityIndex=808;
    rewardedMission.progress=9;rewardedMission.updatedUtc=1100;
    CHECK(durable::store_mission(rewardedMission));
    durable::RewardDebt firstDebt{},duplicateDebt{};
    CHECK(durable::offer_reward(loaded.primarySoid,loaded.characters[0].soid,0xA001,7,
        rewardedMission.missionHash,0xBC53E66EU,1000,1101,firstDebt));
    CHECK(durable::offer_reward(loaded.primarySoid,loaded.characters[0].soid,0xA001,7,
        rewardedMission.missionHash,0xBC53E66EU,1000,1102,duplicateDebt));
    CHECK(firstDebt.debtId==duplicateDebt.debtId);
    CHECK(durable::load_mission(loaded.characters[0].soid,rewardedMission.missionHash,found,restored));
    CHECK(found&&restored.completed);
    namespace reward=sunrise::state::activity::nightfall::rewards;
    reward::Ticket retryTicket{};
    for(std::size_t retry=0;retry<reward::detail::kCapacity+4;++retry) {
        CHECK(reward::claim(0xA001,loaded.primarySoid,retryTicket));
        reward::release(retryTicket);
    }
    CHECK(reward::claim(0xA001,loaded.primarySoid,retryTicket));
    reward::release(retryTicket);
    reward::clear();
    durable::shutdown();
    CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    durable::RewardDebt replayDebt{};
    CHECK(durable::offer_reward(loaded.primarySoid,loaded.characters[0].soid,0xA001,7,
        rewardedMission.missionHash,0xBC53E66EU,1000,1200,replayDebt));
    CHECK(replayDebt.debtId!=firstDebt.debtId&&replayDebt.runtimeEpoch!=firstDebt.runtimeEpoch);
    CHECK(!durable::finish_reward(firstDebt.debtId,1));
    CHECK(durable::finish_reward(firstDebt.debtId,0));
    CHECK(durable::finish_reward(replayDebt.debtId,0));

    durable::RewardDebt creditedDebt{};
    CHECK(durable::offer_reward(loaded.primarySoid,loaded.characters[0].soid,0xA001,8,
        rewardedMission.missionHash,0xBC53E66EU,1000,1201,creditedDebt));
    AccountState creditedAccount=loaded;
    creditedAccount.profileItems[0].quantity+=1000;
    ++creditedAccount.profileItems[0].mutationSerial;
    CHECK(durable::commit_account_and_reward(loaded,creditedAccount,creditedDebt.debtId,1000));
    loaded=creditedAccount;
    durable::RewardDebt pending{};
    CHECK(durable::load_pending_reward(loaded.primarySoid,found,pending)&&!found);

    AccountState dismantled=loaded;dismantled.characters[0].inventory.values[0]={};
    dismantled.characters[0].inventory.count=0;
    CHECK(durable::commit_account(loaded,dismantled));
    durable::shutdown();

    CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    CHECK(loaded.characters[0].inventory.count==0);
    CHECK(durable::next_item_instance_soid(loaded,next)&&next>0x4000000000000100ULL);
    durable::shutdown();

    // SQLite's conversion APIs narrow permissively; reject out-of-range stored values instead
    // of wrapping malformed durable data into a valid-looking runtime field.
    CHECK(sqlite3_open16(database_path().c_str(),&rival)==SQLITE_OK);
    CHECK(sqlite3_exec(rival,"UPDATE characters SET movement_ability=256 WHERE position=0",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_close(rival);
    CHECK(!durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    CHECK(sqlite3_open16(database_path().c_str(),&rival)==SQLITE_OK);
    CHECK(sqlite3_exec(rival,"UPDATE characters SET movement_ability=0 WHERE position=0",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_close(rival);
    CHECK(durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    durable::shutdown();

    // A known-newer database is rejected without resetting its contents.
    CHECK(sqlite3_open16(database_path().c_str(),&rival)==SQLITE_OK);
    CHECK(sqlite3_exec(rival,"PRAGMA user_version=99",nullptr,nullptr,nullptr)==SQLITE_OK);sqlite3_close(rival);
    CHECK(!durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    CHECK(std::filesystem::file_size(database_path())>0);

    // An existing zero-byte file is treated as malformed rather than as a first-run database.
    remove_database();std::filesystem::create_directories(database_path().parent_path());
    { std::ofstream empty(database_path(),std::ios::binary); }
    CHECK(!durable::initialize(GetModuleHandleW(nullptr),legacy,legacyUnlocks,family,loaded,unlocks,loadedFamily));
    CHECK(std::filesystem::file_size(database_path())==0);
    remove_database();
    return 0;
}
