#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "client/hooking/call_gate.h"
#include "client/hooks/bootflow/spawn_hold_policy.h"

namespace {

namespace policy = sunrise::client::hooks::bootflow::spawn_hold_policy;
using sunrise::client::hooking::CallGate;

using NativeCall = bool (*)(std::int32_t) noexcept;

[[nodiscard]] constexpr policy::Input input(bool nativeAllowed,
                                            policy::Phase phase,
                                            bool holdEnabled,
                                            bool timedOut,
                                            bool alreadyReleased,
                                            bool loaderBusy) noexcept {
    return policy::Input{nativeAllowed, phase, holdEnabled, timedOut, alreadyReleased, loaderBusy};
}

constexpr policy::Decision kTransitioningHold =
    policy::decide(input(true, policy::Phase::transitioning, true, false, false, false));
static_assert(kTransitioningHold.loading);
static_assert(!kTransitioningHold.releaseFade);
static_assert(!kTransitioningHold.result);

constexpr policy::Decision kArrivedLoaderBusy =
    policy::decide(input(true, policy::Phase::arrived, true, false, false, true));
static_assert(kArrivedLoaderBusy.loaderLoading);
static_assert(kArrivedLoaderBusy.loading);
static_assert(!kArrivedLoaderBusy.releaseFade);
static_assert(!kArrivedLoaderBusy.result);

constexpr policy::Decision kArrivedReady =
    policy::decide(input(true, policy::Phase::arrived, true, false, false, false));
static_assert(!kArrivedReady.loading);
static_assert(kArrivedReady.releaseFade);
static_assert(kArrivedReady.result);

constexpr policy::Decision kArrivedTimedOut =
    policy::decide(input(true, policy::Phase::arrived, true, true, false, true));
static_assert(kArrivedTimedOut.loaderLoading);
static_assert(!kArrivedTimedOut.loading);
static_assert(kArrivedTimedOut.releaseFade);
static_assert(kArrivedTimedOut.result);

constexpr policy::Decision kTransitionTimedOut =
    policy::decide(input(true, policy::Phase::transitioning, true, true, false, false));
static_assert(!kTransitionTimedOut.loading);
static_assert(!kTransitionTimedOut.releaseFade);
static_assert(kTransitionTimedOut.result);

constexpr policy::Decision kAlreadyReleased =
    policy::decide(input(true, policy::Phase::arrived, true, false, true, true));
static_assert(!kAlreadyReleased.loaderLoading);
static_assert(!kAlreadyReleased.loading);
static_assert(!kAlreadyReleased.releaseFade);
static_assert(kAlreadyReleased.result);

[[nodiscard]] constexpr policy::TowerfallReadiness towerfall_ready_input() noexcept {
    return policy::TowerfallReadiness{
        true, policy::Phase::arrived, true, true, 3, true, true, true};
}

static_assert(policy::towerfall_ready(towerfall_ready_input()));
static_assert(!policy::towerfall_ready([] {
    auto value = towerfall_ready_input();
    value.initialSliceComplete = false;
    return value;
}()));
static_assert(!policy::towerfall_ready([] {
    auto value = towerfall_ready_input();
    value.scriptRuntime = false;
    return value;
}()));
static_assert(!policy::towerfall_ready([] {
    auto value = towerfall_ready_input();
    value.directorRuntime = false;
    return value;
}()));
static_assert(!policy::towerfall_ready([] {
    auto value = towerfall_ready_input();
    value.worldState = 2;
    return value;
}()));

/** Exhaustively checks the source-linked pure policy over every Boolean input combination. */
[[nodiscard]] constexpr bool policy_truth_table_holds() noexcept {
    constexpr std::array phases{
        policy::Phase::idle,
        policy::Phase::transitioning,
        policy::Phase::arrived,
    };
    constexpr std::array values{false, true};
    for (const policy::Phase phase : phases) {
        for (const bool nativeAllowed : values) {
            for (const bool holdEnabled : values) {
                for (const bool timedOut : values) {
                    for (const bool alreadyReleased : values) {
                        for (const bool loaderBusy : values) {
                            const policy::Decision decision = policy::decide(input(nativeAllowed,
                                                                                   phase,
                                                                                   holdEnabled,
                                                                                   timedOut,
                                                                                   alreadyReleased,
                                                                                   loaderBusy));
                            const bool expectedLoaderLoading =
                                phase == policy::Phase::arrived && !alreadyReleased && loaderBusy;
                            const bool expectedLoading =
                                holdEnabled && !timedOut && !alreadyReleased
                                && (phase == policy::Phase::transitioning || expectedLoaderLoading);
                            const bool expectedRelease = phase == policy::Phase::arrived
                                                         && !expectedLoading && !alreadyReleased;
                            if (decision.loaderLoading != expectedLoaderLoading
                                || decision.loading != expectedLoading
                                || decision.releaseFade != expectedRelease
                                || decision.result != (nativeAllowed && !expectedLoading)) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

static_assert(policy_truth_table_holds());

/**
 * Generic CallGate model only. This exercises publication and admission primitives; it does not
 * simulate production detour removal or claim coverage of spawn-owner state retention.
 */
class GenericCallGateForwarder final {
public:
    void accept() noexcept {
        gate_.accept();
    }

    void quiesce() noexcept {
        gate_.quiesce();
    }

    void publish(NativeCall original) noexcept {
        sunrise::client::hooking::publish_original(original_, original);
    }

    [[nodiscard]] bool invoke(std::int32_t datum) noexcept {
        CallGate::Scope call(gate_);
        const NativeCall original = sunrise::client::hooking::await_original(original_);
        const bool result = original(datum);
        if (call.accepts_side_effects()) {
            sideEffectCalls_.fetch_add(1U, std::memory_order_relaxed);
        }
        return result;
    }

    [[nodiscard]] bool idle() const noexcept {
        return gate_.idle();
    }

    [[nodiscard]] std::uint32_t active_calls() const noexcept {
        return gate_.active_calls();
    }

    [[nodiscard]] std::uint32_t side_effect_calls() const noexcept {
        return sideEffectCalls_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<NativeCall> original_{};
    std::atomic_uint32_t sideEffectCalls_{};
    CallGate gate_{};
};

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

std::atomic_uint32_t g_nativeCalls{};
std::atomic_bool g_nativeEntered{};
std::atomic_bool g_releaseNative{};

bool immediate_native(std::int32_t datum) noexcept {
    g_nativeCalls.fetch_add(1U, std::memory_order_relaxed);
    return datum == 41;
}

bool blocked_native(std::int32_t datum) noexcept {
    g_nativeCalls.fetch_add(1U, std::memory_order_relaxed);
    g_nativeEntered.store(true, std::memory_order_release);
    g_nativeEntered.notify_all();
    g_releaseNative.wait(false, std::memory_order_acquire);
    return datum == 41;
}

void reset_native_barrier() noexcept {
    g_nativeCalls.store(0U, std::memory_order_relaxed);
    g_nativeEntered.store(false, std::memory_order_relaxed);
    g_releaseNative.store(false, std::memory_order_relaxed);
}

template <typename Predicate> [[nodiscard]] bool wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    return predicate();
}

void generic_publication_window_waits_then_forwards_exactly_once() {
    reset_native_barrier();
    GenericCallGateForwarder forwarder{};
    forwarder.accept();

    bool result = false;
    std::thread caller([&forwarder, &result] { result = forwarder.invoke(41); });
    const bool entered = wait_until([&forwarder] { return forwarder.active_calls() == 1U; });
    CHECK(entered);
    CHECK(g_nativeCalls.load(std::memory_order_relaxed) == 0U);

    forwarder.publish(&immediate_native);
    caller.join();

    CHECK(result);
    CHECK(g_nativeCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(forwarder.side_effect_calls() == 1U);
    CHECK(forwarder.idle());
}

void generic_quiesced_call_forwards_without_side_effects() {
    reset_native_barrier();
    GenericCallGateForwarder forwarder{};
    forwarder.publish(&immediate_native);
    forwarder.accept();
    forwarder.quiesce();

    CHECK(forwarder.invoke(41));
    CHECK(g_nativeCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(forwarder.side_effect_calls() == 0U);
    CHECK(forwarder.idle());
}

void generic_active_call_stays_owned_through_native_interval() {
    reset_native_barrier();
    GenericCallGateForwarder forwarder{};
    forwarder.publish(&blocked_native);
    forwarder.accept();

    bool result = false;
    std::thread caller([&forwarder, &result] { result = forwarder.invoke(41); });
    const bool entered = wait_until([] { return g_nativeEntered.load(std::memory_order_acquire); });
    CHECK(entered);
    forwarder.quiesce();
    CHECK(!forwarder.idle());
    CHECK(forwarder.active_calls() == 1U);

    g_releaseNative.store(true, std::memory_order_release);
    g_releaseNative.notify_all();
    caller.join();

    CHECK(result);
    CHECK(g_nativeCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(forwarder.side_effect_calls() == 0U);
    CHECK(forwarder.idle());
}

} // namespace

int main() {
    generic_publication_window_waits_then_forwards_exactly_once();
    generic_quiesced_call_forwards_without_side_effects();
    generic_active_call_stays_owned_through_native_interval();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " call-gate/spawn-policy check(s) failed\n";
        return 1;
    }

    std::cout << "all call-gate and source-linked spawn-policy checks passed\n";
    return 0;
}
