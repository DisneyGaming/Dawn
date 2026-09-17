// Tree of Probabilities (strike_pact), Infinite Forest B traversal: registry 0x2763EC91
// (object 80F550B8), held region 72. Every slot index, member count, hash and bound here is
// read out of the SDK export of scenario 80F54AE7; every ordering, count and threshold is
// carried verbatim from the mission Lua that already runs this strike, and the line that
// carries one names the file it came from. Nothing here is inferred from a sibling area.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include "catalog.h"
namespace dawn::state::activity::strike_pact {

// The SDK stamps slot ids with object tags only; registry keys come from the accepted
// Dawn host's own published roster. "slot/80f550b8/..." is 0x2763EC91, and so on.
inline constexpr std::uint32_t kForest=0x2763EC91U, kForestTag=0x80F550B8U;
// Forest interior triggers (80F550C9) and the exit trigger group (80F550DA) are separate
// registries: route.lua owns triggers by the held region, not by the destination state.
inline constexpr std::uint32_t kForestArea=0xA9350228U, kForestAreaTag=0x80F550C9U;
inline constexpr std::uint32_t kForestExit=0x7E558786U, kForestExitTag=0x80F550DAU;
// state/80f54ae7/0009/0000/80f54adc: slice set and region index are both 72, hash A522CBE0.
inline constexpr int kForestRegion=72;
inline constexpr std::uint32_t kForestStateHash=0xA522CBE0U;
// music.lua: the Forest section of the authored music sensor (root 11/1), selected on arrival.
inline constexpr std::uint8_t kForestMusicSection=2;

// ---------------------------------------------------------------------------------------
// 1. The authored map generator
// ---------------------------------------------------------------------------------------
// map_generator_sensor = slot/80f550b8/00001e, index 30, type 37, component 80804EF6,
// auth schema 80805007 (1750..11350 bits).
inline constexpr std::uint16_t kMapGenerator=30;
inline constexpr std::uint8_t kMapGeneratorType=37;
inline constexpr std::uint32_t kMapGeneratorComponent=0x80804EF6U;

// Presence bits on the first native record. The host overrides only what it names; every
// unnamed field keeps the authored worker definition, so the mask is part of the recipe.
inline constexpr std::uint8_t kGeneratorOverrideSeed=1, kGeneratorOverrideAnchors=4,
    kGeneratorOverrideEnabled=8;
inline constexpr std::uint8_t kGeneratorOverrides=
    kGeneratorOverrideSeed|kGeneratorOverrideAnchors|kGeneratorOverrideEnabled;

// strike_pact.lua initialize_forest passes values={6}. The first of the record's five biased
// i32 fields is the solver's encounter placement budget; this generator's authored default is
// zero, so a host that leaves the field at its -1 sentinel places no encounter at all.
inline constexpr std::int32_t kEncounterPlacementBudget=6;
inline constexpr std::array<std::int32_t,5> kGeneratorValues{
    kEncounterPlacementBudget,-1,-1,-1,-1};

// The seed is host entropy chosen once per run, masked to the native positive range and
// forced nonzero. Reattachment and region streaming reuse the retained value; only a new
// strike draws again, so the layout is stable for the whole run (strike_pact.lua).
inline constexpr std::uint32_t kGeneratorSeedMask=0x7FFFFFFFU;

// One authored connection selector (native schema 8080500F). Column and height are signed
// bytes; -1/-1 is the native absent selector, which is why every enabled anchor names both.
struct GeneratorAnchor final { std::int8_t column,height; float progress; bool enabled; };
// strike_pact.lua initialize_forest, in the native +X, -X, +Y, -Y order the encoder expects.
// The authored Forest B placement runs from the -Y entrance to the +Y exit, and all four keep
// their endpoint geometry. Only the exit carries progress 1: with every anchor left at 0 the
// native navigation solver has no forward gradient and treats each placed encounter as an
// equally good goal, so the higher value marks the single endpoint that is the way out.
inline constexpr std::array<GeneratorAnchor,4> kGeneratorAnchors{{
    {2,0,0.F,true}, // +X
    {0,2,0.F,true}, // -X
    {2,0,1.F,true}, // +Y exit
    {0,2,0.F,true}, // -Y entrance
}};

// Generated encounter definitions gate their authored faction alternatives on these typed
// lifetime hashes; the weighted groups behind the selected value choose both the population
// and its member counts. strike_pact.lua initialize_forest_selection publishes them before
// the generator ever runs, because a late change cannot reselect rows that already exist.
struct HashSwitch final { std::uint32_t key,value; };
inline constexpr std::uint32_t kForestPopulationSelector=0x050C5D2EU;
inline constexpr std::array<HashSwitch,4> kForestHashSwitches{{
    {0x67AF9045U,kForestPopulationSelector},
    {0x0D979BCDU,kForestPopulationSelector},
    {0xAD3780EEU,kForestPopulationSelector},
    {0x89567586U,kForestPopulationSelector},
}};
// UNRESOLVED: the numeric lifetime state that accompanies the hash switches. The proven Lua
// names it symbolically (context.sdk.lifetime_states.default) and neither the SDK export nor
// Dawn carries a lifetime-state enum, so the value is not written here.

// ---------------------------------------------------------------------------------------
// 2. The portal exit defense
// ---------------------------------------------------------------------------------------
// Ten authored squads at slots 6..15. Every one carries exactly one entry in the SDK's
// mission.Squad members[], so each is one native category with one loose request and no
// named second member; count = loose + second, as in the opening table.
// populations.lua joins SQ_PORTAL_GUARD_0..5 and SQ_PORTAL_SNIPER_0..3 into one `defenders`
// list: they are placed together on the first of load_post / see_portal / reached_portal and
// the barrier only drops once all ten have cleared. That is one gate, so one cohort.
inline constexpr std::uint8_t kForestLastCohort=1;
inline constexpr std::array<Spawn,10> kForestSpawns{{
    {6,kForest,1,0,1,1,1,true,-1,0,"sq_portal_guard[0]"},
    {7,kForest,1,0,1,1,1,true,-1,0,"sq_portal_guard[1]"},
    {8,kForest,1,0,1,1,1,true,-1,0,"sq_portal_guard[2]"},
    {9,kForest,1,0,1,1,1,true,-1,0,"sq_portal_guard[3]"},
    {10,kForest,1,0,1,1,1,true,-1,0,"sq_portal_guard[4]"},
    {11,kForest,1,0,1,1,1,true,-1,0,"sq_portal_guard[5]"},
    {12,kForest,1,0,1,1,1,true,-1,0,"sq_portal_sniper[0]"},
    {13,kForest,1,0,1,1,1,true,-1,0,"sq_portal_sniper[1]"},
    {14,kForest,1,0,1,1,1,true,-1,0,"sq_portal_sniper[2]"},
    {15,kForest,1,0,1,1,1,true,-1,0,"sq_portal_sniper[3]"},
}};
static_assert([] { for(const auto& s:kForestSpawns) { if(s.count!=s.loose+s.second) { return false; } } return true; }());
[[nodiscard]] constexpr const Spawn* forest_spawn(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& s:kForestSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr;
}

// ---------------------------------------------------------------------------------------
// 3. Barrier, audience and the objective the defense is assigned against
// ---------------------------------------------------------------------------------------
// d_shield_wall = slot/80f550b8/000011, index 17, type 23, component 80804F45. It is the
// same native device component as the opening shield: position 1 displays the Cabal wall and
// position 0 removes it (strike_pact.lua update_gate). populations.lua on_held powers it on,
// unlocks it and snaps it to position 1 when the region is first held; the transition back to
// position 0 is the reward for clearing all ten defenders.
inline constexpr std::uint16_t kForestShieldWall=17;
inline constexpr std::uint8_t kForestShieldWallType=23;
inline constexpr std::uint8_t kShieldPresent=1, kShieldRemoved=0;

// m_engagement_sensor = slot/80f550b8/00001c, index 28, type 70, component 808094EE. The
// Forest publishes its own audience; the opening's is absent in this region, so every Forest
// directive is addressed here (strike_pact.lua initialize_forest, route.lua areas[forest]).
inline constexpr std::uint16_t kForestAudience=28;
inline constexpr std::uint8_t kForestAudienceType=70;
inline constexpr std::uint8_t kForestAudienceFlags=0, kForestAudienceRevision=1;

// obj_ifb = slot/80f550b8/000010, index 16, type 3. populations.lua constructs the defense
// controller with three authored task groups; task costs outside that range are not read.
inline constexpr std::uint16_t kForestObjectiveSlot=16;
inline constexpr std::uint8_t kForestObjectiveGroups=3, kForestObjectiveRevision=1;

// ---------------------------------------------------------------------------------------
// 4. Forest volumes, triggers and monitors
// ---------------------------------------------------------------------------------------
// Type-60 boxes exactly as mission.trigger_volumes exports them: the registry, the volume's
// own slot index and the authored min/max. The SDK carries no polygon, only these bounds.
inline constexpr std::array<Volume,7> kForestVolumes{{
    // tv_load_post, named pt_load_post in the export.
    {kForest,45,"pt_load_post",{-648.999878F,-37.2999573F,-106.500046F},{-433.999847F,176.442703F,-6.5F}},
    // tv_begin: the arrival box strike_pact.lua fires to open the Forest.
    {kForest,46,"pt_begin",{-834.311829F,-430.939789F,-30.6876411F},{-782.811829F,-415.439789F,14.3123589F}},
    // tv_end. The export leaves this row unnamed, but slot 52 of this registry is tv_end and
    // it is the only type-60 box left here once pt_load_post and pt_begin are taken; route.lua
    // states that PM_END names a real type-60 volume. The monitor->volume edge is not exported.
    {kForest,52,"tv_end",{-632.603455F,23.8969574F,-59.4608154F},{-465.421295F,45.0812836F,-29.4608154F}},
    {kForestArea,3,"pt_see_portal",{-678.842346F,-86.5864868F,-112.000008F},{-376.238556F,189.364792F,-12.F}},
    {kForestArea,6,"pt_reached_portal",{-639.F,-5.5F,-63.8789711F},{-465.730774F,148.646744F,-23.8789673F}},
    {kForestArea,7,"pt_start_forest",{-864.702576F,-382.185211F,-34.F},{-678.61908F,-251.627258F,6.00000381F}},
    {kForestExit,2,"pt_leaving_forest",{-547.F,152.F,-39.F},{-529.F,179.F,-19.F}},
}};
[[nodiscard]] constexpr const Volume* forest_volume(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& v:kForestVolumes) { if(v.registry==registry && v.slot==slot) { return &v; } } return nullptr;
}

// The type-31 trigger or type-30 monitor that owns each box, with the stage string route.lua
// reports for it. route.initialize resolves every row for the region, then arms the audience,
// fires all four triggers and sets the monitor's occupancy condition in one pass, so a stage
// can never be reported before its siblings exist. `volume` is 0xFFFF when the export gives
// the sensor no box of its own.
inline constexpr std::uint16_t kNoVolume=0xFFFFU;
struct Sensor final {
    std::uint32_t registry; std::uint16_t slot; std::uint8_t type; std::uint16_t volume; std::string_view stage;
};
inline constexpr std::array<Sensor,8> kForestSensors{{
    // strike_pact.lua fires pt_begin itself as part of opening the Forest; it is not one of
    // route.lua's four staged triggers.
    {kForest,31,31,46,"begin"},
    {kForest,33,31,45,"load_post"},
    {kForestArea,1,31,3,"see_portal"},
    {kForestArea,2,31,6,"reached_portal"},
    {kForestExit,1,31,2,"leaving_forest"},
    // pt_start_forest is authored and boxed but no proven script fires or reads it; it is
    // carried so a controller can recognise the incident rather than drop it.
    {kForestArea,0,31,7,"start_forest"},
    // route.lua's forest monitor. Type-30 feedback is a distinct incident from a type-31
    // player trigger and is what publishes the barrier directive on the far side.
    {kForest,32,30,52,"forest_end"},
    // pm_ifb lives on the lighthouse side (slot/80f55205/000004, registry F8D0DADC) and is
    // the arrival monitor for the Forest bubble. UNRESOLVED: mission.trigger_volumes exports
    // no type-60 row for it, so its box is unknown and no bounds are written.
    {kLighthouse,4,30,kNoVolume,"ifb_arrival"},
}};
// route.lua: an omitted filter selects the native registered players, and the int32 the
// monitor carries is an echoed value, not an occupancy threshold.
inline constexpr std::int32_t kMonitorOccupancyValue=0;

// route.lua advance_forest is monotonic: a step never publishes behind one already reached.
// Step 0 is strike_pact.lua initialize_forest, published the moment region 72 is held; the
// rest are route.lua. A zero target registry is the no-marker case.
struct ForestStep final {
    std::uint8_t step; std::uint32_t event; std::uint32_t targetRegistry; std::uint16_t targetSlot; std::string_view source;
};
inline constexpr std::array<ForestStep,4> kForestSteps{{
    {0,kTrackCabal,0,0,"region held"},
    {1,kDisableBarrier,kForestArea,5,"see_portal or forest_end"},
    {2,kTraverseForest,kForestArea,5,"portal_defense_cleared"},
    // The goal text stays Traverse while the authored destination moves past the portal to
    // the far end of its tunnel.
    {3,kTraverseForest,kForestExit,7,"leaving_forest"},
}};

// ---------------------------------------------------------------------------------------
// 5. Respawn spawn sets and authored navigation targets
// ---------------------------------------------------------------------------------------
// route.lua: a death respawns on the lifetime's spawn set, which otherwise stays the
// opening's, and a bubble with no set of its own has no spawn location at all. The Forest
// therefore claims a checkpoint twice: the entrance set when region 72 is first held, and the
// exit set at the see_portal trigger, both against the Forest state (region index 72).
inline constexpr std::uint32_t kForestEntranceSpawnSet=0xFEB62946U;
inline constexpr std::uint32_t kForestExitSpawnSet=0x54A5A748U;

// ap_forest_portal, type 47, one per registry. These are the authored marker targets the
// Forest directives point at; the lighthouse-side point (kLighthouse 47/8) is already carried
// by the opening catalog as kForestPortalPoint and is what strike_pact.lua show_directive uses
// before the player is in the tunnel.
inline constexpr std::uint8_t kNavigationPointType=47;
inline constexpr std::uint16_t kForestPortalAreaPoint=5;  // slot/80f550c9/000005
inline constexpr std::uint16_t kForestPortalExitPoint=7;  // slot/80f550da/000007
} // namespace dawn::state::activity::strike_pact
