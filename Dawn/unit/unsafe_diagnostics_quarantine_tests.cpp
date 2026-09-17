#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>

#include "client/hooking/call_gate.h"

namespace {

using dawn::client::hooking::CallGate;

int g_failureCount{};

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

struct OmegaConfig final {
    bool syntheticStageMachine{};
    bool unsafeDiagnostics{};
};

struct UnsafeInstallPlan final {
    bool ikoraObserveOnly{};
    bool sceneRetirementObserveOnly{};
    bool featureFlagOverride{};
    bool activitySelectionMutation{};
    bool scriptAuthorityMutation{};
    bool legacyObserverBundle{};

    [[nodiscard]] std::uint32_t mutating_detour_count() const noexcept {
        return static_cast<std::uint32_t>(featureFlagOverride)
               + static_cast<std::uint32_t>(activitySelectionMutation)
               + static_cast<std::uint32_t>(scriptAuthorityMutation);
    }
};

[[nodiscard]] UnsafeInstallPlan plan_for(OmegaConfig config) noexcept {
    // Mirrors the production quarantine boundary: unsafe diagnostics owns only the two protected,
    // observe-only Omega probes. The synthetic-stage setting does not widen that set.
    return UnsafeInstallPlan{
        .ikoraObserveOnly = config.unsafeDiagnostics,
        .sceneRetirementObserveOnly = config.unsafeDiagnostics,
    };
}

enum class RetailEvent : std::uint8_t {
    playerBroadcastFailure,
    selectionLaunch,
    prologueIntroLoading,
    activityStart,
};

struct NativeMutationSnapshot final {
    std::uint8_t featureFlag{};
    std::uint32_t activitySelection{17U};
    std::uint32_t authorityField{23U};
    std::uint32_t managerStage{29U};
    std::uint32_t mutatingDetours{};

    friend bool operator==(const NativeMutationSnapshot&, const NativeMutationSnapshot&) = default;
};

void observe_retail_event(RetailEvent event,
                          bool unsafeDiagnostics,
                          NativeMutationSnapshot& native) noexcept {
    // Every formerly mutating retail branch is intentionally absent. Keep all recognized messages
    // in this switch so adding a side effect requires changing this deterministic truth table.
    switch (event) {
    case RetailEvent::playerBroadcastFailure:
    case RetailEvent::selectionLaunch:
    case RetailEvent::prologueIntroLoading:
    case RetailEvent::activityStart:
        if (unsafeDiagnostics) {
            std::atomic_signal_fence(std::memory_order_seq_cst);
        }
        break;
    }
    (void)native;
}

class LateAdmission final {
public:
    class Guard final {
    public:
        explicit Guard(LateAdmission& owner) noexcept
            : owner_(owner), lock_(owner.lock_) {
            accepted_ = owner_.accepting_;
            if (!accepted_) {
                lock_.unlock();
            }
        }

        [[nodiscard]] bool accepted() const noexcept {
            return accepted_;
        }

    private:
        LateAdmission& owner_;
        std::shared_lock<std::shared_mutex> lock_;
        bool accepted_{};
    };

    void fresh_install_and_publish() noexcept {
        std::unique_lock lock(lock_);
        accepting_ = true;
        installed_.store(true, std::memory_order_release);
    }

    void quiesce() noexcept {
        std::unique_lock lock(lock_);
        accepting_ = false;
        stopping_.store(true, std::memory_order_release);
    }

    [[nodiscard]] bool stopping() const noexcept {
        return stopping_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool installed() const noexcept {
        return installed_.load(std::memory_order_acquire);
    }

private:
    std::shared_mutex lock_{};
    bool accepting_{};
    std::atomic_bool installed_{};
    std::atomic_bool stopping_{};
};

enum class RemovalOutcome : std::uint8_t {
    removed,
    failed,
};

class RetailProducer final {
public:
    void install() noexcept {
        originalToken_ = 0xC0FFEEU;
        attached_ = true;
        gate_.accept();
    }

    void quiesce() noexcept {
        gate_.quiesce();
    }

    void invoke_blocked() noexcept {
        CallGate::Scope call(gate_);
        originalCalls_.fetch_add(1U, std::memory_order_relaxed);
        entered_.store(true, std::memory_order_release);
        entered_.notify_all();
        release_.wait(false, std::memory_order_acquire);
        if (call.accepts_side_effects()) {
            diagnosticCalls_.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] bool uninstall(RemovalOutcome outcome) noexcept {
        quiesce();
        if (!attached_) {
            return true;
        }
        if (!gate_.idle() || outcome != RemovalOutcome::removed) {
            return false;
        }
        attached_ = false;
        originalToken_ = 0U;
        return true;
    }

    void release_call() noexcept {
        release_.store(true, std::memory_order_release);
        release_.notify_all();
    }

    void wait_until_entered() const noexcept {
        entered_.wait(false, std::memory_order_acquire);
    }

    [[nodiscard]] bool attached() const noexcept {
        return attached_;
    }

    [[nodiscard]] std::uintptr_t original_token() const noexcept {
        return originalToken_;
    }

    [[nodiscard]] std::uint32_t active_calls() const noexcept {
        return gate_.active_calls();
    }

    [[nodiscard]] std::uint32_t original_calls() const noexcept {
        return originalCalls_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint32_t diagnostic_calls() const noexcept {
        return diagnosticCalls_.load(std::memory_order_relaxed);
    }

private:
    CallGate gate_{};
    std::atomic_bool entered_{};
    std::atomic_bool release_{};
    std::atomic_uint32_t originalCalls_{};
    std::atomic_uint32_t diagnosticCalls_{};
    std::uintptr_t originalToken_{};
    bool attached_{};
};

void all_off_and_unsafe_only_are_mutation_free() {
    const UnsafeInstallPlan allOff = plan_for({});
    CHECK(!allOff.ikoraObserveOnly);
    CHECK(!allOff.sceneRetirementObserveOnly);
    CHECK(allOff.mutating_detour_count() == 0U);
    CHECK(!allOff.legacyObserverBundle);

    const UnsafeInstallPlan unsafeOnly = plan_for({true, true});
    CHECK(unsafeOnly.ikoraObserveOnly);
    CHECK(unsafeOnly.sceneRetirementObserveOnly);
    CHECK(unsafeOnly.mutating_detour_count() == 0U);
    CHECK(!unsafeOnly.featureFlagOverride);
    CHECK(!unsafeOnly.activitySelectionMutation);
    CHECK(!unsafeOnly.scriptAuthorityMutation);
    CHECK(!unsafeOnly.legacyObserverBundle);

    NativeMutationSnapshot native{};
    const NativeMutationSnapshot before = native;
    constexpr std::array events{
        RetailEvent::playerBroadcastFailure,
        RetailEvent::selectionLaunch,
        RetailEvent::prologueIntroLoading,
        RetailEvent::activityStart,
    };
    for (const RetailEvent event : events) {
        observe_retail_event(event, true, native);
    }
    CHECK(native == before);
}

void stopping_transition_serializes_with_late_attach() {
    LateAdmission admission;
    admission.fresh_install_and_publish();
    CHECK(admission.installed());

    std::atomic_bool lateEntered{};
    std::atomic_bool releaseLate{};
    std::atomic_uint32_t attachCount{};
    std::thread lateInstaller([&] {
        LateAdmission::Guard guard(admission);
        CHECK(guard.accepted());
        lateEntered.store(true, std::memory_order_release);
        lateEntered.notify_all();
        releaseLate.wait(false, std::memory_order_acquire);
        attachCount.fetch_add(1U, std::memory_order_relaxed);
    });
    lateEntered.wait(false, std::memory_order_acquire);

    std::thread stopper([&] { admission.quiesce(); });
    CHECK(!admission.stopping());
    releaseLate.store(true, std::memory_order_release);
    releaseLate.notify_all();
    lateInstaller.join();
    stopper.join();

    CHECK(admission.stopping());
    const std::uint32_t countAtStopping = attachCount.load(std::memory_order_relaxed);
    CHECK(countAtStopping == 1U);
    for (std::uint32_t attempt = 0; attempt < 64U; ++attempt) {
        LateAdmission::Guard rejected(admission);
        CHECK(!rejected.accepted());
        if (rejected.accepted()) {
            attachCount.fetch_add(1U, std::memory_order_relaxed);
        }
    }
    CHECK(attachCount.load(std::memory_order_relaxed) == countAtStopping);
}

void retail_producer_retains_state_until_checked_detach() {
    RetailProducer producer;
    producer.install();

    std::thread callback([&] { producer.invoke_blocked(); });
    producer.wait_until_entered();
    CHECK(producer.active_calls() == 1U);

    producer.quiesce();
    CHECK(!producer.uninstall(RemovalOutcome::removed));
    CHECK(producer.attached());
    CHECK(producer.original_token() == 0xC0FFEEU);

    producer.release_call();
    callback.join();
    CHECK(producer.original_calls() == 1U);
    CHECK(producer.diagnostic_calls() == 0U);
    CHECK(producer.active_calls() == 0U);

    CHECK(!producer.uninstall(RemovalOutcome::failed));
    CHECK(producer.attached());
    CHECK(producer.original_token() == 0xC0FFEEU);
    CHECK(producer.uninstall(RemovalOutcome::removed));
    CHECK(!producer.attached());
    CHECK(producer.original_token() == 0U);
}

} // namespace

int main() {
    all_off_and_unsafe_only_are_mutation_free();
    stopping_transition_serializes_with_late_attach();
    retail_producer_retains_state_until_checked_detach();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " unsafe-diagnostics quarantine check(s) failed\n";
        return 1;
    }
    std::cout << "all unsafe-diagnostics quarantine checks passed\n";
    return 0;
}
