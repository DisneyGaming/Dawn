// Tree of Probabilities (strike_pact), the bomb ledge and its Thresher dropship: registry
// 0xA5F083B5 (object 80F54E07), held region 0. Every slot index, member count, hash and bound
// here is read out of the SDK export of scenario 80F54AE7; every ordering, count, gate and
// revision is carried verbatim from the mission Lua that already runs this strike, and the line
// that carries one names the file it came from. Nothing here is inferred from a sibling area.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "catalog.h"
#include "../coo/objective_service.h"
namespace dawn::state::activity::strike_pact {

// The SDK stamps slot ids with object tags only; registry keys come from the accepted Dawn
// host's own published roster. "slot/80f54e07/..." is 0xA5F083B5, and so on.
inline constexpr std::uint32_t kLedge=0xA5F083B5U, kLedgeTag=0x80F54E07U;
// The boss approach group (80F54E12) owns pt_reached_boss and the ledge's authored destination
// point; the boss room points group (80F54E1D) owns pt_see_tree. route.lua's bomb area reaches
// into both, because trigger ownership is the held region and not the destination state.
inline constexpr std::uint32_t kBossApproach=0x73CBF939U, kBossApproachTag=0x80F54E12U;
inline constexpr std::uint32_t kBossRoomPoints=0xC6F46FAFU, kBossRoomPointsTag=0x80F54E1DU;
// state/80f54ae7/0000/0000/80f54ad3: slice set 0, region index 0, hash DA8AE171. The state index
// doubles as the bubble, exactly as the opening's 000F pairs bubble 15 with region 120.
inline constexpr int kLedgeRegion=0;
inline constexpr std::uint8_t kLedgeBubble=0;
inline constexpr std::uint32_t kLedgeStateHash=0xDA8AE171U;
// route.lua: a death respawns on the lifetime's spawn set, and a bubble without a set of its own
// would fall back to the opening's, so region 0 claims one the moment it is held. The hash is
// route.lua's LEDGE_SPAWNS; it is not in the SDK export. boss.lua claims a second set of its own
// at pt_reached_boss, which belongs to the boss section rather than here.
inline constexpr std::uint32_t kLedgeSpawnSet=0x3360C6BEU;
// music.lua: the ledge section of the authored music sensor, selected by route.lua when region 0
// is first held. music.select never lowers the section, so 10..16 (the boss controller's) can
// only follow it; those belong to the boss catalog and are not restated here.
inline constexpr std::uint8_t kLedgeMusicSection=8;

// ---------------------------------------------------------------------------------------
// 1. The ledge population
// ---------------------------------------------------------------------------------------
// populations.lua builds one ledge roster in this exact order and starts it in four requests.
// Cohort 0 stays invalid, as in catalog.h.
inline constexpr std::uint8_t kCohortLedgeArrival=1,   // placed the moment region 0 is held
    kCohortHarvesterCargo=2,                           // reserved, scene-requested passengers
    kCohortLedgeReinforcements=3,                      // released when the ship has landed
    kCohortHarvester=4,                                // the ship itself, at the ledge_final stage
    kCohortPrefightSkirmish=5;                         // boss.lua's, started while still on the ledge
inline constexpr std::uint8_t kLedgeLastCohort=5;

// Reuses catalog.h's Spawn. count = loose + second, one loose request per authored member
// category: the SDK export carries the categories but not the spawner's per-category default
// request, and populations.lua overrides no ledge count except the Harvester's. A category whose
// actor_class is unexported still occupies its category.
// required mirrors the proven joins: populations.on_squad_state waits on every arrival squad
// before it reports "ledge_cleared", and depart() waits on every cargo squad being *seen* before
// the ship may leave. The Vex reinforcements are never joined on, so they can never block.
// The row order is populations.lua's ledge_roster order (cabal ledge, vex ledge, cargo, vex
// reinforcements, ship = roster indices 1..37). The controller's retained masks are keyed by
// roster position, so reordering would re-key a live run's bits.
inline constexpr std::array<Spawn,43> kLedgeSpawns{{
    {2,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[0]"},
    {3,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[1]"},
    {4,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[2]"},
    {5,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[3]"},
    {6,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[4]"},
    {7,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[5]"},
    {8,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[6]"},
    {9,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[7]"},
    // The only two-category squads on the ledge; both faction lists pair them at the same anchor.
    {10,kLedge,1,1,2,2,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[8]"},
    {11,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[9]"},
    {12,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[10]"},
    {13,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[11]"},
    {14,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_cabal_ledge[12]"},
    {22,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[0]"},
    {23,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[1]"},
    {24,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[2]"},
    {25,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[3]"},
    {26,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[4]"},
    {27,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[5]"},
    {28,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[6]"},
    {29,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[7]"},
    {30,kLedge,1,1,2,2,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[8]"},
    {31,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[9]"},
    {32,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[10]"},
    {33,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[11]"},
    {34,kLedge,1,0,1,1,kCohortLedgeArrival,true,-1,0,"sq_vex_ledge[12]"},
    // The ship's cargo. populations.lua marks exactly these five reserved and scene-requested.
    {15,kLedge,1,0,1,1,kCohortHarvesterCargo,true,-1,0,"sq_cabal_reinforcement[0]"},
    {16,kLedge,1,0,1,1,kCohortHarvesterCargo,true,-1,0,"sq_cabal_reinforcement[1]"},
    {17,kLedge,1,0,1,1,kCohortHarvesterCargo,true,-1,0,"sq_cabal_reinforcement[2]"},
    {18,kLedge,1,0,1,1,kCohortHarvesterCargo,true,-1,0,"sq_cabal_reinforcement[3]"},
    {19,kLedge,1,0,1,1,kCohortHarvesterCargo,true,-1,0,"sq_cabal_reinforcement[4]"},
    // Ordinary ground reinforcements: requested when the ship's arrival program completes, in
    // the same handler as the passenger delivery, but placed the normal way.
    {35,kLedge,1,0,1,1,kCohortLedgeReinforcements,false,-1,0,"sq_vex_reinforcement[0]"},
    {36,kLedge,1,0,1,1,kCohortLedgeReinforcements,false,-1,0,"sq_vex_reinforcement[1]"},
    {37,kLedge,1,0,1,1,kCohortLedgeReinforcements,false,-1,0,"sq_vex_reinforcement[2]"},
    {38,kLedge,1,0,1,1,kCohortLedgeReinforcements,false,-1,0,"sq_vex_reinforcement[3]"},
    {39,kLedge,1,0,1,1,kCohortLedgeReinforcements,false,-1,0,"sq_vex_reinforcement[4]"},
    // populations.lua: counts = {0}. The named pilot owns the vehicle, so a loose request would
    // put a second ship on the ledge. Zero loose, one named member, as catalog.h's sq_cabal_anchor
    // writes it; the ship's own lifecycle is driven by actor-path feedback, never by a squad
    // clear, so it is not a join and cannot become a barrier.
    {20,kLedge,0,0,0,1,kCohortHarvester,false,-1,0,"sq_harvester"},
    // boss.lua appends these six to its own "pact.boss" roster against obj_bomb_boss, with the
    // first room's task groups, and starts them from boss.on_held while the player is still on
    // the ledge. They are authored in this registry but the ledge's own controller never requests
    // them; a ledge controller must leave this cohort disabled.
    {40,kLedge,1,0,1,1,kCohortPrefightSkirmish,true,-1,0,"sq_prefight_skirmish[0]"},
    {41,kLedge,1,0,1,1,kCohortPrefightSkirmish,true,-1,0,"sq_prefight_skirmish[1]"},
    {42,kLedge,1,0,1,1,kCohortPrefightSkirmish,true,-1,0,"sq_prefight_skirmish[2]"},
    {43,kLedge,1,0,1,1,kCohortPrefightSkirmish,true,-1,0,"sq_prefight_skirmish[3]"},
    {44,kLedge,1,0,1,1,kCohortPrefightSkirmish,true,-1,0,"sq_prefight_skirmish[4]"},
    {45,kLedge,1,0,1,1,kCohortPrefightSkirmish,true,-1,0,"sq_prefight_skirmish[5]"},
}};
static_assert([] { for(const auto& s:kLedgeSpawns) {
    if(s.count!=s.loose+s.second || s.registry!=kLedge
       || s.cohort==0 || s.cohort>kLedgeLastCohort) { return false; } }
    return true; }());
[[nodiscard]] constexpr const Spawn* ledge_spawn(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& s:kLedgeSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr;
}
struct LedgeEnemyReceipt final {
    std::uint64_t run{}; std::uint32_t actor{},owner{},generation{}; std::uint16_t source{}; std::uint32_t registry{};
    bool valid() const noexcept { return run!=0 && actor!=UINT32_MAX && owner!=UINT32_MAX && generation!=0 && registry==kLedge; }
    friend bool operator==(const LedgeEnemyReceipt&,const LedgeEnemyReceipt&)=default;
};

// The reserved cargo, kept as its own list as well as its own cohort: populations.lua does not
// place these itself. It reserves their host rows with the scene-request placement mode and the
// ship's delivery component then creates and releases the actors, so their clear evidence is a
// zero population with the member-removal flag rather than a consumed request.
inline constexpr std::array<std::uint16_t,5> kHarvesterCargoSources{{15,16,17,18,19}};
// The ordinary reinforcements, placed with the plain replace mode against the same objective.
inline constexpr std::array<std::uint16_t,5> kLedgeReinforcementSources{{35,36,37,38,39}};
[[nodiscard]] constexpr bool reserved_cargo(std::uint16_t source) noexcept {
    for(const auto s:kHarvesterCargoSources) { if(s==source) { return true; } } return false;
}
[[nodiscard]] constexpr std::span<const std::uint16_t> cohort_sources(std::uint8_t cohort) noexcept {
    if(cohort==kCohortHarvesterCargo) { return kHarvesterCargoSources; }
    if(cohort==kCohortLedgeReinforcements) { return kLedgeReinforcementSources; }
    return {};
}
// UNRESOLVED: the authored default per-category member counts. strike_encounters.lua takes them
// from the live squad handle (handle.default_counts) and populations.lua's cargo() only manifests
// a reserved squad whose total is above zero, so which of the five cargo rows actually carries
// passenger demand cannot be decided from the SDK export.
// UNRESOLVED: the numeric value of the scene-request placement mode. The proven Lua names it
// symbolically (context.sdk.squad_modes.scene_request) and neither the export nor Dawn carries
// the enum, so only the fact that the cargo uses it is recorded here.

// ---------------------------------------------------------------------------------------
// 2. The Thresher and its authored point sequences
// ---------------------------------------------------------------------------------------
// sq_harvester       = slot/80f54e07/000014, index 20, type 1,  component 80809A3B.
// sq_harvester__pilot= slot/80f54e07/000015, index 21, type 2,  component 8080834E (dynamic).
// cps_harvester_entry= slot/80f54e07/000127, index 295, type 58, component 80807D9B.
// cps_harvester_exit = slot/80f54e07/000128, index 296, type 58, component 80807D9B.
inline constexpr std::uint16_t kHarvesterSource=20;
inline constexpr std::uint8_t kHarvesterSourceType=1;
inline constexpr std::uint16_t kHarvesterPilot=21;
inline constexpr std::uint8_t kHarvesterPilotType=2;
inline constexpr std::uint16_t kHarvesterEntryPath=295, kHarvesterExitPath=296;
inline constexpr std::uint8_t kPointSequenceType=58;
// UNRESOLVED: the point lists behind the two type-58 sequences. The SDK export gives each row a
// slot id and component class only (sense and auth schemas are both -1), so the entry transform
// and the exit curve are referenced but their geometry is not available here.

// ---------------------------------------------------------------------------------------
// 3. The flight recipe, exactly as populations.lua publishes it
// ---------------------------------------------------------------------------------------
// populations.lua lines 75-79: this actor's action definition 80C0E599 puts these names in its
// exact "dropship" group; an empty group fails the native lookup, it is not a wildcard.
inline constexpr std::uint32_t kHarvesterActionDefinition=0x80C0E599U;
// populations.lua lines 76-78: controller 80FE21CE rebases arrival root motion around its
// terminal transform, while departure starts at the actor's current transform.
inline constexpr std::uint32_t kHarvesterFlightController=0x80FE21CEU;
// populations.lua line 79: local ACTION_GROUP, ENTER_45, EXIT_45.
inline constexpr std::uint32_t kHarvesterActionGroup=0x07EBF354U;
inline constexpr std::uint32_t kHarvesterEnterAction=0x4482A76DU;
inline constexpr std::uint32_t kHarvesterExitAction=0x7D0D39A9U;
// populations.lua line 80: local DOORS. The ship's own authored channel; the delivery component's
// optional door programs are unbound, so the flight program drives the doors itself.
inline constexpr std::uint32_t kHarvesterDoors=0x80296344U;
inline constexpr float kDoorsClosed=0.F, kDoorsOpen=1.F;

// populations.lua line 82: action() = {kind="ability", values={ACTION_GROUP, name, 0x811C9DC5},
// value = 127}. The third identity is the canonical absent sentinel and 127 is the marker byte
// on the wire, which is the encoder's logical marker -1; the ability mode is never set.
inline constexpr std::uint32_t kActionAbsentIdentity=0x811C9DC5U;
inline constexpr std::uint8_t kActionMarkerWire=127;
inline constexpr std::int8_t kActionMarker=-1, kActionMode=-1;
// populations.lua lines 147 and 165: the snap takes byte 0, the exit move takes byte 1 and is
// the only lane that sets the move-enabled flag.
inline constexpr std::uint8_t kEntrySnapValue=0, kExitMoveValue=1;
inline constexpr bool kExitMoveEnabled=true;

// populations.lua lines 145, 163, 184 and 189: one actor generation for everything the live ship
// is told to do, and generation 2 only to retire it. Both flight programs are published on the
// pilot with binding "self"; only the arrival carries spawn.
inline constexpr std::uint32_t kHarvesterGeneration=1, kHarvesterRetireGeneration=2;
inline constexpr std::uint32_t kHarvesterArrivalRevision=1, kHarvesterDepartureRevision=2;
inline constexpr std::uint32_t kHarvesterDeliveryRevision=1;
// populations.lua line 175: reports from any other incarnation of the pilot are discarded.
inline constexpr std::uint32_t kHarvesterSpawnRevision=1;
// populations.lua lines 178, 187 and 191: the three native feedback values the sequence turns on.
// The export names none of them, so only the numbers the proven Lua tests are recorded.
inline constexpr std::uint8_t kPathStateLanded=4, kPathStateDeparted=3, kDeliveryStateReleased=0;

// The order of operations, and the gate that is allowed to advance each step. populations.lua
// keeps its progress in the retained "pact.populations.done" bit field, so a reattach or a script
// reload replays nothing: every step below is published exactly once.
inline constexpr std::uint32_t kDoneLedgeCleared=4,      // every arrival squad cleared
    kDoneArrivalPublished=32,                            // the ledge_final stage was consumed
    kDoneLandingHandled=64,                              // the landing handler ran
    kDoneDeliveryRequested=128,                          // deliver_squads was published
    kDoneRetired=256,                                    // retire_actor was published
    kDoneDeliveryReleased=512,                           // the passengers detached
    kDoneDeparturePublished=1024;                        // the exit program was published
struct FlightStep final {
    std::uint8_t step;
    std::uint32_t generation,revision,doneBit;
    std::string_view action,gate;
};
inline constexpr std::array<FlightStep,4> kHarvesterFlight{{
    // populations.lua on_stage: start the ship's own squad, reserve the cargo rows before the
    // delivery component asks for those same squads, then publish the spawning program: doors
    // closed, snap onto the entry transform, enter action, doors open.
    {0,kHarvesterGeneration,kHarvesterArrivalRevision,kDoneArrivalPublished,
     "start sq_harvester, reserve the cargo, then spawn+run: "
     "doors closed, snap to entry, enter action, doors open",
     "ledge_final"},
    // populations.lua on_combatant_state: the same handler releases the ground reinforcements and
    // publishes the passenger manifest built from the reserved cargo rows with demand.
    {1,kHarvesterGeneration,kHarvesterDeliveryRevision,kDoneLandingHandled|kDoneDeliveryRequested,
     "start the reinforcements, then deliver_squads with the reserved cargo",
     "path revision 1 reaches path state 4"},
    // populations.lua depart: only once the delivery reported release, every cargo row was seen
    // alive, and neither the retire nor the departure has been published yet.
    {2,kHarvesterGeneration,kHarvesterDepartureRevision,kDoneDeparturePublished,
     "run: doors closed, move to the exit sequence, exit action",
     "delivery revision 1 reaches delivery state 0 and every cargo row was seen"},
    // populations.lua on_combatant_state: the ship is removed only after its own exit program
    // reports completion, so a departure that never finishes leaves the actor in the world.
    {3,kHarvesterRetireGeneration,0,kDoneRetired,
     "retire_actor",
     "path revision 2 reaches path state 3 and the departure was published"},
}};
// populations.lua depart(): the exact precondition, restated so a controller cannot reorder it.
// Both required bits must be set and neither forbidden bit may be.
inline constexpr std::uint32_t kDepartureRequires=kDoneDeliveryRequested|kDoneDeliveryReleased;
inline constexpr std::uint32_t kDepartureForbids=kDoneRetired|kDoneDeparturePublished;

// ---------------------------------------------------------------------------------------
// 4. Triggers, objective, audience and the route ladder
// ---------------------------------------------------------------------------------------
// Type-60 boxes exactly as mission.trigger_volumes exports them: the registry, the volume's own
// slot index and the authored min/max. The SDK carries no polygon, only these bounds.
inline constexpr std::array<Volume,1> kLedgeVolumes{{
    {kBossRoomPoints,3,"pt_see_tree",{1623.53162F,-1490.50146F,4.F},{1634.58618F,-1475.72461F,24.F}},
}};
[[nodiscard]] constexpr const Volume* ledge_volume(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& v:kLedgeVolumes) { if(v.registry==registry && v.slot==slot) { return &v; } } return nullptr;
}

// The type-31 trigger or type-30 monitor that owns each box, with the stage string route.lua
// reports for it. route.initialize resolves every row for region 0, arms the audience, fires the
// trigger and sets the monitor's occupancy condition in one pass. `volume` is 0xFFFF when the
// export gives the sensor no box of its own.
inline constexpr std::uint16_t kLedgeNoVolume=0xFFFFU;
struct LedgeSensor final {
    std::uint32_t registry; std::uint16_t slot; std::uint8_t type; std::uint16_t volume; std::string_view stage;
};
inline constexpr std::array<LedgeSensor,2> kLedgeSensors{{
    // route.lua's bomb area has exactly one player trigger, so its per-area seen mask keys
    // see_tree at bit 0 and the monitor at bit 1. Reordering would re-key a live run's bits.
    {kBossRoomPoints,0,31,3,"see_tree"},
    // route.lua's ledge monitor: type-30 feedback is a distinct incident from a type-31 player
    // trigger, and it is what publishes the second bomb directive and calls in the Thresher.
    // UNRESOLVED: mission.TriggerVolume exports no name for pm_ledge_final, and the eight type-60
    // boxes of registry 0xA5F083B5 carry no slot-to-volume edge, so its bounds are unknown and
    // none are written. The proven route.lua identifies this monitor by slot alone, so nothing in
    // the ported logic needs the box.
    {kLedge,177,30,kLedgeNoVolume,"ledge_final"},
}};
// route.lua: an omitted filter selects the native registered players, and the int32 the monitor
// carries is an echoed value, not an occupancy threshold.
inline constexpr std::int32_t kLedgeMonitorOccupancyValue=0;
// pt_reached_boss (kBossApproach 31/0, box kBossApproach 60/4) is authored in this region but
// route.lua states that the boss controller owns it and its activation, so it is not armed here.

// obj_bomb = slot/80f54e07/000000, index 0, type 3, component 80808348. populations.lua builds
// the ledge controller with encounters.new("pact.ledge", OBJ_BOMB, 15, ledge_roster); the export's
// mission.Task table is empty, so 15 is the proven Lua's value and the only one available. The
// same registry's slot 1 is obj_bomb_boss, which belongs to the boss section.
inline constexpr std::uint16_t kLedgeObjectiveSlot=0;
inline constexpr std::uint8_t kLedgeObjectiveType=3;
inline constexpr std::uint8_t kLedgeTaskGroups=15;
// strike_encounters.lua: a controller is constructed at revision 1 and never bumps it; a squad
// report whose objective revision differs is ignored, and -1 is the initial unassigned task.
inline constexpr std::uint8_t kLedgeObjectiveRevision=1;
inline constexpr std::int8_t kLedgeTaskUnassigned=-1;

// m_engagement_sensor = slot/80f54e07/0000b0, index 176, type 70, component 808094EE. The ledge
// publishes its own audience and every region-0 directive is addressed here, boss.lua's included.
inline constexpr std::uint16_t kLedgeAudience=176;
inline constexpr std::uint8_t kLedgeAudienceType=70;
inline constexpr std::uint8_t kLedgeAudienceFlags=0, kLedgeAudienceRevision=1;

// ap_boss_room = slot/80f54e12/000001, type 47. Both ledge directives aim at it; route.lua's bomb
// area declares no second target, so the goal marker never moves while the player is on the ledge.
inline constexpr coo::Asset kApBossRoomApproach{kBossApproach,kBossApproachTag,47,1};

// route.lua's bomb ladder. advance_bomb never lowers the step, so the order is the gate.
struct LedgeDirectiveStep final { std::uint8_t step; std::uint32_t event; coo::Asset target; std::string_view source; };
inline constexpr std::array<LedgeDirectiveStep,2> kLedgeSteps{{
    {1,kFindLeader,kApBossRoomApproach,"region held"},
    {2,kFindMap,kApBossRoomApproach,"ledge_final"},
}};
// route.lua cue_once rows, indices into catalog.h's kDialogueRows. CUE_17 opens the ledge on
// Sagira's line about the window in time, CUE_18 carries the Harvester's approach and the
// coordinates to the map, CUE_19 is the first sight of the tree.
inline constexpr std::uint8_t kCueLedgeStart=17, kCueLedgeFinal=18, kCueSeeTree=19;
// populations.on_squad_state returns this once every arrival squad has cleared. No proven module
// consumes it; it is carried so a controller can recognise the report rather than drop it.
inline constexpr std::string_view kLedgeClearedStage{"ledge_cleared"};
} // namespace dawn::state::activity::strike_pact
