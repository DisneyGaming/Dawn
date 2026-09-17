#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::client::hooks::activity_lifecycle {

inline constexpr std::size_t kNativeActivationHookCount = 5U;

/** Serialized ownership phase for the all-or-none five-hook batch. */
enum class NativeActivationBatchPhase : std::uint8_t {
    detached,
    installing,
    running,
    quiescing,
};

/** Result supplied by the protected Detours adapter after all consumers are gone. */
enum class NativeActivationDetachDisposition : std::uint8_t {
    removed,
    deferred,
    failed,
};

struct NativeActivationBatchSnapshot final {
    std::array<std::uintptr_t, kNativeActivationHookCount> originals{};
    NativeActivationBatchPhase phase{NativeActivationBatchPhase::detached};
};

/**
 * Deterministic ownership model shared by runtime orchestration and no-game lifecycle tests.
 * The caller serializes access and owns the actual detour handles/atomic trampoline slots.
 */
class NativeActivationBatchState final {
public:
    /** Starts one attach attempt. A validation failure must occur before this transition. */
    [[nodiscard]] bool begin_install() noexcept {
        if (phase_ != NativeActivationBatchPhase::detached) {
            return false;
        }
        phase_ = NativeActivationBatchPhase::installing;
        return true;
    }

    /**
     * Records all five published originals atomically at the model boundary.
     * A partial/null original set is rejected without publishing any of it.
     */
    [[nodiscard]] bool complete_install(
        std::span<const std::uintptr_t, kNativeActivationHookCount> originals) noexcept {
        if (phase_ != NativeActivationBatchPhase::installing
            || std::any_of(originals.begin(), originals.end(), [](std::uintptr_t original) {
                   return original == 0U;
               })) {
            return false;
        }
        std::copy(originals.begin(), originals.end(), originals_.begin());
        phase_ = NativeActivationBatchPhase::running;
        return true;
    }

    /** Rolls an unsuccessful all-or-none attach attempt back to a retryable detached state. */
    [[nodiscard]] bool rollback_install() noexcept {
        if (phase_ != NativeActivationBatchPhase::installing) {
            return false;
        }
        originals_ = {};
        phase_ = NativeActivationBatchPhase::detached;
        return true;
    }

    /**
     * Retains an unexpectedly attached batch in teardown-only state. Runtime trampolines must be
     * published before this is called so every replacement can continue native forwarding.
     */
    [[nodiscard]] bool retain_install_failure(
        std::span<const std::uintptr_t, kNativeActivationHookCount> originals) noexcept {
        if (phase_ != NativeActivationBatchPhase::installing) {
            return false;
        }
        std::copy(originals.begin(), originals.end(), originals_.begin());
        phase_ = NativeActivationBatchPhase::quiescing;
        return true;
    }

    /** Invalidates publication before consumers begin uninstalling. */
    [[nodiscard]] bool quiesce() noexcept {
        if (phase_ == NativeActivationBatchPhase::quiescing) {
            return true;
        }
        if (phase_ != NativeActivationBatchPhase::running) {
            return false;
        }
        phase_ = NativeActivationBatchPhase::quiescing;
        return true;
    }

    /**
     * Applies the protected detach result. Deferred/failed attempts retain every original and the
     * quiescing phase; only a committed removal clears ownership.
     */
    [[nodiscard]] bool record_detach(NativeActivationDetachDisposition disposition) noexcept {
        if (phase_ != NativeActivationBatchPhase::quiescing) {
            return false;
        }
        if (disposition == NativeActivationDetachDisposition::removed) {
            originals_ = {};
            phase_ = NativeActivationBatchPhase::detached;
        }
        return true;
    }

    [[nodiscard]] NativeActivationBatchSnapshot snapshot() const noexcept {
        return NativeActivationBatchSnapshot{originals_, phase_};
    }

private:
    std::array<std::uintptr_t, kNativeActivationHookCount> originals_{};
    NativeActivationBatchPhase phase_{NativeActivationBatchPhase::detached};
};

} // namespace dawn::client::hooks::activity_lifecycle
