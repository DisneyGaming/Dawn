#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace sunrise::client::hooks::bootflow::opening_authority::ikora_ground_dwell {

/**
 * Inert source support for a future sole-owner Phase-2 observation cohort.
 *
 * This module can admit the real runtime/content artifacts and can execute guarded capture calls,
 * but no detour is attached and no lifecycle is registered by this delivery. It contains no
 * authority, Scene, actor, provider, spawner, pose, visibility, transition, timing, or policy
 * writer.
 */
inline constexpr bool kObservationOnly = true;
inline constexpr bool kSourceOnlyInert = true;
inline constexpr bool kOwnsInstalledDetour = false;
inline constexpr bool kCapturePerformsIo = false;
inline constexpr bool kAdmissionReadsArtifacts = true;
inline constexpr bool kProvidesAnyWriter = false;
inline constexpr bool kProvidesElapsedGate = false;
inline constexpr bool kProvidesForcedSpawner = false;
inline constexpr bool kProvidesType26StrictGate = false;
inline constexpr bool kDefaultTelemetryContainsPointers = false;
inline constexpr bool kDefaultTelemetryContainsPackedHandles = false;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;
using StateKey16 = std::array<std::byte, 16U>;

inline constexpr wchar_t kPinnedPackedRuntimePath[] = L"D:\\Destiny3\\destiny2.exe";
inline constexpr std::uint64_t kPinnedPackedRuntimeBytes = 122'984'224U;
inline constexpr Sha256 kPinnedPackedRuntimeSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED}};

inline constexpr wchar_t kPinnedMappedProvenancePath[] =
    L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe";
inline constexpr std::uint64_t kPinnedMappedProvenanceBytes = 145'091'072U;
inline constexpr Sha256 kPinnedMappedProvenanceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD}};

inline constexpr wchar_t kPinnedOmegaPackagePath[] =
    L"D:\\Destiny3\\packages\\w64_mercury_destination_activities_03a3_5.pkg";
inline constexpr std::uint64_t kPinnedOmegaPackageBytes = 407'552U;
inline constexpr Sha256 kPinnedOmegaPackageSha256{
    std::byte{0x99}, std::byte{0x77}, std::byte{0xBA}, std::byte{0xAA},
    std::byte{0xE8}, std::byte{0x9B}, std::byte{0xF0}, std::byte{0x7A},
    std::byte{0x81}, std::byte{0x59}, std::byte{0x02}, std::byte{0xEC},
    std::byte{0x81}, std::byte{0xEE}, std::byte{0xCD}, std::byte{0x28},
    std::byte{0x9A}, std::byte{0x7B}, std::byte{0x1B}, std::byte{0xA8},
    std::byte{0x1E}, std::byte{0x7E}, std::byte{0x32}, std::byte{0x8C},
    std::byte{0xD0}, std::byte{0xF6}, std::byte{0x7D}, std::byte{0x1C},
    std::byte{0x25}, std::byte{0x07}, std::byte{0x5F}, std::byte{0x17}};

inline constexpr std::uint16_t kPinnedMachine = 0x8664U;
inline constexpr std::uint16_t kPinnedSectionCount = 11U;
inline constexpr std::uint32_t kPinnedPeTimestamp = 0x5F43138BU;
inline constexpr std::uint32_t kPinnedEntryRva = 0x0187CDD8U;
inline constexpr std::uint32_t kPinnedSizeOfImage = 0x08A5EA00U;
inline constexpr std::uintptr_t kPinnedPackedImageBase = 0x140000000ULL;

inline constexpr std::uint32_t kActivityRegistry = 0xD00142CFU;
inline constexpr std::uint32_t kOpeningBubble = 15U;
inline constexpr std::uint32_t kSceneDefinition = 0x80F47B73U;
inline constexpr std::uint32_t kSceneEntity = 0x80F47B74U;
inline constexpr std::uint32_t kSceneWrapper = 0x80F47B75U;
inline constexpr std::uint32_t kSceneComponent = 0x80806382U;
inline constexpr std::uint32_t kSceneAuthoritySchema = 0x8080626BU;
inline constexpr std::uint32_t kSceneSenseSchema = 0x8080626AU;
inline constexpr std::uint32_t kSceneSelector = 0x80EC0F96U;
inline constexpr std::uint32_t kSceneScheduler = 0x80EC0F95U;
inline constexpr std::uint32_t kSharedActorEntity = 0x80EC0F27U;
inline constexpr std::uint32_t kInitialVisibleScheduler = 0x80EC0F0EU;
inline constexpr std::uint32_t kConcurrentCarrierScheduler = 0x80EC0FA8U;
inline constexpr std::uint32_t kTrueSuccessorScheduler = 0x80EC0FA6U;
inline constexpr std::uint64_t kActorCastBinding = 0xF88D7FB078DD1EA2ULL;
inline constexpr std::uint64_t kTimelineCastBinding = 0xB0721C83E6021D60ULL;
inline constexpr std::uint32_t kTimelineComponent = 0x80808344U;
inline constexpr std::uint32_t kSquadKind = 0x80809A3BU;
inline constexpr std::uint32_t kSquadAuthoritySchema = 0x80807EC9U;
inline constexpr std::uint32_t kSquadSenseSchema = 0x80807ECCU;
inline constexpr std::uint32_t kType26AuthoritySchema = 0x8080954BU;
inline constexpr std::uint32_t kType31AuthoritySchema = 0x80809524U;

struct RootTransform final {
    float position_x{};
    float position_y{};
    float position_z{};
    float quaternion_x{};
    float quaternion_y{};
    float quaternion_z{};
    float quaternion_w{};
};

inline constexpr RootTransform kExactTimelineRoot{347.396667F,
                                                  249.845093F,
                                                  99.459267F,
                                                  0.0F,
                                                  0.0F,
                                                  -0.999766F,
                                                  -0.021611F};

enum class NativeSurface : std::uint8_t {
    spawner_authority_apply,
    spawner_deficit,
    spawner_request,
    spawner_resolve,
    spawner_drain_submit,
    spawner_drain_resolve,
    type31_authority_apply,
    type31_predicate,
    type31_terminal,
    incident_dynamic_listener,
    type26_authority_apply,
    type26_reconcile,
    type26_materialize,
    type26_subscriber,
    type26_generation,
    type26_sense,
    type26_clear,
    dialogue_request,
    dialogue_generation_scan,
    dialogue_dispatch,
    dialogue_consumer,
    scene_decode,
    scene_authority_apply,
    scene_reconcile,
    scene_root_start,
    scene_terminal_sense,
    scene_export,
    actor_materialize,
    entity_factory_thunk,
    entity_factory,
    actor_transition_tick,
    actor_terminal,
    provider_cache,
    provider_descriptor_walk,
    provider_source_row,
    provider_resolver,
    provider_selection,
    provider_candidate,
    provider_pose_dispatch,
    count,
};

inline constexpr std::size_t kNativeSurfaceCount = static_cast<std::size_t>(NativeSurface::count);

enum class NativeAbi : std::uint8_t {
    instance_packet,
    instance_count_context,
    four_register_request,
    instance_only,
    internal,
    scene_decode,
    scene_apply,
    scene_reconcile,
    scene_start,
    actor_scheduler,
    actor_tick,
    actor_terminal,
    factory_thunk,
    factory,
    provider,
    pose_dispatch,
};

struct NativeTarget final {
    NativeSurface surface{};
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
    Sha256 prefix_sha256{};
    NativeAbi abi{};
};

[[nodiscard]] NativeTarget native_target(NativeSurface surface) noexcept;

enum class LaneKind : std::uint8_t {
    pre_lift_owner,
    point_listener,
    type26_hold,
    ghost_dialogue,
    scene_authority,
    actor_provider,
    count,
};

inline constexpr std::size_t kLaneCount = static_cast<std::size_t>(LaneKind::count);

enum class BehaviorCase : std::uint8_t {
    walk,
    sprint_before_ghost_end,
    linger_ghost_volume,
    reverse_approach,
    separate_point_entries,
    reload_inside,
    late_join,
    checkpoint_reload,
    wipe_replay,
    portal_return,
    count,
};

inline constexpr std::uint32_t kRequiredBehaviorMask =
    (1U << static_cast<std::uint8_t>(BehaviorCase::count)) - 1U;

enum class TokenDomain : std::uint8_t {
    cohort,
    activation,
    call,
    record,
    component,
    authority,
    publication,
    incident,
    actor,
    provider,
    factory,
    source_row,
    thread,
};

class OpaqueToken final {
public:
    constexpr OpaqueToken() noexcept = default;
    [[nodiscard]] constexpr bool present() const noexcept {
        return serial_ != 0U && authenticator_ != 0U;
    }
    [[nodiscard]] constexpr std::uint64_t opaque_id() const noexcept {
        return authenticator_;
    }
    [[nodiscard]] constexpr TokenDomain domain() const noexcept {
        return domain_;
    }
    friend constexpr bool operator==(OpaqueToken, OpaqueToken) noexcept = default;

private:
    TokenDomain domain_{};
    std::uint64_t epoch_{};
    std::uint64_t serial_{};
    std::uint64_t subject_{};
    std::uint64_t authenticator_{};

    friend class CaptureOwner;
};

struct ActivityIdentity final {
    std::uint64_t session_id{};
    std::uint64_t activity_id{};
    std::uint64_t activation_generation{};
    std::uint32_t registry{};
    std::uint32_t bubble{};
    std::uint64_t region_generation{};
    std::uint64_t wipe_replay_generation{};
    std::uint64_t roster_record_id{};
    std::uint64_t roster_generation{};
    std::uint64_t scene_component_id{};
    std::uint64_t scene_component_generation{};
};

class ActivationHandle final {
public:
    constexpr ActivationHandle() noexcept = default;
    [[nodiscard]] constexpr bool present() const noexcept {
        return token_.present();
    }
    [[nodiscard]] constexpr BehaviorCase behavior() const noexcept {
        return behavior_;
    }

private:
    OpaqueToken token_{};
    ActivityIdentity identity_{};
    BehaviorCase behavior_{BehaviorCase::count};
    friend class CaptureOwner;
    friend class CaptureTransaction;
};

enum class AdmissionResult : std::uint8_t {
    admitted,
    wrong_phase,
    module_path_mismatch,
    packed_file_mismatch,
    mapped_pe_mismatch,
    mapped_page_invalid,
    mapped_prefix_mismatch,
    omega_package_mismatch,
    random_failure,
    io_failure,
};

enum class CopyOutcome : std::uint8_t {
    complete,
    empty,
    too_large,
    fault,
    generation_fault,
    torn_generation,
    stale_call,
    duplicate_role_phase,
    payload_full,
};

enum class CapturePhase : std::uint8_t {
    before_original,
    after_original,
};

enum class BlockRole : std::uint8_t {
    spawner_authority,
    spawner_request_descriptor,
    spawner_resolver,
    spawner_drain,
    factory_descriptor,
    actor_scalar,
    actor_descriptor,
    actor_source_row,
    actor_transition_row,
    actor_terminal_state,
    provider_scalar,
    provider_component,
    provider_candidate,
    provider_locator,
    provider_transform,
    provider_effect,
    type31_authority_raw,
    type31_authority_decoded,
    type31_membership,
    incident_root,
    listener_row,
    authority_publication,
    type26_authority_raw,
    type26_authority_decoded,
    type34_closure,
    hold_linked_objects,
    hold_subscriber,
    hold_generation_channels,
    hold_sense,
    hold_clear_owner,
    dialogue_request,
    dialogue_dispatch,
    dialogue_start,
    dialogue_bank_state,
    dialogue_consume,
    dialogue_listener,
    scene_state_key,
    scene_authority_raw,
    scene_authority_decoded,
    scene_apply_derived,
    scene_effective,
    scene_committed,
    scene_new_words,
    scene_encoder_first,
    scene_encoder_second,
    scene_live_state,
    scene_sense,
    policy_consumer,
};

using PostCopyProbe = void (*)(void*) noexcept;

struct NativeBlockView final {
    const void* source{};
    std::size_t bytes{};
    const volatile std::uint64_t* generation{};
    std::size_t significant_bits{};
    TokenDomain owner_domain{TokenDomain::component};
    std::uint64_t stable_subject{};
    PostCopyProbe post_copy_probe{};
    void* post_copy_context{};
};

inline constexpr std::size_t kMaximumEvidenceBlocks = 16U;
inline constexpr std::size_t kSecuredPayloadBytes = 4096U;

struct EvidenceBlock final {
    BlockRole role{};
    CapturePhase phase{};
    CopyOutcome outcome{CopyOutcome::empty};
    OpaqueToken owner_token{};
    std::uintptr_t source_address{}; // secured raw ring only; never projected
    std::uint64_t generation_before{};
    std::uint64_t generation_after{};
    std::uint32_t payload_offset{};
    std::uint32_t source_bytes{};
    std::uint32_t copied_bytes{};
    std::uint32_t significant_bits{};
    Sha256 digest{};
};

struct EventStamp final {
    std::uint64_t entry_clock{};
    std::uint64_t exit_clock{};
    std::uint64_t entry_serial{};
    std::uint64_t exit_serial{};
    std::uintptr_t caller_rva{}; // secured raw ring only
    OpaqueToken thread_token{};
};

enum class RecordValidation : std::uint8_t {
    complete,
    partial,
    invalid_owner,
    invalid_call,
    missing_original,
    repeated_original,
    invalid_order,
    invalid_lane_shape,
    invalid_scene_shape,
    invalid_scene_apply,
    invalid_scene_reconcile,
    invalid_round_trip,
    invalid_actor_identity,
    forbidden_behavior,
};

struct RawEvidenceRecord final {
    OpaqueToken cohort_token{};
    OpaqueToken activation_token{};
    OpaqueToken call_token{};
    OpaqueToken record_token{};
    ActivityIdentity activity{};
    BehaviorCase behavior{BehaviorCase::count};
    LaneKind lane{LaneKind::count};
    NativeSurface surface{NativeSurface::count};
    EventStamp stamp{};
    std::uint64_t capture_epoch{};
    std::uint64_t authority_epoch{};
    std::uint64_t publication_epoch{};
    std::uint16_t block_count{};
    std::uint16_t original_call_count{};
    std::uint32_t payload_bytes{};
    RecordValidation validation{RecordValidation::partial};
    std::array<EvidenceBlock, kMaximumEvidenceBlocks> blocks{};
    std::array<std::byte, kSecuredPayloadBytes> payload{};
};

/** Privacy-safe, fixed hook record. No address, pointer, packed handle, or raw stable digest. */
struct CompactEvidenceRecord final {
    std::uint64_t sequence{};
    std::uint64_t capture_epoch{};
    std::uint64_t cohort_id{};
    std::uint64_t activation_id{};
    std::uint64_t call_id{};
    std::uint64_t record_id{};
    std::uint64_t thread_id{};
    std::uint64_t entry_clock{};
    std::uint64_t exit_clock{};
    std::uint64_t entry_serial{};
    std::uint64_t exit_serial{};
    std::uint64_t authority_epoch{};
    std::uint64_t publication_epoch{};
    std::uint32_t copied_bytes{};
    std::uint16_t block_count{};
    LaneKind lane{LaneKind::count};
    NativeSurface surface{NativeSurface::count};
    RecordValidation validation{RecordValidation::partial};
    Sha256 keyed_record_digest{};
};

static_assert(std::is_trivially_copyable_v<RawEvidenceRecord>);
static_assert(std::is_trivially_copyable_v<CompactEvidenceRecord>);
static_assert(sizeof(CompactEvidenceRecord) <= 192U);

enum class QueueResult : std::uint8_t {
    success,
    empty,
    full,
    busy,
    closed,
    stale_epoch,
    sequence_exhausted,
};

template <typename Value, std::size_t Capacity> class BoundedMpmcQueue final {
    static_assert(Capacity >= 2U && (Capacity & (Capacity - 1U)) == 0U);

public:
    BoundedMpmcQueue() noexcept {
        reset_slots();
    }
    BoundedMpmcQueue(const BoundedMpmcQueue&) = delete;
    BoundedMpmcQueue& operator=(const BoundedMpmcQueue&) = delete;

    void open(std::uint64_t epoch) noexcept {
        epoch_.store(epoch, std::memory_order_release);
        accepting_.store(true, std::memory_order_release);
    }
    void close() noexcept {
        accepting_.store(false, std::memory_order_release);
    }

    [[nodiscard]] QueueResult try_push(std::uint64_t epoch, const Value& value) noexcept {
        if (!accepting_.load(std::memory_order_acquire)) {
            return QueueResult::closed;
        }
        if (epoch == 0U || epoch != epoch_.load(std::memory_order_acquire)) {
            return QueueResult::stale_epoch;
        }
        std::size_t position = enqueue_position_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = slots_[position & (Capacity - 1U)];
            const std::size_t sequence = slot.sequence.load(std::memory_order_acquire);
            const std::intptr_t difference = static_cast<std::intptr_t>(sequence)
                                             - static_cast<std::intptr_t>(position);
            if (difference == 0) {
                if (enqueue_position_.compare_exchange_weak(
                        position, position + 1U, std::memory_order_relaxed)) {
                    slot.value = value;
                    slot.sequence.store(position + 1U, std::memory_order_release);
                    count_.fetch_add(1U, std::memory_order_release);
                    return QueueResult::success;
                }
            } else if (difference < 0) {
                return QueueResult::full;
            } else {
                position = enqueue_position_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] QueueResult try_pop(Value& output) noexcept {
        std::size_t position = dequeue_position_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = slots_[position & (Capacity - 1U)];
            const std::size_t sequence = slot.sequence.load(std::memory_order_acquire);
            const std::intptr_t difference = static_cast<std::intptr_t>(sequence)
                                             - static_cast<std::intptr_t>(position + 1U);
            if (difference == 0) {
                if (dequeue_position_.compare_exchange_weak(
                        position, position + 1U, std::memory_order_relaxed)) {
                    output = slot.value;
                    slot.sequence.store(position + Capacity, std::memory_order_release);
                    count_.fetch_sub(1U, std::memory_order_release);
                    return QueueResult::success;
                }
            } else if (difference < 0) {
                return QueueResult::empty;
            } else {
                position = dequeue_position_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] bool secured_reset(std::uint64_t newEpoch) noexcept {
        if (accepting_.load(std::memory_order_acquire) || count_.load(std::memory_order_acquire) != 0U
            || reset_claim_.test_and_set(std::memory_order_acquire)) {
            return false;
        }
        reset_slots();
        epoch_.store(newEpoch, std::memory_order_release);
        reset_claim_.clear(std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return count_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool accepting() const noexcept {
        return accepting_.load(std::memory_order_acquire);
    }

private:
    struct Slot final {
        std::atomic<std::size_t> sequence{};
        Value value{};
    };

    void reset_slots() noexcept {
        enqueue_position_.store(0U, std::memory_order_relaxed);
        dequeue_position_.store(0U, std::memory_order_relaxed);
        count_.store(0U, std::memory_order_relaxed);
        for (std::size_t index = 0U; index < Capacity; ++index) {
            slots_[index].value = {};
            slots_[index].sequence.store(index, std::memory_order_relaxed);
        }
    }

    std::array<Slot, Capacity> slots_{};
    std::atomic<std::size_t> enqueue_position_{};
    std::atomic<std::size_t> dequeue_position_{};
    std::atomic<std::size_t> count_{};
    std::atomic<std::uint64_t> epoch_{};
    std::atomic_bool accepting_{};
    std::atomic_flag reset_claim_ = ATOMIC_FLAG_INIT;
};

struct CapturedSceneDerived final {
    std::uint32_t scalar{};
    std::uint8_t active{};
};

struct CapturedSceneLive final {
    std::uint64_t full_handle{};
    std::uint64_t handle_generation{};
    std::uint8_t terminal_latch{};
};

struct CapturedActorScalar final {
    std::uint32_t scheduler{};
    std::uint32_t entity{};
    std::uint64_t full_handle{};
    std::uint64_t record_generation{};
    std::uint64_t component_generation{};
    std::uint64_t authority_generation{};
    std::uint64_t factory_generation{};
    std::uint64_t source_row_generation{};
    std::uint64_t provider_generation{};
    std::uint64_t create_clock{};
    std::uint64_t terminal_clock{};
    std::uint64_t create_serial{};
    std::uint64_t terminal_serial{};
    std::uint32_t terminal_selector{};
    std::uint32_t spawner_key{};
    RootTransform root{};
    std::uint8_t rendered{};
    std::uint8_t pose_valid{};
};

struct CapturedProviderScalar final {
    std::uint64_t owner_actor_handle{};
    std::uint64_t owner_actor_generation{};
    std::uint64_t provider_handle{};
    std::uint64_t provider_generation{};
    std::uint64_t source_row_generation{};
    std::uint32_t candidate_key{};
    std::uint32_t selection{};
    std::uint32_t effect_definition{};
    std::uint8_t output_slot{};
    std::uint8_t valid{};
    RootTransform transform{};
};

enum class PolicyConsumerKind : std::uint8_t {
    none,
    elapsed_time_gate,
    type26_strict_scene_gate,
    forced_spawner,
    newest_or_wildcard,
    body_transfer,
};

struct CapturedPolicyConsumer final {
    PolicyConsumerKind kind{PolicyConsumerKind::none};
    std::uint64_t source_generation{};
    std::uint64_t target_generation{};
    std::uint64_t consumer_serial{};
};

enum class GroundedClassification : std::uint8_t {
    unknown,
    squad_predecessor_handoff,
    same_scene_wrapper_dwell,
    different_predecessor,
    authority_publication_skip,
};

struct EvidenceAssessment final {
    RecordValidation validation{RecordValidation::partial};
    GroundedClassification grounded{GroundedClassification::unknown};
    bool type26_closure_complete{};
    bool point_listener_closed{};
    bool scene_sequence_closed{};
    bool actor_provider_closed{};
    bool initial_carrier_concurrent{};
    bool true_successor_closed{};
    bool forbidden_behavior_observed{};
    bool duplicate_t_pose_observed{};
};

class CaptureOwner;

class CaptureTransaction final {
public:
    CaptureTransaction() noexcept = default;
    CaptureTransaction(CaptureTransaction&& other) noexcept;
    CaptureTransaction& operator=(CaptureTransaction&&) = delete;
    CaptureTransaction(const CaptureTransaction&) = delete;
    CaptureTransaction& operator=(const CaptureTransaction&) = delete;
    ~CaptureTransaction() noexcept;

    [[nodiscard]] bool admitted() const noexcept;
    [[nodiscard]] CopyOutcome copy(BlockRole role,
                                   CapturePhase phase,
                                   const NativeBlockView& view) noexcept;

    template <typename Function, typename... Arguments>
    [[nodiscard]] std::invoke_result_t<Function, Arguments...>
    invoke_original(Arguments&&... arguments) noexcept {
        static_assert(std::is_pointer_v<Function>);
        static_assert(std::is_nothrow_invocable_v<Function, Arguments...>);
        using Result = std::invoke_result_t<Function, Arguments...>;
        const std::uintptr_t address = original_address();
        if (address == 0U || original_invoked_) {
            original_error_ = true;
            if constexpr (!std::is_void_v<Result>) {
                return Result{};
            } else {
                return;
            }
        }
        original_invoked_ = true;
        ++record_.original_call_count;
        const Function original = std::bit_cast<Function>(address);
        if constexpr (std::is_void_v<Result>) {
            std::invoke(original, std::forward<Arguments>(arguments)...);
            original_returned_ = true;
            return;
        } else {
            Result result = std::invoke(original, std::forward<Arguments>(arguments)...);
            original_returned_ = true;
            return result;
        }
    }

    [[nodiscard]] QueueResult finish(std::uint64_t authorityEpoch = 0U,
                                     std::uint64_t publicationEpoch = 0U) noexcept;

private:
    CaptureTransaction(CaptureOwner& owner,
                       const ActivationHandle& activation,
                       LaneKind lane,
                       NativeSurface surface,
                       std::uintptr_t callerRva) noexcept;
    void release_without_commit() noexcept;
    [[nodiscard]] std::uintptr_t original_address() const noexcept;

    CaptureOwner* owner_{};
    RawEvidenceRecord record_{};
    bool gate_entered_{};
    bool observation_admitted_{};
    bool tls_pushed_{};
    bool original_invoked_{};
    bool original_returned_{};
    bool original_error_{};
    bool finished_{};
    friend class CaptureOwner;
};

enum class OwnerPhase : std::uint8_t {
    detached,
    admitted,
    originals_published,
    running,
    quiescing,
    removed_pending_reset,
};

enum class ProtectedRemovalDisposition : std::uint8_t {
    removed,
    deferred,
    failed,
};

enum class ProtectedRemovalResult : std::uint8_t {
    removed,
    active_calls,
    queues_not_drained,
    deferred,
    failed,
    wrong_phase,
};

class CaptureOwner final {
public:
    CaptureOwner() noexcept;
    CaptureOwner(const CaptureOwner&) = delete;
    CaptureOwner& operator=(const CaptureOwner&) = delete;

    [[nodiscard]] AdmissionResult admit_current_process() noexcept;
#if defined(SUNRISE_IKORA_GROUND_DWELL_CAPTURE_TEST)
    [[nodiscard]] AdmissionResult admit_artifact_fixture() noexcept;
#endif
    [[nodiscard]] bool publish_originals(
        const std::array<std::uintptr_t, kNativeSurfaceCount>& originals) noexcept;
    [[nodiscard]] bool start() noexcept;
    [[nodiscard]] ActivationHandle begin_activation(const ActivityIdentity& activity,
                                                    BehaviorCase behavior) noexcept;
    [[nodiscard]] CaptureTransaction begin_capture(const ActivationHandle& activation,
                                                   LaneKind lane,
                                                   NativeSurface surface,
                                                   std::uintptr_t callerRva = 0U) noexcept;

    [[nodiscard]] QueueResult try_pop_raw(LaneKind lane, RawEvidenceRecord& output) noexcept;
    [[nodiscard]] QueueResult try_pop_compact(CompactEvidenceRecord& output) noexcept;
    [[nodiscard]] RecordValidation validate_record(const RawEvidenceRecord& record) const noexcept;
    [[nodiscard]] EvidenceAssessment
    assess(std::span<const RawEvidenceRecord> records) const noexcept;

    [[nodiscard]] bool quiesce() noexcept;
    [[nodiscard]] ProtectedRemovalResult
    record_protected_removal(ProtectedRemovalDisposition disposition) noexcept;
    [[nodiscard]] bool finalize_reset() noexcept;

    [[nodiscard]] OwnerPhase phase() const noexcept;
    [[nodiscard]] std::uint64_t capture_epoch() const noexcept;
    [[nodiscard]] std::uint32_t behavior_mask() const noexcept;
    [[nodiscard]] std::uint32_t active_calls() const noexcept;
    [[nodiscard]] std::size_t raw_queue_size(LaneKind lane) const noexcept;
    [[nodiscard]] std::size_t compact_queue_size() const noexcept;

private:
    struct SurfaceGate final {
        std::atomic_bool accepting{};
        std::atomic<std::uint32_t> active{};
    };

    [[nodiscard]] AdmissionResult admit_artifacts(bool currentProcess) noexcept;
    [[nodiscard]] OpaqueToken issue_token(TokenDomain domain,
                                          std::uint64_t subject,
                                          std::uint64_t epoch) noexcept;
    [[nodiscard]] bool authentic(const OpaqueToken& token,
                                 TokenDomain domain,
                                 std::uint64_t subject,
                                 std::uint64_t epoch) const noexcept;
    [[nodiscard]] bool authentic_activation(const ActivationHandle& activation) const noexcept;
    [[nodiscard]] CopyOutcome copy_block(RawEvidenceRecord& record,
                                         BlockRole role,
                                         CapturePhase phase,
                                         const NativeBlockView& view) noexcept;
    [[nodiscard]] QueueResult commit(CaptureTransaction& transaction) noexcept;
    void enter_gate(NativeSurface surface, bool& admitted) noexcept;
    void leave_gate(NativeSurface surface) noexcept;
    [[nodiscard]] bool push_tls(const RawEvidenceRecord& record) noexcept;
    [[nodiscard]] bool tls_matches(const RawEvidenceRecord& record) const noexcept;
    void pop_tls(const RawEvidenceRecord& record) noexcept;
    [[nodiscard]] std::uintptr_t original(NativeSurface surface) const noexcept;
    [[nodiscard]] bool all_gates_idle() const noexcept;
    [[nodiscard]] bool queues_drained() const noexcept;
    void compact_from(const RawEvidenceRecord& raw, CompactEvidenceRecord& compact) noexcept;

    std::array<SurfaceGate, kNativeSurfaceCount> gates_{};
    std::array<std::atomic<std::uintptr_t>, kNativeSurfaceCount> originals_{};
    std::array<BoundedMpmcQueue<RawEvidenceRecord, 4U>, kLaneCount> raw_queues_{};
    BoundedMpmcQueue<CompactEvidenceRecord, 64U> compact_queue_{};
    std::array<std::atomic<std::uint32_t>, static_cast<std::size_t>(BehaviorCase::count)>
        behavior_lane_masks_{};
    std::array<std::uint64_t, 4U> secret_{};
    OpaqueToken cohort_token_{};
    std::atomic<OwnerPhase> phase_{OwnerPhase::detached};
    std::atomic<std::uint64_t> capture_epoch_{};
    std::atomic<std::uint64_t> token_serial_{1U};
    std::atomic<std::uint64_t> native_serial_{1U};
    std::atomic<std::uint64_t> compact_sequence_{1U};
    std::atomic<std::uint32_t> active_transactions_{};
    std::atomic<std::uint32_t> active_tls_frames_{};
    friend class CaptureTransaction;
};

[[nodiscard]] bool significant_bits_equal(std::span<const std::byte> left,
                                          std::span<const std::byte> right,
                                          std::size_t bits) noexcept;
[[nodiscard]] bool unused_low_bits_zero(std::span<const std::byte> bytes,
                                        std::size_t bits) noexcept;
[[nodiscard]] Sha256 sha256(std::span<const std::byte> bytes) noexcept;

} // namespace sunrise::client::hooks::bootflow::opening_authority::ikora_ground_dwell
