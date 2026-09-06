#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace sunrise::client::hooks::bootflow {

namespace legacy_owner_sentinel {

struct HookOwnership final {
    bool attached{};
    bool originalPublished{};
};

[[nodiscard]] constexpr bool any_handle_attached(
    std::span<const HookOwnership> hooks) noexcept {
    for (const HookOwnership hook : hooks) {
        if (hook.attached) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr bool any_original_published(
    std::span<const HookOwnership> hooks) noexcept {
    for (const HookOwnership hook : hooks) {
        if (hook.originalPublished) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr bool has_ownership(std::span<const HookOwnership> hooks,
                                           std::span<const bool> claims = {}) noexcept {
    if (any_handle_attached(hooks) || any_original_published(hooks)) {
        return true;
    }
    for (const bool claim : claims) {
        if (claim) {
            return true;
        }
    }
    return false;
}

inline constexpr std::size_t kQuarantinedOwnerCount = 9U;
inline constexpr std::array<std::size_t, kQuarantinedOwnerCount> kOwnerHookCounts{
    40U, 8U, 1U, 5U, 116U, 1U, 9U, 18U, 3U};
inline constexpr std::size_t kCompleteLegacyHookCount = 201U;

[[nodiscard]] constexpr std::size_t mapped_hook_count() noexcept {
    std::size_t result{};
    for (const std::size_t count : kOwnerHookCounts) {
        result += count;
    }
    return result;
}

[[nodiscard]] constexpr bool any_quarantined_legacy_ownership(
    const std::array<bool, kQuarantinedOwnerCount>& owners) noexcept {
    for (const bool owner : owners) {
        if (owner) {
            return true;
        }
    }
    return false;
}

static_assert(kOwnerHookCounts.size() == kQuarantinedOwnerCount);
static_assert(mapped_hook_count() == kCompleteLegacyHookCount);

} // namespace legacy_owner_sentinel

// These snapshots read legacy Handle::attached fields and, for selection, the raw VEH token.
// Callers must serialize them with late-install admission exactly as the bootflow lifecycle does.
[[nodiscard]] bool activity_selection_probe_has_ownership() noexcept;
[[nodiscard]] bool player_broadcast_create_probe_has_ownership() noexcept;
[[nodiscard]] bool activity_feature_flag_probe_has_ownership() noexcept;
[[nodiscard]] bool activity_script_event_probe_has_ownership() noexcept;
[[nodiscard]] bool activity_script_upstream_probe_has_ownership() noexcept;
[[nodiscard]] bool activity_notification_type1_apply_probe_has_ownership() noexcept;
[[nodiscard]] bool activity_behavior_condition_probe_has_ownership() noexcept;
[[nodiscard]] bool activity_schema_decode_legacy_bundle_has_ownership() noexcept;
[[nodiscard]] bool group_initial_update_decode_probe_has_ownership() noexcept;

} // namespace sunrise::client::hooks::bootflow
