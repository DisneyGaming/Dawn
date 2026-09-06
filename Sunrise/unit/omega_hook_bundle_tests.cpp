#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <vector>
#include "client/hooking/detour.h"
#include "client/hooking/call_gate.h"
#include "omega_hook_targets.generated.h"
#include "client/hooks/bootflow/omega_reveal_bindings.h"
#include "client/hooks/bootflow/omega_portal_probe.h"

// Isolated harness: no game process, no injected DLL, no native game code calls.
// The real transaction implementation still suspends/enlists this test process.
namespace sunrise::client::process::freeze {
SRWLOCK testLock = SRWLOCK_INIT;
void enter_exclusive() noexcept { AcquireSRWLockExclusive(&testLock); }
void leave_exclusive() noexcept { ReleaseSRWLockExclusive(&testLock); }
}
namespace sunrise::client::hooking::detour {
// Protected removal is outside this test; ordinary batch removal is production code.
UninstallResult uninstall(std::span<Handle>, std::span<const ProtectedCodeEntry>) noexcept {
    return UninstallResult::failed;
}
}
namespace {
void require(bool passed, const char* message) {
    if (!passed) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
__declspec(noinline) void replacement() noexcept { std::atomic_signal_fence(std::memory_order_seq_cst); }
}
int main(int argc, char** argv) {
    namespace hook = sunrise::client::hooking;
    namespace detour = hook::detour;
    require(argc == 2, "provide pinned mapped image path");
    std::ifstream stream(argv[1], std::ios::binary | std::ios::ate);
    require(stream.good(), "open image");
    const auto length = stream.tellg();
    require(length > 0, "image length");
    std::vector<char> source(static_cast<std::size_t>(length));
    stream.seekg(0);
    stream.read(source.data(), static_cast<std::streamsize>(source.size()));
    require(stream.good(), "read image");
    auto* image = static_cast<unsigned char*>(VirtualAlloc(nullptr, source.size() + 4096,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    require(image != nullptr, "allocate isolated code copy");
    std::memcpy(image, source.data(), source.size());
    constexpr auto targets = [] {
        std::array<std::uintptr_t, kOmegaHookRvas.size() + 22> values{};
        std::copy(kOmegaHookRvas.begin(), kOmegaHookRvas.end(), values.begin());
        values[kOmegaHookRvas.size()] = 0x4EDC20;
        values[kOmegaHookRvas.size() + 1] = 0x106AB20;
        values[kOmegaHookRvas.size() + 2] = 0xAB6600;
        values[kOmegaHookRvas.size() + 3] = 0xC70180;
        values[kOmegaHookRvas.size() + 4] = 0xF4E660;
        values[kOmegaHookRvas.size() + 5] = 0x104B7A0;
        values[kOmegaHookRvas.size() + 6] = 0xC72390;
        values[kOmegaHookRvas.size() + 7] = 0x10B0030;
        values[kOmegaHookRvas.size() + 8] = 0x10ADF70;
        values[kOmegaHookRvas.size() + 9] = 0xB438B0;
        values[kOmegaHookRvas.size() + 10] = 0xD99620;
        values[kOmegaHookRvas.size() + 11] = 0xF36640;
        values[kOmegaHookRvas.size() + 12] = 0xC71C30;
        values[kOmegaHookRvas.size() + 13] = 0x104DA00;
        values[kOmegaHookRvas.size() + 14] = 0x4B25F0;
        values[kOmegaHookRvas.size() + 15] = 0x58E260;
        values[kOmegaHookRvas.size() + 16] = 0x5873F0;
        values[kOmegaHookRvas.size() + 17] = 0x1212AF0;
        values[kOmegaHookRvas.size() + 18] = 0xB804E0;
        values[kOmegaHookRvas.size() + 19] = 0xCDCB60;
        values[kOmegaHookRvas.size() + 20] = 0xB7E3C0;
        values[kOmegaHookRvas.size() + 21] = 0xFFB850;
        return values;
    }();
    for (const auto& binding : sunrise::client::hooks::bootflow::omega_portal_probe::kBindings) {
        require(binding.rva + binding.prefix.size() <= source.size(), "portal binding in image");
        require(std::memcmp(image + binding.rva, binding.prefix.data(), binding.prefix.size()) == 0,
                "portal callback matches pinned image");
        require(std::find(targets.begin(), targets.end(), binding.rva) != targets.end(),
                "portal hook included in atomic attach regression");
    }
    for (const auto& binding : sunrise::client::hooks::bootflow::omega_reveal_native::kBindings) {
        require(binding.rva + binding.prefix.size() <= source.size(), "reveal binding in image");
        require(std::memcmp(image + binding.rva, binding.prefix.data(), binding.prefix.size()) == 0,
                "all reveal and binding-lookup native signatures match pinned image");
    }
    std::array<detour::Spec, targets.size()> specs{};
    std::array<detour::Handle, targets.size()> handles{};
    std::array<std::array<unsigned char, 64>, targets.size()> saved{};
    for (std::size_t i = 0; i < specs.size(); ++i) {
        require(targets[i] + 64 < source.size(), "target in image");
        specs[i] = {image + targets[i], reinterpret_cast<void*>(&replacement)};
        std::memcpy(saved[i].data(), specs[i].target, saved[i].size());
    }
    const auto untouched = [&] {
        for (std::size_t i = 0; i < specs.size(); ++i)
            require(std::memcmp(saved[i].data(), specs[i].target, saved[i].size()) == 0, "all targets restored");
        for (const auto& handle : handles) require(!handle.attached && !handle.original, "no partial handles");
    };
    detour::InstallFailure failure{};
    const auto supportHandles = std::span(handles).first(kOmegaHookRvas.size());
    const auto supportSpecs = std::span(specs).first(kOmegaHookRvas.size());
    require(!detour::install(specs, handles, failure) && failure.stage == detour::InstallStage::validation,
            "51 hooks must retain separate native transactions within the fixed 32-slot limit");
    untouched();
    auto invalid = specs;
    invalid[5].replacement = nullptr;
    require(!detour::install(std::span(invalid).first(kOmegaHookRvas.size()), supportHandles, failure)
            && failure.stage == detour::InstallStage::validation,
            "invalid batch rejected before mutation");
    untouched();

    // A single RET cannot hold a detour: reject the sixth attach and roll back the first five.
    auto* tiny = image + source.size();
    std::memset(tiny, 0x01, 64); // Non-padding bytes prevent Detours extending past RET.
    tiny[0] = 0xC3;
    invalid = specs;
    invalid[5].target = tiny;
    const bool invalidInstalled = detour::install(std::span(invalid).first(kOmegaHookRvas.size()),
                                                supportHandles, failure);
    std::printf("injected_failure installed=%u stage=%u slot=%zu error_known=%u error=%ld\n",
        invalidInstalled, static_cast<unsigned>(failure.stage), failure.index, failure.hasNativeError, failure.nativeError);
    require(!invalidInstalled && failure.stage == detour::InstallStage::attach
            && failure.index == 5 && failure.hasNativeError && failure.nativeError != 0,
            "failed attach identifies slot and native error");
    untouched();
    std::printf("PASS: rejected sixth attach, error=%ld; first five rolled back\n", failure.nativeError);
    // Every failure position must leave no partial animation/authority hook set.
    const auto revealSpecs = std::span(specs).subspan(kOmegaHookRvas.size(), 21);
    std::array<detour::Handle, 21> revealHandles{};
    for (std::size_t failedSlot = 0; failedSlot < revealSpecs.size(); ++failedSlot) {
        std::array<detour::Spec,21> failedReveal{};
        std::copy(revealSpecs.begin(),revealSpecs.end(),failedReveal.begin());
        failedReveal[failedSlot].target = tiny;
        require(!detour::install(failedReveal, revealHandles, failure)
            && failure.stage == detour::InstallStage::attach && failure.index == failedSlot,
            "failed reveal attach rolls back every earlier native hook");
        for (const auto& handle : revealHandles)
            require(!handle.attached && !handle.original, "no partial reveal hooks");
        untouched();
    }
    require(detour::install(revealSpecs, revealHandles, failure), "all twenty-one reveal hooks attach together");
    require(detour::uninstall(revealHandles), "all twenty-one reveal hooks detach together");
    untouched();
    for (unsigned pass = 0; pass < 3; ++pass) {
        if (!detour::install(supportSpecs, supportHandles, failure)) {
            std::fprintf(stderr, "stage=%u index=%zu error_known=%u error=%ld\n",
                static_cast<unsigned>(failure.stage), failure.index, failure.hasNativeError, failure.nativeError);
            require(false, "attach all support entry points in their production transaction");
        }
        // Production installs and publishes support hooks before reveal checks
        // its native entry prefixes. A clean-image-only check missed 10699C0.
        namespace reveal=sunrise::client::hooks::bootflow::omega_reveal_native;
        require(reveal::first_mismatched_binding(reinterpret_cast<const std::byte*>(image))==0,
                "reveal preflight passes after support hooks attach in production order");
        const auto probe=reveal::kBindings.front().rva;
        image[probe]^=1;
        require(reveal::first_mismatched_binding(reinterpret_cast<const std::byte*>(image))==probe,
                "reveal preflight still rejects a changed unowned entry");
        image[probe]^=1;
        const auto navHandles = std::span(handles).last(1);
        require(detour::install(std::span(specs).last(1),navHandles,failure),"native navigation hook coexists");
        const auto currentRevealHandles = std::span(handles).subspan(kOmegaHookRvas.size(),21);
        require(detour::install(revealSpecs, currentRevealHandles, failure),
                "reveal transaction coexists with the support transaction");
        for (const auto& handle : handles) require(handle.attached && handle.original, "all trampolines published");
        require(detour::uninstall(currentRevealHandles), "detach reveal transaction");
        require(detour::uninstall(navHandles),"detach navigation hook");
        require(detour::uninstall(supportHandles), "detach support transaction");
        untouched();
    }
    require(VirtualFree(image, 0, MEM_RELEASE) != FALSE, "release isolated copy");
    using Function = void(*)() noexcept;
    std::atomic<Function> original{};
    auto waiting = std::async(std::launch::async, [&] { return hook::await_original(original); });
    require(waiting.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout,
            "replacement cannot proceed before original publication");
    hook::publish_original(original, &replacement);
    require(waiting.get() == &replacement, "committed original wakes replacement");
    std::printf("PASS: %zu pinned hooks in 3 cycles of 29+21+1 atomic transactions; twenty-one reveal rollback positions; %zu reveal and %zu portal signatures; publication wait/notify; no game code executed\n",
        targets.size(), sunrise::client::hooks::bootflow::omega_reveal_native::kBindings.size(),
        sunrise::client::hooks::bootflow::omega_portal_probe::kBindings.size());
}
