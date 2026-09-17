#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>

#include "client/hooking/detour.h"
#include "client/hooks/bootflow/prologue_filler_ready_test_support.h"

namespace production_harness {

using dawn::client::hooking::detour::UninstallResult;
namespace support = dawn::client::hooks::bootflow::prologue_filler_ready_test_support;

std::shared_mutex g_lateInstallLock{};
bool g_acceptLateInstalls{};

std::atomic_bool g_forcedActive{};
std::string g_forcedPackage{};
std::atomic_uint32_t g_logCalls{};
std::atomic_uint32_t g_observeCalls{};
std::atomic_uint32_t g_bootstrapResetCalls{};

std::atomic_uint32_t g_readyNativeCalls{};
std::atomic_uint32_t g_stateNativeCalls{};
std::atomic_uint32_t g_activityNativeCalls{};
std::atomic_bool g_blockReady{};
std::atomic_bool g_readyEntered{};
std::atomic_bool g_releaseReady{};
std::atomic_bool g_publicationOrderingViolated{};

void observe_publication_order() noexcept {
    if (support::accepting() && support::published_original_count() != 3U) {
        g_publicationOrderingViolated.store(true, std::memory_order_relaxed);
    }
}

bool __fastcall native_ready() noexcept {
    observe_publication_order();
    g_readyNativeCalls.fetch_add(1U, std::memory_order_relaxed);
    if (g_blockReady.load(std::memory_order_acquire)) {
        g_readyEntered.store(true, std::memory_order_release);
        g_readyEntered.notify_all();
        g_releaseReady.wait(false, std::memory_order_acquire);
    }
    return false;
}

std::int32_t __fastcall native_state_update(std::byte*) noexcept {
    observe_publication_order();
    g_stateNativeCalls.fetch_add(1U, std::memory_order_relaxed);
    return -7;
}

std::int32_t __fastcall native_activity_ready(std::byte*) noexcept {
    observe_publication_order();
    g_activityNativeCalls.fetch_add(1U, std::memory_order_relaxed);
    return 1;
}

struct Backend final {
    std::atomic_int attachFailureIndex{-1};
    std::atomic<UninstallResult> nextRemoval{UninstallResult::removed};
    std::atomic_uint32_t installCalls{};
    std::atomic_uint32_t uninstallCalls{};
    std::atomic_uint32_t queuedAttachCount{};
    std::atomic_bool assemblyValid{};
    std::atomic_bool protectedBatchValid{};
    std::atomic_bool pauseAttach{};
    std::atomic_bool attachEntered{};
    std::atomic_bool releaseAttach{};
    std::array<void*, 3U> targets{};
    std::array<void*, 3U> replacements{};
};

Backend g_backend{};

void reset_backend() noexcept {
    g_backend.attachFailureIndex.store(-1, std::memory_order_relaxed);
    g_backend.nextRemoval.store(UninstallResult::removed, std::memory_order_relaxed);
    g_backend.installCalls.store(0U, std::memory_order_relaxed);
    g_backend.uninstallCalls.store(0U, std::memory_order_relaxed);
    g_backend.queuedAttachCount.store(0U, std::memory_order_relaxed);
    g_backend.assemblyValid.store(false, std::memory_order_relaxed);
    g_backend.protectedBatchValid.store(false, std::memory_order_relaxed);
    g_backend.pauseAttach.store(false, std::memory_order_relaxed);
    g_backend.attachEntered.store(false, std::memory_order_relaxed);
    g_backend.releaseAttach.store(false, std::memory_order_relaxed);
    g_backend.targets = {};
    g_backend.replacements = {};
}

void reset_native_state() noexcept {
    g_readyNativeCalls.store(0U, std::memory_order_relaxed);
    g_stateNativeCalls.store(0U, std::memory_order_relaxed);
    g_activityNativeCalls.store(0U, std::memory_order_relaxed);
    g_blockReady.store(false, std::memory_order_relaxed);
    g_readyEntered.store(false, std::memory_order_relaxed);
    g_releaseReady.store(false, std::memory_order_relaxed);
    g_publicationOrderingViolated.store(false, std::memory_order_relaxed);
    g_observeCalls.store(0U, std::memory_order_relaxed);
    g_bootstrapResetCalls.store(0U, std::memory_order_relaxed);
    g_forcedActive.store(false, std::memory_order_relaxed);
    g_forcedPackage.clear();
}

void open_late_admission() noexcept {
    const std::unique_lock lock(g_lateInstallLock);
    g_acceptLateInstalls = true;
}

void close_late_admission() noexcept {
    const std::unique_lock lock(g_lateInstallLock);
    g_acceptLateInstalls = false;
}

} // namespace production_harness

namespace dawn::client::hooks::bootflow::prologue_filler_ready_test_support {

std::byte* resolve_target(TargetSlot slot) noexcept {
    switch (slot) {
    case TargetSlot::ready:
        return reinterpret_cast<std::byte*>(&production_harness::native_ready);
    case TargetSlot::state_update:
        return reinterpret_cast<std::byte*>(&production_harness::native_state_update);
    case TargetSlot::activity_ready:
        return reinterpret_cast<std::byte*>(&production_harness::native_activity_ready);
    default:
        return nullptr;
    }
}

} // namespace dawn::client::hooks::bootflow::prologue_filler_ready_test_support

// Compile the real production owner into this executable. Only its native-image and Detours
// boundaries are supplied below; install/replacement/publication/quiesce/removal logic is
// unchanged.
#define DAWN_PROLOGUE_FILLER_READY_TESTING 1
#include "client/hooks/bootflow/prologue_filler_ready.cpp"

namespace dawn::client::hooking::detour {

bool install(std::span<const Spec> specs, std::span<Handle> outputs) noexcept {
    using namespace production_harness;
    g_backend.installCalls.fetch_add(1U, std::memory_order_relaxed);
    g_backend.queuedAttachCount.store(0U, std::memory_order_relaxed);

    const std::array expectedTargets{
        reinterpret_cast<void*>(&native_ready),
        reinterpret_cast<void*>(&native_state_update),
        reinterpret_cast<void*>(&native_activity_ready),
    };
    bool valid = specs.size() == expectedTargets.size() && outputs.size() == specs.size();
    if (valid) {
        for (std::size_t index = 0U; index < specs.size(); ++index) {
            g_backend.targets[index] = specs[index].target;
            g_backend.replacements[index] = specs[index].replacement;
            valid = valid && specs[index].target == expectedTargets[index]
                    && specs[index].replacement != nullptr && !outputs[index].attached;
        }
        valid = valid && g_backend.replacements[0] != g_backend.replacements[1]
                && g_backend.replacements[0] != g_backend.replacements[2]
                && g_backend.replacements[1] != g_backend.replacements[2];
    }
    g_backend.assemblyValid.store(valid, std::memory_order_release);
    g_backend.attachEntered.store(true, std::memory_order_release);
    g_backend.attachEntered.notify_all();
    if (g_backend.pauseAttach.load(std::memory_order_acquire)) {
        g_backend.releaseAttach.wait(false, std::memory_order_acquire);
    }
    if (!valid) {
        return false;
    }

    std::array<Handle, 3U> pending{};
    const int failureIndex = g_backend.attachFailureIndex.load(std::memory_order_acquire);
    for (std::size_t index = 0U; index < specs.size(); ++index) {
        pending[index] = Handle{specs[index].target, specs[index].replacement, true};
        g_backend.queuedAttachCount.fetch_add(1U, std::memory_order_relaxed);
        if (failureIndex == static_cast<int>(index)) {
            return false;
        }
    }
    std::copy(pending.begin(), pending.end(), outputs.begin());
    return true;
}

UninstallResult uninstall(std::span<Handle> handles,
                          std::span<const ProtectedCodeEntry> protectedEntries,
                          IdleCheck idleCheck) noexcept {
    using namespace production_harness;
    g_backend.uninstallCalls.fetch_add(1U, std::memory_order_relaxed);
    bool valid = handles.size() == 3U && protectedEntries.size() == 5U && idleCheck != nullptr;
    if (valid) {
        for (std::size_t index = 0U; index < handles.size(); ++index) {
            valid = valid && handles[index].attached
                    && protectedEntries[index].address == g_backend.replacements[index];
        }
        valid = valid && protectedEntries[3].address != nullptr
                && protectedEntries[4].address != nullptr
                && protectedEntries[3].address != protectedEntries[4].address;
    }
    g_backend.protectedBatchValid.store(valid, std::memory_order_release);
    if (!valid) {
        return UninstallResult::failed;
    }
    if (!idleCheck()) {
        return UninstallResult::protectedCodeActive;
    }

    const UninstallResult result = g_backend.nextRemoval.load(std::memory_order_acquire);
    if (result == UninstallResult::removed) {
        for (Handle& handle : handles) {
            handle = {};
        }
    }
    return result;
}

} // namespace dawn::client::hooking::detour

namespace dawn::client::hooks::bootflow {

LateInstallGuard::LateInstallGuard() noexcept {
    production_harness::g_lateInstallLock.lock_shared();
    if (production_harness::g_acceptLateInstalls) {
        ownsLock_ = true;
        accepted_ = true;
        return;
    }
    production_harness::g_lateInstallLock.unlock_shared();
}

LateInstallGuard::~LateInstallGuard() noexcept {
    if (ownsLock_) {
        production_harness::g_lateInstallLock.unlock_shared();
    }
}

bool LateInstallGuard::accepted() const noexcept {
    return accepted_;
}

void observe_activity_script_manager_table() noexcept {
    production_harness::g_observeCalls.fetch_add(1U, std::memory_order_relaxed);
}

void reset_activity_script_bootstrap() noexcept {
    production_harness::g_bootstrapResetCalls.fetch_add(1U, std::memory_order_relaxed);
}

} // namespace dawn::client::hooks::bootflow

namespace dawn::state::activity::forced {

void snapshot(ForcedDestination& value) noexcept {
    value = {};
    const std::size_t length =
        (std::min)(value.packageName.size(), production_harness::g_forcedPackage.size());
    std::copy_n(production_harness::g_forcedPackage.data(), length, value.packageName.data());
    value.packageNameLength = static_cast<std::uint8_t>(length);
}

bool override_active() noexcept {
    return production_harness::g_forcedActive.load(std::memory_order_acquire);
}

} // namespace dawn::state::activity::forced

namespace dawn::core::log {

void write(Channel, Level, std::string_view) noexcept {
    production_harness::g_logCalls.fetch_add(1U, std::memory_order_relaxed);
}

} // namespace dawn::core::log

namespace {

namespace bootflow = dawn::client::hooks::bootflow;
namespace support = bootflow::prologue_filler_ready_test_support;
using dawn::client::hooking::detour::UninstallResult;

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename Predicate> [[nodiscard]] bool wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    return predicate();
}

void set_forced_package(std::string_view package, bool active) {
    production_harness::g_forcedPackage.assign(package);
    production_harness::g_forcedActive.store(active, std::memory_order_release);
}

void late_admission_rejects_the_real_owner_entrypoint() {
    production_harness::reset_backend();
    production_harness::reset_native_state();
    production_harness::close_late_admission();

    CHECK(!bootflow::install_prologue_filler_ready());
    CHECK(production_harness::g_backend.installCalls.load(std::memory_order_relaxed) == 0U);
    CHECK(support::ownership_count() == 0U);
    CHECK(support::published_original_count() == 0U);
}

void real_three_spec_attach_rolls_back_then_publishes_atomically() {
    production_harness::open_late_admission();
    production_harness::g_backend.attachFailureIndex.store(1, std::memory_order_release);
    CHECK(!bootflow::install_prologue_filler_ready());
    CHECK(production_harness::g_backend.assemblyValid.load(std::memory_order_acquire));
    CHECK(production_harness::g_backend.queuedAttachCount.load(std::memory_order_relaxed) == 2U);
    CHECK(support::ownership_count() == 0U);
    CHECK(support::published_original_count() == 0U);
    CHECK(!support::accepting());

    production_harness::g_backend.attachFailureIndex.store(-1, std::memory_order_release);
    bool readyResult = true;
    std::int32_t stateResult{};
    std::int32_t activityResult{};
    std::byte state{};
    std::thread readyCaller([&readyResult] { readyResult = support::invoke_ready(); });
    std::thread stateCaller(
        [&stateResult, &state] { stateResult = support::invoke_state_update(&state); });
    std::thread activityCaller(
        [&activityResult, &state] { activityResult = support::invoke_activity_ready(&state); });
    CHECK(wait_until([] { return support::active_calls() == 3U; }));
    CHECK(production_harness::g_readyNativeCalls.load(std::memory_order_relaxed) == 0U);

    CHECK(bootflow::install_prologue_filler_ready());
    readyCaller.join();
    stateCaller.join();
    activityCaller.join();
    CHECK(!readyResult);
    CHECK(stateResult == -7);
    CHECK(activityResult == 1);
    CHECK(production_harness::g_readyNativeCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(production_harness::g_stateNativeCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(production_harness::g_activityNativeCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(!production_harness::g_publicationOrderingViolated.load(std::memory_order_relaxed));
    CHECK(support::ownership_count() == 3U);
    CHECK(support::published_original_count() == 3U);
    CHECK(support::accepting());
    CHECK(production_harness::g_backend.installCalls.load(std::memory_order_relaxed) == 2U);

    CHECK(bootflow::install_prologue_filler_ready());
    CHECK(production_harness::g_backend.installCalls.load(std::memory_order_relaxed) == 2U);
}

void real_replacements_forward_once_and_keep_the_red_war_scope_narrow() {
    std::byte state{};
    CHECK(support::invoke_state_update(&state) == -7);
    CHECK(support::invoke_activity_ready(&state) == 1);
    CHECK(production_harness::g_stateNativeCalls.load(std::memory_order_relaxed) == 2U);
    CHECK(production_harness::g_activityNativeCalls.load(std::memory_order_relaxed) == 2U);
    CHECK(production_harness::g_observeCalls.load(std::memory_order_relaxed) == 0U);

    set_forced_package("mission_towerfall", true);
    bootflow::reset_prologue_filler_ready();
    CHECK(!support::armed());
    CHECK(production_harness::g_bootstrapResetCalls.load(std::memory_order_relaxed) == 1U);
    bootflow::arm_prologue_filler_ready();
    CHECK(support::armed());

    CHECK(support::invoke_ready());
    CHECK(support::invoke_state_update(&state) == 3);
    CHECK(support::invoke_activity_ready(&state) == 3);
    CHECK(production_harness::g_readyNativeCalls.load(std::memory_order_relaxed) == 2U);
    CHECK(production_harness::g_stateNativeCalls.load(std::memory_order_relaxed) == 3U);
    CHECK(production_harness::g_activityNativeCalls.load(std::memory_order_relaxed) == 3U);
    CHECK(production_harness::g_observeCalls.load(std::memory_order_relaxed) == 2U);

    set_forced_package("mission_scot", true);
    CHECK(!support::invoke_ready());
    CHECK(support::invoke_state_update(&state) == -7);
    CHECK(support::invoke_activity_ready(&state) == 1);
    CHECK(production_harness::g_readyNativeCalls.load(std::memory_order_relaxed) == 3U);
    CHECK(production_harness::g_stateNativeCalls.load(std::memory_order_relaxed) == 4U);
    CHECK(production_harness::g_activityNativeCalls.load(std::memory_order_relaxed) == 4U);
    CHECK(production_harness::g_observeCalls.load(std::memory_order_relaxed) == 2U);
}

void real_quiesce_and_checked_detach_retain_until_removed() {
    set_forced_package("mission_towerfall", true);
    production_harness::g_blockReady.store(true, std::memory_order_release);
    bool result = true;
    std::thread caller([&result] { result = support::invoke_ready(); });
    CHECK(wait_until(
        [] { return production_harness::g_readyEntered.load(std::memory_order_acquire); }));

    bootflow::quiesce_prologue_filler_ready();
    CHECK(!support::accepting());
    CHECK(!bootflow::uninstall_prologue_filler_ready_checked());
    CHECK(production_harness::g_backend.protectedBatchValid.load(std::memory_order_acquire));
    CHECK(support::ownership_count() == 3U);
    CHECK(support::published_original_count() == 3U);

    production_harness::g_releaseReady.store(true, std::memory_order_release);
    production_harness::g_releaseReady.notify_all();
    caller.join();
    CHECK(!result);
    CHECK(production_harness::g_readyNativeCalls.load(std::memory_order_relaxed) == 4U);

    production_harness::g_backend.nextRemoval.store(UninstallResult::failed,
                                                    std::memory_order_release);
    CHECK(!bootflow::uninstall_prologue_filler_ready_checked());
    CHECK(support::ownership_count() == 3U);
    CHECK(support::published_original_count() == 3U);
    CHECK(!bootflow::install_prologue_filler_ready());

    production_harness::g_backend.nextRemoval.store(UninstallResult::removed,
                                                    std::memory_order_release);
    CHECK(bootflow::uninstall_prologue_filler_ready_checked());
    CHECK(support::ownership_count() == 0U);
    CHECK(support::published_original_count() == 0U);
}

[[nodiscard]] bool stop_like_bootflow() noexcept {
    production_harness::close_late_admission();
    bootflow::quiesce_prologue_filler_ready();
    return bootflow::uninstall_prologue_filler_ready_checked();
}

void concurrent_real_late_install_is_drained_before_stop() {
    production_harness::reset_backend();
    production_harness::open_late_admission();
    production_harness::g_backend.pauseAttach.store(true, std::memory_order_release);

    bool installed = false;
    std::thread installer([&installed] { installed = bootflow::install_prologue_filler_ready(); });
    CHECK(wait_until([] {
        return production_harness::g_backend.attachEntered.load(std::memory_order_acquire);
    }));

    std::atomic_bool stopStarted{};
    std::atomic_bool stopDone{};
    bool stopped = false;
    std::thread stopper([&] {
        stopStarted.store(true, std::memory_order_release);
        stopped = stop_like_bootflow();
        stopDone.store(true, std::memory_order_release);
    });
    CHECK(wait_until([&stopStarted] { return stopStarted.load(std::memory_order_acquire); }));
    CHECK(!stopDone.load(std::memory_order_acquire));

    production_harness::g_backend.releaseAttach.store(true, std::memory_order_release);
    production_harness::g_backend.releaseAttach.notify_all();
    installer.join();
    stopper.join();

    CHECK(installed);
    CHECK(stopped);
    CHECK(production_harness::g_backend.installCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(production_harness::g_backend.uninstallCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(support::ownership_count() == 0U);
    CHECK(support::published_original_count() == 0U);
    CHECK(!bootflow::install_prologue_filler_ready());
    CHECK(production_harness::g_backend.installCalls.load(std::memory_order_relaxed) == 1U);
}

[[nodiscard]] std::filesystem::path find_repository_root() {
    std::filesystem::path cursor = std::filesystem::current_path();
    for (std::uint32_t depth = 0U; depth < 8U; ++depth) {
        if (std::filesystem::exists(cursor
                                    / "Dawn/src/client/hooks/bootflow"
                                      "/bootflow_hook_lifecycle.cpp")) {
            return cursor;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return {};
}

[[nodiscard]] std::string read_source(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void source_call_graph_keeps_retail_install_and_checked_bootflow_propagation() {
    const std::filesystem::path root = find_repository_root();
    CHECK(!root.empty());
    if (root.empty()) {
        return;
    }

    const std::string retail =
        read_source(root / "Dawn/src/client/hooks/retail_log/retail_log_enqueue_observer.cpp");
    const std::string lifecycle =
        read_source(root / "Dawn/src/client/hooks/bootflow/bootflow_hook_lifecycle.cpp");
    CHECK(retail.find("bootflow::reset_prologue_filler_ready();") != std::string::npos);
    CHECK(retail.find("bootflow::install_prologue_filler_ready()") != std::string::npos);
    CHECK(retail.find("bootflow::arm_prologue_filler_ready();") != std::string::npos);
    CHECK(lifecycle.find("quiesce_prologue_filler_ready();") != std::string::npos);
    CHECK(lifecycle.find("if (!uninstall_prologue_filler_ready_checked())") != std::string::npos);
    CHECK(lifecycle.find("AcquireSRWLockShared(&g_lateInstallLock);") != std::string::npos);
    CHECK(lifecycle.find("g_acceptLateInstalls.load(std::memory_order_acquire)")
          != std::string::npos);
    CHECK(lifecycle.find("g_acceptLateInstalls.store(false, std::memory_order_release);")
          != std::string::npos);
}

} // namespace

int main() {
    late_admission_rejects_the_real_owner_entrypoint();
    real_three_spec_attach_rolls_back_then_publishes_atomically();
    real_replacements_forward_once_and_keep_the_red_war_scope_narrow();
    real_quiesce_and_checked_detach_retain_until_removed();
    concurrent_real_late_install_is_drained_before_stop();
    source_call_graph_keeps_retail_install_and_checked_bootflow_propagation();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " production prologue owner check(s) failed\n";
        return 1;
    }
    std::cout << "all production prologue owner checks passed\n";
    return 0;
}
