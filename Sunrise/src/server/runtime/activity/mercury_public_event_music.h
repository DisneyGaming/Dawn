#pragma once
#include "mercury_public_event_registries.h"
#include "music_runtime.h"

namespace sunrise::server::runtime::activity::mercury::public_events {
// 80F5E362+648: one A91A199E selector group, four neutral-guard
// candidates. Their audio remains entirely in package 80F1FFFA. Ordinals 0/1
// select the same native playlist; no unsupported phase distinction is added.
inline constexpr std::uint32_t kMusicCandidates[]{0x5DC647A2, 0x6C2A5EF5, 0xBE5B494E, 0x8B2903F1};
inline constexpr state::activity::coo::Asset kMusicAsset{0xC8229B2B, 0x80F5E362, 11, 109};
inline constexpr state::activity::coo::CommandSpec kMusicCommands[]{
    {state::activity::coo::Operation::device, kMusicAsset, 0, state::activity::coo::Wait::requested},
    {state::activity::coo::Operation::device, kMusicAsset, kMusicCandidates[0], state::activity::coo::Wait::requested},
    {state::activity::coo::Operation::device, kMusicAsset, kMusicCandidates[1], state::activity::coo::Wait::requested},
    {state::activity::coo::Operation::device, kMusicAsset, kMusicCandidates[2], state::activity::coo::Wait::requested},
    {state::activity::coo::Operation::device, kMusicAsset, kMusicCandidates[3], state::activity::coo::Wait::requested}};
inline constexpr state::activity::coo::Step kMusicSteps[]{{"selector_clear", 0, {&kMusicCommands[0], 1}},
                                                          {"selector_5DC647A2", 0, {&kMusicCommands[1], 1}},
                                                          {"selector_6C2A5EF5", 0, {&kMusicCommands[2], 1}},
                                                          {"selector_BE5B494E", 0, {&kMusicCommands[3], 1}},
                                                          {"selector_8B2903F1", 0, {&kMusicCommands[4], 1}}};
inline constexpr state::activity::coo::Definition kMusicGraphs[]{
    {"native_music_clear", state::activity::coo::Schema::otherMissions, {&kMusicSteps[0], 1}, {}},
    {"native_music_5DC647A2", state::activity::coo::Schema::otherMissions, {&kMusicSteps[1], 1}, {}},
    {"native_music_6C2A5EF5", state::activity::coo::Schema::otherMissions, {&kMusicSteps[2], 1}, {}},
    {"native_music_BE5B494E", state::activity::coo::Schema::otherMissions, {&kMusicSteps[3], 1}, {}},
    {"native_music_8B2903F1", state::activity::coo::Schema::otherMissions, {&kMusicSteps[4], 1}, {}}};
inline constexpr music::Selection kMusicSelections[]{{0, &kMusicGraphs[0]},
                                                     {kMusicCandidates[0], &kMusicGraphs[1]},
                                                     {kMusicCandidates[1], &kMusicGraphs[2]},
                                                     {kMusicCandidates[2], &kMusicGraphs[3]},
                                                     {kMusicCandidates[3], &kMusicGraphs[4]}};
inline constexpr music::Definition kMusicDefinition{&kRegistries[1], 109, 0xA91A199E, kMusicCandidates,
                                                    kMusicSelections};
} // namespace sunrise::server::runtime::activity::mercury::public_events
