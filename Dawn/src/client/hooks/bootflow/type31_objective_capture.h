#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "state/activity/lifecycle_generation.h"

namespace dawn::client::hooks::bootflow::type31_capture {

inline constexpr std::size_t kQueueCapacity = 64U;
inline constexpr std::size_t kStateKeyBytes = 0x10U;
inline constexpr std::size_t kDecodedBodyBytes = 0x18U;
inline constexpr std::size_t kSha256Bytes = 32U;
using ImageSha256 = std::array<std::byte, kSha256Bytes>;

/** SHA-256 of the supported installed packed destiny2.exe artifact. */
inline constexpr ImageSha256 kPinnedPackedImageSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED}};

inline constexpr std::uint32_t kObjectiveDefinition = 0x80F47BA6U;
inline constexpr std::uint32_t kDialogueDefinition = 0x80F47BA9U;
inline constexpr std::uint32_t kAuthoritySchema = 0x80809524U;
inline constexpr std::uint32_t kLogicalRegistry = 0xD00142CFU;
inline constexpr std::uint32_t kLogicalType = 31U;
inline constexpr std::uint32_t kOccupancyType = 60U;

struct StateKey16 final {
    std::array<std::byte, kStateKeyBytes> bytes{};
};

#if defined(_MSC_VER)
using NativeApply = void(__fastcall*)(void* component, StateKey16* stateKey) noexcept;
#else
using NativeApply = void (*)(void* component, StateKey16* stateKey) noexcept;
#endif

inline constexpr std::uintptr_t kApplyRva = 0xB20640U;
inline constexpr std::array<std::byte, 16U> kApplyPrefix{std::byte{0x40},
                                                         std::byte{0x53},
                                                         std::byte{0x48},
                                                         std::byte{0x83},
                                                         std::byte{0xEC},
                                                         std::byte{0x30},
                                                         std::byte{0x44},
                                                         std::byte{0x8B},
                                                         std::byte{0x02},
                                                         std::byte{0x48},
                                                         std::byte{0x8B},
                                                         std::byte{0xD9},
                                                         std::byte{0x4C},
                                                         std::byte{0x8B},
                                                         std::byte{0x4A},
                                                         std::byte{0x08}};

inline constexpr std::uintptr_t kPredicateRva = 0xB20B00U;
inline constexpr std::array<std::byte, 16U> kPredicatePrefix{std::byte{0x40},
                                                             std::byte{0x55},
                                                             std::byte{0x57},
                                                             std::byte{0x48},
                                                             std::byte{0x8B},
                                                             std::byte{0xEC},
                                                             std::byte{0x48},
                                                             std::byte{0x83},
                                                             std::byte{0xEC},
                                                             std::byte{0x78},
                                                             std::byte{0x80},
                                                             std::byte{0xB9},
                                                             std::byte{0x88},
                                                             std::byte{0x01},
                                                             std::byte{0x00},
                                                             std::byte{0x00}};

inline constexpr std::uintptr_t kTerminalRva = 0xB20820U;
inline constexpr std::array<std::byte, 17U> kTerminalPrefix{std::byte{0x48},
                                                            std::byte{0x89},
                                                            std::byte{0x74},
                                                            std::byte{0x24},
                                                            std::byte{0x20},
                                                            std::byte{0x57},
                                                            std::byte{0x48},
                                                            std::byte{0x81},
                                                            std::byte{0xEC},
                                                            std::byte{0xE0},
                                                            std::byte{0x00},
                                                            std::byte{0x00},
                                                            std::byte{0x00},
                                                            std::byte{0x48},
                                                            std::byte{0x8B},
                                                            std::byte{0x05},
                                                            std::byte{0x54}};

inline constexpr std::uintptr_t kSubscriberRva = 0x4CFA00U;
inline constexpr std::array<std::byte, 17U> kSubscriberPrefix{std::byte{0x48},
                                                              std::byte{0x89},
                                                              std::byte{0x5C},
                                                              std::byte{0x24},
                                                              std::byte{0x10},
                                                              std::byte{0x48},
                                                              std::byte{0x89},
                                                              std::byte{0x74},
                                                              std::byte{0x24},
                                                              std::byte{0x18},
                                                              std::byte{0x57},
                                                              std::byte{0x48},
                                                              std::byte{0x83},
                                                              std::byte{0xEC},
                                                              std::byte{0x30},
                                                              std::byte{0x8B},
                                                              std::byte{0x01}};

/** Generic incident-manager dynamic-listener enumerator; listener identity remains runtime data. */
inline constexpr std::uintptr_t kListenerEnumeratorRva = 0xD82B60U;
inline constexpr std::array<std::byte, 17U> kListenerEnumeratorPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x55}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x50}, std::byte{0xFC}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}};

enum class NativeSurface : std::uint8_t {
    apply,
    predicate,
    terminal,
    subscriber,
    listener_enumerator,
};

struct NativeTarget final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
};

/** Returns an absent target for an invalid NativeSurface value. */
[[nodiscard]] NativeTarget native_target(NativeSurface surface) noexcept;

/** Compares a caller-owned byte snapshot with one exact pinned prefix. */
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;

/**
 * Bounds-checks and reads one target prefix under SEH. The returned pointer is observation-only;
 * this module never installs a detour or otherwise mutates a native target.
 */
[[nodiscard]] const std::byte* validated_native_target(NativeSurface surface,
                                                       const std::byte* image,
                                                       std::size_t imageBytes) noexcept;

enum class ActivationSnapshotState : std::uint8_t {
    absent,
    current,
    quiescing,
    stale,
};

enum class ContextPresence : std::uint32_t {
    activation = 1U << 0U,
    native_identity = 1U << 1U,
    activity = 1U << 2U,
    session = 1U << 3U,
    run_token = 1U << 4U,
    correlation_token = 1U << 5U,
    generation_token = 1U << 6U,
};

inline constexpr std::uint32_t kKnownContextPresenceMask =
    static_cast<std::uint32_t>(ContextPresence::activation)
    | static_cast<std::uint32_t>(ContextPresence::native_identity)
    | static_cast<std::uint32_t>(ContextPresence::activity)
    | static_cast<std::uint32_t>(ContextPresence::session)
    | static_cast<std::uint32_t>(ContextPresence::run_token)
    | static_cast<std::uint32_t>(ContextPresence::correlation_token)
    | static_cast<std::uint32_t>(ContextPresence::generation_token);

[[nodiscard]] constexpr bool has_context_field(std::uint32_t mask,
                                               ContextPresence field) noexcept {
    return (mask & static_cast<std::uint32_t>(field)) != 0U;
}

struct SessionLineageSnapshot final {
    std::uint64_t session_id{};
    std::uint64_t created_revision{};
    std::uint64_t record_revision{};

    friend constexpr bool
    operator==(SessionLineageSnapshot, SessionLineageSnapshot) noexcept = default;
};

/**
 * Exact context copied at hook entry. Presence is explicit: an absent field is never a wildcard,
 * and this module never fills one from component identity, raw Type-31 values, or current state.
 */
struct CaptureContext final {
    std::uint32_t presence_mask{};
    state::activity::NativeActivationKey activation{};
    std::uint64_t native_identity{};
    state::activity::ActivityInstanceKey activity{};
    SessionLineageSnapshot session{};
    std::uint64_t run_token{};
    std::uint64_t correlation_token{};
    std::uint64_t generation_token{};
    ActivationSnapshotState activation_state{ActivationSnapshotState::absent};

    friend constexpr bool operator==(CaptureContext, CaptureContext) noexcept = default;
};

/** True only for a complete, internally consistent exact-context snapshot. */
[[nodiscard]] bool fully_correlated(const CaptureContext& context) noexcept;

/**
 * Canonicalizes one optional context snapshot without suppressing otherwise valid raw evidence.
 * Unknown bits, invalid present composites, and conflicting activity/session IDs are cleared.
 * @return True when the input was already a valid canonical partial or complete context.
 */
[[nodiscard]] bool sanitize_context(const CaptureContext& input,
                                    CaptureContext& output) noexcept;

[[nodiscard]] constexpr bool supported_definition(std::uint32_t definition) noexcept {
    return definition == kObjectiveDefinition || definition == kDialogueDefinition;
}

struct LogicalPointIdentity final {
    std::uint32_t registry{};
    std::uint32_t type{};
    std::uint32_t index{};
    std::uint32_t occupancy_registry{};
    std::uint32_t occupancy_type{};
    std::uint32_t occupancy_index{};

    friend constexpr bool operator==(LogicalPointIdentity, LogicalPointIdentity) noexcept = default;
};

/** Returns the package-pinned logical and local-occupancy identities for BA6/A9 only. */
[[nodiscard]] LogicalPointIdentity logical_identity(std::uint32_t definition) noexcept;

/** Definition/schema result of the two guarded content gates run before native forwarding. */
struct CaptureTarget final {
    std::uint32_t definition{};
    std::uint32_t schema{};
    LogicalPointIdentity logical{};

    friend constexpr bool operator==(CaptureTarget, CaptureTarget) noexcept = default;
};

struct CaptureMetadata final {
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t caller_rva{};
};

/**
 * Exact contiguous component+0x180..+0x19F values. RE proves consumed token, active state,
 * pending generation, and companion respectively; they are not Ghost/Scene/hold gate fields.
 */
struct ComponentSnapshot final {
    std::uint64_t offset_180{};
    std::uint64_t offset_188{};
    std::uint64_t offset_190{};
    std::uint64_t offset_198{};

    friend constexpr bool operator==(ComponentSnapshot, ComponentSnapshot) noexcept = default;
};

/** Decoded-body names remain intentionally limited to those proved by the Type-31 layout. */
struct DecodedBodyFields final {
    bool auth_bool{};
    std::uint64_t u64_0{};
    std::uint64_t u64_1{};
};

struct CaptureRecord final {
    CaptureContext context{};
    std::uint64_t sequence{};
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t caller_rva{};
    std::uintptr_t component_identity{};
    std::uint32_t definition{};
    std::uint32_t schema{};
    LogicalPointIdentity logical{};
    std::array<std::byte, kStateKeyBytes> state_key{};
    std::array<std::byte, kDecodedBodyBytes> decoded_body_pre{};
    ComponentSnapshot before{};
    ComponentSnapshot after{};
    bool state_key_valid{};
    bool decoded_body_pre_valid{};
    bool component_before_valid{};
    bool component_after_valid{};
    bool context_shape_valid{true};
    bool activation_entry_exact{};
    bool activation_current_at_exit{};
};

/** Scalar/hash-only projection permitted in the normal client log. */
struct DefaultLogFields final {
    std::uint64_t component_token{};
    std::uint64_t context_hash{};
    std::uint64_t state_key_hash{};
    std::uint64_t decoded_body_pre_hash{};
    std::uint64_t component_before_hash{};
    std::uint64_t component_after_hash{};
    DecodedBodyFields decoded{};
    bool decoded_valid{};
};

/** Stable bounded non-cryptographic hash used only to redact normal-log byte projections. */
[[nodiscard]] std::uint64_t bounded_hash(std::span<const std::byte> bytes) noexcept;

/** Produces no raw pointer, StateKey, decoded body, or component-window bytes. */
[[nodiscard]] bool default_log_fields(const CaptureRecord& record,
                                      DefaultLogFields& output) noexcept;

/** Raw evidence validity is deliberately independent of optional correlation completeness. */
[[nodiscard]] bool valid_raw_record(const CaptureRecord& record) noexcept;

/** Requires a canonical complete exact context; resolver-invalid shapes cannot match. */
[[nodiscard]] bool record_matches_context(const CaptureRecord& record,
                                          const CaptureContext& expected) noexcept;

/** Additionally requires the entry activation to remain current after the native call. */
[[nodiscard]] bool record_matches_current_context(const CaptureRecord& record,
                                                  const CaptureContext& expected) noexcept;

/** Fails closed when the raw native bool is not exactly zero or one. */
[[nodiscard]] bool decode_body(const CaptureRecord& record, DecodedBodyFields& fields) noexcept;

enum class PushResult : std::uint8_t {
    enqueued,
    duplicate,
    rejected,
    full,
    busy,
    sequence_exhausted,
};

enum class ReadResult : std::uint8_t {
    success,
    empty,
    busy,
    context_mismatch,
    invalid_context,
    not_current,
};

struct QueueCounters final {
    std::uint64_t accepted{};
    std::uint64_t duplicates{};
    std::uint64_t rejected{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};
    std::uint64_t context_resolver_failures{};
    std::uint64_t projection_failures{};

    [[nodiscard]] constexpr std::uint64_t dropped() const noexcept {
        return dropped_full + dropped_busy + dropped_sequence_exhausted + projection_failures;
    }
};

enum class ResetResult : std::uint8_t {
    reset,
    busy,
};

class CaptureQueueLock;

/**
 * Fixed 64-record queue intended for a native observation hook and an off-hook consumer.
 * Producers make one atomic try-lock attempt and never spin. No operation allocates, performs
 * I/O, calls game code, or waits.
 */
class CaptureQueue final {
public:
    CaptureQueue() noexcept = default;

    CaptureQueue(const CaptureQueue&) = delete;
    CaptureQueue& operator=(const CaptureQueue&) = delete;
    CaptureQueue(CaptureQueue&&) = delete;
    CaptureQueue& operator=(CaptureQueue&&) = delete;

    [[nodiscard]] PushResult try_push(const CaptureRecord& record) noexcept;

    /** Accounts for a gate/prepare/finish rejection that never reached try_push. */
    void account_rejected() noexcept;

    /** Accounts for an off-hook scalar/hash projection that could not be serialized intact. */
    void account_projection_failure() noexcept;

    /** Unconditional FIFO diagnostic drain; absent context is valid. */
    [[nodiscard]] ReadResult try_pop_raw(CaptureRecord& output) noexcept;

    /** Leaves the queue/output unchanged unless the front tag is exactly expected. */
    [[nodiscard]] ReadResult try_pop_exact(const CaptureContext& expected,
                                           CaptureRecord& output) noexcept;

    /** Exact-context pop for consumers that require the activation to remain current. */
    [[nodiscard]] ReadResult try_pop_exact_current(const CaptureContext& expected,
                                                   CaptureRecord& output) noexcept;

    /** Exposes only the recorded front tag so a consumer can make an explicit decision. */
    [[nodiscard]] ReadResult try_front_context(CaptureContext& output) noexcept;

    /** Discards one front record only when its tag is exactly expected. */
    [[nodiscard]] ReadResult try_discard_exact(const CaptureContext& expected) noexcept;

    [[nodiscard]] QueueCounters counters() const noexcept;

    /**
     * Clears records, dedupe, sequence, and counters in one non-waiting operation. The owner may
     * call this only for a newly allocated epoch or after protected removal was confirmed.
     */
    [[nodiscard]] ResetResult try_reset() noexcept;

private:
    friend class CaptureQueueLock;

    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;
    void pop_front(CaptureRecord& output) noexcept;

    std::array<CaptureRecord, kQueueCapacity> records_{};
    CaptureRecord last_accepted_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t next_sequence_{1U};
    bool has_last_accepted_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::atomic_uint64_t accepted_{};
    std::atomic_uint64_t duplicates_{};
    std::atomic_uint64_t rejected_{};
    std::atomic_uint64_t dropped_full_{};
    std::atomic_uint64_t dropped_busy_{};
    std::atomic_uint64_t dropped_sequence_exhausted_{};
    std::atomic_uint64_t context_resolver_failures_{};
    std::atomic_uint64_t projection_failures_{};
};

enum class CaptureBuildResult : std::uint8_t {
    ready,
    complete,
    partial,
    unsupported_definition,
    wrong_schema,
    null_pointer,
    unreadable,
    target_changed,
    component_mismatch,
    consumed,
};

/** Guard-copies component+0 and StateKey16+0 and accepts only BA6/A9 plus schema 80809524. */
[[nodiscard]] CaptureBuildResult inspect_capture_target(const void* component,
                                                        const void* stateKey16,
                                                        CaptureTarget& output) noexcept;

/** Stack-owned token spanning the call to the original native apply function. */
class PendingCapture final {
public:
    PendingCapture() noexcept = default;

    PendingCapture(const PendingCapture&) = delete;
    PendingCapture& operator=(const PendingCapture&) = delete;
    PendingCapture(PendingCapture&&) = delete;
    PendingCapture& operator=(PendingCapture&&) = delete;

private:
    friend CaptureBuildResult prepare_capture(PendingCapture&,
                                              const CaptureContext&,
                                              CaptureMetadata,
                                              CaptureTarget,
                                              const void*,
                                              const void*) noexcept;
    friend CaptureBuildResult
    finish_capture(PendingCapture&, const void*, bool, CaptureRecord&) noexcept;

    CaptureContext context_{};
    CaptureMetadata metadata_{};
    CaptureTarget target_{};
    std::uintptr_t component_identity_{};
    std::array<std::byte, kStateKeyBytes> state_key_{};
    std::array<std::byte, kDecodedBodyBytes> decoded_body_pre_{};
    ComponentSnapshot before_{};
    bool component_before_valid_{};
    bool context_shape_valid_{true};
    bool activation_entry_exact_{};
    bool ready_{};
};

/**
 * Revalidates the exact target, copies all 16 state-key bytes and the referenced 0x18 decoded
 * bytes before the original, and independently attempts the pre-call component snapshot.
 */
[[nodiscard]] CaptureBuildResult prepare_capture(PendingCapture& pending,
                                                 const CaptureContext& context,
                                                 CaptureMetadata metadata,
                                                 CaptureTarget target,
                                                 const void* component,
                                                 const void* stateKey16) noexcept;

/**
 * Uses only the immutable pre-call body held by pending. A failed post snapshot still emits a
 * valid partial raw record; component identity mismatch remains a hard internal failure.
 */
[[nodiscard]] CaptureBuildResult finish_capture(PendingCapture& pending,
                                                const void* component,
                                                bool activationCurrentAtExit,
                                                CaptureRecord& output) noexcept;

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "Type-31 hooks require lock-free 64-bit counters");
static_assert(sizeof(void*) == sizeof(std::uint64_t),
              "The pinned Type-31 StateKey layout requires a 64-bit process");
static_assert(sizeof(StateKey16) == kStateKeyBytes);
static_assert(sizeof(ComponentSnapshot) == 0x20U);
static_assert(kDecodedBodyBytes == 0x18U);
static_assert(kQueueCapacity == 64U);

} // namespace dawn::client::hooks::bootflow::type31_capture
