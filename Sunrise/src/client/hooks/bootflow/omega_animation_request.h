#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sunrise::client::hooks::bootflow::omega_animation {
// DB5860 builds this request; F641E0 converts it into a 0x24-byte layer.
// DD7CD0 resolves key[2] as a named bank ROW, not a clip index or name hash.
struct Request {
    std::int32_t clipHandle{-1}, animationHandle{-1}, bankRow{};
    float elapsed{}, duration{}, weight{1.F};
    std::uint32_t loop{};
    float rate{1.F};
};
static_assert(sizeof(Request)==0x20 && offsetof(Request,elapsed)==0xC
    && offsetof(Request,loop)==0x18 && offsetof(Request,rate)==0x1C);
enum class Phase { intro, hoverPose, summon, summonPose };
class Sequence {
public:
    Request request(bool close) noexcept {
        if (phase_==Phase::intro && elapsed_>=8.7F) phase_=Phase::hoverPose;
        if (phase_==Phase::hoverPose && close) { phase_=Phase::summon; elapsed_=0; }
        if (phase_==Phase::summon && elapsed_>=7.2F) phase_=Phase::summonPose;
        const bool intro=phase_==Phase::intro || phase_==Phase::hoverPose;
        const bool hold=phase_==Phase::hoverPose || phase_==Phase::summonPose;
        Request result;
        result.bankRow=intro?5:13;
        result.duration=intro?8.7F:7.2F;
        result.elapsed=std::min(elapsed_,result.duration-0.01F);
        result.rate=hold?0.F:1.F;
        return result;
    }
    void advance(float dt) noexcept {
        if (std::isfinite(dt) && dt>0 && dt<=1.F
            && (phase_==Phase::intro || phase_==Phase::summon)) elapsed_+=dt;
    }
    Phase phase() const noexcept { return phase_; }
private:
    Phase phase_{Phase::intro};
    float elapsed_{};
};
}
