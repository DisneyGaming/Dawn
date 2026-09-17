#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include "client/hooks/bootflow/opening_authority/ikora_ground_dwell_capture.h"

namespace dawn::client::hooks::bootflow::opening_authority::ikora_ground_dwell {

/** Exact caller-owned TLS arm joining one Type-31 incident to one nested listener enumeration. */
struct CorrelationArmToken final {
    std::uint64_t nonce{};
    std::uint64_t activity_activation_generation{};
    std::uint64_t point_authority_generation{};
    std::uint64_t incident_root_generation{};
    std::uint64_t correlation_token{};
    friend constexpr bool operator==(const CorrelationArmToken&,
                                     const CorrelationArmToken&) noexcept = default;
};

class CorrelationArmState final {
public:
    [[nodiscard]] bool arm(CorrelationArmToken token) noexcept {
        if (armed_ || token.nonce == 0U || token.activity_activation_generation == 0U
            || token.point_authority_generation == 0U || token.incident_root_generation == 0U
            || token.correlation_token == 0U) {
            return false;
        }
        token_ = token;
        armed_ = true;
        return true;
    }

    [[nodiscard]] bool matches(const CorrelationArmToken& token) const noexcept {
        return armed_ && token_ == token;
    }

    [[nodiscard]] bool disarm(std::uint64_t nonce) noexcept {
        if (!armed_ || nonce == 0U || token_.nonce != nonce) {
            return false;
        }
        invalidate();
        return true;
    }

    void invalidate() noexcept {
        token_ = {};
        armed_ = false;
    }

    [[nodiscard]] bool armed() const noexcept {
        return armed_;
    }
    [[nodiscard]] CorrelationArmToken token() const noexcept {
        return token_;
    }

private:
    CorrelationArmToken token_{};
    bool armed_{};
};

/**
 * Non-waiting full-call gate. Admission is latched after registering the call, so quiesce rejects
 * newly entering observation work while a call admitted earlier still performs its post-copy.
 */
class FullCallGate final {
public:
    class Scope final {
    public:
        explicit Scope(FullCallGate& gate) noexcept : gate_(gate) {
            gate_.active_calls_.fetch_add(1U, std::memory_order_acq_rel);
            admitted_ = gate_.accepting_.load(std::memory_order_acquire);
        }

        ~Scope() noexcept {
            gate_.active_calls_.fetch_sub(1U, std::memory_order_release);
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        [[nodiscard]] bool accepts_observation() const noexcept {
            return admitted_;
        }

    private:
        FullCallGate& gate_;
        bool admitted_{};
    };

    void accept() noexcept {
        accepting_.store(true, std::memory_order_release);
    }
    void quiesce() noexcept {
        accepting_.store(false, std::memory_order_release);
    }
    [[nodiscard]] bool accepting() const noexcept {
        return accepting_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool idle() const noexcept {
        return active_calls_.load(std::memory_order_acquire) == 0U;
    }
    [[nodiscard]] std::uint32_t active_calls() const noexcept {
        return active_calls_.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool accepting_{};
    std::atomic<std::uint32_t> active_calls_{};
};

/** Calls an untouched void native original exactly once on admitted and quiesced paths. */
template <typename Function, typename Before, typename After, typename... Arguments>
void forward_void_original_once(FullCallGate& gate,
                                Function original,
                                Before&& before,
                                After&& after,
                                Arguments&&... arguments) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Before>);
    static_assert(std::is_nothrow_invocable_v<After>);
    static_assert(std::is_nothrow_invocable_v<Function, Arguments...>);
    FullCallGate::Scope call{gate};
    if (call.accepts_observation()) {
        std::invoke(std::forward<Before>(before));
    }
    std::invoke(original, std::forward<Arguments>(arguments)...);
    if (call.accepts_observation()) {
        std::invoke(std::forward<After>(after));
    }
}

/** Value-returning counterpart; the native result is returned unchanged. */
template <typename Function, typename Before, typename After, typename... Arguments>
[[nodiscard]] std::invoke_result_t<Function, Arguments...>
forward_value_original_once(FullCallGate& gate,
                            Function original,
                            Before&& before,
                            After&& after,
                            Arguments&&... arguments) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(!std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Before>);
    static_assert(std::is_nothrow_invocable_v<After>);
    static_assert(std::is_nothrow_invocable_v<Function, Arguments...>);
    FullCallGate::Scope call{gate};
    if (call.accepts_observation()) {
        std::invoke(std::forward<Before>(before));
    }
    auto result = std::invoke(original, std::forward<Arguments>(arguments)...);
    if (call.accepts_observation()) {
        std::invoke(std::forward<After>(after));
    }
    return result;
}

enum class HookGroupPhase : std::uint8_t {
    detached,
    installing,
    running,
    quiescing,
    removed_pending_reset,
};

enum class ProtectedDetachDisposition : std::uint8_t {
    removed,
    deferred,
    failed,
};

enum class ProtectedDetachResult : std::uint8_t {
    removed,
    protected_calls_active,
    adapter_deferred,
    adapter_failed,
    wrong_phase,
};

struct HookGroupSnapshot final {
    std::array<std::uintptr_t, kNativeSurfaceCount> originals{};
    HookGroupPhase phase{HookGroupPhase::detached};
    std::uint64_t capture_generation{};
    bool runtime_admitted{};
    bool capture_generation_valid{};
};

/**
 * Pure all-or-none ownership state for a future integration owner. Failed/deferred protected
 * removal retains all originals and module state; reset is legal only after confirmed removal.
 */
class HookGroupState final {
public:
    [[nodiscard]] bool begin_install() noexcept {
        if (phase_ != HookGroupPhase::detached) {
            return false;
        }
        phase_ = HookGroupPhase::installing;
        return true;
    }

    [[nodiscard]] bool
    complete_install(bool runtimeAdmitted,
                     std::uint64_t captureGeneration,
                     const std::array<std::uintptr_t, kNativeSurfaceCount>& originals) noexcept {
        if (phase_ != HookGroupPhase::installing || !runtimeAdmitted || captureGeneration == 0U
            || !complete_unique_originals(originals)) {
            return false;
        }
        originals_ = originals;
        capture_generation_ = captureGeneration;
        runtime_admitted_ = true;
        capture_generation_valid_ = true;
        phase_ = HookGroupPhase::running;
        return true;
    }

    [[nodiscard]] bool rollback_install() noexcept {
        if (phase_ != HookGroupPhase::installing) {
            return false;
        }
        reset();
        return true;
    }

    [[nodiscard]] bool quiesce() noexcept {
        if (phase_ == HookGroupPhase::quiescing) {
            return true;
        }
        if (phase_ != HookGroupPhase::running) {
            return false;
        }
        capture_generation_valid_ = false;
        phase_ = HookGroupPhase::quiescing;
        return true;
    }

    [[nodiscard]] ProtectedDetachResult
    record_protected_detach(bool allProtectedCallsIdle,
                            ProtectedDetachDisposition disposition) noexcept {
        if (phase_ != HookGroupPhase::quiescing) {
            return ProtectedDetachResult::wrong_phase;
        }
        if (!allProtectedCallsIdle) {
            return ProtectedDetachResult::protected_calls_active;
        }
        if (disposition == ProtectedDetachDisposition::deferred) {
            return ProtectedDetachResult::adapter_deferred;
        }
        if (disposition == ProtectedDetachDisposition::failed) {
            return ProtectedDetachResult::adapter_failed;
        }
        phase_ = HookGroupPhase::removed_pending_reset;
        return ProtectedDetachResult::removed;
    }

    /** Caller proves its fixed queue/arm state was reset only after confirmed removal. */
    [[nodiscard]] bool finalize_reset(bool moduleStateReset) noexcept {
        if (phase_ != HookGroupPhase::removed_pending_reset || !moduleStateReset) {
            return false;
        }
        reset();
        return true;
    }

    [[nodiscard]] HookGroupSnapshot snapshot() const noexcept {
        return {
            originals_, phase_, capture_generation_, runtime_admitted_, capture_generation_valid_};
    }

private:
    [[nodiscard]] static bool complete_unique_originals(
        const std::array<std::uintptr_t, kNativeSurfaceCount>& originals) noexcept {
        for (std::size_t index = 0U; index < originals.size(); ++index) {
            if (originals[index] == 0U) {
                return false;
            }
            for (std::size_t prior = 0U; prior < index; ++prior) {
                if (originals[prior] == originals[index]) {
                    return false;
                }
            }
        }
        return true;
    }

    void reset() noexcept {
        originals_ = {};
        phase_ = HookGroupPhase::detached;
        capture_generation_ = 0U;
        runtime_admitted_ = false;
        capture_generation_valid_ = false;
    }

    std::array<std::uintptr_t, kNativeSurfaceCount> originals_{};
    HookGroupPhase phase_{HookGroupPhase::detached};
    std::uint64_t capture_generation_{};
    bool runtime_admitted_{};
    bool capture_generation_valid_{};
};

} // namespace dawn::client::hooks::bootflow::opening_authority::ikora_ground_dwell
