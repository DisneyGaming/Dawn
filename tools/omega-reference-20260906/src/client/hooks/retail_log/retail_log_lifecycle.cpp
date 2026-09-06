#include "retail_log_lifecycle.h"

#include <array>

#include "../../../core/logging/log.h"
#include "../../targets/game.h"
#include "retail_log_enqueue_observer.h"

namespace sunrise::client::hooks::retail_log {

SRWLOCK g_lock{SRWLOCK_INIT};
hooking::detour::Handle g_handle{};
std::atomic<Enqueue> g_original{nullptr};
hooking::CallGate g_callGate{};

namespace {

[[nodiscard]] bool calls_idle() noexcept {
    return g_callGate.idle();
}

} // namespace

/** Installs the game retail-log capture hook. */
bool install() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_handle.attached) {
        const bool accepting = g_callGate.accepting();
        ReleaseSRWLockExclusive(&g_lock);
        return accepting;
    }
    if (!targets::game::retail_log::is_resolved()) {
        ReleaseSRWLockExclusive(&g_lock);
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=retail stage=install result=fail reason=target");
        return false;
    }

    g_callGate.quiesce();
    const hooking::detour::Spec spec{targets::game::retail_log::get().enqueue,
                                     enqueue_entry_point()};
    const bool installed = hooking::detour::install(spec, g_handle);
    if (installed) {
        hooking::publish_original(g_original, reinterpret_cast<Enqueue>(g_handle.original));
        g_callGate.accept();
    }
    ReleaseSRWLockExclusive(&g_lock);
    core::log::write(core::log::Channel::client,
                     installed ? core::log::Level::info : core::log::Level::warn,
                     installed ? "ev=retail stage=install result=ok"
                               : "ev=retail stage=install result=fail reason=detour");
    return installed;
}

/** Stops new Sunrise-owned retail-log work while preserving native forwarding. */
void quiesce() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_callGate.quiesce();
    ReleaseSRWLockExclusive(&g_lock);
}

/** Removes the retail-log capture hook. */
bool uninstall() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_callGate.quiesce();
    if (!g_handle.attached) {
        ReleaseSRWLockExclusive(&g_lock);
        return true;
    }
    // The observer body reads the trampoline without a lock, so it must be idle at detach.
    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{enqueue_entry_point()},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    const hooking::detour::UninstallResult result =
        hooking::detour::uninstall(g_handle, protectedEntries, &calls_idle);
    const bool removed = result == hooking::detour::UninstallResult::removed;
    if (removed) {
        g_original.store(nullptr, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_lock);
    core::log::write(
        core::log::Channel::client,
        removed ? core::log::Level::info
                : result == hooking::detour::UninstallResult::failed ? core::log::Level::error
                                                                      : core::log::Level::warn,
        removed ? "ev=retail stage=uninstall result=ok retained=0"
                : result == hooking::detour::UninstallResult::failed
                    ? "ev=retail stage=uninstall result=failed retained=1"
                    : "ev=retail stage=uninstall result=deferred retained=1");
    return removed;
}

/** @return True while the retail-log capture hook is attached. */
bool is_installed() noexcept {
    AcquireSRWLockShared(&g_lock);
    const bool attached = g_handle.attached;
    ReleaseSRWLockShared(&g_lock);
    return attached;
}

} // namespace sunrise::client::hooks::retail_log
