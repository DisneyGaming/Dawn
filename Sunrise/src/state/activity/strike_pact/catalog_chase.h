// Tree of Probabilities (strike_pact) Chase native identities. Recovered from the installed
// scenario 80F54AE7 through the Sunrise activity SDK export, with the cohort split, gating and
// checkpoint choices carried across from the mission logic that already runs this encounter
// (sunrise-strike-missions/strike_pact/{populations,route}.lua). Registry keys are the accepted
// Sunrise host's published roster, so a slot id's object tag selects the group it belongs to.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include "catalog.h"
#include "../coo/objective_service.h"
namespace sunrise::state::activity::strike_pact {

// Chase squads, objective, lasers and engagement audience (object 80F54FFE).
inline constexpr std::uint32_t kChase=0x588E5FB9U, kChaseTag=0x80F54FFEU;
// The trap corridor and its two branch triggers publish from their own group (object 80F55018),
// which also owns the Chase's authored destination point. Their volumes are not kChase rows.
inline constexpr std::uint32_t kChaseTriggers=0xFBD01A06U, kChaseTriggersTag=0x80F55018U;
// The fly-in entry trigger and the first room's approach point (object 80F55029).
inline constexpr std::uint32_t kChaseEntry=0x8E64DB66U, kChaseEntryTag=0x80F55029U;
// state/80f54ae7/0002/0000/80f54ad5: slice set 16, region 16. The state index doubles as the
// bubble, exactly as the opening's 000F pairs bubble 15 with region 120.
inline constexpr int kChaseRegion=16;
inline constexpr std::uint8_t kChaseBubble=2;
inline constexpr std::uint32_t kChaseStateHash=0x95E28335U;
// route.lua: a death respawns on the lifetime's spawn set, and a bubble without a set of its own
// would fall back to the opening's, so the Chase selects one on entry and again once the first
// room is a qualified clear. Neither hash is in the SDK export; both are route.lua's.
inline constexpr std::uint32_t kChaseEntrySpawnSet=0xC803F1CCU, kChaseFirstRoomSpawnSet=0x4DA08186U;

// populations.lua builds one Chase roster in this order and starts it in three requests:
// firstroom on the enter_chase stage, its reinforcements only after firstroom is fully cleared,
// and the conflict group on either sparrow stage. Cohort 0 stays invalid, as in catalog.h.
inline constexpr std::uint8_t kCohortFirstRoom=1, kCohortReinforcements=2, kCohortConflict=3;
inline constexpr std::uint8_t kChaseLastCohort=3;

// Reuses catalog.h's Spawn. count = loose + second, one loose request per authored member
// category: the SDK export carries the categories but not the spawner's per-category default
// request, and populations.lua overrides no Chase count (unlike the opening's named anchor).
// A category whose actor_class is unexported still occupies its category; no Chase squad has a
// type-2 named-member sibling, so nothing here is a bound combatant.
// required mirrors the proven gates: populations.on_squad_state waits on every firstroom squad
// before requesting the reinforcements, then on every reinforcement before it reports
// "firstroom_cleared". The conflict group is never joined on, so it can never block the route.
inline constexpr std::array<Spawn,30> kChaseSpawns{{
    {0,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[0]"},
    {1,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[1]"},
    {2,kChase,1,1,2,2,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[2]"},
    {3,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[3]"},
    {4,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[4]"},
    {5,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[5]"},
    {6,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_cabal_firstroom[6]"},
    {10,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[0]"},
    {11,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[1]"},
    {12,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[2]"},
    {13,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[3]"},
    {14,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[4]"},
    {15,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[5]"},
    {16,kChase,1,0,1,1,kCohortFirstRoom,true,-1,0,"sq_vex_firstroom[6]"},
    {7,kChase,1,0,1,1,kCohortReinforcements,true,-1,0,"sq_cabal_reinforcements[0]"},
    {8,kChase,1,0,1,1,kCohortReinforcements,true,-1,0,"sq_cabal_reinforcements[1]"},
    {9,kChase,1,0,1,1,kCohortReinforcements,true,-1,0,"sq_cabal_reinforcements[2]"},
    {17,kChase,1,0,1,1,kCohortReinforcements,true,-1,0,"sq_vex_reinforcements[0]"},
    {18,kChase,1,0,1,1,kCohortReinforcements,true,-1,0,"sq_vex_reinforcements[1]"},
    {19,kChase,1,0,1,1,kCohortReinforcements,true,-1,0,"sq_vex_reinforcements[2]"},
    {20,kChase,1,0,1,1,kCohortConflict,false,-1,0,"sq_conflict_snipers[0]"},
    {21,kChase,1,0,1,1,kCohortConflict,false,-1,0,"sq_conflict_snipers[1]"},
    {22,kChase,1,0,1,1,kCohortConflict,false,-1,0,"sq_conflict_goons_top[0]"},
    {23,kChase,1,0,1,1,kCohortConflict,false,-1,0,"sq_conflict_goons_top[1]"},
    {24,kChase,1,1,2,2,kCohortConflict,false,-1,0,"sq_conflict_goons[0]"},
    {25,kChase,1,1,2,2,kCohortConflict,false,-1,0,"sq_conflict_goons[1]"},
    {26,kChase,1,1,2,2,kCohortConflict,false,-1,0,"sq_conflict_goons[2]"},
    {27,kChase,1,1,2,2,kCohortConflict,false,-1,0,"sq_conflict_goons[3]"},
    {28,kChase,1,0,1,1,kCohortConflict,false,-1,0,"sq_conflict_goons[4]"},
    {29,kChase,1,0,1,1,kCohortConflict,false,-1,0,"sq_conflict_goons[5]"},
}};
static_assert([] { for(const auto& s:kChaseSpawns) {
    if(s.count!=s.loose+s.second || s.count!=s.categories || s.cohort==0 || s.cohort>kChaseLastCohort) { return false; } }
    return true; }());
[[nodiscard]] constexpr const Spawn* chase_spawn(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& s:kChaseSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr;
}
struct ChaseEnemyReceipt final {
    std::uint64_t run{}; std::uint32_t actor{},owner{},generation{}; std::uint16_t source{}; std::uint32_t registry{};
    bool valid() const noexcept { return run!=0 && actor!=UINT32_MAX && owner!=UINT32_MAX && generation!=0 && registry==kChase; }
    friend bool operator==(const ChaseEnemyReceipt&,const ChaseEnemyReceipt&)=default;
};

// The laser grid, all type 4 and all in kChase. route.lua instantiates every one of them on
// Chase arrival and lets its native behavior run; nothing here schedules or phases them.
inline constexpr std::uint16_t kLaserType=4;
inline constexpr std::array<std::uint16_t,21> kChaseLasers{{
    31,32,33,34,35,36,37,38,39,40,41, // o_static_laser[0..10]
    42,43,44,45,46,47,48,49,50,51,    // o_moving_laser[0..9]
}};
// UNRESOLVED: the type-24 channel components CH_STATIC_LASER_0..14 (kChase slots 95..109) and
// CH_MOVING_LASER_0..14 (slots 110..124) outnumber the 11 static and 10 moving objects and the
// export carries no object-to-channel mapping or authored phase, so no timing is modelled here.

// Every Chase trigger route.lua arms, in its exact order: route.lua's per-area seen mask is
// indexed by this position, so reordering would re-key a live run's retained bits.
// Bounds are the authored type-60 boxes from mission.trigger_volumes; the SDK carries no polygon.
inline constexpr std::array<Volume,6> kChaseVolumes{{
    {kChaseEntry,2,"pt_enter_chase",{762.93927F,-1341.95166F,182.702866F},{784.267761F,-1321.81165F,202.702866F}},
    {kChase,138,"pt_sparrow_run",{878.398438F,-1690.88513F,138.23381F},{1276.87634F,-1319.4845F,188.23381F}},
    {kChaseTriggers,11,"pt_sparrow_jump",{879.020752F,-1462.68567F,158.23381F},{979.801697F,-1357.0061F,188.23381F}},
    {kChaseTriggers,10,"pt_trap_section",{1271.9469F,-1505.F,105.F},{1314.10059F,-1458.73682F,126.F}},
    {kChaseTriggers,12,"pt_hard_path",{1401.58948F,-1208.55664F,58.8833237F},{1461.64514F,-1144.99121F,98.8833237F}},
    {kChaseTriggers,13,"pt_easy_path",{1398.18896F,-1212.86804F,12.3773613F},{1471.23657F,-1134.22717F,52.3773613F}},
}};
// The type-31 player-trigger sensor each volume above belongs to, same order. A sensor always
// shares its volume's registry, so only the slot differs from the Volume row.
inline constexpr std::uint16_t kChaseTriggerType=31;
inline constexpr std::array<std::uint16_t,6> kChaseTriggerSensors{{1,126,1,2,4,3}};
static_assert(kChaseVolumes.size()==kChaseTriggerSensors.size());
// Stage names route.lua returns for each row, in the same order; the parent keys its encounter
// policy off these. Unlike the Forest and the ledge, the Chase arms no player monitor.
inline constexpr std::array<std::string_view,6> kChaseStages{{
    "enter_chase","sparrow_run","sparrow_jump","trap_section","hard_path","easy_path",
}};
static_assert(kChaseVolumes.size()==kChaseStages.size());

// Encounter presentation. The audience scopes every Chase directive; the objective owns the
// task rows the encounter controller costs against.
inline constexpr std::uint16_t kChaseAudience=93;      // m_engagement_sensor, type 70
inline constexpr std::uint16_t kChaseObjectiveSlot=30; // obj_chase, type 3
// populations.lua: encounters.new("pact.chase", OBJ_CHASE, 11, chase_roster). The export's
// mission.Task table is empty, so 11 is the proven Lua's value and the only one available.
inline constexpr std::uint8_t kChaseTaskGroups=11;

// Authored objective target points, both type 47. route.lua aims Eliminate Hostiles at the first
// room and everything after it at the boss room; the two points belong to different groups.
inline constexpr coo::Asset kApFightRoom{kChaseEntry,kChaseEntryTag,47,4};
inline constexpr coo::Asset kApBossRoom{kChaseTriggers,kChaseTriggersTag,47,6};

// route.lua's Chase ladder. advance_chase never lowers the step, so the order is the gate: the
// objective order follows the retail reference recording, not the trigger order.
struct DirectiveStep final { std::uint8_t step; std::uint32_t event; coo::Asset target; std::string_view source; };
inline constexpr std::array<DirectiveStep,3> kChaseSteps{{
    {1,kEliminateHostiles,kApFightRoom,"enter_chase"},      // also published on Chase arrival
    {2,kMountUp,kApBossRoom,"firstroom_cleared"},           // the qualified first-room clear
    {3,kFindLeader,kApBossRoom,"sparrow_run|sparrow_jump"}, // holds through traps and the ledge
}};

// route.lua cue_once rows, indices into catalog.h's kDialogueRows. The entry cue is published
// only while the ladder is still on step 1, so a fly-in never begins it.
inline constexpr std::uint8_t kCueEnterChase=10, kCueFirstRoomCleared=11, kCueSparrowJump=13;
} // namespace sunrise::state::activity::strike_pact
