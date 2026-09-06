#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include "../../../state/activity/lifecycle_generation.h"

namespace sunrise::client::hooks::activity_lifecycle {

/** Native activity handles use thirteen low bits to select one of 8192 pool slots. */
inline constexpr std::uint32_t kNativeActivationSlotMask = 0x1FFFU;
inline constexpr std::size_t kNativeActivationSlotCount =
    static_cast<std::size_t>(kNativeActivationSlotMask) + 1U;
inline constexpr std::uint32_t kInvalidNativeActivityHandle = 0xFFFFFFFFU;

/** Consumer-visible state of one exact native activation publication. */
enum class NativeActivationState : std::uint8_t {
    empty,
    active,
    quiescing,
    retired,
};

/** Immutable copy of one registry slot. Raw native fields are validation data, not identity. */
struct NativeActivationSnapshot final {
    state::activity::NativeActivationKey key{};
    std::uintptr_t wrapper{};
    std::uint64_t identity{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    std::int32_t mode{};
    NativeActivationState state{NativeActivationState::empty};
    bool identityValid{};

    /** @return True when this is a complete active or quiescing publication. */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(key) && wrapper != 0U
               && fullHandle != kInvalidNativeActivityHandle
               && (state == NativeActivationState::active
                   || state == NativeActivationState::quiescing);
    }
};

/** Exact predecessor token carried across a close original. */
struct NativeActivationToken final {
    state::activity::NativeActivationKey key{};
    std::uintptr_t wrapper{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(key) && wrapper != 0U
               && fullHandle != kInvalidNativeActivityHandle;
    }

    friend constexpr bool operator==(NativeActivationToken,
                                     NativeActivationToken) noexcept = default;
};

/** Values copied at activation entry before the native original may invalidate its arguments. */
struct NativeActivationAttempt final {
    std::uintptr_t wrapper{};
    std::uint64_t identity{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    std::int32_t mode{};
    std::uint8_t validateHost{};
    bool identityValid{};
};

enum class BeginModuleStatus : std::uint8_t {
    started,
    alreadyActive,
    generationExhausted,
};

struct BeginModuleResult final {
    BeginModuleStatus status{BeginModuleStatus::alreadyActive};
    state::activity::ModuleGeneration generation{};
};

enum class PublishStatus : std::uint8_t {
    published,
    moduleInactive,
    mutationDisabled,
    invalidAttempt,
    currentConflict,
    slotCollision,
    generationExhausted,
};

struct PublishResult final {
    PublishStatus status{PublishStatus::moduleInactive};
    NativeActivationSnapshot snapshot{};
};

enum class CaptureStatus : std::uint8_t {
    captured,
    moduleInactive,
    mutationDisabled,
    invalidNativeState,
    ownershipConflict,
    notCurrent,
};

struct CaptureResult final {
    CaptureStatus status{CaptureStatus::notCurrent};
    NativeActivationSnapshot predecessor{};
    NativeActivationToken token{};
};

enum class RetireStatus : std::uint8_t {
    retired,
    moduleInactive,
    invalidToken,
    staleToken,
    notQuiescing,
};

enum class GlobalDropBeginStatus : std::uint8_t {
    captured,
    moduleInactive,
    mutationDisabled,
    generationExhausted,
};

/** Small owner token for one bounded in-registry global-drop predecessor cohort. */
struct NativeActivationGlobalDropToken final {
    state::activity::ModuleGeneration module{};
    std::uint64_t epoch{};
    std::size_t captured{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(module) && epoch != 0U;
    }
};

struct GlobalDropBeginResult final {
    GlobalDropBeginStatus status{GlobalDropBeginStatus::moduleInactive};
    NativeActivationGlobalDropToken token{};
};

struct GlobalDropFinishResult final {
    std::size_t retired{};
    std::size_t stale{};
    bool matchedModule{};
};

/** Maximum native activity-wrapper pool extent proved for the pinned PC build. */
inline constexpr std::size_t kNativeActivePoolPinnedCapacity = 3U;

/** One native pool member presented to the registry's exact global-drop join. */
struct NativeActivePoolJoinInput final {
    std::uintptr_t wrapper{};
    std::uint64_t identity{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    std::int32_t mode{};
    bool identityValid{};
};

enum class NativeActivePoolJoinMemberStatus : std::uint8_t {
    none,
    exact,
    poolOnly,
    conflict,
};

/** Typed registry authority returned only for an exact native tuple. */
struct NativeActivePoolJoinOutput final {
    NativeActivePoolJoinMemberStatus status{NativeActivePoolJoinMemberStatus::none};
    NativeActivationSnapshot snapshot{};
    NativeActivationToken token{};
    bool identityMatch{};
    bool modeMatch{};
};

enum class NativeActivePoolJoinStatus : std::uint8_t {
    joined,
    diverged,
    moduleInactive,
    mutationDisabled,
    invalidArguments,
};

/** Exact set reconciliation of native allocated-and-active members against registry-active slots. */
struct NativeActivePoolJoinResult final {
    NativeActivePoolJoinStatus status{NativeActivePoolJoinStatus::invalidArguments};
    state::activity::ModuleGeneration module{};
    std::size_t exact{};
    std::size_t poolOnly{};
    std::size_t registryOnly{};
    std::size_t conflicts{};
};

/** Initial clock values used by deterministic no-game tests and process-lifetime recovery. */
struct NativeActivationClock final {
    state::activity::ModuleGeneration module{};
    state::activity::ActivationGeneration activation{};
    bool moduleExhausted{};
    bool activationExhausted{};
};

/**
 * Fixed direct registry for native activity activations.
 *
 * Each full handle selects exactly one slot with `fullHandle & 0x1FFF`. Every operation then
 * validates the complete wrapper, full handle, and typed generation key. The mutex is held only
 * while copying or changing registry-owned state; callers must never place a native original or
 * logging operation inside a registry method.
 */
class NativeActivationRegistry final {
public:
    explicit NativeActivationRegistry(NativeActivationClock clock = {}) noexcept;

    /** Starts a fresh module generation without resetting the process activation clock. */
    [[nodiscard]] BeginModuleResult begin_module() noexcept;

    /** Makes every current activation unusable before hook consumers begin uninstalling. */
    [[nodiscard]] bool
    quiesce_module(state::activity::ModuleGeneration expectedModule) noexcept;

    /** Clears publications only after consumers and the observer hooks have been removed. */
    [[nodiscard]] bool finish_module(
        state::activity::ModuleGeneration expectedModule) noexcept;

    /** Publishes one successful native activation or fails closed on impossible ownership. */
    [[nodiscard]] PublishResult publish(const NativeActivationAttempt& attempt) noexcept;

    /**
     * Captures and quiesces the exact current predecessor before a close original. A readable
     * native-active mismatch disables mutation without returning any token to retire.
     */
    [[nodiscard]] CaptureResult capture_predecessor(std::uintptr_t wrapper,
                                                    std::uint32_t fullHandle,
                                                    bool nativeActive) noexcept;

    /** Retires only the exact captured predecessor; a nested successor is left untouched. */
    [[nodiscard]] RetireStatus retire(NativeActivationToken token) noexcept;

    /**
     * Marks every currently active registry token quiescing before the global native drop.
     * Exact cohort membership is stored in the registry's fixed 8192-slot mark array.
     */
    [[nodiscard]] GlobalDropBeginResult begin_global_drop() noexcept;

    /** Retires only predecessor slots still carrying the exact captured global-drop epoch. */
    [[nodiscard]] GlobalDropFinishResult
    finish_global_drop(NativeActivationGlobalDropToken token) noexcept;

    /**
     * Reconciles the exact pinned-build native pool cohort under one short registry lock.
     *
     * Only exact (slot, wrapper, full-handle, identity, mode) publications receive typed tokens.
     * Pool-only, registry-only, metadata, and slot conflicts are reported distinctly and disable
     * later registry mutation; they are never silently absorbed into an all-active cohort.
     */
    [[nodiscard]] NativeActivePoolJoinResult join_native_active_pool(
        const NativeActivePoolJoinInput* inputs,
        std::size_t inputCount,
        NativeActivePoolJoinOutput* outputs,
        std::size_t outputCapacity) noexcept;

    /** Returns a copy of an exact active/quiescing publication and never mutates registry state. */
    [[nodiscard]] NativeActivationSnapshot snapshot(std::uintptr_t wrapper,
                                                    std::uint32_t fullHandle) const noexcept;

    /** Validates complete active ownership, including key, wrapper, and full native handle. */
    [[nodiscard]] bool is_current(NativeActivationToken token) const noexcept;

    [[nodiscard]] state::activity::ModuleGeneration current_module() const noexcept;
    [[nodiscard]] NativeActivationClock clock() const noexcept;
    [[nodiscard]] bool mutation_enabled() const noexcept;

private:
    [[nodiscard]] static constexpr std::size_t slot_index(std::uint32_t fullHandle) noexcept {
        return static_cast<std::size_t>(fullHandle & kNativeActivationSlotMask);
    }

    [[nodiscard]] static bool exact(const NativeActivationSnapshot& slot,
                                    NativeActivationToken token) noexcept;
    void disable_mutation_locked() noexcept;

    mutable std::mutex mutex_{};
    std::array<NativeActivationSnapshot, kNativeActivationSlotCount> slots_{};
    std::array<std::uint64_t, kNativeActivationSlotCount> globalDropMarks_{};
    NativeActivationClock clock_{};
    state::activity::ModuleGeneration currentModule_{};
    std::uint64_t globalDropEpoch_{};
    bool globalDropEpochExhausted_{};
    bool moduleActive_{};
    bool mutationEnabled_{};
};

namespace detail {

/** Process-wide registry owned by the five-hook observer batch. */
[[nodiscard]] NativeActivationRegistry& native_activation_registry() noexcept;

} // namespace detail

} // namespace sunrise::client::hooks::activity_lifecycle
