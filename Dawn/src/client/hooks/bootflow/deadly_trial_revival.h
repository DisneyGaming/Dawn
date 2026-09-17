#pragma once
#include <cmath>
#include "gateway_native_read.h"
namespace dawn::client::hooks::bootflow::deadly_trial_revival {
// Native E4D770/E4A590, verified against the live 37.75-second Ghost-link.
// A timer, missing actor, or completed parent selector is not a completion.
struct Playback {
    std::uint32_t revision{};std::uint8_t mode{},active{};float elapsed{},duration{};
    bool valid(std::uint32_t generation) const noexcept {
        return generation && revision==generation && mode==2 && active<=1
            && std::isfinite(elapsed) && elapsed>=0.F
            && duration==37.75F;
    }
    bool started(std::uint32_t generation,bool participant) const noexcept {
        return valid(generation) && active==1 && elapsed>0.F && elapsed<duration && participant;
    }
    bool finished(std::uint32_t generation,bool wasStarted) const noexcept {
        return valid(generation) && wasStarted && active==0 && elapsed>=duration;
    }
};
bool install() noexcept;
void quiesce() noexcept;
bool uninstall() noexcept;
}
