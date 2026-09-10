#pragma once
#include "cue_presentation_service.h"
namespace sunrise::server::runtime::activity::haunted_forest_hud {
namespace cue=cue_presentation;
inline constexpr std::uint32_t kNativeCaptureComplete=1,kGeneratorRequested=2;
// GeneratorRequested means the authoritative enabled generator command is
// retained for publication. It does not claim observed native worker ignition.
inline constexpr std::uint32_t kEnter=1,kBranchOne=2;
inline std::array<cue::Action,2> actions() noexcept {
    cue::wire::Request enter{0x1EB557AD,0x000D87C7,0,0,UINT32_MAX};
    auto branch=enter;branch.event=0x6A3CC92F;branch.variant=1;branch.ring=1;
    return {cue::Action{kEnter,enter,cue::TimerOperation::absent,0,0},
            cue::Action{kBranchOne,branch,cue::TimerOperation::start,900ULL*673200,
                        kNativeCaptureComplete|kGeneratorRequested}};
}
// No branch-percentage writer or branch/Terror completion trigger exists here.
// Later authored cues may use generic retain/pause/resume only after the graph
// has qualified their actual native completion evidence.
}
