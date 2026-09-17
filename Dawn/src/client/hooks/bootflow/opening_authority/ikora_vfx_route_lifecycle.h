#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include "client/hooks/bootflow/opening_authority/ikora_vfx_route_capture.h"

namespace dawn::client::hooks::bootflow::opening_authority::ikora_vfx_route {

inline constexpr std::size_t kRouteArmDepth = 4U;

struct RouteArmToken final {
    OwnerScopeToken scope{};
    std::uint64_t call_id{};
    std::uint64_t nonce{};
    std::uintptr_t runtime_identity{};
    std::uintptr_t output_identity{};
    std::uint32_t actor_full_handle{};
    std::uint64_t provider_generation{};
    friend constexpr bool operator==(const RouteArmToken&, const RouteArmToken&) noexcept = default;
};

class RouteArmStack final {
public:
    [[nodiscard]] bool push(RouteArmToken token) noexcept {
        if (!token.scope.valid() || token.call_id == 0U || token.nonce == 0U
            || token.runtime_identity == 0U || token.output_identity == 0U
            || token.actor_full_handle == 0U || token.provider_generation == 0U
            || depth_ == entries_.size()) {
            return false;
        }
        entries_[depth_++] = token;
        return true;
    }

    [[nodiscard]] bool pop(RouteArmToken token) noexcept {
        if (depth_ == 0U || entries_[depth_ - 1U] != token) {
            invalidate();
            return false;
        }
        entries_[--depth_] = {};
        return true;
    }

    [[nodiscard]] const RouteArmToken* current() const noexcept {
        return depth_ == 0U ? nullptr : &entries_[depth_ - 1U];
    }
    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    void invalidate() noexcept {
        entries_ = {};
        depth_ = 0U;
    }

private:
    std::array<RouteArmToken, kRouteArmDepth> entries_{};
    std::size_t depth_{};
};

// The actual TLS owner is defined here; separate threads cannot observe one another's arms.
inline thread_local RouteArmStack g_route_arm_stack{};

class ScopedRouteArm final {
public:
    explicit ScopedRouteArm(RouteArmToken token) noexcept
        : token_(token), armed_(g_route_arm_stack.push(token)) {}
    ~ScopedRouteArm() noexcept {
        if (armed_) {
            (void)g_route_arm_stack.pop(token_);
        }
    }
    ScopedRouteArm(const ScopedRouteArm&) = delete;
    ScopedRouteArm& operator=(const ScopedRouteArm&) = delete;
    [[nodiscard]] bool armed() const noexcept { return armed_; }

private:
    RouteArmToken token_{};
    bool armed_{};
};

/**
 * Nonblocking full-call gate. An admitted scope retains observation ownership through return;
 * quiesce rejects only new observation work. Every wrapper below calls the untouched original
 * exactly once regardless of admission.
 */
class FullCallGate final {
public:
    class Scope final {
    public:
        explicit Scope(FullCallGate& gate) noexcept : gate_(gate) {
            gate_.active_calls_.fetch_add(1U, std::memory_order_acq_rel);
            admitted_ = gate_.accepting_.load(std::memory_order_acquire);
        }
        ~Scope() noexcept { gate_.active_calls_.fetch_sub(1U, std::memory_order_release); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        [[nodiscard]] bool observation_admitted() const noexcept { return admitted_; }

    private:
        FullCallGate& gate_;
        bool admitted_{};
    };

    void accept() noexcept { accepting_.store(true, std::memory_order_release); }
    void quiesce() noexcept { accepting_.store(false, std::memory_order_release); }
    [[nodiscard]] bool accepting() const noexcept { return accepting_.load(std::memory_order_acquire); }
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

struct OriginalCallOutcome final {
    OriginalOnceReceipt receipt{};
    bool observation_admitted{};
};

template <typename Result>
struct OriginalValueOutcome final {
    Result value{};
    OriginalOnceReceipt receipt{};
    bool observation_admitted{};
};

// Capture callbacks are intentionally absent: value-owned snapshots are collected by typed
// surface code, while this helper owns only exact original dispatch and its receipt.
template <typename Function, typename... Arguments>
[[nodiscard]] OriginalCallOutcome invoke_void_original_once(
    FullCallGate& gate,
    std::uint64_t call_id,
    Function original,
    Arguments&&... arguments) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Function, Arguments...>);
    FullCallGate::Scope scope{gate};
    std::invoke(original, std::forward<Arguments>(arguments)...);
    return {{call_id, 1U}, scope.observation_admitted()};
}

template <typename Function, typename... Arguments>
[[nodiscard]] auto invoke_value_original_once(
    FullCallGate& gate,
    std::uint64_t call_id,
    Function original,
    Arguments&&... arguments) noexcept
    -> OriginalValueOutcome<std::invoke_result_t<Function, Arguments...>> {
    using Result = std::invoke_result_t<Function, Arguments...>;
    static_assert(std::is_pointer_v<Function>);
    static_assert(!std::is_void_v<Result> && !std::is_reference_v<Result>);
    static_assert(std::is_nothrow_invocable_v<Function, Arguments...>);
    FullCallGate::Scope scope{gate};
    auto result = std::invoke(original, std::forward<Arguments>(arguments)...);
    return {std::move(result), {call_id, 1U}, scope.observation_admitted()};
}

enum class SourceOnlyCohortPhase : std::uint8_t {
    inert,
    admission_validated,
    quiesced,
};

/**
 * Honest bookkeeping for this unintegrated prototype. It can remember an evidence-derived
 * admission token and quiesce test gates, but it can never become attached or integration-ready.
 */
class SourceOnlyCohortState final {
public:
    [[nodiscard]] bool remember_admission(RuntimeCohortToken cohort,
                                          OwnerScopeToken scope) noexcept {
        if (phase_ != SourceOnlyCohortPhase::inert || !cohort.valid() || !scope.valid()
            || cohort.cohort_id() != scope.cohort_id()) {
            return false;
        }
        cohort_ = cohort;
        scope_ = scope;
        phase_ = SourceOnlyCohortPhase::admission_validated;
        return true;
    }
    [[nodiscard]] bool quiesce() noexcept {
        if (phase_ != SourceOnlyCohortPhase::admission_validated) { return false; }
        phase_ = SourceOnlyCohortPhase::quiesced;
        return true;
    }
    [[nodiscard]] SourceOnlyCohortPhase phase() const noexcept { return phase_; }
    [[nodiscard]] bool can_attach() const noexcept { return false; }
    [[nodiscard]] bool integration_ready() const noexcept { return false; }
    [[nodiscard]] RuntimeCohortToken cohort() const noexcept { return cohort_; }
    [[nodiscard]] OwnerScopeToken scope() const noexcept { return scope_; }

private:
    RuntimeCohortToken cohort_{};
    OwnerScopeToken scope_{};
    SourceOnlyCohortPhase phase_{SourceOnlyCohortPhase::inert};
};

static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

} // namespace dawn::client::hooks::bootflow::opening_authority::ikora_vfx_route
