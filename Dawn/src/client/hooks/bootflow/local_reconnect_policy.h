#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::client::hooks::bootflow::local_reconnect {
inline constexpr std::uint64_t kRetailCooldown = 10000;
inline constexpr std::uint64_t kLocalCooldown = 250;
inline constexpr std::size_t kChannelBase = 0xA8;
inline constexpr std::size_t kChannelStride = 0x41F0;
inline constexpr std::size_t kChannelState = 0x1D18;
inline constexpr std::size_t kCloseReason = 0x1D1C;
inline constexpr std::size_t kManagerState = 0x3040;
inline constexpr std::size_t kStateTime = 0x3048;
inline constexpr std::size_t kRetry = 0x3089;
inline constexpr std::size_t kAddress = 0x309C;
using Address = std::array<std::byte, 86>;

struct Snapshot {
    std::uint32_t managerState{};
    std::uint32_t channelState{};
    std::uint32_t closeReason{};
    std::uint32_t cooldown{};
    std::uint64_t stateTime{};
    std::uint8_t retry{};
    Address address{};
};

// Native 17CC1A0 resets security and finishes by entering waiting (1).
// Native 1802D4B..1802D94 then compares elapsed state time with config+0xC.
// Only owners-released (9) qualifies, from established (4) or already waiting (1).
// The native update can first close without resetting owners, then reset them on
// its next pass while already waiting. That second call arms the retry flag.
// Never accelerate failed establishment, timeouts, remote peers or unknown tuning.
[[nodiscard]] inline bool shortened_time(const Snapshot& before, const Snapshot& after,
                                         bool localReady, const Address& expected,
                                         std::uint64_t& result) noexcept {
    if (!localReady || (before.managerState != 4 && before.managerState != 1) || before.channelState > 2
        || before.closeReason != 9 || before.address != expected
        || before.cooldown != kRetailCooldown || after.cooldown != before.cooldown
        || after.managerState != 1 || after.retry != 1 || after.address != before.address
        || after.stateTime < kRetailCooldown - kLocalCooldown) {
        return false;
    }
    result = after.stateTime - (kRetailCooldown - kLocalCooldown);
    return true;
}
}
