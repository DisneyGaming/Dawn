#pragma once

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"

namespace dawn::client::hooks::retail_log {

using Enqueue = void(__fastcall*)(std::int32_t, const char*) noexcept;

extern SRWLOCK g_lock;
extern hooking::detour::Handle g_handle;
extern std::atomic<Enqueue> g_original;
extern hooking::CallGate g_callGate;

/** @return The enqueue observer body itself, with internal linkage. */
[[nodiscard]] void* enqueue_entry_point() noexcept;

/** Applies the configured category threshold, once we know the log block exists. */
void assert_verbosity() noexcept;

/**
 * Adds one runtime class/schema identifier to the bounded activity-start reference scan.
 * Content extraction discovers these identifiers before the native activity starts.
 */
void register_schema_marker(std::uint32_t marker) noexcept;

/**
 * Retains the exact activity-host descriptor Dawn is about to publish. The retail-log observer
 * compares it with native memory at the membership-consumer and secure-channel boundaries.
 */
void register_gameplay_join_descriptor(const std::byte* descriptor,
                                       std::size_t size,
                                       std::int32_t regionIndex,
                                       std::uint64_t hostSessionId) noexcept;

/**
 * Copies the latest activity-host descriptor into caller-owned storage. This keeps the opaque,
 * session-bearing bytes behind the observer lock and lets native consumers use an exact snapshot.
 * @return True only after the gameplay host has registered a complete 128-byte descriptor.
 */
[[nodiscard]] bool snapshot_gameplay_join_descriptor(
    std::array<std::byte, 128>& descriptor,
    std::int32_t& regionIndex,
    std::uint64_t& hostSessionId,
    std::uint32_t& generation) noexcept;

} // namespace dawn::client::hooks::retail_log
