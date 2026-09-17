#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <type_traits>

#include "client/hooks/bootflow/opening_authority/objective_banner_variant_capture.h"

namespace {

using namespace dawn::client::hooks::bootflow::opening_authority::
    objective_banner_variant_capture;

static_assert(kObservationOnly);
static_assert(!kOwnsNativeDetour);
static_assert(!kWritesNativeState);
static_assert(!kWritesBannerQueue);
static_assert(!kWritesManagerState);
static_assert(!kSynthesizesType68Authority);
static_assert(!kInvokesUiWriter);
static_assert(!kInvokesNativeProducer);
static_assert(!kInvokesNativeQueueHelper);
static_assert(!kInvokesNativeManagerHelper);
static_assert(!kDefaultTelemetryContainsRawPayload);
static_assert(!kPresentationCompletionIsAuthority);
static_assert(!kQueueHasReplicatedAcknowledgement);
static_assert(kBannerProviderRecordCount == 64U);
static_assert(kNativeBannerQueueCapacity == 16U);
static_assert(kCompileTimeOmegaObjectiveBranch == ObjectiveResourceBranch::unknown);
static_assert(kOmegaObjectiveBranchGrade == EvidenceGrade::unknown);
static_assert(kTransientTitleBodyBindingGrade == EvidenceGrade::unknown);
static_assert(std::is_trivially_copyable_v<QueueSnapshot>);
static_assert(std::is_trivially_copyable_v<ResourceNodeSnapshot>);
static_assert(std::is_trivially_copyable_v<CaptureRecord>);
static_assert(std::is_trivially_copyable_v<ScalarHashTelemetry>);

int g_failures{};

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename Value, std::size_t Size>
void write_value(std::array<std::byte, Size>& bytes,
                 std::size_t offset,
                 const Value& value) noexcept {
    std::memcpy(bytes.data() + offset, &value, sizeof value);
}

void write_pair(std::array<std::byte, kObjectivePayloadBytes>& bytes,
                std::size_t offset,
                LocalizedPair pair) noexcept {
    write_value(bytes, offset, pair.bank);
    write_value(bytes, offset + 4U, pair.hash);
}

[[nodiscard]] CaptureContext exact_context(std::uint64_t callId = 77U) noexcept {
    CaptureContext context{};
    context.presence_mask = kCompleteContextMask;
    context.artifact = ArtifactKind::pc_packed_runtime;
    context.build_identity = artifact_descriptor(ArtifactKind::pc_packed_runtime).identity;
    context.build_verified = true;
    context.runtime_admission_generation = 10U;
    context.capture_epoch = 20U;
    context.session_id = 30U;
    context.generations = {40U, 41U, 42U, 43U, 44U, 45U, 46U, 47U, 48U};
    context.thread_id = 50U;
    context.call_id = callId;
    context.monotonic_tick = 60U;
    context.return_rva = 0x137A170U;
    context.activity_index = kOmegaActivityIndex;
    context.activity_definition = kOmegaActivityDefinition;
    return context;
}

[[nodiscard]] CaptureHeader header(CapturePhase phase,
                                   NativeSurface surface,
                                   std::uint64_t callId = 77U) noexcept {
    CaptureHeader result{};
    result.context = exact_context(callId);
    result.phase = phase;
    result.surface = surface;
    result.pre_generations = result.context.generations;
    result.post_generations = result.context.generations;
    result.pre_monotonic_tick = 100U;
    result.post_monotonic_tick = 101U;
    result.participant_identity = 0xABCDEFU;
    return result;
}

[[nodiscard]] QueueSnapshot queue_snapshot(std::uint32_t count = 0U,
                                           std::int32_t handle = 7,
                                           std::int64_t sequence = -1,
                                           std::int32_t command = -1,
                                           std::uint64_t payloadHash = 0U,
                                           std::uint32_t retire = 0U,
                                           std::int64_t nextSequence = -999) noexcept {
    QueueSnapshot result{};
    result.queue_identity = 0x6000U;
    result.queued_count = count;
    result.dispatch_definition_handle = handle;
    result.current_sequence = sequence;
    result.current_retire_or_supersede = retire;
    result.dispatch_metadata = {1U, 2U, 3U};
    result.current_validity_state = sequence >= 0 ? 1U : 0U;
    result.current_command = command;
    result.current_payload_hash = payloadHash;
    result.next_sequence = nextSequence == -999 ? (sequence >= 0 ? sequence + 1 : 10)
                                                 : nextSequence;
    result.priority_compatibility_table_hash = handle >= 0 ? 0x9911U : 0U;
    return seal_queue_snapshot(result);
}

[[nodiscard]] Command5Payload command5_payload() noexcept {
    Command5Payload payload{};
    payload.title = kOmegaTitle;
    payload.category = kMissionPresentationCategory;
    payload.icon_theme = 0x10203040U;
    payload.visual_tuple[0] = std::byte{0xAA};
    payload.visual_tuple[15] = std::byte{0x55};
    return seal_command5_payload(payload);
}

[[nodiscard]] FormatterObservation formatter(std::uint8_t mode = 0U,
                                             std::int32_t variant = 0) noexcept {
    FormatterObservation result{};
    result.event = kOpeningEvent;
    result.record_variant = variant;
    result.authored_pairs = {kOpeningPair1, kOpeningPair2, kAbsentPair, kAbsentPair};
    result.manager_event_pairs = result.authored_pairs;
    result.mode = mode;
    result.hud_identity = type68_hud_identity(result.event, result.record_variant);
    result.exact_manager_event_hash = 0xA1B2C3D4U;
    return result;
}

[[nodiscard]] ObjectiveModelObservation model(std::uint8_t mode = 0U) noexcept {
    ObjectiveModelObservation result{};
    result.activity_index = static_cast<std::uint16_t>(kOmegaActivityIndex);
    result.mode = mode;
    result.resolved_activity_title = kOmegaTitle;
    result.entry_header = mode == 1U ? kOpeningPair1 : kOmegaTitle;
    result.entry_secondary = kOpeningPair2;
    result.first_subentry_pairs = {
        mode == 1U ? kOpeningPair2 : kOpeningPair1, kAbsentPair, kAbsentPair};
    result.changed = true;
    result.manager_ready = true;
    result.exact_model_hash = 0x11223344U;
    return result;
}

[[nodiscard]] ObjectivePayload objective_payload(std::uint32_t command = 1U,
                                                 std::uint8_t mode = 0U) noexcept {
    std::array<std::byte, kObjectivePayloadBytes> raw{};
    const ObjectiveModelObservation source = model(mode);
    write_pair(raw, 0x04U, source.entry_secondary);
    write_pair(raw, 0x0CU, source.first_subentry_pairs[1]);
    write_pair(raw, 0x14U, source.entry_header);
    write_pair(raw, 0x1CU, source.first_subentry_pairs[2]);
    raw[0x24U] = std::byte{3U};
    const std::int32_t command6Value = command == 6U ? 42 : -1;
    write_value(raw, 0x28U, command6Value);
    raw[0x2CU] = command == 2U ? std::byte{1U} : std::byte{0U};
    raw[0x2DU] = std::byte{1U};
    return project_objective_payload(command, raw);
}

[[nodiscard]] PriorityCompatibilityRow priority(std::uint32_t command) noexcept {
    return {command, 2, 90U, 0x11U, 0xABC000U + command, true};
}

[[nodiscard]] QueueEvidence accepted_objective_insert() noexcept {
    QueueEvidence evidence{};
    evidence.event = QueueEventKind::objective_insert;
    evidence.disposition = QueueDisposition::accepted;
    evidence.command = 1U;
    evidence.pre = queue_snapshot(0U);
    evidence.post = queue_snapshot(1U);
    evidence.objective_payload = objective_payload();
    evidence.priority_row = priority(1U);
    return evidence;
}

[[nodiscard]] ManagerEntrySnapshot manager_snapshot(std::uint32_t count,
                                                    std::int32_t lifecycle,
                                                    std::uint64_t identity = 0xC0DEU) noexcept {
    const LifecycleMapping mapping = map_type68_lifecycle(lifecycle);
    ManagerEntrySnapshot result{};
    result.manager_identity = 0x7000U;
    result.entry_count = count;
    result.entry_identity_hash = identity;
    result.decoded_lifecycle = lifecycle;
    result.external_status = mapping.external;
    result.internal_status = mapping.internal;
    result.flags = 1U;
    result.new_mode = 1U;
    return seal_manager_snapshot(result);
}

[[nodiscard]] ResourceNodeSnapshot resource_node(std::uint32_t tag,
                                                 std::uint32_t visibleHash,
                                                 std::int32_t command,
                                                 bool visible,
                                                 std::uint32_t tagClass =
                                                     kLayoutResourceClass) noexcept {
    ResourceNodeSnapshot node{};
    node.cui_instance_identity = 0x8000U;
    node.resource_tag = tag;
    node.resource_class = tagClass;
    node.program_tag = 0x80BC6F5AU;
    node.node_index = 130U;
    node.property_id = 0x1234U;
    node.visible_constant_hash = visibleHash;
    node.queue_sequence = 22;
    node.queue_command = command;
    node.active = visible;
    node.visible = visible;
    if (tag == kObjectiveBranchATag) {
        node.exact_resource_hash = resource_fingerprint(ResourceRole::objective_branch_a);
    } else if (tag == kObjectiveBranchBTag) {
        node.exact_resource_hash = resource_fingerprint(ResourceRole::objective_branch_b);
    } else if (tag == kControlAccessibilityTag) {
        node.exact_resource_hash =
            resource_fingerprint(ResourceRole::control_accessibility_excluded);
    } else {
        node.exact_resource_hash =
            resource_fingerprint(ResourceRole::mission_activity_intro);
    }
    return seal_resource_node(node);
}

[[nodiscard]] ResourceNodeEvidence branch_resource(ObjectiveResourceBranch branch) noexcept {
    const std::uint32_t tag = branch == ObjectiveResourceBranch::branch_a
                                  ? kObjectiveBranchATag
                                  : kObjectiveBranchBTag;
    const std::uint32_t hash = branch == ObjectiveResourceBranch::branch_a
                                   ? kObjectiveBranchAHash
                                   : kObjectiveBranchBHash;
    ResourceNodeEvidence result{};
    result.pre = resource_node(tag, hash, 1, false);
    result.post = resource_node(tag, hash, 1, true);
    result.observed_branch = branch;
    return result;
}

[[nodiscard]] NodeBindingEvidence binding(PayloadPairSlot slot,
                                          NodeSemantic semantic,
                                          ObjectiveResourceBranch branch,
                                          const ObjectivePayload& payload) noexcept {
    const ProviderPairDescriptor provider = provider_descriptor(slot);
    const std::uint32_t tag = branch == ObjectiveResourceBranch::branch_a
                                  ? kObjectiveBranchATag
                                  : kObjectiveBranchBTag;
    const std::uint32_t hash = branch == ObjectiveResourceBranch::branch_a
                                   ? kObjectiveBranchAHash
                                   : kObjectiveBranchBHash;
    NodeBindingEvidence result{};
    result.slot = slot;
    result.wrapper_id = provider.wrapper_id;
    result.getter_id = provider.getter_id;
    result.returned_pair = payload_pair(payload, slot);
    result.resolved_pair = result.returned_pair;
    result.resolved_text_hash = 0x12340000U + static_cast<std::uint8_t>(slot);
    result.semantic = semantic;
    result.node = resource_node(tag, hash, 1, true);
    return result;
}

void exact_identity_and_native_contracts() {
    CHECK(artifact_descriptor(ArtifactKind::pc_packed_runtime).identity.file_bytes
          == 122'984'224U);
    CHECK(artifact_descriptor(ArtifactKind::pc_unpacked_reference).identity.file_bytes
          == 145'091'072U);
    CHECK(artifact_descriptor(ArtifactKind::omega_activity_package).identity.file_bytes
          == 407'552U);
    CHECK(matches_artifact(ArtifactKind::ps4_eboot_reference,
                           artifact_descriptor(ArtifactKind::ps4_eboot_reference).identity));
    CHECK(artifact_fingerprint(ArtifactKind::pc_packed_runtime) != 0U);

    const BannerRegistryDescriptor registry = banner_registry_descriptor();
    CHECK(registry.class_accessor_rva == 0x7DCA0U);
    CHECK(registry.initializer_rva == 0x1323DD0U);
    CHECK(registry.record_count == 64U);
    CHECK(registry.record_stride == 0x40U);
    CHECK(kNativeBannerQueueRecordStride == 0x38U);
    CHECK(kQueueCurrentSequenceOffset == 0x390U);
    CHECK(kQueueCurrentCommandOffset == 0x3ACU);

    CHECK(native_boundary(NativeSurface::command5_producer).rva == 0x131FDF0U);
    CHECK(native_boundary(NativeSurface::command5_insert).rva == 0x131A630U);
    CHECK(native_boundary(NativeSurface::type68_apply).rva == 0x1009C00U);
    CHECK(native_boundary(NativeSurface::type68_formatter).rva == 0x1008B40U);
    CHECK(native_boundary(NativeSurface::objective_model_materializer).rva == 0x137E6F0U);
    CHECK(native_boundary(NativeSurface::objective_payload_builder).rva == 0x1382710U);
    CHECK(native_boundary(NativeSurface::objective_insert).rva == 0x137A170U);
    CHECK(native_boundary(NativeSurface::queue_tick).rva == 0x13A0220U);
    CHECK(native_boundary(NativeSurface::queue_erase).rva == 0x131E0B0U);
    CHECK(native_boundary(NativeSurface::queue_reset).rva == 0x13991B0U);
    CHECK(native_boundary(NativeSurface::queue_teardown).rva == 0x139A1C0U);
    CHECK(native_boundary(NativeSurface::dispatch_loader).rva == 0x139A2B0U);
    CHECK(native_boundary(NativeSurface::manager_terminal_predicate).rva == 0x137DC10U);
    CHECK(native_boundary(NativeSurface::component_teardown).rva == 0x10091F0U);

    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeBoundaryDescriptor descriptor =
            native_boundary(static_cast<NativeSurface>(index));
        CHECK(!descriptor.direct_call_authorized);
        CHECK(!descriptor.native_write_authorized);
    }
}

void exact_resource_catalogs_and_unknown_branch() {
    const ResourceDescriptor& a = resource_descriptor(ResourceRole::objective_branch_a);
    const ResourceDescriptor& b = resource_descriptor(ResourceRole::objective_branch_b);
    const ResourceDescriptor& control =
        resource_descriptor(ResourceRole::control_accessibility_excluded);
    CHECK(a.tag == kObjectiveBranchATag);
    CHECK(a.new_objective_hash == kObjectiveBranchAHash);
    CHECK(a.node_base == 126U && a.node_count == 190U);
    CHECK(a.new_objective_offset == 0x12194U);
    CHECK(b.tag == kObjectiveBranchBTag);
    CHECK(b.new_objective_hash == kObjectiveBranchBHash);
    CHECK(b.node_base == 325U && b.node_count == 182U);
    CHECK(b.new_objective_offset == 0x1E524U);
    CHECK(control.tag == kControlAccessibilityTag);
    CHECK(control.new_objective_hash == kControlAccessibilityNewObjectiveHash);
    CHECK(kCompileTimeOmegaObjectiveBranch == ObjectiveResourceBranch::unknown);
    CHECK(classify_objective_resource(kObjectiveBranchATag, kObjectiveBranchAHash)
          == ObjectiveResourceBranch::branch_a);
    CHECK(classify_objective_resource(kObjectiveBranchBTag, kObjectiveBranchBHash)
          == ObjectiveResourceBranch::branch_b);
    CHECK(classify_objective_resource(kControlAccessibilityTag,
                                      kControlAccessibilityNewObjectiveHash)
          == ObjectiveResourceBranch::excluded_control_accessibility);
    CHECK((kOmegaTitle == LocalizedPair{0x81331697U, 0x47CAC8CFU}));
    CHECK((kMissionCatalogPair == LocalizedPair{0x8132F809U, 0x980BA1D8U}));
    CHECK((kOpeningPair1 == LocalizedPair{0x80C71DD2U, 0x4BCAD15BU}));
    CHECK((kOpeningPair2 == LocalizedPair{0x80C71DD2U, 0xA0071ABBU}));
}

void runtime_admission_requires_every_pinned_prefix() {
    RuntimeAdmissionEvidence evidence{};
    evidence.packed_runtime = artifact_descriptor(ArtifactKind::pc_packed_runtime).identity;
    evidence.mapped_image_base = 0x140000000ULL;
    evidence.mapped_image_bytes = 0x3000000U;
    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeBoundaryDescriptor descriptor =
            native_boundary(static_cast<NativeSurface>(index));
        if (descriptor.prefix.empty()) {
            continue;
        }
        std::copy(descriptor.prefix.begin(),
                  descriptor.prefix.end(),
                  evidence.prefixes[index].bytes.begin());
        evidence.prefixes[index].byte_count =
            static_cast<std::uint8_t>(descriptor.prefix.size());
    }
    CHECK(validate_runtime_admission(evidence) == RuntimeAdmissionResult::admitted);

    RuntimeAdmissionEvidence changed = evidence;
    changed.prefixes[static_cast<std::size_t>(NativeSurface::queue_tick)].bytes[0]
        ^= std::byte{1U};
    CHECK(validate_runtime_admission(changed)
          == RuntimeAdmissionResult::prefix_missing_or_mismatch);
    changed = evidence;
    changed.packed_runtime.file_bytes += 1U;
    CHECK(validate_runtime_admission(changed)
          == RuntimeAdmissionResult::packed_identity_mismatch);
    changed = evidence;
    changed.mapped_image_bytes = 0x1000U;
    CHECK(validate_runtime_admission(changed) == RuntimeAdmissionResult::target_out_of_range);
}

void exact_context_and_generation_stability() {
    const CaptureContext context = exact_context();
    CHECK(fully_correlated(context));
    CaptureContext partial = context;
    partial.presence_mask &= ~context_cui_instance_generation;
    CHECK(!fully_correlated(partial));
    CaptureContext wrongBuild = context;
    wrongBuild.build_identity.file_bytes += 1U;
    CHECK(!fully_correlated(wrongBuild));
    CaptureContext wrongActivity = context;
    wrongActivity.activity_index = 298U;
    CHECK(!fully_correlated(wrongActivity));

    CaptureHeader exact = header(CapturePhase::objective_insert,
                                 NativeSurface::objective_insert);
    CHECK(valid_capture_header(exact));
    exact.post_generations.queue += 1U;
    CHECK(!valid_capture_header(exact));
    exact = header(CapturePhase::objective_insert, NativeSurface::queue_tick);
    CHECK(!valid_capture_header(exact));
}

void type68_and_localized_pair_flow() {
    Type68AuthorityObservation authority{};
    authority.registry = kActivityRegistry;
    authority.type = kType68;
    authority.index = kType68Index;
    authority.component_class = kType68ComponentClass;
    authority.authority_schema = kType68AuthoritySchema;
    authority.definition = kType68Definition;
    authority.content_bank = kType68ContentBank;
    authority.body_bits = kType68WireBits;
    authority.decoded_bytes = kType68DecodedBytes;
    authority.decoded_body_hash = 0x12345678U;
    authority.event = kOpeningEvent;
    authority.selector = 0;
    authority.record_variant = 0;
    authority.decoded_lifecycle = 0;
    authority.canonical_round_trip = true;
    CHECK(valid_type68_authority(authority));
    CHECK(type68_hud_identity(kOpeningEvent, 0) == 0x32678D66U);
    authority.body_bits -= 1U;
    CHECK(!valid_type68_authority(authority));

    const FormatterObservation normalFormatter = formatter();
    const ObjectiveModelObservation normalModel = model();
    CHECK(valid_formatter_observation(normalFormatter));
    CHECK(valid_objective_model(normalFormatter, normalModel));
    CHECK(normalModel.entry_header == kOmegaTitle);
    CHECK(normalModel.first_subentry_pairs[0] == kOpeningPair1);
    const ObjectivePayload initial = objective_payload();
    CHECK(valid_objective_payload(initial));
    CHECK(payload_matches_model(initial, normalModel));
    CHECK(initial.command == 1U);
    CHECK(initial.command6_value == -1);
    CHECK(initial.bit1_field == 0U);

    const FormatterObservation modeOneFormatter = formatter(1U, -8);
    const ObjectiveModelObservation modeOneModel = model(1U);
    CHECK(valid_formatter_observation(modeOneFormatter));
    CHECK(valid_objective_model(modeOneFormatter, modeOneModel));
    CHECK(modeOneModel.entry_header == kOpeningPair1);
    CHECK(modeOneModel.first_subentry_pairs[0] == kOpeningPair2);
    CHECK(payload_matches_model(objective_payload(2U, 1U), modeOneModel));
    CHECK(valid_objective_payload(objective_payload(6U)));

    ObjectivePayload malformed = initial;
    malformed.raw[0x2CU] = std::byte{1U};
    malformed = project_objective_payload(1U, malformed.raw);
    CHECK(!valid_objective_payload(malformed));
}

void queue_serialization_and_rejections() {
    QueueEvidence accepted = accepted_objective_insert();
    CHECK(valid_queue_evidence(accepted));

    QueueEvidence unresolved{};
    unresolved.event = QueueEventKind::command5_insert;
    unresolved.disposition = QueueDisposition::rejected_definition_unresolved;
    unresolved.command = kMissionHeaderCommand;
    unresolved.pre = queue_snapshot(0U, -1);
    unresolved.post = unresolved.pre;
    unresolved.command5_payload = command5_payload();
    CHECK(valid_queue_evidence(unresolved));

    QueueEvidence full = accepted;
    full.disposition = QueueDisposition::rejected_full;
    full.pre = queue_snapshot(kNativeBannerQueueCapacity);
    full.post = full.pre;
    CHECK(valid_queue_evidence(full));

    QueueEvidence policy = accepted;
    policy.disposition = QueueDisposition::rejected_authored_policy;
    policy.pre = queue_snapshot(3U);
    policy.post = policy.pre;
    CHECK(valid_queue_evidence(policy));

    QueueEvidence mutated = policy;
    mutated.post = queue_snapshot(4U);
    CHECK(!valid_queue_evidence(mutated));

    const Command5Payload missionPayload = command5_payload();
    QueueEvidence promote{};
    promote.event = QueueEventKind::promote;
    promote.command = kMissionHeaderCommand;
    promote.pre = queue_snapshot(1U);
    promote.post = queue_snapshot(0U,
                                  7,
                                  10,
                                  static_cast<std::int32_t>(kMissionHeaderCommand),
                                  missionPayload.exact_payload_hash);
    promote.priority_row = priority(kMissionHeaderCommand);
    CHECK(valid_queue_evidence(promote));

    QueueEvidence supersede{};
    supersede.event = QueueEventKind::supersede;
    supersede.pre = promote.post;
    supersede.post = queue_snapshot(0U,
                                    7,
                                    10,
                                    static_cast<std::int32_t>(kMissionHeaderCommand),
                                    missionPayload.exact_payload_hash,
                                    1U);
    CHECK(valid_queue_evidence(supersede));

    QueueEvidence retire{};
    retire.event = QueueEventKind::retire;
    retire.pre = supersede.post;
    retire.post = queue_snapshot(0U);
    CHECK(valid_queue_evidence(retire));

    QueueEvidence erase{};
    erase.event = QueueEventKind::erase;
    erase.pre = queue_snapshot(2U);
    erase.post = queue_snapshot(1U);
    erase.erased_index = 1U;
    erase.erased_record_hash = 0x9999U;
    CHECK(valid_queue_evidence(erase));

    QueueEvidence reset{};
    reset.event = QueueEventKind::reset;
    reset.pre = promote.post;
    reset.post = queue_snapshot(0U, 7, -1, -1, 0U, 0U, 0);
    CHECK(valid_queue_evidence(reset));

    QueueEvidence teardown = reset;
    teardown.event = QueueEventKind::teardown;
    teardown.post = queue_snapshot(0U, -1, -1, -1, 0U, 0U, 0);
    CHECK(valid_queue_evidence(teardown));

    QueueEvidence dispatch{};
    dispatch.event = QueueEventKind::dispatch_load;
    dispatch.pre = queue_snapshot(0U, -1);
    dispatch.post = queue_snapshot(0U, 17);
    dispatch.dispatch.candidate_count = 2U;
    dispatch.dispatch.candidates[0] = {0x80000001U, 0x80800000U, 9, false};
    dispatch.dispatch.candidates[1] = {0x80000002U, 0x80800000U, 17, true};
    dispatch.dispatch.selected_tag = 0x80000002U;
    dispatch.dispatch.selected_class = 0x80800000U;
    dispatch.dispatch.selected_package_handle = 17;
    dispatch.dispatch.exact_enumeration_hash = 0x123U;
    CHECK(valid_queue_evidence(dispatch));
}

void manager_terminal_remove_and_teardown() {
    CHECK(map_type68_lifecycle(-1).external == 0);
    CHECK(map_type68_lifecycle(0).internal == 1);
    CHECK(map_type68_lifecycle(1).external == 5);
    CHECK(map_type68_lifecycle(9).internal == 3);

    ManagerEvidence add{};
    add.event = ManagerEventKind::add;
    add.pre = manager_snapshot(0U, -1, 0U);
    add.post = manager_snapshot(1U, 0, 0x100U);
    CHECK(valid_manager_evidence(add));

    ManagerEvidence terminal{};
    terminal.event = ManagerEventKind::terminal_predicate;
    terminal.pre = manager_snapshot(1U, 1);
    terminal.post = terminal.pre;
    terminal.terminal_predicate_result = true;
    CHECK(valid_manager_evidence(terminal));

    ManagerEvidence remove{};
    remove.event = ManagerEventKind::remove;
    remove.pre = terminal.pre;
    remove.post = manager_snapshot(0U, 1);
    remove.terminal_predicate_result = true;
    CHECK(valid_manager_evidence(remove));

    ManagerEvidence replacement{};
    replacement.event = ManagerEventKind::replacement_old_status;
    replacement.pre = manager_snapshot(1U, 2, 0x111U);
    replacement.post = manager_snapshot(1U, 0, 0x222U);
    replacement.old_status_sent_before_new_install = true;
    CHECK(valid_manager_evidence(replacement));

    ManagerEvidence teardown{};
    teardown.event = ManagerEventKind::component_teardown;
    teardown.pre = manager_snapshot(1U, 0);
    teardown.post = manager_snapshot(0U, -1);
    CHECK(valid_manager_evidence(teardown));
}

void dynamic_branch_and_node_binding_closure() {
    const ObjectivePayload payload = objective_payload();
    const ResourceNodeEvidence a = branch_resource(ObjectiveResourceBranch::branch_a);
    const ResourceNodeEvidence b = branch_resource(ObjectiveResourceBranch::branch_b);
    CHECK(valid_visible_variant(a));
    CHECK(valid_visible_variant(b));
    CHECK(kCompileTimeOmegaObjectiveBranch == ObjectiveResourceBranch::unknown);

    VariantClosureEvidence closure{};
    closure.visible_resource = a;
    closure.bindings[0] = binding(PayloadPairSlot::at_14,
                                  NodeSemantic::transient_title,
                                  ObjectiveResourceBranch::branch_a,
                                  payload);
    closure.bindings[1] = binding(PayloadPairSlot::at_04,
                                  NodeSemantic::transient_body,
                                  ObjectiveResourceBranch::branch_a,
                                  payload);
    closure.binding_count = 2U;
    CHECK(close_dynamic_variant(closure, payload)
          == VariantClosureResult::closed_branch_a);
    closure.visible_resource = b;
    closure.bindings[0] = binding(PayloadPairSlot::at_14,
                                  NodeSemantic::transient_title,
                                  ObjectiveResourceBranch::branch_b,
                                  payload);
    closure.bindings[1] = binding(PayloadPairSlot::at_04,
                                  NodeSemantic::transient_body,
                                  ObjectiveResourceBranch::branch_b,
                                  payload);
    CHECK(close_dynamic_variant(closure, payload)
          == VariantClosureResult::closed_branch_b);

    closure.binding_count = 1U;
    CHECK(close_dynamic_variant(closure, payload)
          == VariantClosureResult::incomplete_title_or_body);

    ResourceNodeEvidence control{};
    control.pre = resource_node(kControlAccessibilityTag,
                                kControlAccessibilityNewObjectiveHash,
                                1,
                                false,
                                0x80804832U);
    control.post = resource_node(kControlAccessibilityTag,
                                 kControlAccessibilityNewObjectiveHash,
                                 1,
                                 true,
                                 0x80804832U);
    control.observed_branch = ObjectiveResourceBranch::excluded_control_accessibility;
    closure.visible_resource = control;
    CHECK(!valid_visible_variant(control));
    CHECK(close_dynamic_variant(closure, payload)
          == VariantClosureResult::excluded_control_resource);

    MissionHeaderNodeEvidence missionTitle{};
    missionTitle.semantic = NodeSemantic::mission_title;
    missionTitle.node = resource_node(0x80BC7233U,
                                      0x47CAC8CFU,
                                      static_cast<std::int32_t>(kMissionHeaderCommand),
                                      true);
    missionTitle.resolved_pair = kOmegaTitle;
    missionTitle.resolved_text_hash = text_hash("Omega");
    CHECK(valid_mission_header_node(missionTitle));
    MissionHeaderNodeEvidence missionCategory = missionTitle;
    missionCategory.semantic = NodeSemantic::mission_category;
    missionCategory.resolved_pair = kMissionCatalogPair;
    missionCategory.resolved_text_hash = text_hash("Mission");
    missionCategory.node.visible_constant_hash = kMissionCatalogPair.hash;
    missionCategory.node = seal_resource_node(missionCategory.node);
    CHECK(valid_mission_header_node(missionCategory));
}

void shared_banner_serialization_and_ghost_independent_overlap() {
    const PresentationInterval mission{PresentationLane::mission_header_shared_banner,
                                       100U,
                                       200U,
                                       10,
                                       5,
                                       true};
    const PresentationInterval objective{PresentationLane::objective_shared_banner,
                                         201U,
                                         350U,
                                         11,
                                         1,
                                         true};
    const PresentationInterval ghost{PresentationLane::ghost_dialogue_independent,
                                     300U,
                                     500U,
                                     -1,
                                     -1,
                                     true};
    const PresentationInterval tracker{PresentationLane::persistent_objective_display,
                                       400U,
                                       700U,
                                       -1,
                                       -1,
                                       true};
    CHECK(assess_retail_timeline(mission, objective, ghost, tracker)
          == TimelineResult::serialized_with_ghost_overlap);

    PresentationInterval overlappingObjective = objective;
    overlappingObjective.first_tick = 200U;
    CHECK(assess_retail_timeline(mission, overlappingObjective, ghost, tracker)
          == TimelineResult::shared_banner_overlap_invalid);
    PresentationInterval stackedObjective = objective;
    stackedObjective.queue_sequence = mission.queue_sequence;
    CHECK(assess_retail_timeline(mission, stackedObjective, ghost, tracker)
          == TimelineResult::shared_banner_overlap_invalid);

    PresentationInterval laterGhost = ghost;
    laterGhost.first_tick = 800U;
    laterGhost.last_tick = 900U;
    CHECK(assess_retail_timeline(mission, objective, laterGhost, tracker)
          == TimelineResult::serialized_without_measured_ghost_overlap);
}

[[nodiscard]] CaptureRecord valid_queue_record(std::uint64_t callId = 77U) noexcept {
    CaptureRecord record{};
    record.header = header(CapturePhase::objective_insert,
                           NativeSurface::objective_insert,
                           callId);
    record.kind = CaptureKind::queue;
    record.queue = accepted_objective_insert();
    return record;
}

void fixed_nonblocking_scalar_hash_queue() {
    CaptureRecord record = valid_queue_record();
    CHECK(valid_capture_record(record));
    ScalarHashTelemetry telemetry{};
    CHECK(default_telemetry(record, telemetry));
    CHECK(telemetry.pre_hash == record.queue.pre.exact_state_hash);
    CHECK(telemetry.post_hash == record.queue.post.exact_state_hash);
    CHECK(telemetry.payload_or_resource_hash
          == record.queue.objective_payload.exact_payload_hash);
    CHECK(telemetry.command == 1U);

    FixedCaptureQueue<2U> queue;
    CHECK(queue.try_push(record) == QueuePushResult::enqueued);
    record.header.context.call_id += 1U;
    CHECK(queue.try_push(record) == QueuePushResult::enqueued);
    CHECK(queue.try_push(record) == QueuePushResult::full);
    CHECK(queue.try_reset(true) == QueueResetResult::not_empty);
    CaptureRecord popped{};
    CHECK(queue.try_pop(popped) == QueuePopResult::success);
    CHECK(popped.sequence == 1U);
    CHECK(queue.try_pop(popped) == QueuePopResult::success);
    CHECK(popped.sequence == 2U);
    CHECK(queue.try_pop(popped) == QueuePopResult::empty);
    CHECK(queue.try_reset(false) == QueueResetResult::not_confirmed_detached);
    CHECK(queue.try_reset(true) == QueueResetResult::reset);
    const QueueCounters resetCounters = queue.counters();
    CHECK(resetCounters.enqueued == 0U);
    CHECK(resetCounters.dropped_full == 0U);

    CHECK(queue.testing_lock());
    CHECK(queue.try_push(valid_queue_record()) == QueuePushResult::busy);
    queue.testing_unlock();

    FixedCaptureQueue<1U> exhausted;
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted.try_push(valid_queue_record()) == QueuePushResult::sequence_exhausted);

    CaptureRecord invalid = valid_queue_record();
    invalid.header.post_generations.queue += 1U;
    CHECK(queue.try_push(invalid) == QueuePushResult::invalid);
}

void original_once_participant_contracts() {
    for (std::uint8_t raw = static_cast<std::uint8_t>(ParticipantKind::command5_insert);
         raw <= static_cast<std::uint8_t>(ParticipantKind::cui_node_observer);
         ++raw) {
        const ParticipantContract contract =
            participant_contract(static_cast<ParticipantKind>(raw));
        CHECK(contract.original_required);
        CHECK(contract.required_original_calls == 1U);
        CHECK(!contract.may_write_native_state);
        CHECK(!contract.may_call_native_directly);
    }

    OriginalOnceParticipant participant{ParticipantKind::objective_insert};
    int originals{};
    int pre{};
    int post{};
    auto original = [&originals](int value) noexcept {
        ++originals;
        return value + 1;
    };
    auto preObserver = [&pre]() noexcept { ++pre; };
    auto postObserver = [&post]() noexcept { ++post; };

    CHECK(forward_value_original_once(participant,
                                      original,
                                      preObserver,
                                      postObserver,
                                      4)
          == 5);
    CHECK(originals == 1 && pre == 0 && post == 0);
    CHECK(!participant.activate(0U, 1U));
    CHECK(participant.activate(8U, 9U));
    CHECK(!participant.activate(8U, 10U));
    CHECK(forward_value_original_once(participant,
                                      original,
                                      preObserver,
                                      postObserver,
                                      5)
          == 6);
    CHECK(originals == 2 && pre == 1 && post == 1);

    OriginalOnceParticipant nestedParticipant{ParticipantKind::queue_tick};
    CHECK(nestedParticipant.activate(8U, 9U));
    {
        OriginalOnceParticipant::CallScope outer{participant};
        OriginalOnceParticipant::CallScope sameParticipantRecursion{participant};
        OriginalOnceParticipant::CallScope distinctParticipant{nestedParticipant};
        CHECK(outer.accepts_observation());
        CHECK(!sameParticipantRecursion.accepts_observation());
        CHECK(distinctParticipant.accepts_observation());
    }
    CHECK(nestedParticipant.begin_quiesce());
    CHECK(nestedParticipant.confirm_removed(true));
    CHECK(nestedParticipant.finalize_reset(true));

    {
        OriginalOnceParticipant::CallScope inFlight{participant};
        CHECK(inFlight.accepts_observation());
        CHECK(participant.begin_quiesce());
        CHECK(!participant.confirm_removed(true));
    }
    CHECK(participant.confirm_removed(true));
    CHECK(participant.snapshot().phase == ParticipantPhase::removed_pending_reset);
    CHECK(!participant.finalize_reset(false));
    CHECK(participant.finalize_reset(true));
    CHECK(participant.snapshot().phase == ParticipantPhase::detached);
    CHECK(participant.snapshot().admission_generation == 0U);
    CHECK(participant.snapshot().capture_epoch == 0U);

    CHECK(forward_value_original_once(participant,
                                      original,
                                      preObserver,
                                      postObserver,
                                      6)
          == 7);
    CHECK(originals == 3 && pre == 1 && post == 1);
}

void capture_record_phase_rejection_and_immutable_evidence() {
    CaptureRecord authorityRecord{};
    authorityRecord.header = header(CapturePhase::type68_apply, NativeSurface::type68_apply);
    authorityRecord.kind = CaptureKind::type68_authority;
    authorityRecord.authority = {kActivityRegistry,
                                 kType68,
                                 kType68Index,
                                 kType68ComponentClass,
                                 kType68AuthoritySchema,
                                 kType68Definition,
                                 kType68ContentBank,
                                 kType68WireBits,
                                 kType68DecodedBytes,
                                 0x123U,
                                 kOpeningEvent,
                                 0,
                                 7,
                                 0,
                                 true};
    CHECK(valid_capture_record(authorityRecord));
    CaptureRecord wrongPhase = authorityRecord;
    wrongPhase.header.phase = CapturePhase::type68_format;
    wrongPhase.header.surface = NativeSurface::type68_formatter;
    CHECK(!valid_capture_record(wrongPhase));

    CaptureRecord resourceRecord{};
    resourceRecord.header = header(CapturePhase::instantiated_resource,
                                   NativeSurface::instantiated_cui_node);
    resourceRecord.kind = CaptureKind::resource_node;
    resourceRecord.resource = branch_resource(ObjectiveResourceBranch::branch_a);
    const ResourceNodeSnapshot immutablePre = resourceRecord.resource.pre;
    const ResourceNodeSnapshot immutablePost = resourceRecord.resource.post;
    CHECK(valid_capture_record(resourceRecord));
    CHECK(resourceRecord.resource.pre == immutablePre);
    CHECK(resourceRecord.resource.post == immutablePost);
}

} // namespace

int main() {
    exact_identity_and_native_contracts();
    exact_resource_catalogs_and_unknown_branch();
    runtime_admission_requires_every_pinned_prefix();
    exact_context_and_generation_stability();
    type68_and_localized_pair_flow();
    queue_serialization_and_rejections();
    manager_terminal_remove_and_teardown();
    dynamic_branch_and_node_binding_closure();
    shared_banner_serialization_and_ghost_independent_overlap();
    fixed_nonblocking_scalar_hash_queue();
    original_once_participant_contracts();
    capture_record_phase_rejection_and_immutable_evidence();
    if (g_failures != 0) {
        std::cerr << g_failures << " objective-banner variant capture test(s) failed\n";
        return 1;
    }
    std::cout << "objective-banner variant capture tests passed\n";
    return 0;
}
