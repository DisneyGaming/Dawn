#pragma once
#include "mercury_public_event_definition.h"
#include "adventure_cue_feedback.h"
#include "population_service.h"
#include "public_event_initial_runtime.h"
#include "mercury_public_event_presentation.h"
#include "mercury_public_event_sequences.h"
#include "mercury_public_event_world.h"
#include "mercury_public_event_keys.h"
#include "mercury_public_event_travel.h"
#include "mercury_public_event_music.h"

namespace dawn::server::runtime::activity::mercury::public_events {
// Authored Crossroads opening and exact native original-code presentation.
// Lifetime fields are supplied by the retained world/event owner before binding.
inline constexpr auto kOpeningCue=[] {
    adventure::cue_feedback::Ticket t{};
    t.activity=29;t.definition=0x80F5E337;t.nativeScope=UINT32_MAX;t.table=0x80F5E35B;t.admissionScope=15;
    t.stringBank=0x80F56034;t.title=0x50EFBC4D;t.detail=0x290979DD;t.definitionOffset=0xB88;
    t.request={0xC8229B2B,0x00D1C5B9,107,0,15};
    t.request.readiness={0xC8229B2B,70,106};
    t.presentation=adventure::cue_feedback::Presentation::authoredProgress;t.progressLabel=0x6267DC7C;
    return t;
}();
// Finite development policy: nine cumulative native source requests per side.
// Request totals are a test population budget, not an asserted retail schedule.
// Each source retains its authored member alternatives and native rule/AI.
// The traveling cohorts use row 4 of the central tactical objective. Its
// package-authored provider is type 45/137, backed by firing area 44/257.
// This reconstructed assignment was verified through native source apply and
// visible movement/combat; the original host's complete row schedule is unknown.
inline constexpr std::array<population::Capability,2> kOpeningSources{{
    {&kRegistries[1],44,174,{0xC8229B2B,82,4},true},
    {&kRegistries[1],46,239,{0xC8229B2B,82,4},true}
}};
inline constexpr coo::CommandSpec kOpeningCueCommands[]{
    {coo::Operation::objective,{0xC8229B2B,0x80F5E337,68,107},0x00D1C5B9,coo::Wait::nativeReady}
};
inline constexpr coo::CommandSpec kOpeningSourceCommands[]{
    {coo::Operation::population,{0xC8229B2B,0x80F5E3A4,1,44},9,coo::Wait::nativeReady},
    {coo::Operation::population,{0xC8229B2B,0x80F5E3AA,1,46},9,coo::Wait::nativeReady}
};
// Working quota reconstructed from the reference's approximately 11 percentage
// point increments. It is separate from the native source request budget.
inline constexpr coo::CommandSpec kOpeningDefeatCommands[]{
    {coo::Operation::observation,{0xC8229B2B,0x80F5E337,68,107},9,coo::Wait::observed}
};
inline constexpr coo::Step kOpeningSteps[]{
    {"publish_authored_crossroads_objective",0,kOpeningCueCommands},
    {"admit_finite_traveling_vex_sources",1,kOpeningSourceCommands},
    {"observe_traveling_vex_defeats",2,kOpeningDefeatCommands}
};
inline constexpr coo::Definition kOpeningGraph{
    "mercury_crossroads_opening_diagnostic",coo::Schema::otherMissions,kOpeningSteps,{}};
inline constexpr public_event::OpeningDefinition kOpeningDefinition{
    &kRegistries[1],&kOpeningGraph,kOpeningCue,kOpeningSources,9};

inline constexpr auto kGatekeeperCue=[] {
    auto t=kOpeningCue;t.request.event=0x00D1C5BA;t.request.ring=1;
    t.detail=0x290979DE;t.progressLabel=0xD0A18DC4;
    t.progressFormat=adventure::cue_feedback::ProgressFormat::count;
    // Four warp gates, not a Gatekeeper kill count. Only native deposit/use
    // receipts may advance this counter; death alone must leave it at zero.
    t.request.hasProgress=true;t.request.progress={0,4};return t;
}();
// Two Gatekeepers, one per side, matching the corrected retail reference.
// Each drops two separate keys for the four central gates.
// Native source/rule and named tactical providers retain authored placement.
inline constexpr std::array<population::Capability,2> kGatekeeperSources{{
    {&kRegistries[1],48,244,{0xC8229B2B,82,2},true},
    {&kRegistries[1],49,246,{0xC8229B2B,82,3},true}
}};
inline constexpr coo::CommandSpec kGatekeeperCueCommands[]{
    {coo::Operation::objective,{0xC8229B2B,0x80F5E337,68,107},0x00D1C5BA,coo::Wait::nativeReady}
};
inline constexpr coo::CommandSpec kGatekeeperSourceCommands[]{
    {coo::Operation::population,{0xC8229B2B,0x80F5E3B0,1,48},1,coo::Wait::nativeReady},
    {coo::Operation::population,{0xC8229B2B,0x80F5E3B3,1,49},1,coo::Wait::nativeReady}
};
inline constexpr coo::Step kGatekeeperSteps[]{
    {"publish_authored_gatekeeper_objective",0,kGatekeeperCueCommands},
    {"admit_native_gatekeepers",1,kGatekeeperSourceCommands}
};
inline constexpr coo::Definition kGatekeeperGraph{
    "mercury_crossroads_gatekeeper_admission",coo::Schema::otherMissions,kGatekeeperSteps,{}};
inline constexpr public_event::OpeningDefinition kGatekeeperDefinition{
    &kRegistries[1],&kGatekeeperGraph,kGatekeeperCue,kGatekeeperSources};

inline constexpr std::array<public_event::deferred_placement::Ticket,4> kInitialGates{{
    {{},0,0,0,0,0xC8229B2B,0x80F5E481,1,0x4C8,62,15,0x80F58FAE,0xB09EA2FC12589314ULL,0x80F58FAC,0x1F80},
    {{},0,0,0,0,0xC8229B2B,0x80F5E484,1,0x4C8,63,15,0x80F58FC6,0xAACDE2714D597398ULL,0x80F58FC4,0x1F80},
    {{},0,0,0,0,0xC8229B2B,0x80F5E487,1,0x4C8,64,15,0x80F58F7D,0x9022BFEEA9D3E6BEULL,0x80F58F7B,0x1F80},
    {{},0,0,0,0,0xC8229B2B,0x80F5E48A,1,0x4C8,65,15,0x80F58F96,0xE3CCC1D399517D7CULL,0x80F58F94,0x1F80}
}};
inline constexpr coo::CommandSpec kInitialGateCommands[]{
    {coo::Operation::device,{0xC8229B2B,0x80F5E481,4,62},1,coo::Wait::nativeReady},
    {coo::Operation::device,{0xC8229B2B,0x80F5E484,4,63},1,coo::Wait::nativeReady},
    {coo::Operation::device,{0xC8229B2B,0x80F5E487,4,64},1,coo::Wait::nativeReady},
    {coo::Operation::device,{0xC8229B2B,0x80F5E48A,4,65},1,coo::Wait::nativeReady}
};
inline constexpr coo::CommandSpec kInitialEngagementCommands[]{
    {coo::Operation::mechanic,{0xC8229B2B,0x80F5E466,70,106},1,coo::Wait::nativeReady}
};
inline constexpr coo::Step kInitialSteps[]{
    {"create_authored_gate_point_dependencies",0,kInitialGateCommands},
    {"observe_native_active_player_engagement",1,kInitialEngagementCommands}
};
inline constexpr coo::Definition kInitialGraph{"mercury_crossroads_initial_dependencies",coo::Schema::otherMissions,kInitialSteps,{}};
inline constexpr auto kInitialEngagement=[] {
    public_event::engagement_feedback::Ticket t{};t.definition=0x80F5E466;t.definitionOffset=0x358;
    t.request.registry=0xC8229B2B;t.request.slot=106;t.request.scope=15;t.request.generation=1;
    t.request.collection=public_event::engagement_feedback::wire::Collection::activePlayers;return t;
}();
inline constexpr std::array<public_event::InitialDefinition,1> kInitialDefinitions{{
    {&kOpeningDefinition,&kInitialGraph,kInitialGates,kInitialEngagement,"public_event_opening_probe",&kGatekeeperDefinition,&kIncomingVoice,
        &kIncomingSequence,"public_event_intro_delay_ms","public_event_voice_delay_ms","public_event_opening_duration_ms",&world::kDefinition,world::kOpening,&kMainlandKeyDefinition,
        &kTravelDirective,{world::kCannonsOff,world::kReorient,world::kIslands},"public_event_island_duration_ms","public_event_cannon_rotation_delay_ms","public_event_cannon_activation_delay_ms",
        {{},0,0,0,0,0x80F5E33D,0x228,{0xC8229B2B,110,15}},"public_event_incoming_cue_delay_ms","public_event_incoming_duration_ms",&kJoinedVoice,
        &kMusicDefinition,kMusicCandidates[0]}
}};
}
