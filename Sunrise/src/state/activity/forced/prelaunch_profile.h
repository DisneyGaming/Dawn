#pragma once

#include <string_view>
#include "definition.h"

namespace sunrise::state::activity::forced::prelaunch {

// Pinned investment rows, independently joined to the installed package table.
// A profile permits selection-time derivation, not mission authority publication.
struct Profile final {
    std::string_view package;
    std::int16_t activity;
    std::uint32_t investmentHash;
    std::uint32_t packageHash;
    std::uint32_t activityTag;
    std::uint32_t launchTag;
    const char* event;
};
inline constexpr std::int16_t kDonorActivity = 282;
inline constexpr Profile kTowerfall{"mission_towerfall", 266, 0x62D85FB3U,
    0x9ACCB518U, 0x80B500ACU, 0x80FDB97FU, "towerfall_direct"};
inline constexpr Profile kGateway{"mission_abs", 292, 0x5A2E3FF4U,
    0x986985D0U, 0x80F46D99U, 0x80F9FDD2U, "gateway_direct"};

inline constexpr Profile kDeadlyTrial{"adventure_ginger",293,0x87D9CA16U,
    0xC9BC773AU,0x80B2E004U,0x80FDB97FU,"deadly_trial_direct"};

inline constexpr Profile kBeyondInfinity{"adventure_vod",294,0x3E9433BDU,
    0x03632571U,0x80F46000U,0x80F9FDD2U,"beyond_infinity_direct"};

[[nodiscard]] constexpr const Profile* find(std::string_view package) noexcept {
    if (package == kTowerfall.package) { return &kTowerfall; }
    if (package == kGateway.package) { return &kGateway; }
    if (package == kDeadlyTrial.package) { return &kDeadlyTrial; }
    if (package == kBeyondInfinity.package) { return &kBeyondInfinity; }
    return nullptr;
}
[[nodiscard]] constexpr const Profile* configured(const ForcedDestination& value) noexcept {
    return active(value)
        ? find({value.packageName.data(), value.packageNameLength}) : nullptr;
}
[[nodiscard]] constexpr bool donor(std::int16_t source, std::int16_t destination) noexcept {
    return source == kDonorActivity && destination == kDonorActivity;
}
[[nodiscard]] constexpr bool matches(const Profile& profile, std::int16_t source,
    std::int16_t destination, std::string_view package) noexcept {
    return source == profile.activity && destination == profile.activity && package == profile.package;
}

} // namespace sunrise::state::activity::forced::prelaunch
