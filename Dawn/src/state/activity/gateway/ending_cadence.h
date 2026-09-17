#pragma once
#include "frame.h"
namespace dawn::state::activity::gateway {
// Use the existing authoritative roster transport for responsive ending devices
// and dialogue cues. A failed send leaves another opportunity after 100 ms.
class EndingCadence final {
public:
    void reset() noexcept { run_=0;next_=0;enabled_=false; }
    void snapshot(std::uint64_t run,std::uint64_t now,const Frame& frame) noexcept {
        run_=run;enabled_=frame.enabled && (frame.moduleVulnerable || frame.pendingServices || frame.returnCuePending) && !frame.finished;next_=now+100;
    }
    [[nodiscard]] bool due(std::uint64_t run,std::uint64_t now) noexcept {
        if(!run || run!=run_ || !enabled_ || now<next_) { return false; }
        next_=now+100;return true;
    }
private:
    std::uint64_t run_{},next_{};bool enabled_{};
};
}
