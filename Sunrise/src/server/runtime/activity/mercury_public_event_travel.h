#pragma once
#include "public_event_directive_runtime.h"
namespace sunrise::server::runtime::activity::mercury::public_events {
// All words/icons remain in authored table80F5E35B and string bank80F56034.
inline constexpr auto kFirstIslandTravelCue=[] {
    adventure::cue_feedback::Ticket t{};
    t.activity=29;t.definition=0x80F5E337;t.nativeScope=UINT32_MAX;t.table=0x80F5E35B;t.admissionScope=15;
    t.stringBank=0x80F56034;t.title=0x50EFBC4D;t.detail=0x290979DF;t.definitionOffset=0xB88;
    t.request={0xC8229B2B,0x0730D92A,107,2,15};t.request.readiness={0xC8229B2B,70,106};
    t.presentation=adventure::cue_feedback::Presentation::authoredProgress;
    t.progressFormat=static_cast<adventure::cue_feedback::ProgressFormat>(0);return t;
}();
inline constexpr state::activity::coo::CommandSpec kTravelCueCommands[]{
    {state::activity::coo::Operation::objective,{0xC8229B2B,0x80F5E337,68,107},0x0730D92A,state::activity::coo::Wait::nativeReady}
};
inline constexpr state::activity::coo::Step kTravelCueSteps[]{{"publish_native_island_travel_directive",0,kTravelCueCommands}};
inline constexpr state::activity::coo::Definition kTravelCueGraph{"public_event_island_travel",state::activity::coo::Schema::otherMissions,kTravelCueSteps,{}};
inline constexpr public_event::directive::Definition kTravelDirective{kFirstIslandTravelCue,&kTravelCueGraph};
}
