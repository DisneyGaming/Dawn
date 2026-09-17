#pragma once

#include <cstdint>

namespace dawn::state::activity::omega_presentation {

/** The boss reveal is a type-6 cinematic, separate from Osiris's later type-43 Scene. */
inline constexpr std::uint32_t kIntroRegistry = 0xF4D0E0B2U;
inline constexpr std::uint32_t kIntroDefinition = 0x80F478D3U;
inline constexpr std::uint32_t kIntroResource = 0x80F44F66U;
inline constexpr std::uint32_t kIntroSelector = 0xA74B2200U;
inline constexpr std::uint16_t kIntroSlot = 22;
inline constexpr std::uint32_t kBossRegistry = 0x95FB2E01U;
inline constexpr std::uint32_t kBossDefinition = 0x80F4756AU;
inline constexpr std::uint32_t kBossEntity = 0x80F45253U;
inline constexpr std::uint16_t kBossSpawnRule = 57;

enum class IntroPhase : std::uint8_t { dormant, waiting, offered, playing, complete, failed, priming };
struct IntroCommand final { std::uint32_t revision{}; bool play{}; };

/** One reveal per mission run. Only a native active latch acknowledges playback; elapsed
 * time can cancel a request, but cannot report success or advance the boss encounter. */
class Intro final {
public:
    void reset(std::uint64_t run) noexcept {
        *this = {};
        // Separate new-run commands from packets/receipts belonging to the previous run.
        nextRevision_ = static_cast<std::uint32_t>(run * 32U) | 1U;
    }
    void request(std::uint64_t now) noexcept {
        if (phase_ != IntroPhase::dormant) { return; }
        phase_ = IntroPhase::waiting;
        requestedAt_ = now;
    }
    [[nodiscard]] IntroPhase phase() const noexcept { return phase_; }
    [[nodiscard]] IntroCommand command() const noexcept { return command_; }
    [[nodiscard]] bool busy() const noexcept {
        return phase_ == IntroPhase::priming || phase_ == IntroPhase::offered
            || phase_ == IntroPhase::playing;
    }
    void advance(std::uint64_t now) noexcept {
        if ((phase_ == IntroPhase::waiting || phase_ == IntroPhase::priming || phase_ == IntroPhase::offered)
            && now - requestedAt_ >= 30000) { finish(IntroPhase::failed); }
        // The retail reveal is approximately seven seconds. This is an abort deadline,
        // not a replacement for the native terminal/skip signal.
        if (phase_ == IntroPhase::playing && now - startedAt_ >= 30000) {
            finish(IntroPhase::failed);
        }
    }
    void observe(std::uint32_t revision, bool active, bool resourceReady,
                 bool mayStart, bool flightReady, std::uint64_t now) noexcept {
        advance(now);
        if (phase_ == IntroPhase::waiting && resourceReady && mayStart) {
            // Native boss creation and the authored graph hold precede the camera.
            // Only the current owned fly-node receipt can release playback.
            phase_ = IntroPhase::priming;
        } else if (phase_ == IntroPhase::priming && resourceReady && mayStart && flightReady) {
            command_ = {nextRevision_++, true};
            phase_ = IntroPhase::offered;
            offeredAt_ = now;
            ++attempts_;
        } else if (phase_ == IntroPhase::offered && revision == command_.revision) {
            if (active) { phase_ = IntroPhase::playing; startedAt_ = now; }
            else if (now - offeredAt_ >= 1000) {
                if (attempts_ >= 3) { finish(IntroPhase::failed); }
                else { phase_ = IntroPhase::priming; }
            }
        } else if (phase_ == IntroPhase::playing && revision == command_.revision && !active) {
            finish(IntroPhase::complete);
        }
    }
    void passed() noexcept {
        if (phase_ == IntroPhase::waiting || phase_ == IntroPhase::priming || phase_ == IntroPhase::offered) {
            finish(IntroPhase::failed);
        } else if (phase_ == IntroPhase::dormant) {
            phase_ = IntroPhase::complete; // direct arena arrival must not play the entry later
        }
    }
private:
    void finish(IntroPhase phase) noexcept {
        command_ = {nextRevision_++, false};
        phase_ = phase;
    }
    IntroPhase phase_{};
    IntroCommand command_{};
    std::uint32_t nextRevision_{1};
    std::uint8_t attempts_{};
    std::uint64_t requestedAt_{}, offeredAt_{}, startedAt_{};
};

} // namespace dawn::state::activity::omega_presentation
