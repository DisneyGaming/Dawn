#pragma once
#include <cmath>
#include <cstdint>
namespace sunrise::state::activity::coo {
// E4D770/E4A590 use effective native duration after source overrides.
// Always use the effective controller duration after the source overrides.
struct ScanPlayback {
    std::uint32_t revision{};std::uint8_t mode{},active{};float elapsed{},duration{};
    bool valid(std::uint32_t generation) const noexcept {
        return generation && revision==generation && mode==2 && active<=1
            && std::isfinite(elapsed) && elapsed>=0.F && std::isfinite(duration) && duration>0.F;
    }
    bool started(std::uint32_t generation,bool participant) const noexcept {
        return valid(generation) && active==1 && elapsed>0.F && elapsed<duration && participant;
    }
    bool finished(std::uint32_t generation,bool wasStarted) const noexcept {
        return valid(generation) && wasStarted && active==0 && elapsed>=duration;
    }
};
}
