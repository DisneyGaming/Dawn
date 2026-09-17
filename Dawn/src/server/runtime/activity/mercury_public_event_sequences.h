#pragma once
#include "mercury_public_event_registries.h"
#include "public_event_sequence_runtime.h"
namespace dawn::server::runtime::activity::mercury::public_events {
// Installed source80F5E355+658 references entity80C0125A at+58, with no
// parameter table at+78. Its graph80C01258 spawns80C01257, which owns the
// native event-intro audio80BF54CD and screen-effect80C01255 resources.
inline constexpr state::activity::coo::CommandSpec kIncomingSequenceCommands[]{
    {state::activity::coo::Operation::scene,{0xC8229B2B,0x80F5E355,5,88},1,state::activity::coo::Wait::requested}
};
inline constexpr state::activity::coo::Step kIncomingSequenceSteps[]{
    {"request_authored_event_intro_sequence",0,kIncomingSequenceCommands}
};
inline constexpr state::activity::coo::Definition kIncomingSequenceGraph{
    "public_event_incoming_sequence",state::activity::coo::Schema::otherMissions,kIncomingSequenceSteps,{}};
inline constexpr public_event::sequence::Definition kIncomingSequence{
    &kRegistries[1],&kIncomingSequenceGraph,88,0x80F5E355,0x80C0125A,0};
} // namespace dawn::server::runtime::activity::mercury::public_events
