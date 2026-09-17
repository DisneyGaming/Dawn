#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace dawn::client::hooks::activity_lifecycle {

inline constexpr std::uint32_t kNativeActivationGlobalDropObserverAbiVersion = 1U;

/** Neutral scalar identity of one activation-registry cohort around the global-drop original. */
struct NativeActivationGlobalDropCohort final {
    std::uint64_t moduleGeneration{};
    std::uint64_t cohortEpoch{};
    std::uint32_t captured{};
    bool tokenValid{};

    friend constexpr bool operator==(NativeActivationGlobalDropCohort,
                                     NativeActivationGlobalDropCohort) noexcept = default;
};

using NativeActivationGlobalDropCallback =
    void (*)(void* context, const NativeActivationGlobalDropCohort& cohort) noexcept;

/**
 * Immutable, caller-owned callback table published as one versioned pointer.
 * The caller retains this table, its context, and both callback code ranges until unregister
 * succeeds or the native provider reports confirmed removal. Callbacks must use fixed storage and
 * must not allocate, format/log, open files, perform I/O, or call registry mutation APIs.
 */
struct NativeActivationGlobalDropObserverTable final {
    std::uint32_t abiVersion{kNativeActivationGlobalDropObserverAbiVersion};
    void* context{};
    NativeActivationGlobalDropCallback pre{};
    NativeActivationGlobalDropCallback post{};
};

static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic_bool::is_always_lock_free);
static_assert(std::atomic<const NativeActivationGlobalDropObserverTable*>::is_always_lock_free);

/** Generation-safe registration token. A token from a prior publication cannot remove a reuse. */
struct NativeActivationGlobalDropObserverHandle final {
    std::uint64_t generation{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return generation != 0U;
    }

    friend constexpr bool operator==(NativeActivationGlobalDropObserverHandle,
                                     NativeActivationGlobalDropObserverHandle) noexcept = default;
};

/**
 * Single-sink, allocation-free global-drop fanout.
 *
 * Dispatch ownership may span the native original solely to retain the immutable table/context.
 * The pre and post callbacks are separate bounded calls; no callback or fanout lock spans native.
 * Every operation is lock-free/non-waiting at this API boundary. Native global drops may be
 * concurrent or nested, so the registered sink must make its own fixed storage reentrant-safe.
 */
class NativeActivationGlobalDropFanout final {
public:
    class Dispatch final {
    public:
        Dispatch() noexcept = default;
        ~Dispatch() noexcept;

        Dispatch(const Dispatch&) = delete;
        Dispatch& operator=(const Dispatch&) = delete;

        Dispatch(Dispatch&& other) noexcept;
        Dispatch& operator=(Dispatch&& other) noexcept;

        [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }

        /** Invokes the pre callback once; the fanout owns no lock while callback code runs. */
        void notify_pre(const NativeActivationGlobalDropCohort& cohort) noexcept;

        /** Invokes the post callback once and separately from pre/native forwarding. */
        void notify_post(const NativeActivationGlobalDropCohort& cohort) noexcept;

        /** Releases the immutable table/context lease immediately after the post callback. */
        void finish() noexcept;

    private:
        friend class NativeActivationGlobalDropFanout;

        Dispatch(NativeActivationGlobalDropFanout& owner,
                 const NativeActivationGlobalDropObserverTable& table,
                 std::uint64_t generation) noexcept;

        NativeActivationGlobalDropFanout* owner_{};
        const NativeActivationGlobalDropObserverTable* table_{};
        std::uint64_t generation_{};
        bool preCalled_{};
        bool postCalled_{};
    };

    NativeActivationGlobalDropFanout() noexcept = default;

    /** Publishes one complete immutable table; registration performs no allocation or waiting. */
    [[nodiscard]] bool register_observer(
        const NativeActivationGlobalDropObserverTable* table,
        NativeActivationGlobalDropObserverHandle& output) noexcept;

    /**
     * Begins generation-safe retirement. False means a dispatch still owns the table/context and
     * the caller must retain them and retry; the operation never waits.
     */
    [[nodiscard]] bool
    unregister_observer(NativeActivationGlobalDropObserverHandle handle) noexcept;

    /** Opens dispatch admission only after the provider's complete five-hook publication. */
    void accept() noexcept;

    /** Stops new dispatch acquisition without disturbing a pre/native/post pair already admitted. */
    void quiesce() noexcept;

    /** Acquires one generation-stable table/context lease without locking or waiting. */
    [[nodiscard]] Dispatch acquire() noexcept;

    /** Clears registration only after confirmed provider removal; false retains it for retry. */
    [[nodiscard]] bool clear_after_removed() noexcept;

    [[nodiscard]] bool idle() const noexcept;
    [[nodiscard]] bool accepting() const noexcept;
    [[nodiscard]] bool has_registration() const noexcept;

private:
    friend class Dispatch;

    static constexpr std::uint64_t kFree = 0U;
    static constexpr std::uint64_t kActive = 1U;
    static constexpr std::uint64_t kRetiring = 2U;
    static constexpr std::uint64_t kReserved = 3U;
    static constexpr std::uint64_t kClearing = 4U;
    static constexpr std::uint64_t kControlStateMask = 7U;
    static constexpr std::uint64_t kControlGenerationShift = 3U;
    static constexpr std::uint64_t kMaximumObserverGeneration =
        (std::numeric_limits<std::uint64_t>::max)() >> kControlGenerationShift;

    [[nodiscard]] static constexpr std::uint64_t make_control(
        std::uint64_t generation,
        std::uint64_t state) noexcept {
        return (generation << kControlGenerationShift) | state;
    }

    [[nodiscard]] static constexpr std::uint64_t control_generation(
        std::uint64_t control) noexcept {
        return control >> kControlGenerationShift;
    }

    [[nodiscard]] static constexpr std::uint64_t control_state(
        std::uint64_t control) noexcept {
        return control & kControlStateMask;
    }

    void release_dispatch(std::uint64_t generation) noexcept;
    [[nodiscard]] bool retire_current(std::uint64_t expectedGeneration,
                                      bool requireGeneration) noexcept;

    std::atomic<std::uint64_t> control_{};
    std::atomic<std::uint32_t> inFlight_{};
    std::atomic<const NativeActivationGlobalDropObserverTable*> table_{};
    std::atomic_bool accepting_{};
};

/** Process-wide registration surface consumed by the sole native +3C8EB0 owner. */
[[nodiscard]] bool register_global_drop_observer(
    const NativeActivationGlobalDropObserverTable* table,
    NativeActivationGlobalDropObserverHandle& output) noexcept;
[[nodiscard]] bool
unregister_global_drop_observer(NativeActivationGlobalDropObserverHandle handle) noexcept;

namespace detail {

[[nodiscard]] NativeActivationGlobalDropFanout& native_activation_global_drop_fanout() noexcept;

/** Out-of-line dispatch seams used only inside the sole +3C8EB0 full-call scope. */
void notify_native_activation_global_drop_pre(
    NativeActivationGlobalDropFanout::Dispatch& dispatch,
    const NativeActivationGlobalDropCohort& cohort) noexcept;
void notify_native_activation_global_drop_post(
    NativeActivationGlobalDropFanout::Dispatch& dispatch,
    const NativeActivationGlobalDropCohort& cohort) noexcept;

} // namespace detail

} // namespace dawn::client::hooks::activity_lifecycle
