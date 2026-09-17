#pragma once

#include <cstddef>
#include <cstdint>

#include "sensor_state_heap_full_cohort_core.h"

namespace dawn::client::hooks::network::lifecycle::sensor_state_heap_full_cohort {

/**
 * Fixed token owned by the existing +4D7470 detour. That detour must call receive_enter before its
 * one native invocation and receive_exit immediately after it returns, including nested calls.
 */
struct ReceiveToken final {
    void* previous{};
    std::uintptr_t sensorTable{};
    std::uintptr_t peerView{};
    std::uintptr_t stream{};
    std::uintptr_t record{};
    std::uintptr_t destination{};
    std::uint64_t nativeResult{};
    std::uint32_t recordGeneration{};
    std::uint32_t bitsBefore{};
    std::uint32_t operationOrdinal{};
    std::uint64_t cookie{};
    bool active{};
    bool admitted{};
};

/** Raw +4C72E0 boundary evidence. Consumers assign their own semantics outside this module. */
struct DecodeObservation final {
    void* stream{};
    void* destination{};
    std::int32_t* schemaKey{};
    std::uintptr_t record{};
    std::uint64_t nativeResult{};
    std::uint32_t schema{};
    std::uint32_t nativeFlags{};
    std::uint32_t bitsBefore{};
    std::uint32_t bitsAfter{};
    std::uint32_t recordGeneration{};
    EventPhase phase{EventPhase::pre};
    Member member{Member::none};
    bool recordValid{};
};

using DecodeObserver = void (*)(void* context, const DecodeObservation& observation) noexcept;

struct DecodeObserverHandle final {
    std::uint32_t slot{};
    std::uint32_t generation{};
    [[nodiscard]] explicit operator bool() const noexcept { return generation != 0U; }
};

/**
 * Installs one of four fixed fanout subscribers. Callbacks are invoked as discrete pre/post calls;
 * no fanout lock or callback spans the native original, and callbacks must not log, allocate, or
 * perform I/O. Registration is lifecycle/off-hook work.
 */
[[nodiscard]] bool register_decode_observer(DecodeObserver observer,
                                            void* context,
                                            DecodeObserverHandle& output) noexcept;

/**
 * Begins retirement and succeeds only after the last callback has left. A false return retains the
 * slot/context for a retry, so the consumer must keep both code and context alive.
 */
[[nodiscard]] bool unregister_decode_observer(DecodeObserverHandle handle) noexcept;

/** Default-off install. Disabled mode performs no hashing, target reads, file work, or Detours. */
[[nodiscard]] bool install() noexcept;

/** Stops new cohort observations while every replacement continues to call its original once. */
void quiesce() noexcept;

/**
 * Protected, retryable detach for the eleven-site core batch. Both external owners (+3C8EB0 and
 * +4D7470) must be removed first; deferred detach or flush failure retains all owned state.
 */
[[nodiscard]] bool uninstall() noexcept;

/** @return True while any core handle, trampoline, mapped ledger, or receive handoff is retained. */
[[nodiscard]] bool has_ownership() noexcept;

[[nodiscard]] Readiness readiness() noexcept;

/** Handoff called only after the existing +4D7470 owner validates its exact 16-byte entry. */
void receive_owner_attached() noexcept;

/** Called before the existing owner starts its protected detach. */
void receive_owner_quiescing() noexcept;

/** Called after that protected detach; false retains readiness ownership for a later retry. */
void receive_owner_detached(bool removed) noexcept;

/** Handoff from the sole native-activation +3C8EB0 owner after its 17-byte validation. */
void global_drop_owner_attached() noexcept;
void global_drop_owner_quiescing() noexcept;
void global_drop_owner_detached(bool removed) noexcept;

/** Unconditional entry/exit bridge used by the existing +4D7470 replacement. */
void receive_enter(ReceiveToken& token, void* sensorTable, void* bitStream) noexcept;
void receive_exit(ReceiveToken& token, std::uint64_t nativeResult) noexcept;

/** Assertion-safe callback: exact NUL-inclusive match, reserved event, freeze, and durable flush. */
void notify_assert(const char* text) noexcept;

/** Allocation-free retail correlation, called before retail capture's log-acceptance early return. */
void observe_retail_line(const char* sanitizedText, std::size_t length) noexcept;

/** Off-hook text projection and non-assert flush service. */
void drain(std::size_t maximumEvents) noexcept;
void service_flush() noexcept;

/** Exact 16-byte +4D7470 contract for the single existing owner. */
[[nodiscard]] const SiteContract& receive_owner_contract() noexcept;

} // namespace dawn::client::hooks::network::lifecycle::sensor_state_heap_full_cohort
