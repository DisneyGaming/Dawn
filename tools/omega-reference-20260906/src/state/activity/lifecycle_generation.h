#pragma once

#include <cstdint>
#include <limits>

namespace sunrise::state::activity {

/** Zero is reserved for an absent generation. */
inline constexpr std::uint64_t kAbsentGeneration = 0;
/** The first generation allocated within an owning scope. */
inline constexpr std::uint64_t kFirstGeneration = 1;
/** The last valid generation. Advancing it exhausts the owning allocator. */
inline constexpr std::uint64_t kMaximumGeneration =
    (std::numeric_limits<std::uint64_t>::max)();

/**
 * A lifetime generation whose tag prevents values from different ownership domains from mixing.
 * Zero means absent; every nonzero value identifies one generation within its owning scope.
 */
template <class Tag>
struct Generation final {
    std::uint64_t value{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return value != kAbsentGeneration;
    }

    friend constexpr bool operator==(Generation, Generation) noexcept = default;
};

/**
 * Advances one generation without wrapping.
 *
 * Once exhaustion is observed, later calls remain disabled even if the caller presents an
 * inconsistent lower value. On the first attempt to advance the maximum valid value, the value is
 * preserved, the owning allocator is marked exhausted, and false is returned.
 */
template <class Tag>
[[nodiscard]] constexpr bool advance(Generation<Tag>& generation, bool& exhausted) noexcept {
    if (exhausted) {
        return false;
    }
    if (generation.value == kMaximumGeneration) {
        exhausted = true;
        return false;
    }

    ++generation.value;
    return true;
}

namespace generation_tags {

struct Module;
struct Connection;
struct Authentication;
struct Binding;
struct ActivityIncarnation;
struct HostRegion;
struct Activation;
struct NativeTransition;
struct RegionGraph;
struct RegionRecord;
struct RosterGraph;
struct Scene;
struct Checkpoint;
struct Wipe;
struct Life;
struct Publication;

} // namespace generation_tags

using ModuleGeneration = Generation<generation_tags::Module>;
using ConnectionGeneration = Generation<generation_tags::Connection>;
using AuthenticationGeneration = Generation<generation_tags::Authentication>;
using BindingGeneration = Generation<generation_tags::Binding>;
using ActivityIncarnation = Generation<generation_tags::ActivityIncarnation>;
using HostRegionGeneration = Generation<generation_tags::HostRegion>;
using ActivationGeneration = Generation<generation_tags::Activation>;
using NativeTransitionGeneration = Generation<generation_tags::NativeTransition>;
using RegionGraphGeneration = Generation<generation_tags::RegionGraph>;
using RegionRecordGeneration = Generation<generation_tags::RegionRecord>;
using RosterGraphGeneration = Generation<generation_tags::RosterGraph>;
using SceneGeneration = Generation<generation_tags::Scene>;
using CheckpointGeneration = Generation<generation_tags::Checkpoint>;
using WipeGeneration = Generation<generation_tags::Wipe>;
using LifeGeneration = Generation<generation_tags::Life>;
using PublicationGeneration = Generation<generation_tags::Publication>;

/** Identifies one activity incarnation within a protocol session. */
struct ActivityInstanceKey final {
    std::uint64_t sessionId{};
    ActivityIncarnation incarnation{};

    /** @return True only when both the session and its incarnation are present. */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return sessionId != 0 && static_cast<bool>(incarnation);
    }

    friend constexpr bool operator==(ActivityInstanceKey, ActivityInstanceKey) noexcept = default;
};

/** Identifies one lifetime of a protocol connection. */
struct ConnectionKey final {
    std::uint32_t connectionId{};
    ConnectionGeneration generation{};

    /**
     * @return True when the connection generation is present.
     *
     * Connection ID zero is a valid protocol value, so it does not denote absence.
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(generation);
    }

    friend constexpr bool operator==(ConnectionKey, ConnectionKey) noexcept = default;
};

/** Identifies one authentication lifetime owned by a connection lifetime. */
struct AuthenticationKey final {
    ConnectionKey connection{};
    AuthenticationGeneration generation{};

    /** @return True only when the owning connection and authentication are present. */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(connection) && static_cast<bool>(generation);
    }

    friend constexpr bool operator==(AuthenticationKey, AuthenticationKey) noexcept = default;
};

/** Identifies one binding lifetime owned by an authentication lifetime. */
struct BindingKey final {
    AuthenticationKey authentication{};
    BindingGeneration generation{};

    /** @return True only when the owning authentication and binding are present. */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(authentication) && static_cast<bool>(generation);
    }

    friend constexpr bool operator==(BindingKey, BindingKey) noexcept = default;
};

/** Identifies one host-region lifetime owned by an activity incarnation. */
struct HostRegionKey final {
    ActivityInstanceKey activity{};
    HostRegionGeneration generation{};

    /** @return True only when the owning activity and host region are present. */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(activity) && static_cast<bool>(generation);
    }

    friend constexpr bool operator==(HostRegionKey, HostRegionKey) noexcept = default;
};

/** Identifies one native activation lifetime within a module lifetime. */
struct NativeActivationKey final {
    ModuleGeneration module{};
    ActivationGeneration generation{};

    /** @return True only when both the owning module and activation are present. */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(module) && static_cast<bool>(generation);
    }

    friend constexpr bool operator==(NativeActivationKey, NativeActivationKey) noexcept = default;
};

} // namespace sunrise::state::activity
