#pragma once
#include "mercury_public_event_registries.h"
#include "public_event_dialogue_runtime.h"

namespace sunrise::server::runtime::activity::mercury::public_events {
// Installed type53 definition80F5E33A+1408 selects bank80F5C030. Its ordered
// 80808D18 row0 is A206D1F2, 7.041540s; the actual native graph chooses among
// five authored opening variants, all with speaker32E8F62C. No media or text
// is embedded here. Duration describes audio only, never the event lifetime.
inline constexpr state::activity::coo::CommandSpec kIncomingVoiceCommands[]{
    {state::activity::coo::Operation::dialogue,{0xC8229B2B,0x80F5E33A,53,108},
        0xA206D1F2,state::activity::coo::Wait::nativeReady}
};
inline constexpr state::activity::coo::Step kIncomingVoiceSteps[]{
    {"submit_native_incoming_conversation",0,kIncomingVoiceCommands}
};
inline constexpr state::activity::coo::Definition kIncomingVoiceGraph{
    "public_event_incoming_dialogue",state::activity::coo::Schema::otherMissions,kIncomingVoiceSteps,{}};
inline constexpr public_event::dialogue::Definition kIncomingVoice{
    &kRegistries[1],&kIncomingVoiceGraph,108,{0x80F5E33A,0x80F5C030,0xA206D1F2,7042,8,0}};
// Fixed retail D02 (participation) resolves to a native row1 variant; D03
// (observed successful outcome) resolves to row5. No extra island/boss speech
// is established by that recording, so those transitions add no guessed row.
inline constexpr state::activity::coo::CommandSpec kJoinedVoiceCommands[]{
    {state::activity::coo::Operation::dialogue,{0xC8229B2B,0x80F5E33A,53,108},
        0xF821E425,state::activity::coo::Wait::nativeReady}
};
inline constexpr state::activity::coo::Step kJoinedVoiceSteps[]{
    {"submit_native_joined_conversation",0,kJoinedVoiceCommands}
};
inline constexpr state::activity::coo::Definition kJoinedVoiceGraph{
    "public_event_joined_dialogue",state::activity::coo::Schema::otherMissions,kJoinedVoiceSteps,{}};
inline constexpr public_event::dialogue::Definition kJoinedVoice{
    &kRegistries[1],&kJoinedVoiceGraph,108,{0x80F5E33A,0x80F5C030,0xF821E425,6913,8,1}};
inline constexpr state::activity::coo::CommandSpec kObservedSuccessVoiceCommands[]{
    {state::activity::coo::Operation::dialogue,{0xC8229B2B,0x80F5E33A,53,108},
        0xFF84E179,state::activity::coo::Wait::nativeReady}
};
inline constexpr state::activity::coo::Step kObservedSuccessVoiceSteps[]{
    {"submit_native_observed_success_conversation",0,kObservedSuccessVoiceCommands}
};
inline constexpr state::activity::coo::Definition kObservedSuccessVoiceGraph{
    "public_event_observed_success_dialogue",state::activity::coo::Schema::otherMissions,kObservedSuccessVoiceSteps,{}};
inline constexpr public_event::dialogue::Definition kObservedSuccessVoice{
    &kRegistries[1],&kObservedSuccessVoiceGraph,108,{0x80F5E33A,0x80F5C030,0xFF84E179,8512,8,5}};
} // namespace sunrise::server::runtime::activity::mercury::public_events
