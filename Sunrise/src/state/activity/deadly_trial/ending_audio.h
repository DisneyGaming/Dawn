#pragma once
#include <cmath>
#include <cstdint>
namespace sunrise::state::activity::deadly_trial {
// Authored 8080658D events in 80F275D9 (via 80FBDA45 -> 80F275DB).
// Row 9 starts at 0s, row 10 at 10s. The native animation owns both.
inline constexpr float kRevivalAudioCueSeconds=10.F;
inline constexpr std::uint64_t kRevivalAudioEndMs=10000+38675+250;
// The owner clock stops at 37.75s; dialogue continues beyond Ghost's return.
// Keep that tail anchored to a witnessed native cue, plus native scene end.
inline bool valid_revival_audio_cue(float elapsed) noexcept {
    return std::isfinite(elapsed) && elapsed>=kRevivalAudioCueSeconds && elapsed<37.75F;
}
inline std::uint64_t revival_audio_remaining(float elapsed) noexcept {
    return kRevivalAudioEndMs-static_cast<std::uint64_t>(elapsed*1000.F);
}
}
