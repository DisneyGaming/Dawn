#pragma once

#include "cue_presentation_service.h"

namespace sunrise::server::runtime::activity::haunted_forest_hud {
namespace cue = cue_presentation;

// These ids are stable data ids consumed by the round definition. The optional
// action 7 is reserved for the qualified pre-entry Terror pause; action 8 is a
// one-shot no-timer fallback for the first enter presentation because the
// current cue service requires a started timer before retain can be requested.
inline constexpr std::uint32_t kEnter = 1;
inline constexpr std::uint32_t kStart = 2;
inline constexpr std::uint32_t kResume = 3;
inline constexpr std::uint32_t kTerror = 4;
inline constexpr std::uint32_t kCollapse = 5;
inline constexpr std::uint32_t kResults = 6;
inline constexpr std::uint32_t kPauseTerror = 7;
inline constexpr std::uint32_t kInitialEnter = 8;

inline constexpr std::uint32_t kNativeCaptureComplete = 1;
inline constexpr std::uint32_t kGeneratorRequested = 2;
inline constexpr std::uint32_t kQualifiedEntryEvidence =
    kNativeCaptureComplete | kGeneratorRequested;
inline constexpr std::uint64_t kTraversalTimerTicks = 900ULL * 673200ULL;

inline constexpr auto actions() noexcept {
    cue::wire::Request enter{0x1EB557ADU,0x000D87C7U,0,0,UINT32_MAX};
    enter.variant = 0;

    auto start = enter;
    start.event = 0x6A3CC92FU;
    start.variant = 1;
    start.ring = 1;
    start.hasProgress = true;
    start.progress = {0,100};

    auto terror = enter;
    terror.event = 0xE0491E48U;

    auto collapse = enter;
    collapse.event = 0xC972D9EDU;
    collapse.variant = 1;

    auto results = enter;
    results.event = 0xB300293AU;
    results.variant = 1;

    return std::array<cue::Action,8>{
        cue::Action{kEnter,enter,cue::TimerOperation::retain,0,0,8,{}},
        cue::Action{kStart,start,cue::TimerOperation::start,kTraversalTimerTicks,
            kQualifiedEntryEvidence,26,100},
        cue::Action{kResume,start,cue::TimerOperation::resume,0,
            kQualifiedEntryEvidence,26,100},
        cue::Action{kTerror,terror,cue::TimerOperation::retain,0,0,{},{}},
        cue::Action{kCollapse,collapse,cue::TimerOperation::retain,0,0,20,100},
        cue::Action{kResults,results,cue::TimerOperation::pause,0,0,26,{}},
        cue::Action{kPauseTerror,terror,cue::TimerOperation::pause,0,0,{},{}},
        cue::Action{kInitialEnter,enter,cue::TimerOperation::absent,0,0,8,{}},
    };
}

inline constexpr coo::Asset kCompletionTimerAsset{0x4786C0E0U,0x80C10856U,18,2};

} // namespace sunrise::server::runtime::activity::haunted_forest_hud
