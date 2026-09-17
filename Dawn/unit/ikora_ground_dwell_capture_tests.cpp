#define DAWN_IKORA_GROUND_DWELL_CAPTURE_TEST 1

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

#include "client/hooks/bootflow/opening_authority/ikora_ground_dwell_capture.h"
#include "client/hooks/bootflow/opening_authority/ikora_ground_dwell_lifecycle.h"

using namespace dawn::client::hooks::bootflow::opening_authority::ikora_ground_dwell;

namespace {

int g_failure_count{};

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expression << '\n';    \
            ++g_failure_count;                                                                     \
        }                                                                                          \
    } while (false)

inline constexpr std::uint32_t kExecutableBegin = 0x1000U;
inline constexpr std::uint32_t kExecutableEnd = 0x01800000U;

[[nodiscard]] constexpr std::uint8_t hex_nibble(char value) noexcept {
    return value >= '0' && value <= '9'
               ? static_cast<std::uint8_t>(value - '0')
               : (value >= 'A' && value <= 'F' ? static_cast<std::uint8_t>(value - 'A' + 10)
                                               : static_cast<std::uint8_t>(value - 'a' + 10));
}

[[nodiscard]] Sha256 sha256_from_hex(std::string_view hex) noexcept {
    Sha256 output{};
    if (hex.size() != output.size() * 2U) {
        return output;
    }
    for (std::size_t index = 0U; index < output.size(); ++index) {
        output[index] = std::byte{static_cast<std::uint8_t>((hex_nibble(hex[index * 2U]) << 4U)
                                                            | hex_nibble(hex[index * 2U + 1U]))};
    }
    return output;
}

[[nodiscard]] RuntimeAdmissionEvidence exact_admission() noexcept {
    RuntimeAdmissionEvidence evidence{};
    evidence.packed_runtime = {kPinnedPackedRuntimeBytes, kPinnedPackedRuntimeSha256};
    evidence.mapped_image_base = 0x140000000ULL;
    evidence.mapped_image_bytes = kPinnedSizeOfImage;
    evidence.executable_rva_begin = kExecutableBegin;
    evidence.executable_rva_end = kExecutableEnd;
    evidence.pe_timestamp = kPinnedPeTimestamp;
    evidence.entry_rva = kPinnedEntryRva;
    evidence.machine = kPinnedMachine;
    evidence.section_count = kPinnedSectionCount;
    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeTarget target = native_target(static_cast<NativeSurface>(index));
        MappedPrefixObservation& observed = evidence.mapped_prefixes[index];
        observed.byte_count = static_cast<std::uint8_t>(target.mapped_prefix.size());
        std::copy(target.mapped_prefix.begin(), target.mapped_prefix.end(), observed.bytes.begin());
    }
    return evidence;
}

[[nodiscard]] CaptureLineage exact_lineage() noexcept {
    return {1U, 0x1001U, 0x2001U, 3U, 0x3001U, 4U, 5U, 0x400100U, 6U, 0x500100U, 7U, 16U, 17U};
}

[[nodiscard]] ActorIdentity actor_identity(std::uint32_t scheduler,
                                           std::uint64_t fullHandle,
                                           std::uintptr_t record,
                                           std::uint64_t recordGeneration,
                                           std::uintptr_t component,
                                           std::uint64_t componentGeneration,
                                           std::uint64_t factoryGeneration) noexcept {
    return {scheduler,
            kSharedActorEntity,
            fullHandle,
            packed_low_index(fullHandle),
            record,
            recordGeneration,
            component,
            componentGeneration,
            16U,
            factoryGeneration};
}

[[nodiscard]] ActorObservation actor_observation(const ActorIdentity& identity,
                                                 std::uint64_t createdTick,
                                                 std::uint64_t terminalTick,
                                                 std::uint64_t createdSerial,
                                                 std::uint64_t terminalSerial,
                                                 std::uintptr_t sourceRow,
                                                 std::uint64_t sourceRowGeneration,
                                                 std::uint64_t provider,
                                                 std::uint64_t providerGeneration) noexcept {
    ActorObservation actor{};
    actor.identity = identity;
    actor.source = ActorSource::scene;
    actor.spawner_key = kSceneSpawnerKey;
    actor.created_tick = createdTick;
    actor.terminal_tick = terminalTick;
    actor.created_serial = createdSerial;
    actor.terminal_serial = terminalSerial;
    actor.descriptor_hash = 0xD0000000U + identity.scheduler_tag;
    actor.descriptor_bytes = 0x50U;
    actor.source_row_identity = sourceRow;
    actor.source_row_generation = sourceRowGeneration;
    actor.provider_full_handle = provider;
    actor.provider_generation = providerGeneration;
    actor.root = kExactSharedRootTransform;
    return actor;
}

[[nodiscard]] CastBindingObservation exact_cast() noexcept {
    return {kActorCastClass,
            kActorCastBinding,
            kSquadReference,
            kSquadDefinition,
            kSquadKind,
            kSharedActorEntity,
            kTimelineCastClass,
            kTimelineCastBinding,
            kTimelineReference,
            kTimelineDefinition,
            kTimelinePlacement,
            0x91FAA008U,
            17U,
            0x92FAA009U,
            18U,
            19U,
            kExactSharedRootTransform};
}

[[nodiscard]] GroundDwellCaptureRecord exact_record() noexcept {
    GroundDwellCaptureRecord record{};
    record.metadata = {0U, 140'000U, 0xABCDEFU, 31U};
    record.lineage = exact_lineage();

    record.point_listener.point = kVignettePoint;
    record.point_listener.authority = {
        15U, 0x310001U, 0x310002U, kType31AuthorityBits, kUnsetGeneration, 22U, 23U, 1U, true};
    record.point_listener.apply_serial = 10U;
    record.point_listener.predicate_serial = 20U;
    record.point_listener.terminal_serial = 30U;
    record.point_listener.listener_enumeration_serial = 40U;
    record.point_listener.local_player_full_handle = 0x52FAA005U;
    record.point_listener.local_player_generation = 24U;
    record.point_listener.volume_membership_generation = 25U;
    record.point_listener.local_player_membership_bit = true;
    record.point_listener.incident_root_identity = 0x610000U;
    record.point_listener.incident_root_generation = 26U;
    record.point_listener.incident_root_hash = 0x610001U;
    record.point_listener.listener_row_identity = 0x620000U;
    record.point_listener.listener_row_generation = 27U;
    record.point_listener.listener_row_hash = 0x620001U;
    record.point_listener.listener_callback_rva = 0xD90000U;
    record.point_listener.incident_manager_generation = 28U;

    SceneAuthorityObservation authority{};
    authority.scene = kSceneReference;
    authority.schema = kSceneAuthoritySchema;
    authority.definition = kSceneDefinition;
    authority.authority_generation = record.lineage.authority_generation;
    authority.publication_epoch = record.lineage.publication_epoch;
    authority.publication_serial = 50U;
    authority.body_hash = 0x430001U;
    authority.decoded_hash = 0x430002U;
    authority.entry_count = 1U;
    authority.word_count = 2U;
    authority.significant_bits = kSceneAuthorityMinimumBits + 55U + 64U;
    authority.old_effective_root = 4U;
    authority.effective_root = 6U;
    authority.committed_root = 5U;
    authority.nested_scalar = 0x20U;
    authority.entries_hash = 0x430003U;
    authority.words_hash = 0x430004U;
    authority.live_scene_full_handle = 0x18FAA010U;
    authority.live_scene_handle_generation = 29U;
    authority.authority_active = 1U;
    authority.terminal_latch = 0U;
    authority.native_decode_succeeded = true;
    authority.private_encode_deterministic = true;
    authority.significant_bits_round_trip = true;

    record.point_listener.resulting_publication = {kSceneReference,
                                                   kSceneAuthoritySchema,
                                                   authority.authority_generation,
                                                   authority.publication_epoch,
                                                   authority.publication_serial,
                                                   authority.body_hash,
                                                   authority.significant_bits};
    record.point_publication_relation = AuthorityPublicationRelation::direct_scene_start_authority;

    const ActorIdentity initialIdentity = actor_identity(
        kInitialVisibleSchedulerTag, kFrozenInitialFullHandle, 0x710000U, 31U, 0x711000U, 41U, 51U);
    const ActorIdentity carrierIdentity = actor_identity(kConcurrentCarrierSchedulerTag,
                                                         kFrozenCarrierFullHandle,
                                                         0x720000U,
                                                         32U,
                                                         0x721000U,
                                                         42U,
                                                         52U);
    const ActorIdentity successorIdentity = actor_identity(kTrueSuccessorSchedulerTag,
                                                           kFrozenSuccessorFullHandle,
                                                           0x730000U,
                                                           33U,
                                                           0x731000U,
                                                           43U,
                                                           53U);

    const ActorObservation initial = actor_observation(initialIdentity,
                                                       kFrozenInitialAndCarrierBornTick,
                                                       kFrozenInitialTerminalTick,
                                                       82U,
                                                       120U,
                                                       0x740000U,
                                                       61U,
                                                       0x810001U,
                                                       71U);
    const ActorObservation carrier = actor_observation(carrierIdentity,
                                                       kFrozenInitialAndCarrierBornTick,
                                                       kFrozenCarrierTerminalTick,
                                                       84U,
                                                       150U,
                                                       0x750000U,
                                                       62U,
                                                       0x820001U,
                                                       72U);
    const ActorObservation successor = actor_observation(
        successorIdentity, kFrozenSuccessorBornTick, 0U, 130U, 0U, 0x760000U, 63U, 0x830001U, 73U);

    record.scene_start.authority = authority;
    record.scene_start.cast = exact_cast();
    record.scene_start.reconcile_serial = 60U;
    record.scene_start.root_advance_start_serial = 70U;
    record.scene_start.start_effective_root = authority.effective_root;
    record.scene_start.initial_visible = {initial, 70U, 80U, 81U, 82U, 0U};
    record.scene_start.concurrent_carrier = {carrier, 70U, 80U, 83U, 84U, 1U};

    record.chronology = {
        initial, carrier, successor, kTerminalSelector, true, true, true, false, false};
    record.successor_creation = {successor, 70U, 125U, 127U, 130U, 2U};

    record.countdown.owner = initialIdentity;
    record.countdown.source_row_identity = initial.source_row_identity;
    record.countdown.source_row_generation = initial.source_row_generation;
    record.countdown.transition_tick_serial = 110U;
    record.countdown.transition_ordinal = 4U;
    record.countdown.delta_seconds = 0.25F;
    record.countdown.remaining_before = 1.0F;
    record.countdown.remaining_after = 0.75F;
    record.countdown.row_callback_rva = 0x58E790U;
    record.countdown.row_expired = false;
    record.countdown.owns_local_actor_countdown = true;
    record.countdown.claims_ground_dwell = false;
    record.countdown.used_as_authority_gate = false;

    record.retail_grounded.classification = GroundedIdentityClassification::unknown;
    record.retail_grounded.grade = EvidenceGrade::unknown;
    record.original_forwarded_exactly_once = true;
    return record;
}

void module_is_permanently_observation_only() {
    CHECK(kObservationOnly);
    CHECK(!kOwnsNativeDetour);
    CHECK(!kPerformsIo);
    CHECK(!kProvidesAuthorityWriter);
    CHECK(!kProvidesSceneWriter);
    CHECK(!kProvidesActorWriter);
    CHECK(!kProvidesSpawnerWriter);
    CHECK(!kProvidesTransitionWriter);
    CHECK(!kProvidesElapsedTimeGate);
    CHECK(!kProvidesType26StrictGate);
    CHECK(!kProvidesBodyTransfer);
    CHECK(!kDefaultTelemetryContainsRawBytes);
    CHECK(kRetailGroundedIdentityGrade == EvidenceGrade::unknown);
    CHECK(!kRetailGroundedFullHandleKnown);
    CHECK(!kRetailGroundedProviderOwnerKnown);
    CHECK(!kType26StrictSceneGateKnown);
    CHECK(!kType31StrictSceneGateKnown);
    CHECK(!kSuccessorSourceRowKnown);
}

void exact_packed_and_mapped_cohort_is_required() {
    RuntimeAdmissionEvidence evidence = exact_admission();
    CHECK(matches_pinned_packed_runtime(evidence.packed_runtime));
    CHECK(!matches_pinned_unpacked_provenance(UnpackedProvenanceIdentity{
        evidence.packed_runtime.file_bytes, evidence.packed_runtime.sha256}));
    CHECK(validate_runtime_admission(evidence) == RuntimeAdmissionResult::admitted);

    RuntimeAdmissionEvidence wrong = evidence;
    wrong.packed_runtime.sha256 = kPinnedUnpackedProvenanceSha256;
    CHECK(validate_runtime_admission(wrong) == RuntimeAdmissionResult::packed_identity_mismatch);

    wrong = evidence;
    wrong.mapped_image_bytes -= 0x1000U;
    CHECK(validate_runtime_admission(wrong) == RuntimeAdmissionResult::mapped_pe_mismatch);

    wrong = evidence;
    wrong.executable_rva_end = 0xD82B60U;
    CHECK(validate_runtime_admission(wrong) == RuntimeAdmissionResult::target_out_of_range);

    wrong = evidence;
    wrong.mapped_prefixes[static_cast<std::size_t>(NativeSurface::scene_reconcile_commit)]
        .bytes[31] ^= std::byte{1U};
    CHECK(validate_runtime_admission(wrong) == RuntimeAdmissionResult::prefix_mismatch);

    wrong = evidence;
    --wrong.mapped_prefixes[static_cast<std::size_t>(NativeSurface::entity_factory_thunk)]
          .byte_count;
    CHECK(validate_runtime_admission(wrong) == RuntimeAdmissionResult::prefix_mismatch);

    CHECK(native_target(NativeSurface::scene_reconcile_commit).mapped_rva == 0xB41330U);
    CHECK(native_target(NativeSurface::scene_root_advance_start).mapped_rva == 0xB43220U);
    CHECK(native_target(NativeSurface::actor_slot_scheduler).mapped_rva == 0x5902C0U);
    CHECK(native_target(NativeSurface::entity_factory_thunk).mapped_rva == 0x56D990U);
    CHECK(native_target(NativeSurface::entity_factory).mapped_rva == 0x56D9B0U);
    CHECK(native_target(NativeSurface::actor_local_transition_tick).mapped_rva == 0x58FFD0U);
    CHECK(native_target(NativeSurface::incident_dynamic_listener_enumeration).mapped_rva
          == 0xD82B60U);
    CHECK(native_target(NativeSurface::entity_factory_thunk).mapped_prefix.size() == 31U);
    CHECK(native_target(NativeSurface::scene_reconcile_commit).function_hash_recovered);
    CHECK(!native_target(NativeSurface::type31_predicate).function_hash_recovered);

    struct ExpectedHashedTarget final {
        NativeSurface surface{};
        std::uintptr_t rva{};
        std::uint32_t bytes{};
        std::string_view sha256{};
    };
    constexpr std::array<ExpectedHashedTarget, 8U> expected{{
        {NativeSurface::scene_authority_apply,
         0xB41DD0U,
         0x5EU,
         "91330D155F8FE2F3EA4DC511D7AA44537FEA5F4E58EF92E84ECDCA641B4A7E39"},
        {NativeSurface::scene_reconcile_commit,
         0xB41330U,
         0x1AFU,
         "17679B6C99F72A813851F3C630690FEC0C7D725ED0A1CAB7957E9C3106491C5B"},
        {NativeSurface::scene_root_advance_start,
         0xB43220U,
         0x284U,
         "8CF74A90055B264E298712F629407D360E665172E558DEF631908A80E9820D33"},
        {NativeSurface::actor_slot_scheduler,
         0x5902C0U,
         0x2E8U,
         "194F23AF75297503D954A21CBD627187C294C44942F7F37F211A2EEA5ABAE595"},
        {NativeSurface::entity_factory_thunk,
         0x56D990U,
         0x1FU,
         "7462906CAAFEBF3B88A6C6EA001611343F4AAB4CB30388AAB405278800700A48"},
        {NativeSurface::entity_factory,
         0x56D9B0U,
         0x448U,
         "61FD5D85E9BB99D1E5C976C74F659406B9CCF605298AA8C796E07B1C193D2093"},
        {NativeSurface::actor_local_transition_tick,
         0x58FFD0U,
         0x2EDU,
         "435586E0DF8ED4CC0061A2D52FC1848BBE81D26A45D35D6551F5DD84E0F9FE0E"},
        {NativeSurface::actor_transition_terminal,
         0x58B9A0U,
         0x1B0U,
         "8193409DCDE25B37FFDB3D1007CE716FE2610F5A509031EDEA81DCBFC9E77ECD"},
    }};
    for (const ExpectedHashedTarget& item : expected) {
        const NativeTarget target = native_target(item.surface);
        CHECK(target.mapped_rva == item.rva);
        CHECK(target.recovered_function_bytes == item.bytes);
        CHECK(target.function_hash_recovered);
        if (target.recovered_function_sha256 != sha256_from_hex(item.sha256)) {
            std::cerr << "function hash mismatch at RVA 0x" << std::hex << item.rva << std::dec
                      << '\n';
        }
        CHECK(target.recovered_function_sha256 == sha256_from_hex(item.sha256));
    }

    struct ExpectedPrefixOnlyTarget final {
        NativeSurface surface{};
        std::uintptr_t rva{};
        std::uint32_t bytes{};
    };
    constexpr std::array<ExpectedPrefixOnlyTarget, 4U> prefixOnly{{
        {NativeSurface::type31_authority_apply, 0xB20640U, 0x4AU},
        {NativeSurface::type31_predicate, 0xB20B00U, 0x346U},
        {NativeSurface::type31_terminal, 0xB20820U, 0x1B5U},
        {NativeSurface::incident_dynamic_listener_enumeration, 0xD82B60U, 0x2FCU},
    }};
    for (const ExpectedPrefixOnlyTarget& item : prefixOnly) {
        const NativeTarget target = native_target(item.surface);
        CHECK(target.mapped_rva == item.rva);
        CHECK(target.recovered_function_bytes == item.bytes);
        CHECK(!target.function_hash_recovered);
        CHECK(target.recovered_function_sha256 == Sha256{});
    }
}

void exact_listener_publication_and_scene_start_chain_validate() {
    GroundDwellCaptureRecord record = exact_record();
    CHECK(validate_capture_record(record, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::valid);
    CHECK(record.point_listener.resulting_publication.publication_epoch
          == record.lineage.publication_epoch);
    CHECK(record.scene_start.authority.effective_root
          > record.scene_start.authority.committed_root);
    CHECK(record.scene_start.reconcile_serial < record.scene_start.root_advance_start_serial);
    CHECK(record.scene_start.root_advance_start_serial
          < record.scene_start.initial_visible.materialize_serial);
    CHECK(record.scene_start.initial_visible.materialize_serial
          < record.scene_start.initial_visible.factory_thunk_serial);
    CHECK(record.scene_start.initial_visible.factory_thunk_serial
          < record.scene_start.initial_visible.factory_serial);

    GroundDwellCaptureRecord preceding = record;
    preceding.point_publication_relation =
        AuthorityPublicationRelation::precedes_scene_start_authority;
    preceding.point_listener.resulting_publication = {
        {kActivityRegistry, 53U, 2U}, 0x80804F77U, 15U, 16U, 45U, 0x530002U, 100U};
    CHECK(validate_capture_record(preceding, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::valid);

    preceding.point_publication_relation =
        AuthorityPublicationRelation::direct_scene_start_authority;
    CHECK(validate_capture_record(preceding, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::publication_mismatch);

    GroundDwellCaptureRecord wrong = record;
    wrong.point_listener.local_player_membership_bit = false;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_point_listener);

    wrong = record;
    wrong.point_listener.listener_row_generation = 0U;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_point_listener);

    wrong = record;
    wrong.point_listener.listener_callback_rva = kExecutableEnd;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_point_listener);

    wrong = record;
    ++wrong.point_listener.resulting_publication.publication_epoch;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_point_listener);

    wrong = record;
    ++wrong.scene_start.authority.body_hash;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::publication_mismatch);

    wrong = record;
    wrong.scene_start.reconcile_serial = wrong.scene_start.authority.publication_serial;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_start);

    wrong = record;
    wrong.scene_start.initial_visible.factory_thunk_serial =
        wrong.scene_start.initial_visible.materialize_serial;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_start);
}

void scene_shape_cast_and_full_generations_are_strict() {
    GroundDwellCaptureRecord record = exact_record();
    CHECK(valid_scene_authority(record.scene_start.authority, record.lineage));
    CHECK(valid_cast_binding(record.scene_start.cast));
    CHECK(exact_root_transform(record.scene_start.cast.timeline_root));

    GroundDwellCaptureRecord wrong = record;
    wrong.scene_start.authority.significant_bits = 37U;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_authority);

    wrong = record;
    wrong.scene_start.authority.entry_count = 9U;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_authority);

    wrong = record;
    wrong.scene_start.authority.private_encode_deterministic = false;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_authority);

    wrong = record;
    wrong.scene_start.cast.actor_binding_id ^= 1U;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_start);

    wrong = record;
    wrong.scene_start.cast.resolved_actor_source_generation = 0U;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_start);

    wrong = record;
    wrong.scene_start.concurrent_carrier.actor.root.position_x += 1.0F;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_scene_start);

    wrong = record;
    wrong.chronology.true_successor.identity.object_record_generation =
        wrong.chronology.initial_visible.identity.object_record_generation;
    wrong.successor_creation.actor.identity.object_record_generation =
        wrong.chronology.true_successor.identity.object_record_generation;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_chronology);
}

void carrier_is_concurrent_and_fa6_is_the_true_successor() {
    GroundDwellCaptureRecord record = exact_record();
    CHECK(valid_actor_chronology(record.chronology));
    CHECK(matches_frozen_current_chronology(record.chronology));
    CHECK(record.chronology.initial_visible.created_tick
          == record.chronology.concurrent_carrier.created_tick);
    CHECK(record.chronology.true_successor.created_tick
              - record.chronology.initial_visible.terminal_tick
          == kFrozenSuccessorGapMs);
    CHECK(record.chronology.initial_visible.identity.packed_low_index
          == record.chronology.true_successor.identity.packed_low_index);
    CHECK(record.chronology.initial_visible.identity.packed_full_handle
          != record.chronology.true_successor.identity.packed_full_handle);
    CHECK(record.chronology.concurrent_carrier.identity.scheduler_tag
          == kConcurrentCarrierSchedulerTag);
    CHECK(record.chronology.true_successor.identity.scheduler_tag == kTrueSuccessorSchedulerTag);

    GroundDwellCaptureRecord wrong = record;
    wrong.chronology.true_successor.identity.scheduler_tag = kConcurrentCarrierSchedulerTag;
    wrong.successor_creation.actor.identity.scheduler_tag = kConcurrentCarrierSchedulerTag;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_chronology);

    wrong = record;
    wrong.chronology.claims_initial_to_carrier_transfer = true;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_chronology);

    wrong = record;
    wrong.chronology.claims_low_index_continuity = true;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_chronology);

    wrong = record;
    wrong.chronology.initial_and_carrier_simultaneous = false;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_chronology);
}

void countdown_is_local_ownership_not_a_dwell_or_authority_gate() {
    GroundDwellCaptureRecord record = exact_record();
    CHECK(valid_countdown_observation(record.countdown, kExecutableBegin, kExecutableEnd));
    CHECK(record.countdown.owns_local_actor_countdown);
    CHECK(!record.countdown.claims_ground_dwell);
    CHECK(!record.countdown.used_as_authority_gate);

    GroundDwellCaptureRecord wrong = record;
    wrong.countdown.remaining_after = 0.5F;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_countdown);

    wrong = record;
    wrong.countdown.source_row_identity = 0U;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_countdown);

    wrong = record;
    wrong.countdown.claims_ground_dwell = true;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_countdown);

    wrong = record;
    wrong.countdown.used_as_authority_gate = true;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::invalid_countdown);
}

void forbidden_policy_recipes_and_retail_overclaim_are_rejected() {
    GroundDwellCaptureRecord record = exact_record();

    auto expect_forbidden = [&](ForbiddenInferenceClaims claims) noexcept {
        GroundDwellCaptureRecord wrong = record;
        wrong.forbidden_claims = claims;
        CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
              == CaptureValidationResult::forbidden_inference);
    };
    ForbiddenInferenceClaims claims{};
    claims.initial_to_carrier_body_transfer = true;
    expect_forbidden(claims);
    claims = {};
    claims.elapsed_time_gate = true;
    expect_forbidden(claims);
    claims = {};
    claims.type26_strict_scene_gate = true;
    expect_forbidden(claims);
    claims = {};
    claims.forced_squad_spawner = true;
    expect_forbidden(claims);
    claims = {};
    claims.duplicate_t_pose_accepted = true;
    expect_forbidden(claims);
    claims = {};
    claims.carrier_is_visible_successor = true;
    expect_forbidden(claims);
    claims = {};
    claims.pointer_only_identity = true;
    expect_forbidden(claims);
    claims = {};
    claims.low_index_only_identity = true;
    expect_forbidden(claims);
    claims = {};
    claims.newest_or_wildcard_matching = true;
    expect_forbidden(claims);

    GroundDwellCaptureRecord wrong = record;
    wrong.retail_grounded.classification = GroundedIdentityClassification::same_scene_wrapper_dwell;
    wrong.retail_grounded.grade = EvidenceGrade::inferred;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::retail_identity_overclaimed);

    wrong = record;
    wrong.retail_grounded.actor = record.chronology.initial_visible.identity;
    CHECK(validate_capture_record(wrong, kExecutableBegin, kExecutableEnd)
          == CaptureValidationResult::retail_identity_overclaimed);
}

void telemetry_is_fixed_scalar_and_hash_only() {
    GroundDwellCaptureRecord record = exact_record();
    record.metadata.queue_sequence = 44U;
    const TelemetryProjection projection = project_telemetry(record);
    CHECK(std::is_trivially_copyable_v<TelemetryProjection>);
    CHECK(projection.queue_sequence == 44U);
    CHECK(projection.capture_epoch == record.lineage.capture_epoch);
    CHECK(projection.point_result_publication_epoch
          == record.point_listener.resulting_publication.publication_epoch);
    CHECK(projection.scene_publication_epoch == record.lineage.publication_epoch);
    CHECK(projection.initial_full_handle == kFrozenInitialFullHandle);
    CHECK(projection.carrier_full_handle == kFrozenCarrierFullHandle);
    CHECK(projection.successor_full_handle == kFrozenSuccessorFullHandle);
    CHECK(projection.listener_row_hash == record.point_listener.listener_row_hash);
    CHECK(projection.scene_body_hash == record.scene_start.authority.body_hash);
    CHECK(projection.actor_cast_source_full_handle
          == record.scene_start.cast.resolved_actor_source_full_handle);
    CHECK(projection.actor_cast_source_generation
          == record.scene_start.cast.resolved_actor_source_generation);
    CHECK(projection.timeline_cast_full_handle
          == record.scene_start.cast.resolved_timeline_full_handle);
    CHECK(projection.timeline_cast_generation
          == record.scene_start.cast.resolved_timeline_generation);
    CHECK(projection.initial_transform_hash == transform_hash(kExactSharedRootTransform));
    CHECK(projection.countdown_delta_bits == std::bit_cast<std::uint32_t>(0.25F));
    CHECK(projection.countdown_before_bits == std::bit_cast<std::uint32_t>(1.0F));
    CHECK(projection.countdown_after_bits == std::bit_cast<std::uint32_t>(0.75F));
    CHECK(projection.initial_scheduler_tag == kInitialVisibleSchedulerTag);
    CHECK(projection.carrier_scheduler_tag == kConcurrentCarrierSchedulerTag);
    CHECK(projection.successor_scheduler_tag == kTrueSuccessorSchedulerTag);
}

void fixed_queue_is_fifo_try_only_and_accounted() {
    CaptureQueue queue{};
    GroundDwellCaptureRecord record = exact_record();

    queue.hold_claim_for_test();
    CHECK(queue.try_push(record, kExecutableBegin, kExecutableEnd) == QueuePushResult::busy);
    queue.release_claim_for_test();

    GroundDwellCaptureRecord invalid = record;
    invalid.forbidden_claims.elapsed_time_gate = true;
    CHECK(queue.try_push(invalid, kExecutableBegin, kExecutableEnd) == QueuePushResult::invalid);

    for (std::size_t index = 0U; index < kCaptureQueueCapacity; ++index) {
        record.metadata.monotonic_tick = 140'000U + index;
        CHECK(queue.try_push(record, kExecutableBegin, kExecutableEnd)
              == QueuePushResult::published);
    }
    CHECK(queue.size() == kCaptureQueueCapacity);
    CHECK(queue.try_push(record, kExecutableBegin, kExecutableEnd) == QueuePushResult::full);

    GroundDwellCaptureRecord output{};
    for (std::size_t index = 0U; index < kCaptureQueueCapacity; ++index) {
        CHECK(queue.try_pop(output));
        CHECK(output.metadata.queue_sequence == index + 1U);
        CHECK(output.metadata.monotonic_tick == 140'000U + index);
    }
    CHECK(!queue.try_pop(output));
    CHECK(queue.size() == 0U);
    QueueCounters counters = queue.counters();
    CHECK(counters.published == kCaptureQueueCapacity);
    CHECK(counters.invalid == 1U);
    CHECK(counters.busy == 1U);
    CHECK(counters.full == 1U);

    CHECK(queue.try_reset());
    queue.set_next_sequence_for_test((std::numeric_limits<std::uint64_t>::max)());
    CHECK(queue.try_push(record, kExecutableBegin, kExecutableEnd)
          == QueuePushResult::sequence_exhausted);
    counters = queue.counters();
    CHECK(counters.sequence_exhausted == 1U);
    CHECK(queue.try_reset());
    CHECK(queue.counters().published == 0U);
}

int g_original_calls{};
int g_before_calls{};
int g_after_calls{};
FullCallGate* g_gate_to_quiesce{};

void fake_void_original(int& value) noexcept {
    ++g_original_calls;
    ++value;
    if (g_gate_to_quiesce != nullptr) {
        g_gate_to_quiesce->quiesce();
    }
}

int fake_value_original(int value) noexcept {
    ++g_original_calls;
    return value + 7;
}

void original_once_and_call_entry_admission_are_preserved() {
    FullCallGate gate{};
    gate.accept();
    g_original_calls = 0;
    g_before_calls = 0;
    g_after_calls = 0;
    int value = 4;
    forward_void_original_once(
        gate,
        &fake_void_original,
        []() noexcept { ++g_before_calls; },
        []() noexcept { ++g_after_calls; },
        value);
    CHECK(value == 5);
    CHECK(g_original_calls == 1);
    CHECK(g_before_calls == 1);
    CHECK(g_after_calls == 1);
    CHECK(gate.idle());

    gate.quiesce();
    forward_void_original_once(
        gate,
        &fake_void_original,
        []() noexcept { ++g_before_calls; },
        []() noexcept { ++g_after_calls; },
        value);
    CHECK(value == 6);
    CHECK(g_original_calls == 2);
    CHECK(g_before_calls == 1);
    CHECK(g_after_calls == 1);

    gate.accept();
    g_gate_to_quiesce = &gate;
    forward_void_original_once(
        gate,
        &fake_void_original,
        []() noexcept { ++g_before_calls; },
        []() noexcept { ++g_after_calls; },
        value);
    g_gate_to_quiesce = nullptr;
    CHECK(g_original_calls == 3);
    CHECK(g_before_calls == 2);
    CHECK(g_after_calls == 2);
    CHECK(!gate.accepting());

    const int result = forward_value_original_once(
        gate,
        &fake_value_original,
        []() noexcept { ++g_before_calls; },
        []() noexcept { ++g_after_calls; },
        10);
    CHECK(result == 17);
    CHECK(g_original_calls == 4);
    CHECK(g_before_calls == 2);
    CHECK(g_after_calls == 2);
}

void exact_nested_correlation_and_failure_retaining_lifecycle() {
    CorrelationArmState arm{};
    const CorrelationArmToken token{1U, 3U, 15U, 26U, 0xABCDEFU};
    CHECK(arm.arm(token));
    CHECK(!arm.arm(token));
    CHECK(arm.matches(token));
    CorrelationArmToken wrong = token;
    ++wrong.incident_root_generation;
    CHECK(!arm.matches(wrong));
    CHECK(!arm.disarm(2U));
    CHECK(arm.disarm(token.nonce));
    CHECK(!arm.armed());

    HookGroupState lifecycle{};
    std::array<std::uintptr_t, kNativeSurfaceCount> originals{};
    for (std::size_t index = 0U; index < originals.size(); ++index) {
        originals[index] = 0x100000U + index * 0x1000U;
    }
    CHECK(lifecycle.begin_install());
    CHECK(lifecycle.complete_install(true, 9U, originals));
    HookGroupSnapshot snapshot = lifecycle.snapshot();
    CHECK(snapshot.phase == HookGroupPhase::running);
    CHECK(snapshot.capture_generation_valid);
    CHECK(snapshot.originals == originals);
    CHECK(lifecycle.quiesce());
    snapshot = lifecycle.snapshot();
    CHECK(snapshot.phase == HookGroupPhase::quiescing);
    CHECK(!snapshot.capture_generation_valid);
    CHECK(lifecycle.record_protected_detach(false, ProtectedDetachDisposition::removed)
          == ProtectedDetachResult::protected_calls_active);
    CHECK(lifecycle.record_protected_detach(true, ProtectedDetachDisposition::deferred)
          == ProtectedDetachResult::adapter_deferred);
    CHECK(lifecycle.snapshot().originals == originals);
    CHECK(lifecycle.record_protected_detach(true, ProtectedDetachDisposition::failed)
          == ProtectedDetachResult::adapter_failed);
    CHECK(lifecycle.snapshot().originals == originals);
    CHECK(lifecycle.record_protected_detach(true, ProtectedDetachDisposition::removed)
          == ProtectedDetachResult::removed);
    CHECK(!lifecycle.finalize_reset(false));
    CHECK(lifecycle.snapshot().originals == originals);
    CHECK(lifecycle.finalize_reset(true));
    snapshot = lifecycle.snapshot();
    CHECK(snapshot.phase == HookGroupPhase::detached);
    CHECK((snapshot.originals == std::array<std::uintptr_t, kNativeSurfaceCount>{}));

    originals[1] = originals[0];
    CHECK(lifecycle.begin_install());
    CHECK(!lifecycle.complete_install(true, 10U, originals));
    CHECK(lifecycle.rollback_install());
}

} // namespace

int main() {
    module_is_permanently_observation_only();
    exact_packed_and_mapped_cohort_is_required();
    exact_listener_publication_and_scene_start_chain_validate();
    scene_shape_cast_and_full_generations_are_strict();
    carrier_is_concurrent_and_fa6_is_the_true_successor();
    countdown_is_local_ownership_not_a_dwell_or_authority_gate();
    forbidden_policy_recipes_and_retail_overclaim_are_rejected();
    telemetry_is_fixed_scalar_and_hash_only();
    fixed_queue_is_fifo_try_only_and_accounted();
    original_once_and_call_entry_admission_are_preserved();
    exact_nested_correlation_and_failure_retaining_lifecycle();

    if (g_failure_count != 0) {
        std::cerr << g_failure_count << " Ikora ground-dwell capture check(s) failed\n";
        return 1;
    }
    std::cout << "all Ikora ground-dwell capture checks passed\n";
    return 0;
}
