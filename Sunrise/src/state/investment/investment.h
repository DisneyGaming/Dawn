#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::state {

/** Fixed runtime capacity for safe account unlock overrides. */
inline constexpr std::size_t kUnlockOverrideCapacity = 100;

/** One logical unlock-flag value stored by slot. */
struct UnlockFlagOverride {
    std::uint16_t slot{};
    std::uint8_t value{};
    friend bool operator==(const UnlockFlagOverride&, const UnlockFlagOverride&) = default;
};

/** One logical signed unlock value stored by slot. */
struct UnlockValueOverride {
    std::uint16_t slot{};
    std::int32_t value{};
    friend bool operator==(const UnlockValueOverride&, const UnlockValueOverride&) = default;
};

/** Global family-5 object and its bounded account overrides. */
struct Family5State {
    std::uint64_t objectSoid{};
    std::array<UnlockFlagOverride, kUnlockOverrideCapacity> flags{};
    std::size_t flagCount{};
    std::array<UnlockValueOverride, kUnlockOverrideCapacity> values{};
    std::size_t valueCount{};
    bool contentGateArm{};
    // Optional native80807C72 field1. State samples this process-global clock
    // for each publication; settings do not author its value or presence.
    std::int64_t timeSeconds{};
    bool hasTime{};
    friend bool operator==(const Family5State&, const Family5State&) = default;
};

/** Account-wide evaluated content state. */
struct InvestmentState {
    Family5State family5;
};

} // namespace sunrise::state
