#define DAWN_GHOST_VM_BRIDGE_UNIT_TEST 1
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "../src/client/hooks/bootflow/opening_authority/ghost_vm_bridge_capture.h"

namespace {

using namespace dawn::client::hooks::bootflow::opening_authority::ghost_vm_bridge_capture;

int g_failures = 0;
BuildProvenanceSnapshot g_verifiedPcBuild{};

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " #expression "\n";       \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (false)

[[nodiscard]] constexpr std::uint32_t bit(Type60Field field) noexcept {
    return static_cast<std::uint32_t>(field);
}

[[nodiscard]] constexpr std::uint32_t bit(DynamicField field) noexcept {
    return static_cast<std::uint32_t>(field);
}

[[nodiscard]] constexpr std::uint32_t bit(AuthorityField field) noexcept {
    return static_cast<std::uint32_t>(field);
}

[[nodiscard]] constexpr std::uint32_t bit(RecordZeroField field) noexcept {
    return static_cast<std::uint32_t>(field);
}

[[nodiscard]] constexpr std::uint32_t bit(ConsumerCorrelationField field) noexcept {
    return static_cast<std::uint32_t>(field);
}

[[nodiscard]] constexpr std::uint32_t bit(VoiceStateField field) noexcept {
    return static_cast<std::uint32_t>(field);
}

[[nodiscard]] Sha256 digest(std::string_view text) noexcept {
    return sha256(std::as_bytes(std::span{text.data(), text.size()}));
}

[[nodiscard]] CaptureContext pc_context(std::uint64_t callId = 10U) noexcept {
    CaptureContext context{};
    context.presence_mask =
        kCompleteContextMask & ~static_cast<std::uint32_t>(ContextField::build_provenance);
    context.origin = CaptureOrigin::pc_client;
    context.session_pseudonym = 1U;
    context.patch_epoch = 2U;
    context.destination = kOmegaScenario;
    context.roster_generation = 3U;
    context.activity_instance_generation = 4U;
    context.thread_id = 5U;
    context.call_id = callId;
    context.monotonic_tick = 6U + callId;
    context.return_rva = 0x100A34EU;
    if (g_verifiedPcBuild.status() == BuildProvenanceStatus::exact_pinned_bytes_verified) {
        context.build = g_verifiedPcBuild;
        context.presence_mask |= static_cast<std::uint32_t>(ContextField::build_provenance);
    }
    return context;
}

[[nodiscard]] CaptureContext host_context(std::uint64_t callId = 20U) noexcept {
    CaptureContext context = pc_context(callId);
    context.origin = CaptureOrigin::retail_host_external_boundary;
    context.return_rva = 0x1234U;
    context.build = {};
    context.presence_mask &= ~static_cast<std::uint32_t>(ContextField::build_provenance);
    if (!bind_external_build_digest(context, {0xA11CEU, digest("external retail host build")})) {
        ++g_failures;
    }
    return context;
}

[[nodiscard]] Type60Evidence containment(OverlapObservationKind kind,
                                         bool inside,
                                         std::uint64_t callId = 10U,
                                         std::uint64_t player = 0xA0U) noexcept {
    Type60Evidence evidence{};
    evidence.header.context = pc_context(callId);
    evidence.header.phase = CapturePhase::type60_containment;
    evidence.presence_mask =
        bit(Type60Field::exact_identity) | bit(Type60Field::instance_pseudonym)
        | bit(Type60Field::record_pseudonym) | bit(Type60Field::subject_pseudonym)
        | bit(Type60Field::player_pseudonym) | bit(Type60Field::overlap_kind)
        | bit(Type60Field::world_latch) | bit(Type60Field::player_present_latch)
        | bit(Type60Field::inside_result);
    evidence.instance_pseudonym = 0x1000U;
    evidence.record_pseudonym = 0x2000U;
    evidence.subject_pseudonym = player;
    evidence.player_pseudonym = player;
    evidence.definition = kGhostVolumeDefinition;
    evidence.registry = kGhostVolumeRegistry;
    evidence.type = kGhostVolumeType;
    evidence.index = kGhostVolumeIndex;
    evidence.name_hash = kGhostVolumeNameHash;
    evidence.overlap_kind = kind;
    evidence.world_latch = kind != OverlapObservationKind::initial_level;
    evidence.player_present_latch = true;
    evidence.inside = inside;
    evidence.original_result = inside;
    return evidence;
}

[[nodiscard]] Type60Evidence registration() noexcept {
    Type60Evidence evidence = containment(OverlapObservationKind::initial_level, true);
    evidence.header.phase = CapturePhase::type60_register;
    evidence.presence_mask = bit(Type60Field::exact_identity) | bit(Type60Field::instance_pseudonym)
                             | bit(Type60Field::record_pseudonym)
                             | bit(Type60Field::registration_token)
                             | bit(Type60Field::registration_pair);
    evidence.registration_token = 4;
    evidence.registration_pair = {0x111U, 0x222U};
    return evidence;
}

[[nodiscard]] Type60Evidence membership(bool set,
                                        std::uint32_t playerIndex,
                                        std::uint64_t before,
                                        std::uint64_t callId) noexcept {
    Type60Evidence evidence =
        containment(OverlapObservationKind::periodic_query, set, callId, 0xB000U + playerIndex);
    evidence.header.phase =
        set ? CapturePhase::type60_membership_set : CapturePhase::type60_membership_clear;
    evidence.presence_mask = bit(Type60Field::exact_identity) | bit(Type60Field::instance_pseudonym)
                             | bit(Type60Field::record_pseudonym)
                             | bit(Type60Field::player_pseudonym) | bit(Type60Field::player_index)
                             | bit(Type60Field::membership_before)
                             | bit(Type60Field::membership_after);
    evidence.player_index = playerIndex;
    evidence.membership_before = before;
    const std::uint64_t playerBit = std::uint64_t{1U} << playerIndex;
    evidence.membership_after = set ? before | playerBit : before & ~playerBit;
    return evidence;
}

[[nodiscard]] DynamicBridgeEvidence subscriber() noexcept {
    DynamicBridgeEvidence evidence{};
    evidence.header.context = pc_context();
    evidence.header.phase = CapturePhase::subscriber_register;
    evidence.boundary = DynamicBoundaryKind::subscriber_registration;
    evidence.presence_mask = bit(DynamicField::callback_rva)
                             | bit(DynamicField::subscriber_context_pseudonym)
                             | bit(DynamicField::subscriber_mask);
    evidence.callback_rva = 0x4E1870U;
    evidence.subscriber_context_pseudonym = 0x55U;
    evidence.subscriber_mask = std::uint64_t{1U} << 7U;
    return evidence;
}

[[nodiscard]] RecordZeroScalars record_zero(std::uint32_t mode = 2U) noexcept {
    RecordZeroScalars record{};
    record.presence_mask = bit(RecordZeroField::root_reference) | bit(RecordZeroField::value)
                           | bit(RecordZeroField::optional_presence)
                           | bit(RecordZeroField::optional_value)
                           | bit(RecordZeroField::record_reference)
                           | bit(RecordZeroField::generation) | bit(RecordZeroField::mode);
    record.root_reference_sha256 = digest("root reference eight bytes");
    record.value = 0U; // observed zero is distinct from absence
    record.optional_value_present = true;
    record.optional_value = 0U;
    record.record_reference_sha256 = digest("record reference eight bytes");
    record.generation = 0U;
    record.mode = mode;
    return record;
}

[[nodiscard]] Type5PublicationEvidence publication(std::uint32_t mode = 2U) noexcept {
    Type5PublicationEvidence evidence{};
    evidence.header.context = host_context();
    evidence.header.phase = CapturePhase::type5_publication;
    evidence.presence_mask =
        bit(AuthorityField::delivery) | bit(AuthorityField::body_state)
        | bit(AuthorityField::body_present) | bit(AuthorityField::reset)
        | bit(AuthorityField::body_bit_count) | bit(AuthorityField::body_sha256)
        | bit(AuthorityField::record_zero) | bit(AuthorityField::old_host_generation)
        | bit(AuthorityField::new_host_generation) | bit(AuthorityField::trigger_cause_sha256);
    evidence.delivery = Type5Delivery::snapshot;
    evidence.body_state = AuthorityBodyState::active_record_zero;
    evidence.body_present = true;
    evidence.body_bit_count = 20'000U;
    evidence.body_sha256 = digest("decoded type53 authority body");
    evidence.record_zero = record_zero(mode);
    evidence.old_host_generation = 0U;
    evidence.new_host_generation = 0U;
    evidence.trigger_cause_sha256 = digest("observed trigger cause");
    return evidence;
}

[[nodiscard]] Type53ApplyEvidence apply_evidence(std::uint32_t mode = 2U) noexcept {
    Type53ApplyEvidence evidence{};
    evidence.header.context = pc_context();
    evidence.header.phase = CapturePhase::type53_apply;
    evidence.presence_mask = bit(AuthorityField::body_state) | bit(AuthorityField::body_present)
                             | bit(AuthorityField::body_sha256) | bit(AuthorityField::record_zero)
                             | bit(AuthorityField::processed_before)
                             | bit(AuthorityField::processed_after);
    evidence.body_state = AuthorityBodyState::active_record_zero;
    evidence.body_present = true;
    evidence.body_sha256 = digest("decoded type53 authority body");
    evidence.record_zero = record_zero(mode);
    evidence.processed_generation_before = 0U;
    evidence.processed_generation_after = 0U;
    return evidence;
}

[[nodiscard]] Type53ConsumerCorrelation consumer_correlation() noexcept {
    Type53ConsumerCorrelation evidence{};
    evidence.header.context = pc_context();
    evidence.header.phase = CapturePhase::row_zero_submit;
    evidence.presence_mask =
        bit(ConsumerCorrelationField::record_index) | bit(ConsumerCorrelationField::bank)
        | bit(ConsumerCorrelationField::selector) | bit(ConsumerCorrelationField::generation)
        | bit(ConsumerCorrelationField::processed_generation) | bit(ConsumerCorrelationField::mode)
        | bit(ConsumerCorrelationField::disposition);
    evidence.provenance = ConsumerCorrelationProvenance::observed_selected_row_extractor;
    evidence.record_index = kDialogueRecordIndex;
    evidence.bank = kDialogueBank;
    evidence.selector = kGhostSelector;
    evidence.generation = 1U;
    evidence.processed_generation = 0U;
    evidence.mode = 2U;
    evidence.disposition = GenerationDisposition::submitted_and_consumed;
    return evidence;
}

[[nodiscard]] VoiceStateSnapshot
voice_state(std::uint32_t handle, bool started, bool timed) noexcept {
    VoiceStateSnapshot state{};
    state.presence_mask = bit(VoiceStateField::runtime_handle) | bit(VoiceStateField::source_pair)
                          | bit(VoiceStateField::speaker) | bit(VoiceStateField::lookup_pair)
                          | bit(VoiceStateField::audio_handle) | bit(VoiceStateField::started)
                          | bit(VoiceStateField::timed_enabled) | bit(VoiceStateField::correlation);
    state.runtime_record_handle = handle;
    state.source_a = kGhostSelector;
    state.source_b = kDialogueBank;
    state.speaker_hash = kGhostSpeaker;
    state.lookup_bank = kGhostPrimaryLookupBank;
    state.lookup_hash = kGhostPrimaryCue;
    state.audio_handle = started ? 99U : 0U;
    state.started = started;
    state.timed_presentation_enabled = timed;
    state.correlation_value = 0xCA11U;
    return state;
}

[[nodiscard]] PresentationEvidence presentation(CapturePhase phase,
                                                PresentationOutcome outcome) noexcept {
    PresentationEvidence evidence{};
    evidence.header.context = pc_context();
    evidence.header.phase = phase;
    evidence.outcome = outcome;
    evidence.selector = kGhostSelector;
    evidence.bank = kDialogueBank;
    evidence.record_index = kDialogueRecordIndex;
    evidence.generation = 1U;
    evidence.before = voice_state(7U, false, false);
    evidence.after = voice_state(
        7U,
        outcome == PresentationOutcome::started || outcome == PresentationOutcome::timed_presented
            || outcome == PresentationOutcome::stopped || outcome == PresentationOutcome::freed,
        outcome == PresentationOutcome::timed_presented);
    return evidence;
}

void no_writer_and_inference_gate_contracts() {
    CHECK(!kOwnsNativeDetour && !kOwnsAuthorityWriter && !kPublishesType5);
    CHECK(!kMutatesVmState && !kMutatesMembership && !kPerformsIo);
    CHECK(!kStoresRawAuthorityBodies && !kRetailHostPublisherRecovered);
    CHECK(kRetailHostPublisherRva == 0U && !kProducerInstructionRecovered);
    CHECK(!kType53CompletionFeedbackRecovered && !kDialogueSenseExportRecovered);
    CHECK(kMissionResultConsumerRva == 0U);
    CHECK(!kAnyEndpointAttachableFromPrefixOnly);
    CHECK(kVolumeToType54Claim.strength
          == EvidenceStrength::strong_inference_live_capture_required);
    CHECK(kType54ToRecordZeroClaim.strength == EvidenceStrength::unknown_not_recovered);
    CHECK(kCombinedGhostBridgeStrength == EvidenceStrength::unknown_not_recovered);
    CHECK(!kCombinedGhostBridgePromotable && kCombinedGhostBridgeRequiresLiveCapture);
    CHECK(!kLifecycleAuthorizesNativeRemoval);
}

void sha256_and_synthetic_artifact_rejection() {
    const Sha256 abc = digest("abc");
    constexpr std::array<std::byte, 4U> expectedPrefix{
        std::byte{0xBA}, std::byte{0x78}, std::byte{0x16}, std::byte{0xBF}};
    CHECK(std::equal(expectedPrefix.begin(), expectedPrefix.end(), abc.begin()));
    CHECK(std::string_view{kEvidenceDigestAlgorithm} == "SHA-256");

    std::vector<std::byte> synthetic(0x1009B80U);
    VerifiedArtifact token;
    CHECK(verify_pinned_artifact(ArtifactKind::pc_unpacked_reference, synthetic, token)
          == ArtifactVerificationResult::exact_size_mismatch);
    CHECK(!token.valid());
    CHECK(validate_reference_endpoint(token, synthetic, NativeSurface::type53_apply)
          == EndpointValidation::invalid_verified_artifact);
    CHECK(endpoint_attachability(NativeSurface::type53_apply)
          == EndpointAttachability::structural_owner_and_call_edge_required);

    const ArtifactDescriptor& ps4 = artifact_descriptor(ArtifactKind::ps4_eboot_reference);
    std::vector<std::byte> exactSizeSynthetic(static_cast<std::size_t>(ps4.file_bytes));
    CHECK(verify_pinned_artifact(ArtifactKind::ps4_eboot_reference, exactSizeSynthetic, token)
          == ArtifactVerificationResult::sha256_mismatch);
    CHECK(!token.valid());
    CHECK(validate_reference_endpoint(token, exactSizeSynthetic, NativeSurface::type53_apply)
          == EndpointValidation::invalid_verified_artifact);
}

[[nodiscard]] std::vector<std::byte> read_file(const wchar_t* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    CHECK(input.good());
    if (!input.good()) {
        return {};
    }
    const std::streamoff end = input.tellg();
    CHECK(end >= 0);
    if (end < 0) {
        return {};
    }
    const auto byteCount = static_cast<std::size_t>(end);
    std::vector<std::byte> bytes(byteCount);
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    CHECK(input.good() || input.eof());
    CHECK(static_cast<std::size_t>(input.gcount()) == bytes.size());
    return bytes;
}

#if defined(DAWN_GHOST_VM_BRIDGE_REAL_ARTIFACT_TESTS)
void exact_real_artifacts_bind_all_prefixes() {
    const ArtifactDescriptor& pcDescriptor =
        artifact_descriptor(ArtifactKind::pc_unpacked_reference);
    const ArtifactDescriptor& ps4Descriptor =
        artifact_descriptor(ArtifactKind::ps4_eboot_reference);
    const ArtifactDescriptor& packageDescriptor =
        artifact_descriptor(ArtifactKind::mercury_mission_package);
    const std::vector<std::byte> pcBytes = read_file(pcDescriptor.path);
    const std::vector<std::byte> ps4Bytes = read_file(ps4Descriptor.path);
    const std::vector<std::byte> packageBytes = read_file(packageDescriptor.path);
    VerifiedArtifact pc;
    VerifiedArtifact ps4;
    VerifiedArtifact package;
    CHECK(verify_pinned_artifact(ArtifactKind::pc_unpacked_reference, pcBytes, pc)
          == ArtifactVerificationResult::verified);
    CHECK(verify_pinned_artifact(ArtifactKind::ps4_eboot_reference, ps4Bytes, ps4)
          == ArtifactVerificationResult::verified);
    CHECK(verify_pinned_artifact(ArtifactKind::mercury_mission_package, packageBytes, package)
          == ArtifactVerificationResult::verified);
    CHECK(pc.valid() && ps4.valid() && package.valid());

    std::size_t prefixCount = 0U;
    const auto last = static_cast<std::uint32_t>(NativeSurface::ps4_terminal_core);
    for (std::uint32_t raw = 0U; raw <= last; ++raw) {
        const NativeSurface surface = static_cast<NativeSurface>(raw);
        const NativeBoundaryDescriptor descriptor = native_boundary(surface);
        if (descriptor.prefix.empty()) {
            continue;
        }
        ++prefixCount;
        CHECK(sha256(descriptor.prefix) == descriptor.prefix_sha256);
        if (descriptor.platform == Platform::pc_windows_x64) {
            CHECK(validate_reference_endpoint(pc, pcBytes, surface)
                  == EndpointValidation::reference_anchor_valid);
        } else {
            CHECK(validate_reference_endpoint(ps4, ps4Bytes, surface)
                  == EndpointValidation::reference_anchor_valid);
        }
        CHECK(endpoint_attachability(surface)
              == EndpointAttachability::structural_owner_and_call_edge_required);
    }
    CHECK(prefixCount == 25U);
    CHECK(
        validate_reference_endpoint(pc, std::span{pcBytes}.subspan(1U), NativeSurface::type53_apply)
        == EndpointValidation::mapping_not_bound_to_verification);

    CaptureContext context = pc_context();
    CHECK(bind_verified_build(context, pc));
    g_verifiedPcBuild = context.build;
    CHECK(fully_correlated(context));
}
#endif

void endpoint_semantics_and_exact_abi_contracts() {
    const NativeBoundaryDescriptor compare =
        native_boundary(NativeSurface::type53_generation_compare);
    CHECK(compare.rva == 0x100A267U);
    CHECK(compare.kind == BoundaryKind::instruction_before);
    CHECK(compare.observation_bracket == ObservationBracket::instruction_point);
    const NativeBoundaryDescriptor membershipSet =
        native_boundary(NativeSurface::type60_membership_set);
    CHECK(membershipSet.observation_bracket == ObservationBracket::bracket_original);
    const NativeBoundaryDescriptor actualStart = native_boundary(NativeSurface::voice_actual_start);
    CHECK(actualStart.observation_bracket == ObservationBracket::bracket_original);
    const NativeBoundaryDescriptor teardown = native_boundary(NativeSurface::voice_teardown_update);
    CHECK(teardown.observation_bracket == ObservationBracket::bracket_original);

    const NativeBoundaryDescriptor ps4Store =
        native_boundary(NativeSurface::ps4_type53_generation_store);
    CHECK(ps4Store.rva == kPs4GenerationStoreFirstRva);
    CHECK(ps4Store.kind == BoundaryKind::instruction_before);
    const NativeBoundaryDescriptor ps4Post =
        native_boundary(NativeSurface::ps4_type53_post_generation_store);
    CHECK(ps4Post.rva == kPs4PostGenerationStoreRva);
    CHECK(ps4Post.kind == BoundaryKind::post_state);
    CHECK(kPs4GenerationStoreLastRva == 0x004A6015U);

    CHECK(kPcSubmitAbi.r8d == -1 && kPcSubmitAbi.r9d == -1);
    CHECK(kPcSubmitAbi.stack_argument_5 == 0);
    CHECK(kPcSubmitAbi.rcx_is_output && kPcSubmitAbi.rdx_is_selector_pair);
    CHECK(kPs4TerminalAbi.esi == -1 && kPs4TerminalAbi.edx == -1);
    CHECK(kPs4TerminalAbi.ecx == 0 && kPs4TerminalAbi.rdi_is_selector_pair);

    const NativeBoundaryDescriptor setter = native_boundary(NativeSurface::timed_delegate_setter);
    const NativeBoundaryDescriptor wrapper = native_boundary(NativeSurface::ps4_terminal_wrapper);
    CHECK(setter.exact_active_bytes == 8U && !setter.inline_detour_14_safe);
    CHECK(wrapper.exact_active_bytes == 8U && !wrapper.inline_detour_14_safe);
}

void context_presence_and_privacy_are_explicit() {
    CaptureContext exact = pc_context();
    CHECK(fully_correlated(exact)
          == (g_verifiedPcBuild.status() == BuildProvenanceStatus::exact_pinned_bytes_verified));
    CHECK(exact.session_pseudonym == 1U);
    CHECK(exact.privacy == PrivacyClass::default_digest_and_pseudonym);
    if (context_has(exact, ContextField::build_provenance)) {
        CHECK(exact.build.status() == BuildProvenanceStatus::exact_pinned_bytes_verified);
    }

    exact.presence_mask &= ~static_cast<std::uint32_t>(ContextField::roster_generation);
    exact.roster_generation = 0U;
    CHECK(!fully_correlated(exact));
    CHECK(valid_evidence(containment(OverlapObservationKind::periodic_query, true)));

    CaptureContext external = host_context();
    CHECK(fully_correlated(external));
    CHECK(external.build.status() == BuildProvenanceStatus::external_digest_unverified);
}

void cold_overlap_and_two_player_sequences_are_discriminating() {
    const std::array<Type60Evidence, 5U> playerOne{
        containment(OverlapObservationKind::initial_level, true, 1U, 0xA1U),
        containment(OverlapObservationKind::dwell, true, 2U, 0xA1U),
        containment(OverlapObservationKind::exit_edge, false, 3U, 0xA1U),
        containment(OverlapObservationKind::reentry_edge, true, 4U, 0xA1U),
        containment(OverlapObservationKind::periodic_query, true, 5U, 0xA1U)};
    for (const Type60Evidence& row : playerOne) {
        CHECK(valid_evidence(row));
    }
    const std::array<Type60Evidence, 2U> oppositeOrder{
        containment(OverlapObservationKind::enter_edge, true, 6U, 0xB2U),
        containment(OverlapObservationKind::enter_edge, true, 7U, 0xA1U)};
    CHECK(valid_evidence(oppositeOrder[0]) && valid_evidence(oppositeOrder[1]));
    CHECK(oppositeOrder[0].player_pseudonym != oppositeOrder[1].player_pseudonym);

    Type60Evidence mismatch = playerOne[0];
    mismatch.original_result = false;
    CHECK(!valid_evidence(mismatch));
    Type60Evidence missingKind = playerOne[0];
    missingKind.presence_mask &= ~bit(Type60Field::overlap_kind);
    CHECK(!valid_evidence(missingKind));
    CHECK(valid_evidence(registration()));

    Type60Evidence set = membership(true, 1U, 0U, 8U);
    Type60Evidence clear = membership(false, 1U, set.membership_after, 9U);
    CHECK(valid_evidence(set) && valid_evidence(clear));
    CHECK(clear.membership_after == 0U);
}

void dynamic_presence_hash_and_secure_payload_privacy() {
    CHECK(valid_evidence(subscriber()));
    DynamicBridgeEvidence event{};
    event.header.context = pc_context();
    event.header.phase = CapturePhase::event_enqueue;
    event.boundary = DynamicBoundaryKind::bus_enqueue;
    event.presence_mask = bit(DynamicField::event_ordinal) | bit(DynamicField::payload_length)
                          | bit(DynamicField::payload_sha256);
    event.event_ordinal = 7U;
    event.payload_bytes = 40U;
    event.payload_sha256 = digest("forty bytes or a stand-in payload");
    CHECK(valid_evidence(event));
    event.presence_mask &= ~bit(DynamicField::payload_length);
    CHECK(!valid_evidence(event));

    DynamicBridgeEvidence node{};
    node.header.context = pc_context();
    node.header.phase = CapturePhase::vm_node;
    node.boundary = DynamicBoundaryKind::vm_node;
    node.presence_mask = bit(DynamicField::node_rva) | bit(DynamicField::node_class)
                         | bit(DynamicField::owner_provenance)
                         | bit(DynamicField::vm_context_sha256);
    node.node_rva = 0xA21520U;
    node.node_class = 0x80804E3FU;
    node.owner_provenance = OwnerProvenanceState::absent_observed;
    node.vm_context_sha256 = digest("bounded VM context");
    CHECK(valid_evidence(node));
    node.owner_provenance = OwnerProvenanceState::not_observed;
    CHECK(!valid_evidence(node));

    SecurePayloadPrefixEvidence secure{};
    secure.header = event.header;
    secure.privacy = PrivacyClass::secured_re_trace;
    secure.copied_bytes = 32U;
    secure.payload_bytes = 40U;
    secure.payload_sha256 = event.payload_sha256;
    CHECK(valid_evidence(secure));
    secure.privacy = PrivacyClass::default_digest_and_pseudonym;
    CHECK(!valid_evidence(secure));
}

void publication_apply_and_consumer_correlation_are_separate() {
    const Type5PublicationEvidence published = publication(3U);
    CHECK(valid_evidence(published));
    CHECK(record_zero_has(published.record_zero, RecordZeroField::value));
    CHECK(published.record_zero.value == 0U);
    CHECK(published.record_zero.mode == 3U); // retained, behavior unknown

    Type5PublicationEvidence missingMode = published;
    missingMode.record_zero.presence_mask &= ~bit(RecordZeroField::mode);
    CHECK(!valid_evidence(missingMode));

    Type5PublicationEvidence omitted{};
    omitted.header.context = host_context();
    omitted.header.phase = CapturePhase::type5_publication;
    omitted.presence_mask = bit(AuthorityField::delivery) | bit(AuthorityField::body_state)
                            | bit(AuthorityField::body_present) | bit(AuthorityField::reset)
                            | bit(AuthorityField::body_bit_count);
    omitted.delivery = Type5Delivery::snapshot;
    omitted.body_state = AuthorityBodyState::omitted;
    omitted.body_present = false;
    omitted.body_bit_count = 0U; // observed zero, not missing
    CHECK(valid_evidence(omitted));

    CHECK(valid_evidence(apply_evidence(3U)));
    Type53ConsumerCorrelation correlation = consumer_correlation();
    CHECK(valid_evidence(correlation));
    Type53ConsumerCorrelation noExtractor = correlation;
    noExtractor.provenance = ConsumerCorrelationProvenance::absent;
    CHECK(!valid_evidence(noExtractor));
    Type53ConsumerCorrelation defaultCorrelation{};
    defaultCorrelation.header.context = pc_context();
    defaultCorrelation.header.phase = CapturePhase::row_zero_submit;
    CHECK(!valid_evidence(defaultCorrelation));
}

[[nodiscard]] GenerationInputs passing(std::uint32_t mode) noexcept {
    return {2U, 1U, mode, true, false, true, true, true, true, true};
}

void complete_generation_truth_table_and_unknown_mode() {
    GenerationInputs input = passing(3U);
    input.processed_generation = input.record_generation;
    CHECK(evaluate_generation(input).disposition
          == GenerationDisposition::suppressed_equal_generation);

    input = passing(3U);
    GenerationDecision decision = evaluate_generation(input);
    CHECK(decision.disposition == GenerationDisposition::unknown_unrecovered_mode);
    CHECK(!decision.submit && !decision.store_generation && !decision.retry_later);
    input = passing(4U);
    CHECK(evaluate_generation(input).disposition == GenerationDisposition::malformed_mode);

    for (std::uint32_t mode = 0U; mode <= 2U; ++mode) {
        input = passing(mode);
        input.active_time_predicate = false;
        decision = evaluate_generation(input);
        CHECK(decision.disposition == GenerationDisposition::retry_inactive);
        CHECK(decision.retry_later && !decision.store_generation);

        input = passing(mode);
        input.duration_expired = true;
        decision = evaluate_generation(input);
        if (mode == 0U) {
            CHECK(decision.disposition == GenerationDisposition::retry_eligibility_failure);
        } else if (mode == 1U) {
            CHECK(decision.disposition == GenerationDisposition::consumed_without_submit);
        } else {
            CHECK(decision.disposition == GenerationDisposition::submitted_and_consumed);
        }

        for (std::size_t gate = 0U; gate < 4U; ++gate) {
            input = passing(mode);
            bool* gates[] = {&input.record_reference_passed,
                             &input.root_reference_passed,
                             &input.root_predicate_passed,
                             &input.record_predicate_passed};
            *gates[gate] = false;
            decision = evaluate_generation(input);
            CHECK(decision.disposition
                  == (mode == 1U ? GenerationDisposition::consumed_without_submit
                                 : GenerationDisposition::retry_eligibility_failure));
        }
    }
    input = passing(0U);
    input.selected_row_call_observed = false;
    decision = evaluate_generation(input);
    CHECK(decision.disposition == GenerationDisposition::incomplete_capture);
    CHECK(!decision.retry_later); // absence of observation is not a native failure branch
}

void replay_late_join_and_restart_remain_policy_bound() {
    ReplayEvidence evidence{};
    evidence.delivery = Type5Delivery::delta;
    evidence.body_state = AuthorityBodyState::active_record_zero;
    evidence.component = ComponentContinuity::retained;
    evidence.mirror = MirrorContinuity::retained;
    evidence.record_generation = 42U;
    evidence.processed_generation = 42U;
    evidence.processed_mirror_observed = true;
    ReplayAssessment assessment = assess_replay(evidence);
    CHECK(assessment.disposition == ReplayDisposition::suppressed_same_generation);
    CHECK(!assessment.selector_equal); // selector was not observed

    evidence.record_generation = 43U;
    assessment = assess_replay(evidence);
    CHECK(assessment.disposition == ReplayDisposition::may_replay_different_generation);
    evidence.selector_observed = true;
    evidence.previous_selector_observed = true;
    evidence.selector = kGhostSelector;
    evidence.previous_selector = kGhostSelector;
    assessment = assess_replay(evidence);
    CHECK(assessment.selector_equal);

    evidence.delivery = Type5Delivery::snapshot;
    evidence.body_state = AuthorityBodyState::omitted;
    assessment = assess_replay(evidence);
    CHECK(assessment.disposition == ReplayDisposition::no_body_in_snapshot_or_delta);
    CHECK(assessment.host_policy_required);

    evidence.body_state = AuthorityBodyState::active_record_zero;
    evidence.component = ComponentContinuity::replaced;
    evidence.mirror = MirrorContinuity::replaced_initializer_unknown;
    evidence.processed_mirror_observed = false;
    assessment = assess_replay(evidence);
    CHECK(assessment.disposition == ReplayDisposition::unknown_processed_mirror);
}

void presentation_mappings_and_bracketed_state_are_exact() {
    const std::array<PresentationEvidence, 6U> sequence{
        presentation(CapturePhase::row_zero_submit, PresentationOutcome::submitted),
        presentation(CapturePhase::actual_start, PresentationOutcome::started),
        presentation(CapturePhase::timed_presentation, PresentationOutcome::timed_presented),
        presentation(CapturePhase::deadline_drop, PresentationOutcome::deadline_dropped),
        presentation(CapturePhase::stop, PresentationOutcome::stopped),
        presentation(CapturePhase::free_record, PresentationOutcome::freed)};
    for (const PresentationEvidence& row : sequence) {
        CHECK(valid_evidence(row));
    }
    PresentationEvidence mismatch = sequence[1];
    mismatch.outcome = PresentationOutcome::freed;
    CHECK(!valid_evidence(mismatch));
    PresentationEvidence notStarted = sequence[1];
    notStarted.after.started = false;
    CHECK(!valid_evidence(notStarted));

    CHECK(kVoiceSourceAOffset == 0x04U && kVoiceSourceBOffset == 0x08U);
    CHECK(kVoiceStartedOffset == 0x48U && kVoiceTimedEnabledOffset == 0x49U);
    CHECK(kVoiceCorrelationOffset == 0x4CU);
}

void queue_capacity_busy_exhaustion_and_absent_correlation() {
    FixedCaptureQueue<Type60Evidence, 2U> queue;
    Type60Evidence absent = containment(OverlapObservationKind::periodic_query, true);
    absent.header.context = {};
    CHECK(queue.try_push(absent) == QueuePushResult::enqueued);
    CHECK(queue.try_push(absent) == QueuePushResult::enqueued);
    CHECK(queue.counters().duplicates == 0U);
    CHECK(queue.try_push(absent) == QueuePushResult::full);

    Type60CaptureQueue dedupe;
    const Type60Evidence exact = containment(OverlapObservationKind::periodic_query, true);
    CHECK(dedupe.try_push(exact) == QueuePushResult::enqueued);
    CHECK(dedupe.try_push(exact)
          == (fully_correlated(exact.header.context) ? QueuePushResult::duplicate
                                                     : QueuePushResult::enqueued));

    Type60CaptureQueue busy;
    CHECK(busy.testing_lock());
    CHECK(busy.try_push(exact) == QueuePushResult::busy);
    busy.testing_unlock();

    Type60CaptureQueue exhausted;
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted.try_push(exact) == QueuePushResult::sequence_exhausted);
    CHECK(!kLiveQueueCapacityQualified && kLiveQueueRateMeasurementRequired);
}

void real_mpsc_queue_concurrency_accounts_every_attempt() {
    FixedCaptureQueue<Type60Evidence, 128U> queue;
    constexpr std::uint32_t producerCount = 4U;
    constexpr std::uint32_t attemptsPerProducer = 2'000U;
    constexpr std::uint64_t totalAttempts =
        static_cast<std::uint64_t>(producerCount) * attemptsPerProducer;
    std::atomic<std::uint32_t> running{producerCount};
    std::array<std::atomic<std::uint64_t>, 6U> results{};
    std::vector<std::uint64_t> poppedSequences;
    poppedSequences.reserve(static_cast<std::size_t>(totalAttempts));

    std::thread consumer([&]() {
        for (;;) {
            Type60Evidence output{};
            const QueuePopResult result = queue.try_pop(output);
            if (result == QueuePopResult::success) {
                poppedSequences.push_back(output.header.sequence);
                continue;
            }
            if (running.load(std::memory_order_acquire) == 0U && result == QueuePopResult::empty) {
                break;
            }
            std::this_thread::yield();
        }
    });

    std::array<std::thread, producerCount> producers;
    for (std::uint32_t producer = 0U; producer < producerCount; ++producer) {
        producers[producer] = std::thread([&, producer]() {
            for (std::uint32_t index = 0U; index < attemptsPerProducer; ++index) {
                const std::uint64_t unique =
                    static_cast<std::uint64_t>(producer) * attemptsPerProducer + index + 1U;
                Type60Evidence row =
                    containment(OverlapObservationKind::periodic_query, true, unique, unique);
                const QueuePushResult result = queue.try_push(row);
                results[static_cast<std::size_t>(result)].fetch_add(1U, std::memory_order_relaxed);
            }
            running.fetch_sub(1U, std::memory_order_release);
        });
    }
    for (std::thread& producer : producers) {
        producer.join();
    }
    consumer.join();

    std::uint64_t accounted = 0U;
    for (const std::atomic<std::uint64_t>& result : results) {
        accounted += result.load(std::memory_order_relaxed);
    }
    CHECK(accounted == totalAttempts);
    const QueueCounters counters = queue.counters();
    CHECK(counters.accepted == results[static_cast<std::size_t>(QueuePushResult::enqueued)].load());
    CHECK(counters.dropped_full == results[static_cast<std::size_t>(QueuePushResult::full)].load());
    CHECK(counters.dropped_busy == results[static_cast<std::size_t>(QueuePushResult::busy)].load());
    CHECK(poppedSequences.size() == counters.accepted);
    CHECK(std::is_sorted(poppedSequences.begin(), poppedSequences.end()));
    CHECK(std::adjacent_find(poppedSequences.begin(), poppedSequences.end())
          == poppedSequences.end());
}

void rate_limiter_never_samples_causal_transitions() {
    CaptureRateLimiter limiter{3U};
    CHECK(limiter.admit(CapturePhase::vm_runner, 10U));
    CHECK(limiter.admit(CapturePhase::vm_runner, 10U));
    CHECK(limiter.admit(CapturePhase::vm_runner, 10U));
    CHECK(!limiter.admit(CapturePhase::vm_runner, 10U));
    CHECK(limiter.admit(CapturePhase::vm_runner, 11U));
    for (std::uint32_t index = 0U; index < 100U; ++index) {
        CHECK(limiter.admit(CapturePhase::type60_containment, 11U));
        CHECK(limiter.admit(CapturePhase::type5_publication, 11U));
        CHECK(limiter.admit(CapturePhase::type53_apply, 11U));
        CHECK(limiter.admit(CapturePhase::actual_start, 11U));
        CHECK(limiter.admit(CapturePhase::free_record, 11U));
    }
    CHECK(limiter.rate_limited() == 1U);
    CausalWindowLossEvidence lossFree{1U, 2U, {}, 0U};
    CHECK(causal_window_loss_free(lossFree));
    lossFree.rate_limited = 1U;
    CHECK(!causal_window_loss_free(lossFree));
}

std::atomic<std::uint32_t> g_originalCalls{};
std::atomic<std::uint32_t> g_preCalls{};
std::atomic<std::uint32_t> g_postCalls{};
OriginalOnceLifecycle* g_recursiveLifecycle = nullptr;
OriginalOnceLifecycle* g_innerLifecycle = nullptr;
OriginalOnceLifecycle* g_quiesceLifecycle = nullptr;

void pre() noexcept {
    g_preCalls.fetch_add(1U, std::memory_order_relaxed);
}

void post() noexcept {
    g_postCalls.fetch_add(1U, std::memory_order_relaxed);
}

void simple_original() noexcept {
    g_originalCalls.fetch_add(1U, std::memory_order_relaxed);
}

void recursive_original(std::uint32_t depth) noexcept {
    g_originalCalls.fetch_add(1U, std::memory_order_relaxed);
    if (depth != 0U) {
        forward_void_original_once(
            *g_recursiveLifecycle, &recursive_original, &pre, &post, depth - 1U);
    }
}

void outer_original() noexcept {
    g_originalCalls.fetch_add(1U, std::memory_order_relaxed);
    forward_void_original_once(*g_innerLifecycle, &simple_original, &pre, &post);
}

void pre_and_quiesce() noexcept {
    pre();
    const bool changed = g_quiesceLifecycle->begin_quiesce();
    if (!changed) {
        ++g_failures;
    }
}

void original_once_same_and_cross_surface_recursion() {
    g_originalCalls = 0U;
    g_preCalls = 0U;
    g_postCalls = 0U;
    OriginalOnceLifecycle sameSurface{0x100A180U};
    g_recursiveLifecycle = &sameSurface;
    CHECK(sameSurface.activate());
    forward_void_original_once(sameSurface, &recursive_original, &pre, &post, 1U);
    CHECK(g_originalCalls == 2U);
    CHECK(g_preCalls == 1U && g_postCalls == 1U);
    CHECK(sameSurface.begin_quiesce());
    CHECK(sameSurface.try_detach());

    g_originalCalls = 0U;
    g_preCalls = 0U;
    g_postCalls = 0U;
    OriginalOnceLifecycle outer{0x100A180U};
    OriginalOnceLifecycle inner{0x10097D0U};
    g_innerLifecycle = &inner;
    CHECK(outer.activate() && inner.activate());
    forward_void_original_once(outer, &outer_original, &pre, &post);
    CHECK(g_originalCalls == 2U);
    CHECK(g_preCalls == 2U && g_postCalls == 2U);
    CHECK(outer.begin_quiesce() && outer.try_detach());
    CHECK(inner.begin_quiesce() && inner.try_detach());
}

void lifecycle_quiesce_retains_post_and_concurrent_drain() {
    g_originalCalls = 0U;
    g_preCalls = 0U;
    g_postCalls = 0U;
    OriginalOnceLifecycle lifecycle{0xA3D710U};
    g_quiesceLifecycle = &lifecycle;
    CHECK(lifecycle.activate());
    forward_void_original_once(lifecycle, &simple_original, &pre_and_quiesce, &post);
    CHECK(g_originalCalls == 1U && g_preCalls == 1U && g_postCalls == 1U);
    forward_void_original_once(lifecycle, &simple_original, &pre, &post);
    CHECK(g_originalCalls == 2U && g_preCalls == 1U && g_postCalls == 1U);
    CHECK(lifecycle.try_detach());
    CHECK(lifecycle.activate()); // restart is a fresh observation epoch
    CHECK(lifecycle.begin_quiesce() && lifecycle.try_detach());

    OriginalOnceLifecycle concurrent{0xA3E2D0U};
    CHECK(concurrent.activate());
    constexpr std::uint32_t workerCount = 8U;
    std::atomic<std::uint32_t> entered{};
    std::atomic<bool> release{};
    auto blockingOriginal = [&]() noexcept {
        entered.fetch_add(1U, std::memory_order_release);
        while (!release.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    };
    std::array<std::thread, workerCount> workers;
    for (std::thread& worker : workers) {
        worker = std::thread([&]() {
            forward_void_original_once(
                concurrent, blockingOriginal, []() noexcept {}, []() noexcept {});
        });
    }
    while (entered.load(std::memory_order_acquire) != workerCount) {
        std::this_thread::yield();
    }
    CHECK(concurrent.snapshot().calls_in_flight == workerCount);
    CHECK(concurrent.begin_quiesce());
    CHECK(!concurrent.try_detach());
    release.store(true, std::memory_order_release);
    for (std::thread& worker : workers) {
        worker.join();
    }
    CHECK(concurrent.snapshot().calls_in_flight == 0U);
    CHECK(concurrent.try_detach());
}

} // namespace

int main() {
    no_writer_and_inference_gate_contracts();
    sha256_and_synthetic_artifact_rejection();
#if defined(DAWN_GHOST_VM_BRIDGE_REAL_ARTIFACT_TESTS)
    exact_real_artifacts_bind_all_prefixes();
#endif
    endpoint_semantics_and_exact_abi_contracts();
    context_presence_and_privacy_are_explicit();
    cold_overlap_and_two_player_sequences_are_discriminating();
    dynamic_presence_hash_and_secure_payload_privacy();
    publication_apply_and_consumer_correlation_are_separate();
    complete_generation_truth_table_and_unknown_mode();
    replay_late_join_and_restart_remain_policy_bound();
    presentation_mappings_and_bracketed_state_are_exact();
    queue_capacity_busy_exhaustion_and_absent_correlation();
    real_mpsc_queue_concurrency_accounts_every_attempt();
    rate_limiter_never_samples_causal_transitions();
    original_once_same_and_cross_surface_recursion();
    lifecycle_quiesce_retains_post_and_concurrent_drain();

    if (g_failures != 0) {
        std::cerr << g_failures << " remediated Ghost VM bridge capture check(s) failed\n";
        return 1;
    }
    std::cout << "all remediated Ghost VM bridge capture checks passed\n";
    return 0;
}
