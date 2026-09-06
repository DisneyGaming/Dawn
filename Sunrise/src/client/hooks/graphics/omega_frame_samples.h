#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace sunrise::client::hooks::graphics::omega_frame_timing {

inline constexpr std::uint64_t kCaptureSeconds = 600;
struct FrameInput {
    std::uint64_t run{}, chain{}, entry{}, nativeBegin{}, nativeEnd{};
    std::uint32_t phase{}, syncInterval{};
    bool success{};
};
struct FrameReport {
    std::uint64_t elapsed{}, presents{}, failed{}, intervals{}, intervalTicks{};
    std::uint64_t minimum{UINT64_MAX}, maximum{}, presentTicks{}, prePresentTicks{};
    std::uint64_t presentMaximum{}, prePresentMaximum{};
    std::uint32_t syncMinimum{UINT32_MAX}, syncMaximum{};
};
struct FrameResult { bool ready{}, expired{}, modeChanged{}, windowChanged{}; FrameReport report{}; };

// QPC timestamps are supplied by the existing Present hook. Neither native AI
// callback counts nor cinematic tick counts participate in the cadence result.
class FrameWindow final {
public:
    [[nodiscard]] FrameResult push(const FrameInput& input, std::uint64_t frequency) noexcept {
        FrameResult result{};
        if (!frequency || !input.run || input.run == UINT64_MAX || !input.chain || !input.entry
            || input.nativeBegin < input.entry || input.nativeEnd < input.nativeBegin) return result;
        if (input.run != run_) {
            *this = {};
            run_ = input.run;
            budgetStart_ = input.entry;
        }
        if (input.entry < budgetStart_) return result;
        if ((input.entry - budgetStart_) / frequency >= kCaptureSeconds) {
            result.expired = true;
            return result;
        }
        result.modeChanged = chain_ != input.chain;
        if (!windowStart_ || phase_ != input.phase || result.modeChanged) {
            result.windowChanged = true;
            chain_ = input.chain;
            phase_ = input.phase;
            windowStart_ = input.entry;
            previous_ = 0;
            report_ = {};
        }
        if (input.entry < windowStart_ || (previous_ && input.entry < previous_)) return result;
        report_.syncMinimum = std::min(report_.syncMinimum, input.syncInterval);
        report_.syncMaximum = std::max(report_.syncMaximum, input.syncInterval);
        if (input.success) {
            ++report_.presents;
            if (previous_) {
                const auto interval = input.entry - previous_;
                ++report_.intervals;
                report_.intervalTicks += interval;
                report_.minimum = std::min(report_.minimum, interval);
                report_.maximum = std::max(report_.maximum, interval);
            }
            previous_ = input.entry;
            const auto present = input.nativeEnd - input.nativeBegin;
            const auto prePresent = input.nativeBegin - input.entry;
            report_.presentTicks += present;
            report_.prePresentTicks += prePresent;
            report_.presentMaximum = std::max(report_.presentMaximum, present);
            report_.prePresentMaximum = std::max(report_.prePresentMaximum, prePresent);
        } else {
            ++report_.failed;
        }
        if (input.entry - windowStart_ < frequency) return result;
        report_.elapsed = input.entry - windowStart_;
        result.ready = true;
        result.report = report_;
        windowStart_ = input.entry;
        report_ = {};
        return result;
    }
private:
    std::uint64_t run_{}, chain_{}, budgetStart_{}, windowStart_{}, previous_{};
    std::uint32_t phase_{};
    FrameReport report_{};
};
} // namespace sunrise::client::hooks::graphics::omega_frame_timing
