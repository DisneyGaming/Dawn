#pragma once
#include "native_activity_definition.h"
#include "mercury_populations.h"
#include "mercury_placements.h"
#include "vance_animation_capability.h"
#include "adventure_mercury_start_routes.h"
#include "adventure_mercury_openings.h"
#include "mercury_public_event_definition.h"
#include "lost_sector_catalog.h"
namespace sunrise::server::runtime::activity::mercury {
namespace lost=lost_sector::catalog::mercury;
inline constexpr auto kActivityPopulations=lost_sector::catalog::concat(kPopulations,lost::kCapabilities);
inline constexpr auto kLostSectorDefinition=lost::definition(static_cast<std::uint16_t>(kPopulations.size()));
inline constexpr coo::ModuleBinding kPersistentModule{{0x80F4696A,0x80F4696A,0,0},1};
inline constexpr coo::script::Capability kScriptCapabilities[]{
    {"persistent.start","composition",{coo::Operation::mechanic,kPersistentModule.asset,1,coo::Wait::requested}},
    {"vendor_portal","nativeActivity",{coo::Operation::device,{0xF25B938B,0x80F5E5AC,4,0},1,coo::Wait::requested}},
    {"vendor_effect_1","nativeActivity",{coo::Operation::device,{0xF25B938B,0x80F5E5AF,4,1},1,coo::Wait::requested}},
    {"vendor_effect_2","nativeActivity",{coo::Operation::device,{0xF25B938B,0x80F5E5B2,4,2},1,coo::Wait::requested}},
    {"vendor_effect_3","nativeActivity",{coo::Operation::device,{0xF25B938B,0x80F5E5B5,4,3},1,coo::Wait::requested}},
    {"centre_portal","nativeActivity",{coo::Operation::device,{0xF25B938B,0x80F5E5B8,4,4},1,coo::Wait::requested}},
    {"landing_portal","nativeActivity",{coo::Operation::device,{0xF25B938B,0x80F5E5BB,4,5},1,coo::Wait::requested}},
    {"pond","nativeActivity",{coo::Operation::population,{0x74337EDD,0x80F5B68E,1,1},1,coo::Wait::requested}},
    {"vance","nativeActivity",{coo::Operation::population,{0x564C6ECE,0x80F5B9CC,1,0},1,coo::Wait::requested}},
    {"vance.idle","nativeActivity",{coo::Operation::mechanic,{0x564C6ECE,0x80F5BA28,42,2},1,coo::Wait::requested}},
    adventure::mercury::kScriptCapabilities[0],
    adventure::mercury::kScriptCapabilities[1],
    adventure::mercury::kScriptCapabilities[2],
    adventure::mercury::kOpeningCapabilities[0],
    adventure::mercury::kOpeningCapabilities[1],
    adventure::mercury::kOpeningCapabilities[2],
    adventure::mercury::kDialogueCapabilities[0],
    adventure::mercury::kDialogueCapabilities[1],
    adventure::mercury::kDialogueCapabilities[2],
    adventure::mercury::kGatewayCapabilities[0],
    adventure::mercury::kGatewayCapabilities[1],
    adventure::mercury::kGatewayCapabilities[2],
};
inline constexpr auto kAdventureActions=adventure::mercury::actions(kAdventurePlacementStart);
inline constexpr NativeAction kActions[]{
    {{coo::Operation::device,{0xF25B938B,0x80F5E5AC,4,0},1,coo::Wait::requested},0,{}},
    {{coo::Operation::device,{0xF25B938B,0x80F5E5AF,4,1},1,coo::Wait::requested},1,{}},
    {{coo::Operation::device,{0xF25B938B,0x80F5E5B2,4,2},1,coo::Wait::requested},2,{}},
    {{coo::Operation::device,{0xF25B938B,0x80F5E5B5,4,3},1,coo::Wait::requested},3,{}},
    {{coo::Operation::device,{0xF25B938B,0x80F5E5B8,4,4},1,coo::Wait::requested},4,{}},
    {{coo::Operation::device,{0xF25B938B,0x80F5E5BB,4,5},1,coo::Wait::requested},5,{}},
    {{coo::Operation::population,{0x74337EDD,0x80F5B68E,1,1},1,coo::Wait::requested},1,"pond_initial_count"},
    {{coo::Operation::population,{0x564C6ECE,0x80F5B9CC,1,0},1,coo::Wait::requested},2,"vance_count"},
    {{coo::Operation::mechanic,{0x564C6ECE,0x80F5BA28,42,2},1,coo::Wait::requested},0,{}},
    kAdventureActions[0],kAdventureActions[1],kAdventureActions[2],
};
inline constexpr coo::script::ModuleCapability kModules[]{{"persistent",kPersistentModule}};
inline constexpr coo::script::ParameterCapability kParameters[]{
    {"pond_initial_count",1,4,1,false},{"vance_count",1,1,1,false},
    {"ambient_vex_probe_count",0,1,0,false},
    {"ambient_cabal_primary_probe_count",0,1,0,false},
    {"freeroam_population_enabled",0,1,1,false},
    {"freeroam_respawn_ms",1000,3600000,30000,false},
    {"freeroam_normal_patrol_count",1,21,3,false},
    {"freeroam_large_patrol_count",1,16,4,false},
    {"faction_war_enabled",0,1,1,false},
    {"faction_war_interwave_ms",1000,300000,8000,false},
    {"faction_war_wave_1_count",1,5,2,false},
    {"faction_war_wave_2_count",1,5,3,false},
    {"faction_war_wave_3_count",1,5,4,false},
    {"faction_war_wave_4_count",1,5,5,false},
    {"public_event_rally_probe",0,1,0,false},
    {"public_event_opening_probe",0,1,0,false},
    {"public_event_intro_delay_ms",0,30000,5250,false},
    {"public_event_incoming_cue_delay_ms",0,30000,1750,false},
    {"public_event_incoming_duration_ms",1000,3600000,180000,false},
    {"public_event_voice_delay_ms",0,30000,9750,false},
    {"public_event_opening_duration_ms",1000,3600000,240000,false},
    {"public_event_island_duration_ms",1000,3600000,120000,false},
    {"public_event_cannon_rotation_delay_ms",0,30000,5000,false},
    {"public_event_cannon_activation_delay_ms",0,30000,7000,false},
    // Reconstructed host cadence; native context startup is required and was
    // observed missing in Mercury. This does not claim retail cadence recovery.
    {"host.tick_hz",1,120,30,false},
};
// The Cabal source0 primary probe is a standalone diagnostic now; this profile
// owns that source as an ordinary fallback-rule escort, not an occupancy probe.
inline constexpr std::array<ambient_population::InitialBinding,1> kAmbientInitial{{
    ambient::probe::binding(7),
}};
// Authored native bubble scope15; not an endpoint or event revision.
inline constexpr std::array<public_event::RallyBinding,1> kPublicEventRallies{{
    {&public_events::kRallyProbeDefinition,{&kRegistries[5],0},public_events::kRallyFeedback,15,"public_event_rally_probe"},
}};
inline const coo::script::Profile kProfile{"mercury.freeroam.native.v1","nativeOtherActivities",
    coo::Schema::otherMissions,kScriptCapabilities,kModules,{}, {}, {}, {}, {},{},kParameters};
inline constexpr std::array<ambient_population::RegistryBinding,3> kOptionalRegistries{{
    // The primary probe's placement owner is external; its 2571C34D source
    // registry is already part of the always-admitted patrol profile.
    ambient::cabal_probe::kOptionalRegistries[0],
    {&public_events::kRegistries[1],"public_event_opening_probe"},
    {&public_events::kRegistries[2],"public_event_opening_probe"}
}};
inline const NativeActivityDefinition kActivity{"mercury_freeroam",L"mercury_freeroam.json",15,
    &kProfile,kRegistries,kActivityPopulations,kPlacements,kActions,kPersistentModule,kVanceAnimationCapabilities,
    adventure::mercury::kStartRoutes,kAmbientInitial,kPublicEventRallies,adventure::mercury::kOpenings,
    {},{},kOptionalRegistries,"host.tick_hz",{},{},{},{},{},public_events::kInitialDefinitions,true,
    {}, {}, false, {}, nullptr,lost::kRegistries,&kLostSectorDefinition,lost::kRewardRegistries};
}
