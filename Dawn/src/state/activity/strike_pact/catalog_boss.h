// Tree of Probabilities (strike_pact), Valus Thuun and the three boss rooms: registry
// 0xA5F083B5 (object 80F54E07), held region 0. Every slot index, member count, hash, tag and
// bound here is read out of the SDK export of scenario 80F54AE7; every ordering, count,
// threshold and interval is carried verbatim from the mission Lua that already runs this
// encounter (dawn-strike-missions/strike_pact/boss.lua, with music.lua for the sections and
// lib/strike_encounters.lua for what the roster order means), and the line that carries one
// names the file it came from. Nothing here is inferred from a sibling area.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "catalog.h"
#include "catalog_ledge.h"
#include "../coo/objective_service.h"
namespace dawn::state::activity::strike_pact {

// The SDK stamps slot ids with object tags only; registry keys come from the accepted Dawn
// host's own published roster. "slot/80f54e07/..." is 0xA5F083B5, and so on.
inline constexpr std::uint32_t kBoss=0xA5F083B5U, kBossTag=0x80F54E07U;
// The approach trigger and route.lua's ap_boss_room live in their own group (80F54E12); the
// boss room's authored navigation points live in a third (80F54E1D). boss.lua reaches into
// both, so neither is a kBoss slot.
// kBossApproach / kBossApproachTag are declared once, in catalog_ledge.h.
inline constexpr std::uint32_t kBossPoints=0xC6F46FAFU, kBossPointsTag=0x80F54E1DU;
// state/80f54ae7/0000/0000/80f54ad3: slice set and region index are both 0, hash DA8AE171.
// boss.on_held ignores every held region but this one.
inline constexpr int kBossRegion=0;
inline constexpr std::uint8_t kBossBubble=0;
inline constexpr std::uint32_t kBossStateHash=0xDA8AE171U;

// music.lua sections, selected by boss.lua. music.select is monotonic, so these only ever climb.
inline constexpr std::uint8_t kBossMusicIntro=10, kBossMusicRoom2=12, kBossMusicRoom3=14,
    kBossMusicDead=16;
// A room row that publishes no cue, no monitor or no directive carries these sentinels.
inline constexpr std::uint8_t kNoBossCue=0xFFU;
inline constexpr std::uint16_t kNoBossMonitor=0xFFFFU;

// ---------------------------------------------------------------------------------------
// 1. The encounter this whole section costs against
// ---------------------------------------------------------------------------------------
// obj_bomb_boss = slot/80f54e07/000001, index 1, type 3, component 80808348. boss.lua builds
// its controller as encounters.new("pact.boss", OBJ_BOMB_BOSS, 17, roster): seventeen authored
// task groups, so a task cost outside 0..16 is not read. mission.Task is empty in the export,
// so 17 is the proven Lua's count and the only one available. obj_bomb (index 0) is the ledge
// encounter's objective and is deliberately not this one.
inline constexpr std::uint16_t kBossObjectiveSlot=1;
inline constexpr std::uint8_t kBossObjectiveType=3, kBossObjectiveGroups=17, kBossObjectiveRevision=1;

// m_engagement_sensor = slot/80f54e07/0000b0, index 176, type 70, component 808094EE. Every
// boss directive is addressed here; boss.lua show_objective arms it with flags 0, revision 1
// before the first Defeat Valus Thuun publication and never changes it again.
inline constexpr std::uint16_t kBossAudience=176;
inline constexpr std::uint8_t kBossAudienceType=70, kBossAudienceFlags=0, kBossAudienceRevision=1;

// ---------------------------------------------------------------------------------------
// 2. Thuun himself, and the Minotaur that shares his reveal
// ---------------------------------------------------------------------------------------
// sq_boss = slot/80f54e07/00002e, index 46, type 1, component 80809A3B, squad row 160558
// (spawner 80F54BFA, rule 80F54B26). Its one authored member category is actor_class 80F58163.
// sq_boss__val_thooun = slot/80f54e07/00002f, index 47, type 2, component 8080834E, dynamic.
// boss.lua places the type-1 squad with counts = {0} and reserved = true: the zero placement
// requests nobody but still opens the host row and lease, so the combat-objective assignment
// and the death report have a squad to name. The actor itself is created off the type-2
// combatant sensor - boss.lua begin() calls spawn_actor{generation = 1} on it - so it is that
// binding, not the squad placement, that puts Thuun in the world. Every later health reading
// is rejected unless event.spawn_revision is that same generation 1.
inline constexpr std::uint16_t kBossSquad=46, kBossActor=47;
inline constexpr std::uint8_t kBossSquadType=1, kBossActorType=2;
inline constexpr std::uint32_t kBossActorGeneration=1;
// The scenario also authors sq_story_boss (index 48) and its own sq_story_boss__val_thooun
// (index 49), an identical actor_class 80F58163 pair for the story mission. boss.lua asserts on
// the strike pair only: the story boss and the story Scene are separate occurrences.
inline constexpr std::uint16_t kStoryBossSquad=48, kStoryBossActor=49;

// sq_minotaur_intro = slot/80f54e07/0000a7, index 167, type 1, squad row 160679 (spawner
// 80F54D65, rule 80F54B18), one member category, actor_class 80F45DA7. It is the Scene's second
// cast member and has an ordinary squad-only performer binding.
inline constexpr std::uint16_t kMinotaurSquad=167;

// scene_minotaur = slot/80f54e07/0000a8, index 168, type 43, component 80806382, scene row
// 160680. mission.Scene gives its config_tag and resource_tag; the reveal activates this sensor
// and lets native performer readiness (DB0C90) wait on both actors' attachment, which is why
// boss.lua queues the Scene rather than waiting for two host Sense reports first.
inline constexpr std::uint16_t kSceneMinotaur=168;
inline constexpr std::uint8_t kSceneType=43;
inline constexpr std::uint32_t kSceneMinotaurConfigTag=0x80F54D68U, kSceneMinotaurResourceTag=0x80F582FEU;
// scene_minotaur_story (index 169, config 80F54D6B, the same resource) belongs to the story
// mission and is never activated here.
inline constexpr std::uint16_t kSceneMinotaurStory=169;
inline constexpr std::uint32_t kSceneMinotaurStoryConfigTag=0x80F54D6BU;

// ho_invincible = slot/80f54e07/0000a6, index 166, type 26, component 8080953F, dynamic. The
// authored hop-on attachment: its health layer absorbs the parent's damage while attached, so
// boss.lua attaches it to the boss squad for every crossing and detaches it on the player's
// arrival in the next room. Only the crossing makes him untouchable; he takes damage from the
// start of each room's fight. of_invincible (index 180, type 34) is a different authored
// component and the proven graph never touches it.
inline constexpr std::uint16_t kHopOnInvincible=166;
inline constexpr std::uint8_t kHopOnInvincibleType=26;

// Each room consumes one third of full health. The native damage boundary clamps
// heavy hits to these floors and the controller retains immunity through each crossing.
inline constexpr float kBossGateRoom1=2.F/3.F, kBossGateRoom2=1.F/3.F;

// ---------------------------------------------------------------------------------------
// 3. The three rooms
// ---------------------------------------------------------------------------------------
// OBJ_BOMB_BOSS task groups resolve to firing areas in one room each, so the boss may only take
// groups of the room he currently fights in; boss.lua's roster gives him a function of the
// retained state that returns the current room's list. Order inside a room's list is boss.lua's.
inline constexpr std::array<std::uint8_t,8> kRoom1Groups{4,5,6,7,8,9,10,15};
inline constexpr std::array<std::uint8_t,6> kRoom2Groups{2,3,11,12,13,16};
inline constexpr std::array<std::uint8_t,3> kRoom3Groups{0,1,14};

// The six type-23 gate devices, in room order: d_gate_room1_entry .. d_gate_room3_exit, indices
// 120..125, all component 80804F45 - the same native device component as the opening and Forest
// shields. Position 1 is a gate's authored physical body and position 0 removes it, so the
// native "open" transition blocks and "close" permits passage. boss.lua prepare_gates powers on
// and unlocks all six with snap, then snaps every entry closed (passable) and every exit open
// (blocking); an exit only transitions to close once its room is a qualified clear.
inline constexpr std::uint8_t kGateType=23;
inline constexpr float kGateBlocking=1.F, kGatePassable=0.F;

// Everything one room owns. `firstAdd`/`lastAdd` are inclusive 1-based indices into the roster
// below - boss.lua's own indices, because the encounter controller keys its retained
// requested/seen/ready/cleared masks on 1 << (index - 1), so the numbering is load-bearing.
// `shield` names the room's shield Vex by the same index (0 when the room authors none).
struct BossRoom final {
    std::uint8_t index;
    std::span<const std::uint8_t> groups;
    std::uint8_t arrivalGroup;
    std::uint16_t entry,exit,monitor;
    std::uint8_t firstAdd,lastAdd,shield;
    std::uint8_t retreatCue,arrivalCue;
    std::uint32_t directive;
    coo::Asset directiveTarget;
    bool directiveOnRetreat;
    std::uint32_t spawnSet;
    std::uint8_t music;
};
// A zero-registry directive target means the live Thuun actor: boss.lua show_directive follows
// the actor itself through his native scene and his room changes rather than a fixed position.
inline constexpr coo::Asset kBossActorTarget{};
// The export names slot 1 of 80F54E1D ap_room2 and never recovered aliases for slots 2 and 7.
// boss.lua aims the room-1 approach at slot 7 (ap_room1) and the room-3 gauntlet at slot 2
// (ap_room3); ap_room2 itself is authored but the proven graph never publishes it.
inline constexpr std::uint8_t kBossPointType=47;
inline constexpr coo::Asset kApRoom1{kBossPoints,kBossPointsTag,kBossPointType,7};
inline constexpr coo::Asset kApRoom2{kBossPoints,kBossPointsTag,kBossPointType,1};
inline constexpr coo::Asset kApRoom3{kBossPoints,kBossPointsTag,kBossPointType,2};

inline constexpr std::array<BossRoom,3> kBossRooms{{
    // Room 1. boss.lua's rooms[1] entry carries no monitor, cue, directive, spawn set or music
    // of its own - the room is entered through the approach trigger, not a player monitor - so
    // each of those four comes from a different call site and is recorded here at its value:
    // the checkpoint from on_player_trigger, the directive from show_objective inside begin(),
    // the cue and the room's lasers from start_room1 after the Scene finishes, and the music
    // from reveal(). Its exit needs both the retreat to room 2 and a qualified clear of all
    // fifteen adds; neither condition substitutes for the other.
    {1,kRoom1Groups,15,120,121,kNoBossMonitor,3,17,17,kNoBossCue,22,
        kDefeatThuun,kBossActorTarget,false,0x23BAC4D9U,kBossMusicIntro},
    // Room 2, the laser gauntlet. pm_room2 = slot/80f54e07/0000b2, index 178, type 30, over the
    // authored tv_room2 box below. retreat() plays row 24 as he crosses; start_room() takes the
    // checkpoint, raises the music, publishes Evade Vex defenses at ap_room3 and plays row 25.
    {2,kRoom2Groups,3,122,123,178,18,32,32,24,25,
        kEvadeVex,kApRoom3,false,0x61D967C9U,kBossMusicRoom2},
    // Room 3. pm_room3 = slot/80f54e07/0000b3, index 179, type 30. Here the directive and the
    // cue are published at the retreat rather than on the player's arrival, and this is the only
    // room whose exit is gated on DEFEATED instead of on a retreat that never comes.
    {3,kRoom3Groups,1,124,125,179,33,39,0,26,kNoBossCue,
        kDefeatThuun,kBossActorTarget,true,0x61D967C8U,kBossMusicRoom3},
}};
[[nodiscard]] constexpr const BossRoom* boss_room(std::uint8_t index) noexcept {
    for(const auto& r:kBossRooms) { if(r.index==index) { return &r; } } return nullptr;
}
// strike_encounters.lua rejects a task group outside the objective and rejects a duplicate, and
// boss.lua assigns each room's arrival group explicitly, so it has to be one of that room's own.
static_assert([] { for(const auto& r:kBossRooms) {
    bool arrival=false;
    for(std::size_t i=0;i<r.groups.size();++i) {
        if(r.groups[i]>=kBossObjectiveGroups) { return false; }
        for(std::size_t j=0;j<i;++j) { if(r.groups[j]==r.groups[i]) { return false; } }
        if(r.groups[i]==r.arrivalGroup) { arrival=true; }
    }
    if(!arrival) { return false; } }
    return true; }());
// No group belongs to two rooms: a room's firing areas are its own, which is what lets the
// boss's task list follow him from room to room without ever reaching back into the last one.
static_assert([] { for(const auto& a:kBossRooms) { for(const auto& b:kBossRooms) {
    if(a.index==b.index) { continue; }
    for(const auto g:a.groups) { for(const auto h:b.groups) { if(g==h) { return false; } } } } }
    return true; }());

// ---------------------------------------------------------------------------------------
// 4. The roster, in boss.lua's exact order
// ---------------------------------------------------------------------------------------
// Reuses catalog.h's Spawn. Every squad here carries exactly one entry in the SDK's
// mission.Squad members[], so each is one native category with one loose request and no named
// second member; count = loose + second. boss.lua overrides counts on the boss alone, so every
// other row keeps the spawner's authored per-category default, which the export does not carry
// - the same convention the Forest and Chase catalogs use.
// Cohorts are the order boss.lua requests them in, which is not the roster order: the prefight
// is placed while the player is still on the ledge, the two Scene participants at the fight's
// start, and each room's adds one gate ahead of the player.
inline constexpr std::uint8_t kCohortPrefight=1, kCohortParticipants=2, kCohortRoom1=3,
    kCohortRoom2=4, kCohortRoom3=5;
inline constexpr std::uint8_t kBossLastCohort=5;
// `required` mirrors the proven clear gates. Every prefight squad must clear before begin(), and
// every one of a room's adds must supply the helper's qualified clearance evidence before that
// room's exit becomes passable. The two Scene participants are never joined on for a clear:
// on_squad_state explicitly drops a "cleared" report for roster index 1, and the reveal joins on
// their requested/ready state instead, so neither can block a room.
// No boss squad is bootstrapped through a spawn-time tactical row: boss.lua drives every task
// through the encounter controller's assign_combat_objective, so tacticalRow stays -1 on all 45.
inline constexpr std::array<Spawn,45> kBossSpawns{{
    // 1-2: the Scene's cast. The boss is the only zero-count row in the strike.
    {kBossSquad,kBoss,0,0,0,1,kCohortParticipants,false,-1,0,"sq_boss"},
    {kMinotaurSquad,kBoss,1,0,1,1,kCohortParticipants,false,-1,0,"sq_minotaur_intro"},
    // 3-17: room 1's adds, then its Vex, then its shield Vex - boss.lua's join order.
    {50,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[0]"},
    {51,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[1]"},
    {52,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[2]"},
    {53,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[3]"},
    {54,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[4]"},
    {55,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[5]"},
    {56,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_adds[6]"},
    {73,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[0]"},
    {74,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[1]"},
    {75,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[2]"},
    {76,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[3]"},
    {77,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[4]"},
    {78,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[5]"},
    {79,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_vex[6]"},
    {57,kBoss,1,0,1,1,kCohortRoom1,true,-1,0,"sq_room1_shield_vex"}, // room 1's shield Vex
    // 18-32: room 2, same shape.
    {58,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[0]"},
    {59,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[1]"},
    {60,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[2]"},
    {61,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[3]"},
    {62,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[4]"},
    {63,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[5]"},
    {64,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_adds[6]"},
    {80,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[0]"},
    {81,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[1]"},
    {82,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[2]"},
    {83,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[3]"},
    {84,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[4]"},
    {85,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[5]"},
    {86,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_vex[6]"},
    {65,kBoss,1,0,1,1,kCohortRoom2,true,-1,0,"sq_room2_shield_vex"}, // room 2's shield Vex
    // 33-39: room 3 authors adds only. boss.lua asserts that exactly the first two rooms
    // declare a shield Vex, so the absence here is the encounter's own statement, not a gap.
    {66,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[0]"},
    {67,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[1]"},
    {68,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[2]"},
    {69,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[3]"},
    {70,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[4]"},
    {71,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[5]"},
    {72,kBoss,1,0,1,1,kCohortRoom3,true,-1,0,"sq_room3_adds[6]"},
    // 40-45: the prefight. Six authored skirmish squads occupying the first room and its
    // entrance; they take room 1's task groups, not OBJ_BOMB's outside ledge, and all six must
    // clear before the reveal. They are appended to this controller rather than given one of
    // their own so its retained masks also track them.
    {40,kBoss,1,0,1,1,kCohortPrefight,true,-1,0,"sq_prefight_skirmish[0]"},
    {41,kBoss,1,0,1,1,kCohortPrefight,true,-1,0,"sq_prefight_skirmish[1]"},
    {42,kBoss,1,0,1,1,kCohortPrefight,true,-1,0,"sq_prefight_skirmish[2]"},
    {43,kBoss,1,0,1,1,kCohortPrefight,true,-1,0,"sq_prefight_skirmish[3]"},
    {44,kBoss,1,0,1,1,kCohortPrefight,true,-1,0,"sq_prefight_skirmish[4]"},
    {45,kBoss,1,0,1,1,kCohortPrefight,true,-1,0,"sq_prefight_skirmish[5]"},
}};
static_assert([] { for(const auto& s:kBossSpawns) {
    if(s.count!=s.loose+s.second || s.second!=0 || s.categories!=1
        || s.cohort==0 || s.cohort>kBossLastCohort || s.tacticalRow!=-1) { return false; } }
    return true; }());
// strike_encounters.lua caps a roster at the 63-bit retained mask, and boss.lua's 1-based
// indices are this array's positions plus one.
static_assert(kBossSpawns.size()<=63);
// Exactly one reserved zero-count row, and it is the boss.
static_assert([] { std::size_t zero=0; for(const auto& s:kBossSpawns) { if(s.count==0) { ++zero; } }
    return zero==1 && kBossSpawns[0].source==kBossSquad && kBossSpawns[0].count==0; }());
// Each room's declared span covers the rows that actually carry its cohort, and a room that
// names a shield Vex names one inside its own span.
static_assert([] { for(const auto& r:kBossRooms) {
    if(r.firstAdd<1 || r.lastAdd<r.firstAdd || r.lastAdd>kBossSpawns.size()) { return false; }
    if(r.shield!=0 && (r.shield<r.firstAdd || r.shield>r.lastAdd)) { return false; } }
    return true; }());
// The prefight's roster span, in the same 1-based numbering the rooms use. Its two bounds
// happen to equal the SDK slot indices of sq_prefight_skirmish[0] and [5]; that is a
// coincidence of where the six squads sit in both lists, not a relation.
inline constexpr std::uint8_t kFirstPrefightRosterIndex=40, kLastPrefightRosterIndex=45;
static_assert(kBossSpawns[kFirstPrefightRosterIndex-1].cohort==kCohortPrefight
    && kBossSpawns[kLastPrefightRosterIndex-1].cohort==kCohortPrefight
    && kLastPrefightRosterIndex==kBossSpawns.size());
[[nodiscard]] constexpr const Spawn* boss_spawn(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& s:kBossSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr;
}
// The inclusive 1-based roster span a room's adds occupy, as a view of this array.
[[nodiscard]] constexpr std::span<const Spawn> room_adds(const BossRoom& room) noexcept {
    return std::span<const Spawn>{kBossSpawns}.subspan(room.firstAdd-1U,
        static_cast<std::size_t>(room.lastAdd-room.firstAdd+1U));
}

// ---------------------------------------------------------------------------------------
// 5. The security grid
// ---------------------------------------------------------------------------------------
// Two banks per room. The channel sensors are type 24 (component 80804F3B) and the laser bodies
// they drive are type 4 object sensors (component 80809927), the same native object component
// the Chase grid uses. Each channel drives one authored laser object, and the object exists only
// while its type-4 slot is active, so boss.lua activates a room's objects before it drives the
// bank and leaves them alive when it stops driving so the native fade-out can finish.
inline constexpr std::uint8_t kLaserChannelType=24, kBossLaserObjectType=4;
inline constexpr std::array<std::uint16_t,5> kLaserChannelsRm1B1{189,190,191,192,193};
inline constexpr std::array<std::uint16_t,5> kLaserChannelsRm1B2{194,195,196,197,198};
inline constexpr std::array<std::uint16_t,5> kLaserChannelsRm2B1{199,200,201,202,203};
inline constexpr std::array<std::uint16_t,5> kLaserChannelsRm2B2{204,205,206,207,208};
inline constexpr std::array<std::uint16_t,10> kLaserChannelsRm3B1{209,210,211,212,213,214,215,216,217,218};
inline constexpr std::array<std::uint16_t,10> kLaserChannelsRm3B2{219,220,221,222,223,224,225,226,227,228};
inline constexpr std::array<std::uint16_t,4> kLaserObjectsRm1B1{87,88,89,90};
inline constexpr std::array<std::uint16_t,5> kLaserObjectsRm1B2{91,92,93,94,95};
inline constexpr std::array<std::uint16_t,5> kLaserObjectsRm2B1{96,97,98,99,100};
inline constexpr std::array<std::uint16_t,4> kLaserObjectsRm2B2{101,102,103,104};
inline constexpr std::array<std::uint16_t,8> kLaserObjectsRm3B1{105,106,107,108,109,110,111,112};
inline constexpr std::array<std::uint16_t,7> kLaserObjectsRm3B2{113,114,115,116,117,118,119};
// Banks are numbered 1 and 2 exactly as the authored names ch_laser_rm<r>_b<b>[n] are, and both
// ordinal runs are dense from 0, so a run's position in these arrays is its authored ordinal.
struct LaserBank final {
    std::uint8_t room,bank;
    std::span<const std::uint16_t> channels,objects;
};
inline constexpr std::array<LaserBank,6> kBossLaserBanks{{
    {1,1,kLaserChannelsRm1B1,kLaserObjectsRm1B1},
    {1,2,kLaserChannelsRm1B2,kLaserObjectsRm1B2},
    {2,1,kLaserChannelsRm2B1,kLaserObjectsRm2B1},
    {2,2,kLaserChannelsRm2B2,kLaserObjectsRm2B2},
    {3,1,kLaserChannelsRm3B1,kLaserObjectsRm3B1},
    {3,2,kLaserChannelsRm3B2,kLaserObjectsRm3B2},
}};
[[nodiscard]] constexpr const LaserBank* laser_bank(std::uint8_t room,std::uint8_t bank) noexcept {
    for(const auto& b:kBossLaserBanks) { if(b.room==room && b.bank==bank) { return &b; } } return nullptr;
}
// Forty authored channel slots against thirty-three authored laser objects. boss.lua names the
// difference exactly - "Seven authored laser declarations target a null object and expose no
// native channel" - and resolves a bank once at runtime by keeping only the channel slots whose
// object_channel_count is above zero.
inline constexpr std::size_t kLaserChannelSlots=40, kLaserObjectSlots=33, kDeadLaserChannels=7;
static_assert([] { std::size_t c=0,o=0; for(const auto& b:kBossLaserBanks) { c+=b.channels.size();o+=b.objects.size(); }
    return c==kLaserChannelSlots && o==kLaserObjectSlots && c-o==kDeadLaserChannels; }());
// Exact native descriptor channel counts: these seven declarations have no target. All other
// declarations in 189..228 expose one channel. Keep null declarations out of authority entirely.
[[nodiscard]] constexpr bool live_laser_channel(std::uint16_t slot) noexcept {
    return slot>=189 && slot<=228 && slot!=193 && slot!=208 && slot!=217 && slot!=218
        && slot!=226 && slot!=227 && slot!=228;
}

// boss.lua set_laser_channels writes the same value to every channel a resolved slot exposes.
// Its comment: "Channel values span -100..100: full drive on, none off."
inline constexpr std::int32_t kLaserOn=100, kLaserOff=0;
// One durable host clock drives both banks of every armed room together, so a newly prepared
// room joins the next rising edge rather than starting its own cycle and losing its warning.
// boss.lua's own note on these two numbers: the supplied footage shows roughly seven seconds
// fully red and a dark pause before the next yellow warning, so the high interval is the native
// 2.5-second warmup plus that 7-second hold and the low interval is the 1.5-second fade plus a
// 2-second dark hold. These are reconstructed host intervals, NOT constants recovered from the
// client profile or the SDK export.
inline constexpr std::uint32_t kLaserWarmupMs=2500, kLaserRedHoldMs=7000;
inline constexpr std::uint32_t kLaserFadeMs=1500, kLaserDarkHoldMs=2000;
inline constexpr std::uint32_t kLaserHighMs=kLaserWarmupMs+kLaserRedHoldMs;   // 9500
inline constexpr std::uint32_t kLaserLowMs=kLaserFadeMs+kLaserDarkHoldMs;     // 3500
static_assert(kLaserHighMs==9500 && kLaserLowMs==3500);

// ---------------------------------------------------------------------------------------
// 6. Volumes, the chest and the arrival trigger
// ---------------------------------------------------------------------------------------
// Type-60 boxes exactly as mission.trigger_volumes exports them: the registry, the volume's own
// slot index and the authored min/max. The SDK carries no polygon, only these bounds.
inline constexpr std::array<Volume,2> kBossVolumes{{
    // tv_reached_boss, named pt_reached_boss in the export. boss.on_held fires its trigger the
    // moment region 0 is held; the crossing itself takes the room-1 checkpoint and, while the
    // prefight is still alive, publishes Find the map at ap_room1.
    {kBossApproach,4,"pt_reached_boss",{1592.71692F,-1492.88623F,5.5F},{1623.F,-1471.5F,14.8846169F}},
    // tv_room2. The export gives it the name and pm_room2 is this registry's only type-30
    // monitor with a matching name, but the monitor-to-volume edge itself is not exported.
    {kBoss,232,"tv_room2",{1697.74951F,-1541.28516F,-11.9999924F},{1797.03528F,-1448.28015F,28.0000076F}},
}};
[[nodiscard]] constexpr const Volume* boss_volume(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& v:kBossVolumes) { if(v.registry==registry && v.slot==slot) { return &v; } } return nullptr;
}
// pt_reached_boss = slot/80f54e12/000000, index 0, type 31 - the player trigger that owns the
// box above. It is the only trigger boss.lua arms; both rooms past the first are entered through
// their type-30 monitors, which are a distinct incident from a player trigger.
inline constexpr std::uint16_t kReachedBossTrigger=0;
inline constexpr std::uint8_t kBossTriggerType=31, kBossMonitorType=30;
// UNRESOLVED: pm_room3's type-60 box. mission.trigger_volumes exports six unnamed boxes in this
// registry (slots 233, 234, 293, 374, 375 and 376) and names none of them, so unlike tv_room2
// there is no way to choose one and no bounds are written for the room-3 monitor.

// pf_boss_chest.o_chest = slot/80f54e07/0000ac, index 172, type 4. boss.on_held deactivates it
// as part of arming the encounter and defeat() activates it; it is the run's reward object.
inline constexpr std::uint16_t kBossChest=172;
inline constexpr std::uint8_t kBossChestType=4;

// ---------------------------------------------------------------------------------------
// 7. Ending
// ---------------------------------------------------------------------------------------
// lifetime_timer = slot/80fd25e7/000002, index 2, type 18, component 80809917. boss.lua starts
// a countdown on it at each ending phase so the client shows the remaining window.
// UNRESOLVED: this slot's registry key. Its object tag 80FD25E7 is the activity's own lifetime
// group and is not one of the thirteen keys the accepted host publishes for this scenario, so
// only the tag, index and type are written and no registry is guessed.
inline constexpr std::uint32_t kLifetimeTimerTag=0x80FD25E7U;
inline constexpr std::uint16_t kLifetimeTimerSlot=2;
inline constexpr std::uint8_t kLifetimeTimerType=18;

// boss.lua ending_stage, in order: allow loot and dialogue, enter native results, then request
// orbit. These are running native timers with durable host deadlines for the lifetime
// transitions; a phase with a zero duration schedules no successor. `state` is the native type-17
// lifetime state the phase selects - lifetime_states:at(n) is the state numbered n, and 6 is one
// of the three states that passes the native player-spawn gate.
struct EndingPhase final { std::uint8_t state; std::uint32_t milliseconds; std::string_view where; };
inline constexpr std::array<EndingPhase,3> kBossEndingPhases{{
    {6,30000,"boss.lua defeat - loot and dialogue window"},
    {7,30000,"boss.lua on_timer, leaving phase 6 - native results"},
    {8,0,"boss.lua on_timer, leaving phase 7 - orbit request, no further deadline"},
}};
static_assert(kBossEndingPhases.back().milliseconds==0);

// The cue rows boss.lua plays, as indices into catalog.h's kDialogueRows. Rows 20 and 21 bracket
// the Minotaur Scene and are played by boss.lua itself rather than driven by the Scene; row 27
// is the defeat line. Rows 22, 24, 25 and 26 already appear on their rooms above.
inline constexpr std::uint8_t kCueReveal=20, kCueSceneFinished=21, kCueRoom1=22, kCueRetreatRoom2=24,
    kCueRoom2=25, kCueRetreatRoom3=26, kCueBossDead=27;
} // namespace dawn::state::activity::strike_pact
