#pragma once

#include <cstdint>
#include <limits>

namespace sunrise::state::investment_clock {

// Process-global investment time, shared by family5 and native Director content.
// Anchor to UTC once at State initialization; later snapshots use only elapsed
// monotonic milliseconds so a wall-clock adjustment cannot move the clock back.
class Clock final {
public:
    [[nodiscard]] bool begin(std::int64_t utcSeconds, std::uint64_t monotonicMillis) noexcept {
        if (initialized_ || utcSeconds < 0) {
            return false;
        }
        utcSeconds_ = utcSeconds;
        monotonicMillis_ = monotonicMillis;
        initialized_ = true;
        return true;
    }

    [[nodiscard]] bool sample(std::uint64_t monotonicMillis, std::int64_t& seconds) const noexcept {
        seconds = 0;
        if (!initialized_ || monotonicMillis < monotonicMillis_) {
            return false;
        }
        const auto elapsed = (monotonicMillis - monotonicMillis_) / 1000;
        const auto remaining = static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)() - utcSeconds_);
        if (elapsed > remaining) {
            return false;
        }
        seconds = utcSeconds_ + static_cast<std::int64_t>(elapsed);
        return true;
    }

private:
    std::int64_t utcSeconds_{};
    std::uint64_t monotonicMillis_{};
    bool initialized_{};
};

} // namespace sunrise::state::investment_clock
