#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dawn::client::hooks::bootflow::eater_entity_id_startup {

/** Exact recovered Eater entrance whose initial object burst exceeds the retail cache midpoint. */
inline constexpr std::string_view kEntrancePackage = "raid_envy_v310";
inline constexpr std::uint8_t kEntranceBubble = 2U;
inline constexpr std::uint16_t kEntranceSlice = 16U;
inline constexpr std::uint32_t kEntranceSpawn = 0x8BA80878U;

struct CacheProfile final {
    std::uint32_t low{};
    std::uint32_t high{};

    friend constexpr bool operator==(CacheProfile, CacheProfile) noexcept = default;
};

/** Profile measured on the failing local-client path. Its midpoint is only 150 IDs. */
inline constexpr CacheProfile kRetailLocalProfile{100U, 200U};
/** Smallest larger profile already assigned by this native build. Its midpoint is 400 IDs. */
inline constexpr CacheProfile kEntranceStartupProfile{300U, 500U};

[[nodiscard]] constexpr std::uint32_t midpoint(CacheProfile profile) noexcept {
    return profile.low <= profile.high ? profile.low + ((profile.high - profile.low) / 2U)
                                       : profile.low;
}

static_assert(midpoint(kRetailLocalProfile) == 150U);
static_assert(midpoint(kEntranceStartupProfile) == 400U);

/** Scalar evidence required before one native manager may receive the startup profile. */
struct Eligibility final {
    std::string_view package{};
    std::uint8_t bubble{};
    std::uint16_t slice{};
    std::uint32_t spawn{};
    bool launchPending{};
    bool committedSession{};
    bool currentManager{};
    bool hasBubble{};
    bool hasSlice{};
    bool hasSpawn{};
};

[[nodiscard]] constexpr bool exact_entrance(Eligibility value) noexcept {
    return value.launchPending && value.committedSession && value.currentManager
           && value.hasBubble && value.hasSlice && value.hasSpawn
           && value.package == kEntrancePackage && value.bubble == kEntranceBubble
           && value.slice == kEntranceSlice && value.spawn == kEntranceSpawn;
}

struct MaintenanceDecision final {
    CacheProfile profile{};
    std::uint32_t target{};
    std::uint32_t requestDeficit{};
    bool overrideProfile{};
};

/**
 * Selects only the existing larger native profile for the exact pending entrance lifetime.
 * An unexpected role profile is forwarded unchanged rather than being reclassified.
 */
[[nodiscard]] constexpr MaintenanceDecision decide(Eligibility eligibility,
                                                   CacheProfile nativeProfile,
                                                   std::uint32_t available) noexcept {
    if (!exact_entrance(eligibility) || nativeProfile != kRetailLocalProfile) {
        return {nativeProfile, midpoint(nativeProfile), 0U, false};
    }
    const std::uint32_t target = midpoint(kEntranceStartupProfile);
    const std::uint32_t deficit =
        available < kEntranceStartupProfile.low && available < target ? target - available : 0U;
    return {kEntranceStartupProfile, target, deficit, true};
}

[[nodiscard]] constexpr std::uint64_t pack(CacheProfile profile) noexcept {
    return static_cast<std::uint64_t>(profile.low)
           | (static_cast<std::uint64_t>(profile.high) << 32U);
}

[[nodiscard]] constexpr CacheProfile unpack(std::uint64_t value) noexcept {
    return {static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32U)};
}

/**
 * Temporarily swaps an exact adjacent low/high pair and restores it with compare-exchange.
 * Store must provide compare_exchange(expected, desired), updating expected on failure.
 */
template <class Store>
class TemporaryProfileOverride final {
public:
    TemporaryProfileOverride(Store& store,
                             CacheProfile expected,
                             CacheProfile replacement) noexcept
        : store_(&store), original_(pack(expected)), replacement_(pack(replacement)) {
        std::uint64_t observed = original_;
        applied_ = store_->compare_exchange(observed, replacement_);
    }

    TemporaryProfileOverride(const TemporaryProfileOverride&) = delete;
    TemporaryProfileOverride& operator=(const TemporaryProfileOverride&) = delete;

    ~TemporaryProfileOverride() noexcept {
        (void)restore();
    }

    [[nodiscard]] bool applied() const noexcept {
        return applied_;
    }

    /** Restores only if no native lifecycle writer replaced the temporary pair. */
    [[nodiscard]] bool restore() noexcept {
        if (!applied_) {
            return true;
        }
        std::uint64_t observed = replacement_;
        const bool restored = store_->compare_exchange(observed, original_);
        applied_ = false;
        return restored;
    }

private:
    Store* store_{};
    std::uint64_t original_{};
    std::uint64_t replacement_{};
    bool applied_{};
};

} // namespace dawn::client::hooks::bootflow::eater_entity_id_startup
