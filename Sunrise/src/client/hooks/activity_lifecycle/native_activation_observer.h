#pragma once

#include <cstdint>

#include "native_activation_registry.h"

namespace sunrise::client::hooks::activity_lifecycle {

/** Outcome of a protected after-consumer detach attempt. */
enum class ObserverUninstallResult : std::uint8_t {
    removed,
    deferred,
    failed,
};

/**
 * Installs the exact-image five-hook activation observer as one ownership group, including the
 * parameterless global wrapper-drop close boundary.
 * This must complete before any consumer starts retaining NativeActivationSnapshot values.
 */
[[nodiscard]] bool install() noexcept;

/**
 * Stops activation publication and invalidates every active key before consumers uninstall.
 * Native forwarding remains available until the protected batch detach succeeds.
 */
void quiesce_before_consumers() noexcept;

/**
 * Detaches only after callers have removed every snapshot consumer.
 * Deferred/failed attempts retain handles, trampolines, registry storage, and quiescing state.
 */
[[nodiscard]] ObserverUninstallResult uninstall_after_consumers() noexcept;

/** @return True while any observer handle, trampoline, or in-flight call remains owned. */
[[nodiscard]] bool has_ownership() noexcept;

/** Returns an immutable exact wrapper/full-handle snapshot; no mission correlation is guessed. */
[[nodiscard]] NativeActivationSnapshot observe(std::uintptr_t wrapper,
                                                std::uint32_t fullHandle) noexcept;

/** Revalidates a captured exact key/wrapper/full-handle tuple immediately before use. */
[[nodiscard]] bool is_current(NativeActivationToken token) noexcept;

} // namespace sunrise::client::hooks::activity_lifecycle
