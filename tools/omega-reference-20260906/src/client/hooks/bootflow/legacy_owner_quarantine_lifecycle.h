#pragma once

#include <utility>

namespace sunrise::client::hooks::bootflow::legacy_owner_quarantine {

enum class InstallDisposition {
    proceed_with_lock,
    rejected_closed,
    rejected_legacy_ownership,
};

/**
 * Starts one production install attempt. A proceed result retains the exclusive lifecycle lock;
 * either rejection releases it. Dirty ownership closes late admission before that release.
 */
template <typename Operations>
[[nodiscard]] InstallDisposition begin_install(Operations& operations) noexcept {
    operations.lock_install();
    if (operations.installed() && !operations.accepting()) {
        operations.unlock_install();
        return InstallDisposition::rejected_closed;
    }
    if (operations.quarantined_legacy_ownership()) {
        operations.close_late_admission();
        operations.unlock_install();
        return InstallDisposition::rejected_legacy_ownership;
    }
    return InstallDisposition::proceed_with_lock;
}

/** Closes/drains late admission, then decides whether owner teardown may begin. */
template <typename Operations>
[[nodiscard]] bool begin_uninstall(Operations& operations) noexcept {
    operations.quiesce();
    return !operations.quarantined_legacy_ownership();
}

/** Keeps an admitted dynamic writer's guard alive through its complete publication sequence. */
template <typename Guard, typename Writer>
[[nodiscard]] bool execute_admitted_dynamic_writer(Guard&& guard, Writer&& writer) noexcept {
    if (!guard.accepted()) {
        return false;
    }
    std::forward<Writer>(writer)();
    return true;
}

} // namespace sunrise::client::hooks::bootflow::legacy_owner_quarantine
