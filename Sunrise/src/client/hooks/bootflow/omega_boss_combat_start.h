#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include "omega_boss_graph_observation.h"

namespace sunrise::client::hooks::bootflow::omega_boss_combat_start {

inline constexpr std::uint32_t kRightProperty = 0x8496ABD2U;
inline constexpr std::uint16_t kRightClipIndex = 19;
inline constexpr std::uint32_t kRightClip = 0x80F4518AU;
inline constexpr std::uint32_t kRightAction = 0xAEE0A45FU;
inline constexpr std::uint32_t kLeftProperty = 0xA2AE120FU; // FNV-1 panoptes_summon_left
inline constexpr std::uint32_t kLeftClip = 0x80F4518BU;
inline constexpr std::uint32_t kFullBodyAsset = 0x815B5A41U;
inline constexpr std::size_t kFrameBytes = 0xC4;
inline constexpr std::uint32_t kActionAsset = 0x80F45197U;
inline constexpr std::uint32_t kLeftAction = 0x48FBF8E8U;
inline constexpr std::uint16_t kLeftClipIndex = 20;
inline constexpr std::size_t kActionGroupBytes = 0x78;
enum class Stage : std::uint8_t { unissued, requesting, requested, playing, releasing, complete, uncertain };
struct Track {
    Stage stage{};
    std::uint32_t self{UINT32_MAX}, entity{UINT32_MAX};
    std::uint32_t scalarSelf{UINT32_MAX};
    std::int32_t scalarIndex{-1};
    bool boundScalar{};
    float previousTime{};
    std::uint8_t layer{};
    bool previousValid{};
    unsigned rejectedSamples{};
};
struct Playback { float time{}, weight{}; std::uint8_t layer{}; };

// 1047590 emits custom animation rows, and 104A700 copies them to the
// biped's render state. 1040990 consumes +C as a clip-bank index and +18
// as playback seconds. This is NOT the base frame passed into 104B7A0.
// 104C9D0 advances/wraps the native clock against the authored duration.
inline bool left_playback(std::span<const std::byte> group, float duration, Playback& out, bool right = false) noexcept {
    namespace o = omega_boss_graph_observation;
    out = {};
    if (group.size() < kActionGroupBytes || !std::isfinite(duration) || duration <= 0.F) return false;
    const auto count = o::field<std::int32_t>(group, 0x70);
    if (count < 0 || count > 4) return false;
    bool found{};
    for (int i = 0; i < count; ++i) {
        const auto base = static_cast<std::size_t>(i) * 0x1C;
        if (o::field<std::uint16_t>(group, base + 0xC) != (right ? kRightClipIndex : kLeftClipIndex)
            || o::field<std::uint16_t>(group, base + 0xE) != 0
            || o::field<std::uint16_t>(group, base + 0x10) != 0
            || o::field<std::uint8_t>(group, base + 0x13) != 1) continue;
        const auto weight = o::field<float>(group, base + 4);
        const auto time = o::field<float>(group, base + 0x18) / duration;
        const auto layer = o::field<std::uint8_t>(group, base + 0x14);
        if (!std::isfinite(weight) || weight <= 0.001F || weight > 1.F
            || !std::isfinite(time) || time < 0.F || time > 1.F
            || (layer != 1 && layer != 2)) continue;
        if (!found || weight > out.weight) { out = {time, weight, layer}; found = true; }
    }
    return found;
}

// A stable native playback phase crossing the end of its cycle is the release
// boundary. There is deliberately no wall-clock fallback for a missing receipt.
inline bool wrapped(const Track& track, const Playback& playback) noexcept {
    return track.previousValid && track.layer == playback.layer
        && track.previousTime >= 0.8F && playback.time <= 0.2F;
}
} // namespace sunrise::client::hooks::bootflow::omega_boss_combat_start
