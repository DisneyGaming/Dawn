#pragma once

#include <cstddef>

namespace dawn::client::hooks::network::lifecycle::sensor_state_heap {

/** Availability of the optional Phase-0 smoke observer; gameplay admission never depends on it. */
enum class Readiness : unsigned char {
    disabled,
    unavailable,
    ready,
    quiescing,
};

/**
 * Installs the default-off, read-only per-sensor record observer when every pinned prefix matches.
 * A missing or changed target fails closed and leaves all native entries untouched.
 */
[[nodiscard]] bool install() noexcept;

/** Stops new diagnostic observations while every replacement continues native forwarding. */
void quiesce() noexcept;

/**
 * Detaches the whole observer batch only after its replacement bodies are quiescent. Failed or
 * protected removal retains every handle, trampoline, and fixed-storage ledger for a later retry.
 */
[[nodiscard]] bool uninstall() noexcept;

/** @return True while any observer handle or in-flight replacement remains owned. */
[[nodiscard]] bool has_ownership() noexcept;

/** @return Current observer readiness without inspecting native targets. */
[[nodiscard]] Readiness readiness() noexcept;

/**
 * Projects at most maxEvents committed records to the normal client log.
 * This function performs formatting and I/O and therefore must be called only outside hook and
 * assert-handler context.
 */
void drain(std::size_t maxEvents) noexcept;

/** Explicit name for the bounded formatting/I/O path that must run outside callback context. */
void drain_off_hook(std::size_t maxEvents) noexcept;

/** @return True after the first exact heap assertion asks a safe coordinator to drain evidence. */
[[nodiscard]] bool flush_requested() noexcept;

/** Clears one pending flush request. No current caller is wired; this remains coordinator-owned. */
[[nodiscard]] bool consume_flush_request() noexcept;

/**
 * Freezes the fixed ring on the exact known heap assertion. This callback only compares bounded
 * text and updates atomics; it never logs, allocates, opens a file, or reads game storage.
 */
void notify_assert(const char* text) noexcept;

} // namespace dawn::client::hooks::network::lifecycle::sensor_state_heap
