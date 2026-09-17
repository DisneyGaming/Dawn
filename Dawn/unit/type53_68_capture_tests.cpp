#include <Windows.h>

// Standalone unit: intentionally outside src so recursive production globs cannot compile it.
#define DAWN_OPENING_AUTHORITY_CAPTURE_TEST 1

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <thread>
#include <type_traits>

#include "client/hooks/bootflow/opening_authority/type53_68_capture.h"

namespace {

using namespace dawn::client::hooks::bootflow::opening_authority;
namespace activity_lifecycle = dawn::client::hooks::activity_lifecycle;
namespace activity = dawn::state::activity;
inline constexpr std::uintptr_t kOpaqueResolverToken = 0x1112131415161718ULL;

static_assert(!kOwnsNativeDetour && !kPerformsIo && !kDefaultTelemetryContainsRawBodies);
static_assert(sizeof(DialogueDefaultLogFields) < kDialogueBodyBytes);
static_assert(sizeof(DirectiveDefaultLogFields) < kDirectiveBodyBytes);
static_assert(sizeof(PacketReference16) == 16U);
static_assert(offsetof(PacketReference16, resolver_input) == 8U);
static_assert(sizeof(EntryWrapperFields12) == 12U);
static_assert(offsetof(EntryWrapperFields12, resolver_input) == 4U);
static_assert(std::is_trivially_copyable_v<DialogueApplyRecord>);
static_assert(std::is_trivially_copyable_v<DirectiveApplyRecord>);
static_assert(std::is_same_v<DialogueAuthorityApply,
                             void(__fastcall*)(void*, const PacketReference16*) noexcept>);

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
        ++g_failureCount;
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename Value>
void write_value(std::byte* destination, const Value& value) noexcept {
    std::memcpy(destination, &value, sizeof value);
}

template <typename Object, typename Value>
[[nodiscard]] bool object_contains_value(const Object& object, const Value& value) noexcept {
    const auto* const objectBytes = reinterpret_cast<const unsigned char*>(&object);
    const auto* const valueBytes = reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t offset = 0U; offset + sizeof value <= sizeof object; ++offset) {
        if (std::memcmp(objectBytes + offset, valueBytes, sizeof value) == 0) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] RuntimeCohortEvidence build_evidence() noexcept {
    return {{kPinnedPackedDiskBytes, kPinnedPackedDiskSha256},
            kPinnedMappedSizeOfImage,
            0x9D412F35179302A5ULL,
            true,
            true,
            true,
            true,
            true};
}

[[nodiscard]] CaptureMetadata metadata(CaptureWindow window,
                                       std::uint64_t callId = 1U,
                                       std::uint64_t epoch = 7U,
                                       std::uint64_t tick = 11U) noexcept {
    const CaptureWindowDescriptor descriptor = capture_window(window);
    return {build_evidence(),
            epoch,
            tick,
            callId,
            0x4455U,
            0x123456U,
            window,
            descriptor.phase,
            descriptor.rva};
}

[[nodiscard]] CaptureContext exact_context(std::uint64_t activationGeneration = 2U,
                                           std::uint64_t correlation = 9U) noexcept {
    return {kKnownContextPresenceMask,
            {{activity::ModuleGeneration{1U},
              activity::ActivationGeneration{activationGeneration}},
             0xBADC0FFEE0DDF00DULL,
             0xE1234567U},
            0xA001B002C003D004ULL,
            {0x9080706050403020ULL, activity::ActivityIncarnation{3U}},
            activity::RosterGraphGeneration{4U},
            activity::PublicationGeneration{5U},
            6U,
            correlation,
            activity_lifecycle::NativeActivationState::active};
}

[[nodiscard]] DialogueBody canonical_dialogue(std::uint32_t generation = 17U) noexcept {
    DialogueDecodedLayout layout{};
    layout.root.raw[0] = std::byte{1U};
    layout.records[0].value = 0xAABBCCDDEEFF0011ULL;
    layout.records[0].reference.raw[0] = std::byte{0xADU};
    layout.records[0].generation = generation;
    layout.records[0].mode = 1U;
    DialogueBody body{};
    std::memcpy(body.data(), &layout, sizeof layout);
    return body;
}

[[nodiscard]] DirectiveBody canonical_directive(std::int32_t selector = -1) noexcept {
    DirectiveDecodedLayout layout{};
    layout.entries[0].event_key = 0xC252E306U;
    layout.entries[0].discriminator = 7;
    layout.entries[0].lifecycle = -1;
    layout.entries[1].event_key = 0xAD60F465U;
    layout.entries[1].lifecycle = 0;
    layout.entries[2].event_key = 0x11223344U;
    layout.entries[2].lifecycle = 1;
    layout.selector = selector;
    DirectiveBody body{};
    std::memcpy(body.data(), &layout, sizeof layout);
    return body;
}

template <typename Body, std::size_t InstanceBytes>
struct ApplyFixture final {
    std::array<std::byte, InstanceBytes> instance{};
    Body decoded{};
    PacketReference16 packet{};
    void set_cache(const Body& body) noexcept {
        std::memcpy(instance.data() + kPcAuthorityCacheOffset, body.data(), body.size());
    }
};
using DialogueFixture = ApplyFixture<DialogueBody, kPcAuthorityCacheOffset + kDialogueBodyBytes>;
using DirectiveFixture = ApplyFixture<DirectiveBody, kPcAuthorityCacheOffset + kDirectiveBodyBytes>;

[[nodiscard]] DialogueFixture dialogue_fixture() noexcept {
    DialogueFixture fixture{};
    fixture.decoded = canonical_dialogue();
    fixture.set_cache(canonical_dialogue(3U));
    write_value(fixture.instance.data(), kDialogueDefinition);
    fixture.packet.schema = kDialogueAuthoritySchema;
    fixture.packet.ignored_gap_04.fill(std::byte{0xA5U});
    fixture.packet.resolver_input = kOpaqueResolverToken;
    return fixture;
}

[[nodiscard]] DirectiveFixture directive_fixture() noexcept {
    DirectiveFixture fixture{};
    fixture.decoded = canonical_directive();
    fixture.set_cache(canonical_directive(0));
    write_value(fixture.instance.data(), kDirectiveDefinition);
    fixture.packet.schema = kDirectiveAuthoritySchema;
    fixture.packet.ignored_gap_04.fill(std::byte{0x5AU});
    fixture.packet.resolver_input = kOpaqueResolverToken;
    return fixture;
}

template <typename Body>
[[nodiscard]] const void* fake_native_resolver(std::uintptr_t opaqueInput,
                                               const Body& decodedResult) noexcept {
    CHECK(opaqueInput == kOpaqueResolverToken);
    return decodedResult.data();
}

template <typename Fixture>
void fake_native_original_commit(Fixture& fixture) noexcept {
    fixture.set_cache(fixture.decoded);
}

[[nodiscard]] DialogueApplyRecord dialogue_record(
    std::uint64_t callId = 1U,
    const CaptureContext& entryContext = {},
    const CaptureContext& resolverContext = {},
    const CaptureContext& exitContext = {},
    std::uint64_t epoch = 7U) noexcept {
    DialogueFixture fixture = dialogue_fixture();
    PendingDialogueCapture pending;
    DialogueApplyRecord record{};
    CHECK(capture_entry_wrapper(pending, entryContext,
                                metadata(CaptureWindow::type53_apply_entry, callId, epoch),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    CHECK(capture_resolver_result(pending, resolverContext,
                                  metadata(CaptureWindow::type53_resolver_return, callId, epoch),
                                  fake_native_resolver(fixture.packet.resolver_input,
                                                       fixture.decoded))
          == CaptureBuildResult::resolver_captured);
    fake_native_original_commit(fixture);
    CHECK(capture_post_commit(pending, exitContext,
                              metadata(CaptureWindow::type53_post_commit, callId, epoch),
                              fixture.instance.data(), record)
          == CaptureBuildResult::complete);
    return record;
}

[[nodiscard]] DirectiveApplyRecord directive_record(
    std::uint64_t callId = 1U,
    const CaptureContext& entryContext = {},
    const CaptureContext& resolverContext = {},
    const CaptureContext& exitContext = {},
    std::uint64_t epoch = 7U) noexcept {
    DirectiveFixture fixture = directive_fixture();
    PendingDirectiveCapture pending;
    DirectiveApplyRecord record{};
    CHECK(capture_entry_wrapper(pending, entryContext,
                                metadata(CaptureWindow::type68_apply_entry, callId, epoch),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    CHECK(capture_resolver_result(pending, resolverContext,
                                  metadata(CaptureWindow::type68_resolver_return, callId, epoch),
                                  fake_native_resolver(fixture.packet.resolver_input,
                                                       fixture.decoded))
          == CaptureBuildResult::resolver_captured);
    fake_native_original_commit(fixture);
    CHECK(capture_post_commit(pending, exitContext,
                              metadata(CaptureWindow::type68_post_commit, callId, epoch),
                              fixture.instance.data(), record)
          == CaptureBuildResult::complete);
    return record;
}

struct RuntimeClassRecord final { std::uint64_t classId; std::uintptr_t table; std::uint64_t count; };
struct RuntimeHandlerRecord final { std::uintptr_t function; std::uint64_t key; };

inline constexpr std::array<NativeSurface, 19U> kAllSurfaces{
    NativeSurface::type53_apply, NativeSurface::type53_tick,
    NativeSurface::type53_selected_row_dispatch, NativeSurface::type53_terminal_wrapper,
    NativeSurface::type53_terminal_core, NativeSurface::type53_presentation_start,
    NativeSurface::type68_create_tail, NativeSurface::type68_lifecycle_a,
    NativeSurface::type68_lifecycle_b_update, NativeSurface::type68_apply,
    NativeSurface::type68_same_identity_reconcile, NativeSurface::type68_install,
    NativeSurface::manager_ready, NativeSurface::manager_get, NativeSurface::manager_add,
    NativeSurface::manager_status_update, NativeSurface::manager_entry_materialize,
    NativeSurface::manager_terminal_predicate, NativeSurface::manager_materialize_walk};

void initialize_exact_mapped_image(std::byte* mapped) noexcept {
    write_value(mapped, std::uint16_t{0x5A4DU});
    write_value(mapped + 0x3CU, std::uint32_t{0x100U});
    write_value(mapped + 0x100U, std::uint32_t{0x00004550U});
    write_value(mapped + 0x104U, kPinnedPeMachine);
    write_value(mapped + 0x106U, kPinnedPeSections);
    write_value(mapped + 0x108U, kPinnedPeTimestamp);
    write_value(mapped + 0x114U, kPinnedPeOptionalHeaderBytes);
    write_value(mapped + 0x116U, kPinnedPeCharacteristics);
    constexpr std::size_t optional = 0x118U;
    write_value(mapped + optional, std::uint16_t{0x20BU});
    write_value(mapped + optional + 16U, kPinnedPeEntryPointRva);
    write_value(mapped + optional + 24U,
                static_cast<std::uint64_t>(kPinnedUnpackedReferenceImageBase));
    write_value(mapped + optional + 32U, kPinnedPeSectionAlignment);
    write_value(mapped + optional + 36U, kPinnedPeFileAlignment);
    write_value(mapped + optional + 56U,
                static_cast<std::uint32_t>(kPinnedMappedSizeOfImage));
    write_value(mapped + optional + 60U, kPinnedPeHeadersBytes);
    write_value(mapped + optional + 68U, kPinnedPeSubsystem);
    write_value(mapped + optional + 70U, kPinnedPeDllCharacteristics);
    for (const NativeSurface surface : kAllSurfaces) {
        const NativeBoundaryDescriptor boundary = native_boundary(surface);
        std::memcpy(mapped + boundary.rva, boundary.prefix.data(), boundary.prefix.size());
    }
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(mapped);
    write_value(mapped + kDialogueClassRecordRva,
                RuntimeClassRecord{kDialogueComponentClass,
                                   base + kDialogueHandlerTableRva, 2U});
    write_value(mapped + kDirectiveClassRecordRva,
                RuntimeClassRecord{kDirectiveComponentClass,
                                   base + kDirectiveHandlerTableRva, 4U});
    const std::array<RuntimeHandlerRecord, 2U> dialogueHandlers{
        RuntimeHandlerRecord{base + 0x1009B60U, kAuthorityDispatchKey},
        RuntimeHandlerRecord{base + 0x100A180U, kTickDispatchKey}};
    const std::array<RuntimeHandlerRecord, 4U> directiveHandlers{
        RuntimeHandlerRecord{base + 0x1009610U, kCreateDispatchKey},
        RuntimeHandlerRecord{base + 0x10091F0U, kTickDispatchKey},
        RuntimeHandlerRecord{base + 0x100A3A0U, kTickDispatchKey},
        RuntimeHandlerRecord{base + 0x1009C00U, kAuthorityDispatchKey}};
    std::memcpy(mapped + kDialogueHandlerTableRva, dialogueHandlers.data(), sizeof dialogueHandlers);
    std::memcpy(mapped + kDirectiveHandlerTableRva, directiveHandlers.data(), sizeof directiveHandlers);
}

void artifact_and_runtime_cohort_contract() {
    CHECK(kPinnedPackedDiskSha256 != kPinnedUnpackedReferenceSha256);
    CHECK(matches_pinned_packed_disk({kPinnedPackedDiskBytes, kPinnedPackedDiskSha256}));
    CHECK(!matches_pinned_packed_disk({kPinnedUnpackedReferenceBytes,
                                      kPinnedUnpackedReferenceSha256}));
    CHECK(matches_pinned_unpacked_reference({kPinnedUnpackedReferenceBytes,
                                             kPinnedUnpackedReferenceSha256}));
    auto* const mapped = static_cast<std::byte*>(VirtualAlloc(nullptr,
                                                              kPinnedMappedSizeOfImage,
                                                              MEM_COMMIT | MEM_RESERVE,
                                                              PAGE_EXECUTE_READWRITE));
    CHECK(mapped != nullptr);
    if (mapped == nullptr) {
        return;
    }
    initialize_exact_mapped_image(mapped);
    std::array<std::uintptr_t, kAllSurfaces.size()> addresses{};
    RuntimeCohortEvidence evidence{};
    RuntimeImageView image{{mapped, kPinnedMappedSizeOfImage},
                           {kPinnedPackedDiskBytes, kPinnedPackedDiskSha256}, true, true};
    CHECK(validate_runtime_cohort(image, kAllSurfaces, addresses, evidence)
          == RuntimeCohortResult::valid);
    CHECK(evidence.pe_identity_valid && evidence.class_tables_valid
          && evidence.all_prefixes_valid);
    for (std::size_t index = 0U; index < addresses.size(); ++index) {
        CHECK(addresses[index] == reinterpret_cast<std::uintptr_t>(mapped)
                                      + native_boundary(kAllSurfaces[index]).rva);
    }
    auto duplicates = kAllSurfaces;
    duplicates[1] = duplicates[0];
    CHECK(validate_runtime_cohort(image, duplicates, addresses, evidence)
          == RuntimeCohortResult::duplicate_surface);
    CHECK(addresses[0] == 0U);
    mapped[0x108U] ^= std::byte{1U};
    CHECK(validate_runtime_cohort(image, kAllSurfaces, addresses, evidence)
          == RuntimeCohortResult::pe_identity_mismatch);
    mapped[0x108U] ^= std::byte{1U};
    const std::uintptr_t applyRva = native_boundary(NativeSurface::type68_apply).rva;
    mapped[applyRva] ^= std::byte{1U};
    CHECK(validate_runtime_cohort(image, kAllSurfaces, addresses, evidence)
          == RuntimeCohortResult::prefix_mismatch);
    mapped[applyRva] ^= std::byte{1U};
    mapped[kDirectiveClassRecordRva] ^= std::byte{1U};
    CHECK(validate_runtime_cohort(image, kAllSurfaces, addresses, evidence)
          == RuntimeCohortResult::class_table_mismatch);
    mapped[kDirectiveClassRecordRva] ^= std::byte{1U};
    const NativeBoundaryDescriptor apply = native_boundary(NativeSurface::type53_apply);
    DWORD oldProtection{};
    CHECK(VirtualProtect(mapped + apply.rva, apply.prefix.size(), PAGE_READWRITE,
                         &oldProtection) != FALSE);
    CHECK(validate_runtime_cohort(image, kAllSurfaces, addresses, evidence)
          == RuntimeCohortResult::page_contract_failed);
    DWORD ignoredProtection{};
    CHECK(VirtualProtect(mapped + apply.rva, apply.prefix.size(), oldProtection,
                         &ignoredProtection) != FALSE);
    image.packed_disk = {kPinnedUnpackedReferenceBytes, kPinnedUnpackedReferenceSha256};
    CHECK(validate_runtime_cohort(image, kAllSurfaces, addresses, evidence)
          == RuntimeCohortResult::packed_disk_identity_mismatch);
    CHECK(VirtualFree(mapped, 0U, MEM_RELEASE) != FALSE);
}

void exact_boundaries_windows_and_abi() {
    constexpr std::array<std::uintptr_t, kAllSurfaces.size()> expectedRvas{
        0x1009B60U, 0x100A180U, 0x10097D0U, 0xA38490U, 0xA38530U,
        0xA3D710U, 0x1009610U, 0x10091F0U, 0x100A3A0U, 0x1009C00U,
        0x10098C0U, 0x1009ED0U, 0x137E1D0U, 0x137D6F0U, 0x137BD50U,
        0x137E2C0U, 0x1382710U, 0x137DC10U, 0x1382C30U};
    for (std::size_t index = 0U; index < kAllSurfaces.size(); ++index) {
        const NativeBoundaryDescriptor boundary = native_boundary(kAllSurfaces[index]);
        CHECK(boundary.rva == expectedRvas[index]);
        CHECK(!boundary.prefix.empty());
        CHECK(native_prefix_matches(kAllSurfaces[index], boundary.prefix));
    }
    const NativeBoundaryDescriptor getter = native_boundary(NativeSurface::manager_get);
    CHECK(getter.exact_active_bytes == 12U && !getter.inline_detour_14_safe);
    CHECK(native_boundary(NativeSurface::type53_apply).abi == NativeAbi::instance_packet);
    CHECK(native_boundary(NativeSurface::type68_apply).abi == NativeAbi::instance_packet);

    struct ExpectedWindow final { CaptureWindow window; std::uintptr_t rva; CapturePhase phase; };
    constexpr std::array<ExpectedWindow, 18U> expected{
        ExpectedWindow{CaptureWindow::type53_apply_entry, 0x1009B60U, CapturePhase::apply_entry},
        ExpectedWindow{CaptureWindow::type53_resolver_return, 0x1009B8DU, CapturePhase::resolver_return},
        ExpectedWindow{CaptureWindow::type53_post_commit, 0x1009BF9U, CapturePhase::post_commit},
        ExpectedWindow{CaptureWindow::type53_terminal_pre, 0x100A34EU, CapturePhase::pre_call},
        ExpectedWindow{CaptureWindow::type53_terminal_post, 0x100A353U, CapturePhase::post_call},
        ExpectedWindow{CaptureWindow::type53_generation_consumed, 0x100A35EU, CapturePhase::generation_consumed},
        ExpectedWindow{CaptureWindow::type53_terminal_wrapper, 0xA38490U, CapturePhase::pre_call},
        ExpectedWindow{CaptureWindow::type53_terminal_core, 0xA38530U, CapturePhase::pre_call},
        ExpectedWindow{CaptureWindow::type53_presentation_start, 0xA3D710U, CapturePhase::presentation_start},
        ExpectedWindow{CaptureWindow::type68_apply_entry, 0x1009C00U, CapturePhase::apply_entry},
        ExpectedWindow{CaptureWindow::type68_resolver_return, 0x1009C35U, CapturePhase::resolver_return},
        ExpectedWindow{CaptureWindow::type68_content_return, 0x1009C40U, CapturePhase::content_return},
        ExpectedWindow{CaptureWindow::type68_post_commit, 0x1009DF3U, CapturePhase::post_commit},
        ExpectedWindow{CaptureWindow::type68_post_refresh, 0x1009E0FU, CapturePhase::post_refresh},
        ExpectedWindow{CaptureWindow::type68_manager_add_pre, 0x1009FEDU, CapturePhase::pre_call},
        ExpectedWindow{CaptureWindow::type68_manager_add_post, 0x1009FF2U, CapturePhase::post_call},
        ExpectedWindow{CaptureWindow::type68_alternate_add_pre, 0x100A0BCU, CapturePhase::pre_call},
        ExpectedWindow{CaptureWindow::type68_alternate_add_post, 0x100A0C1U, CapturePhase::post_call}};
    for (const ExpectedWindow item : expected) {
        const CaptureWindowDescriptor actual = capture_window(item.window);
        CHECK(actual.rva == item.rva && actual.phase == item.phase
              && actual.instruction_window_proven);
    }
    CHECK(capture_window(CaptureWindow::type68_alternate_builder_return).rva == 0x100A0A6U);
    CHECK(capture_window(CaptureWindow::type68_manager_materialize).rva == 0x1382C73U);
    CHECK(capture_window(CaptureWindow::type68_manager_terminal).rva == 0x1383204U);
    CHECK(capture_window(CaptureWindow::type53_presentation_end_unrecovered).rva == 0U);
    CHECK(!capture_window(CaptureWindow::type53_presentation_end_unrecovered)
               .instruction_window_proven);
    CaptureMetadata requiredTelemetry = metadata(CaptureWindow::type53_apply_entry, 3U);
    CHECK(valid_capture_metadata(requiredTelemetry));
    requiredTelemetry.monotonic_tick = 0U;
    CHECK(!valid_capture_metadata(requiredTelemetry));
    requiredTelemetry = metadata(CaptureWindow::type53_apply_entry, 3U);
    requiredTelemetry.caller_rva = kPinnedMappedSizeOfImage;
    CHECK(!valid_capture_metadata(requiredTelemetry));
    requiredTelemetry = metadata(CaptureWindow::type53_apply_entry, 3U);
    requiredTelemetry.phase = CapturePhase::post_commit;
    CHECK(!valid_capture_metadata(requiredTelemetry));
}

void context_and_canonical_contract() {
    const CaptureContext exact = exact_context();
    CHECK(exact_owner_at_entry(exact) && fully_correlated(exact));
    CHECK(same_owner_current_at_exit(exact, exact));
    CaptureContext changed = exact;
    changed.activation.key.generation.value++;
    CHECK(!same_owner_current_at_exit(exact, changed));
    changed = exact;
    changed.roster_generation.value++;
    CHECK(!same_owner_current_at_exit(exact, changed));
    changed = exact;
    changed.authority_publication.value++;
    CHECK(!same_owner_current_at_exit(exact, changed));
    changed = exact;
    changed.activation_state = activity_lifecycle::NativeActivationState::quiescing;
    CHECK(!exact_owner_at_entry(changed));
    changed = exact;
    changed.presence_mask &= ~static_cast<std::uint32_t>(ContextPresence::activity);
    CHECK(!fully_correlated(changed));
    CHECK(!exact_owner_at_entry({}) && !fully_correlated({}));

    CHECK(sizeof(DialogueDecodedLayout) == 0x1008U);
    CHECK(sizeof(DirectiveDecodedLayout) == 0x300U);
    CHECK(offsetof(DirectiveDecodedLayout, selector) == 0x2F8U);
    CHECK(canonical_dialogue_body(canonical_dialogue()));
    DialogueBody dialogue = canonical_dialogue();
    DialogueDecodedLayout dialogueLayout{};
    std::memcpy(&dialogueLayout, dialogue.data(), sizeof dialogueLayout);
    dialogueLayout.records[127].mode = 4U;
    std::memcpy(dialogue.data(), &dialogueLayout, sizeof dialogueLayout);
    CHECK(!canonical_dialogue_body(dialogue));
    DialogueRecordFields dialogueFields{};
    CHECK(dialogue_record_fields(canonical_dialogue(), 0U, dialogueFields));
    CHECK(dialogueFields.generation == 17U && dialogueFields.mode == 1U);
    CHECK(!dialogue_record_fields(canonical_dialogue(), kDialogueRecordCount, dialogueFields));
    for (std::int32_t selector = -1; selector <= 2; ++selector) {
        CHECK(directive_selector_valid(selector));
        CHECK(canonical_directive_body(canonical_directive(selector)));
    }
    CHECK(!directive_selector_valid(-2) && !directive_selector_valid(3));
    CHECK(!canonical_directive_body(canonical_directive(-2)));
    CHECK(!canonical_directive_body(canonical_directive(3)));
    CHECK(directive_lifecycle_valid(-1) && directive_lifecycle_valid(0)
          && directive_lifecycle_valid(1));
    CHECK(!directive_lifecycle_valid(-2) && !directive_lifecycle_valid(2));
    DirectiveEntryFields entry{};
    CHECK(directive_entry_fields(canonical_directive(), 0U, entry));
    CHECK(entry.event_key == 0xC252E306U && entry.lifecycle == -1);
    CHECK(!directive_entry_fields(canonical_directive(), kDirectiveEntryCount, entry));
}

void staged_resolver_and_lifetime_contract() {
    DialogueFixture fixture = dialogue_fixture();
    PendingDialogueCapture entryChecks;
    PacketReference16 wrongSchema = fixture.packet;
    wrongSchema.schema = kDirectiveAuthoritySchema;
    CHECK(capture_entry_wrapper(entryChecks, {},
                                metadata(CaptureWindow::type53_apply_entry, 39U),
                                fixture.instance.data(), &wrongSchema)
          == CaptureBuildResult::wrong_schema);
    write_value(fixture.instance.data(), kDirectiveDefinition);
    CHECK(capture_entry_wrapper(entryChecks, {},
                                metadata(CaptureWindow::type53_apply_entry, 39U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::wrong_definition);
    write_value(fixture.instance.data(), kDialogueDefinition);
    PendingDialogueCapture pending;
    DialogueApplyRecord record{};
    const CaptureContext context = exact_context();
    std::array<int, 5U> order{};
    std::size_t orderCount{};
    order[orderCount++] = 1;
    CHECK(capture_entry_wrapper(pending, context,
                                metadata(CaptureWindow::type53_apply_entry, 40U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    // The fake resolver consumes an unreadable opaque token and returns unrelated storage.
    CHECK(fixture.packet.resolver_input == kOpaqueResolverToken);
    order[orderCount++] = 2;
    const void* const decodedRax =
        fake_native_resolver(fixture.packet.resolver_input, fixture.decoded);
    CHECK(capture_resolver_result(pending, context,
                                  metadata(CaptureWindow::type53_resolver_return, 40U),
                                  decodedRax)
          == CaptureBuildResult::resolver_captured);
    order[orderCount++] = 3;
    fake_native_original_commit(fixture);
    order[orderCount++] = 4;
    CHECK(capture_post_commit(pending, context,
                              metadata(CaptureWindow::type53_post_commit, 40U),
                              fixture.instance.data(), record)
          == CaptureBuildResult::complete);
    order[orderCount++] = 5;
    constexpr std::array<int, 5U> expectedOrder{1, 2, 3, 4, 5};
    CHECK(order == expectedOrder);
    CHECK(record.entry_wrapper.resolver_input == kOpaqueResolverToken);
    CHECK(record.resolver_body_pre == fixture.decoded);
    CHECK(record.post_commit_cache == fixture.decoded);
    CHECK(record.valid.native_resolver_observed && record.valid.native_resolver_succeeded);
    CHECK(record.valid.decoded_copy_valid && record.valid.decoded_canonical);
    CHECK(record.valid.cache_before_valid && record.valid.cache_after_valid);
    CHECK(record.valid.same_owner_current_at_resolver
          && record.valid.same_owner_current_at_exit);
    CHECK(valid_raw_record(record));

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const std::size_t allocationBytes =
        ((kDialogueBodyBytes + systemInfo.dwPageSize - 1U) / systemInfo.dwPageSize)
        * systemInfo.dwPageSize;
    auto* const transient = static_cast<std::byte*>(VirtualAlloc(nullptr, allocationBytes,
                                                                 MEM_COMMIT | MEM_RESERVE,
                                                                 PAGE_READWRITE));
    CHECK(transient != nullptr);
    if (transient != nullptr) {
        const DialogueBody expected = canonical_dialogue(91U);
        std::memcpy(transient, expected.data(), expected.size());
        PendingDialogueCapture lifetimePending;
        CHECK(capture_entry_wrapper(lifetimePending, {},
                                    metadata(CaptureWindow::type53_apply_entry, 41U),
                                    fixture.instance.data(), &fixture.packet)
              == CaptureBuildResult::entry_ready);
        CHECK(capture_resolver_result(lifetimePending, {},
                                      metadata(CaptureWindow::type53_resolver_return, 41U),
                                      transient)
              == CaptureBuildResult::resolver_captured);
        DWORD oldProtection{};
        CHECK(VirtualProtect(transient, allocationBytes, PAGE_NOACCESS, &oldProtection) != FALSE);
        fixture.set_cache(canonical_dialogue(92U));
        DialogueApplyRecord lifetimeRecord{};
        CHECK(capture_post_commit(lifetimePending, {},
                                  metadata(CaptureWindow::type53_post_commit, 41U),
                                  fixture.instance.data(), lifetimeRecord)
              == CaptureBuildResult::complete);
        CHECK(lifetimeRecord.resolver_body_pre == expected);
        DWORD ignoredProtection{};
        CHECK(VirtualProtect(transient, allocationBytes, oldProtection,
                             &ignoredProtection) != FALSE);
        CHECK(VirtualFree(transient, 0U, MEM_RELEASE) != FALSE);
    }
}

void unreadable_cross_page_and_phase_fail_closed() {
    DirectiveFixture fixture = directive_fixture();
    PendingDirectiveCapture schemaChecks;
    PacketReference16 wrongSchema = fixture.packet;
    wrongSchema.schema = kDialogueAuthoritySchema;
    CHECK(capture_entry_wrapper(schemaChecks, {},
                                metadata(CaptureWindow::type68_apply_entry, 49U),
                                fixture.instance.data(), &wrongSchema)
          == CaptureBuildResult::wrong_schema);
    const std::uint32_t savedDefinition = kDirectiveDefinition;
    write_value(fixture.instance.data(), kDialogueDefinition);
    CHECK(capture_entry_wrapper(schemaChecks, {},
                                metadata(CaptureWindow::type68_apply_entry, 49U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::wrong_definition);
    write_value(fixture.instance.data(), savedDefinition);
    CaptureMetadata invalidMetadata = metadata(CaptureWindow::type68_apply_entry, 49U);
    invalidMetadata.capture_epoch = 0U;
    CHECK(capture_entry_wrapper(schemaChecks, {}, invalidMetadata,
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::invalid_metadata);
    PendingDirectiveCapture pending;
    CHECK(capture_entry_wrapper(pending, {},
                                metadata(CaptureWindow::type68_apply_entry, 50U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    CHECK(capture_resolver_result(pending, {},
                                  metadata(CaptureWindow::type68_resolver_return, 50U),
                                  reinterpret_cast<const void*>(1U))
          == CaptureBuildResult::resolver_failed);
    DirectiveApplyRecord fallback{};
    CHECK(capture_post_commit(pending, {},
                              metadata(CaptureWindow::type68_post_commit, 50U),
                              fixture.instance.data(), fallback)
          == CaptureBuildResult::partial);
    CHECK(valid_raw_record(fallback));
    CHECK(fallback.valid.native_resolver_observed
          && fallback.valid.native_resolver_succeeded);
    CHECK(!fallback.valid.decoded_copy_valid && fallback.valid.cache_after_valid);

    PendingDirectiveCapture noResolver;
    CHECK(capture_entry_wrapper(noResolver, {},
                                metadata(CaptureWindow::type68_apply_entry, 500U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    DirectiveApplyRecord cacheOnly{};
    CHECK(capture_post_commit(noResolver, {},
                              metadata(CaptureWindow::type68_post_commit, 500U),
                              fixture.instance.data(), cacheOnly)
          == CaptureBuildResult::partial);
    CHECK(valid_raw_record(cacheOnly));
    CHECK(!cacheOnly.valid.native_resolver_observed && !cacheOnly.valid.decoded_copy_valid
          && cacheOnly.valid.cache_after_valid);
    DirectiveApplyRecord fabricatedResolver = cacheOnly;
    fabricatedResolver.telemetry.resolver =
        metadata(CaptureWindow::type68_resolver_return, 500U);
    CHECK(!valid_raw_record(fabricatedResolver));

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    auto* const pages = static_cast<std::byte*>(VirtualAlloc(nullptr,
                                                             2U * systemInfo.dwPageSize,
                                                             MEM_COMMIT | MEM_RESERVE,
                                                             PAGE_READWRITE));
    CHECK(pages != nullptr);
    if (pages != nullptr) {
        DWORD oldProtection{};
        CHECK(VirtualProtect(pages + systemInfo.dwPageSize, systemInfo.dwPageSize,
                             PAGE_NOACCESS, &oldProtection) != FALSE);
        PendingDirectiveCapture crossPage;
        CHECK(capture_entry_wrapper(crossPage, {},
                                    metadata(CaptureWindow::type68_apply_entry, 51U),
                                    fixture.instance.data(), &fixture.packet)
              == CaptureBuildResult::entry_ready);
        CHECK(capture_resolver_result(crossPage, {},
                                      metadata(CaptureWindow::type68_resolver_return, 51U),
                                      pages + systemInfo.dwPageSize - 0x100U)
              == CaptureBuildResult::resolver_failed);
        CHECK(capture_post_commit(crossPage, {},
                                  metadata(CaptureWindow::type68_post_commit, 51U),
                                  fixture.instance.data(), fallback)
              == CaptureBuildResult::partial);
        CHECK(!fallback.valid.decoded_copy_valid);
        DWORD ignoredProtection{};
        CHECK(VirtualProtect(pages + systemInfo.dwPageSize, systemInfo.dwPageSize,
                             oldProtection, &ignoredProtection) != FALSE);
        CHECK(VirtualFree(pages, 0U, MEM_RELEASE) != FALSE);
    }
    PendingDirectiveCapture wrongPhase;
    CHECK(capture_entry_wrapper(wrongPhase, {},
                                metadata(CaptureWindow::type68_apply_entry, 52U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    CHECK(capture_resolver_result(wrongPhase, {},
                                  metadata(CaptureWindow::type68_resolver_return, 53U),
                                  fake_native_resolver(fixture.packet.resolver_input,
                                                       fixture.decoded))
          == CaptureBuildResult::phase_mismatch);
    CHECK(capture_resolver_result(wrongPhase, {},
                                  metadata(CaptureWindow::type68_resolver_return, 52U),
                                  fake_native_resolver(fixture.packet.resolver_input,
                                                       fixture.decoded))
          == CaptureBuildResult::resolver_captured);
    CHECK(capture_post_commit(wrongPhase, {},
                              metadata(CaptureWindow::type68_post_commit, 52U),
                              fixture.instance.data() + 1U, fallback)
          == CaptureBuildResult::instance_mismatch);
    CHECK(capture_post_commit(wrongPhase, {},
                              metadata(CaptureWindow::type68_post_commit, 52U),
                              fixture.instance.data(), fallback) == CaptureBuildResult::consumed);
    PendingDirectiveCapture unreadableEntry;
    CHECK(capture_entry_wrapper(unreadableEntry, {},
                                metadata(CaptureWindow::type68_apply_entry, 54U),
                                fixture.instance.data(),
                                reinterpret_cast<const PacketReference16*>(1U))
          == CaptureBuildResult::unreadable);
    CHECK(capture_entry_wrapper(unreadableEntry, {},
                                metadata(CaptureWindow::type68_apply_entry, 54U),
                                nullptr, &fixture.packet) == CaptureBuildResult::null_pointer);
}

void raw_canonical_default_and_privacy_lanes() {
    const DialogueApplyRecord raw = dialogue_record(60U);
    CHECK(valid_raw_record(raw));
    CHECK(!fully_correlated(raw.entry_context) && !semantic_dedupe_key(raw).valid);
    DialogueApplyRecord hiddenField = raw;
    hiddenField.entry_context.native_identity = 0x1234U;
    CHECK(!valid_raw_record(hiddenField));
    DialogueDefaultLogFields dialogueLog{};
    CHECK(default_log_fields(raw, dialogueLog));
    CHECK(dialogueLog.entry_context.presence_mask == 0U);
    CHECK(!raw.valid.cache_before_valid);
    const CaptureContext owner = exact_context();
    DialogueApplyRecord owned = dialogue_record(61U, owner, owner, owner);
    CHECK(semantic_dedupe_key(owned).valid);
    CHECK(default_log_fields(owned, dialogueLog));
    CHECK(dialogueLog.telemetry.entry.build.packed_disk.sha256 == kPinnedPackedDiskSha256);
    CHECK(dialogueLog.telemetry.entry.capture_epoch == 7U);
    CHECK(dialogueLog.telemetry.resolver.native_rva == 0x1009B8DU);
    CHECK(dialogueLog.telemetry.commit.native_rva == 0x1009BF9U);
    CHECK(dialogueLog.entry_context.activation_key == owner.activation.key);
    CHECK(dialogueLog.entry_context.complete_activation_token_present);
    CHECK(!object_contains_value(dialogueLog, owner.activation.wrapper));
    CHECK(!object_contains_value(dialogueLog, owned.instance_identity));
    CHECK(!object_contains_value(dialogueLog, owned.entry_wrapper.resolver_input));
    CaptureContext replacement = owner;
    replacement.activity.incarnation.value++;
    DialogueApplyRecord replaced = dialogue_record(611U, owner, owner, replacement);
    CHECK(valid_raw_record(replaced));
    CHECK(replaced.valid.exact_owner_at_entry
          && !replaced.valid.same_owner_current_at_exit);

    DialogueFixture fixture = dialogue_fixture();
    DialogueDecodedLayout invalidLayout{};
    std::memcpy(&invalidLayout, fixture.decoded.data(), sizeof invalidLayout);
    invalidLayout.records[64].mode = 9U;
    std::memcpy(fixture.decoded.data(), &invalidLayout, sizeof invalidLayout);
    PendingDialogueCapture pending;
    CHECK(capture_entry_wrapper(pending, owner,
                                metadata(CaptureWindow::type53_apply_entry, 62U),
                                fixture.instance.data(), &fixture.packet)
          == CaptureBuildResult::entry_ready);
    CHECK(capture_resolver_result(pending, owner,
                                  metadata(CaptureWindow::type53_resolver_return, 62U),
                                  fake_native_resolver(fixture.packet.resolver_input,
                                                       fixture.decoded))
          == CaptureBuildResult::resolver_captured);
    fake_native_original_commit(fixture);
    DialogueApplyRecord anomalous{};
    CHECK(capture_post_commit(pending, owner,
                              metadata(CaptureWindow::type53_post_commit, 62U),
                              fixture.instance.data(), anomalous) == CaptureBuildResult::complete);
    CHECK(valid_raw_record(anomalous) && !anomalous.valid.decoded_canonical);
    CHECK(!semantic_dedupe_key(anomalous).valid);
    CHECK(!default_log_fields(anomalous, dialogueLog));
    DirectiveDefaultLogFields directiveLog{};
    const DirectiveApplyRecord directive = directive_record(63U, owner, owner, owner);
    CHECK(directive.resolver_body_pre == canonical_directive());
    CHECK(default_log_fields(directive, directiveLog));
    CHECK(directiveLog.entries[0].event_key == 0xC252E306U);
    CHECK(directiveLog.entries[0].lifecycle == -1
          && directiveLog.entries[1].lifecycle == 0
          && directiveLog.entries[2].lifecycle == 1);
    CHECK(directiveLog.selector == -1);
}

template <typename Record>
void set_record_call(Record& record, std::uint64_t callId, std::uint64_t tick) noexcept {
    record.telemetry.entry.call_id = callId;
    record.telemetry.resolver.call_id = callId;
    record.telemetry.commit.call_id = callId;
    record.telemetry.entry.monotonic_tick = tick;
    record.telemetry.resolver.monotonic_tick = tick + 1U;
    record.telemetry.commit.monotonic_tick = tick + 2U;
}

void queue_dedupe_loss_exhaustion_and_epoch() {
    DialogueCaptureQueue absentQueue;
    const DialogueApplyRecord absent = dialogue_record(70U);
    CHECK(absentQueue.try_push(absent) == QueuePushResult::enqueued);
    CHECK(absentQueue.try_push(absent) == QueuePushResult::enqueued);
    CHECK(absentQueue.counters().semantic_duplicates == 0U);
    const CaptureContext owner = exact_context();
    DialogueCaptureQueue semanticQueue;
    DialogueApplyRecord owned = dialogue_record(71U, owner, owner, owner);
    CHECK(semanticQueue.try_push(owned) == QueuePushResult::enqueued);
    DialogueApplyRecord retry = owned;
    set_record_call(retry, 72U, 90U);
    CHECK(semanticQueue.try_push(retry) == QueuePushResult::semantic_duplicate);
    DialogueApplyRecord newEpoch = dialogue_record(73U, owner, owner, owner, 8U);
    CHECK(semanticQueue.try_push(newEpoch) == QueuePushResult::enqueued);
    const CaptureContext successor = exact_context(3U);
    DialogueApplyRecord samePointerSuccessor =
        dialogue_record(74U, successor, successor, successor, 8U);
    samePointerSuccessor.instance_identity = owned.instance_identity;
    CHECK(valid_raw_record(samePointerSuccessor));
    CHECK(semanticQueue.try_push(samePointerSuccessor) == QueuePushResult::enqueued);

    FixedCaptureQueue<DialogueApplyRecord, 2U> tinyQueue;
    CHECK(tinyQueue.try_push(absent) == QueuePushResult::enqueued);
    CHECK(tinyQueue.try_push(absent) == QueuePushResult::enqueued);
    CHECK(tinyQueue.try_push(absent) == QueuePushResult::full);
    DialogueApplyRecord invalid = absent;
    invalid.entry_wrapper.schema = 0U;
    CHECK(tinyQueue.try_push(invalid) == QueuePushResult::rejected);
    QueueCounters counters = tinyQueue.counters();
    CHECK(counters.accepted == 2U && counters.dropped_full == 1U
          && counters.rejected == 1U && counters.pending == 2U);
    CHECK(counters.losses() == 2U);
    DialogueCaptureQueue busy;
    CHECK(busy.testing_lock());
    CHECK(busy.try_push(absent) == QueuePushResult::busy);
    busy.testing_unlock();
    CHECK(busy.counters().dropped_busy == 1U);
    DialogueCaptureQueue exhausted;
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)() - 1U);
    CHECK(exhausted.try_push(absent) == QueuePushResult::enqueued);
    CHECK(exhausted.try_push(absent) == QueuePushResult::sequence_exhausted);
    counters = exhausted.counters();
    CHECK(counters.accepted == 1U && counters.dropped_sequence_exhausted == 1U);
    DialogueApplyRecord popped{};
    CHECK(exhausted.try_pop(popped) == QueuePopResult::success);
    CHECK(popped.sequence == (std::numeric_limits<std::uint64_t>::max)() - 1U);
}

void queue_real_mpsc_consumer_accounting() {
    constexpr std::size_t producerCount = 4U;
    constexpr std::size_t attemptsPerProducer = 48U;
    constexpr std::size_t attempts = producerCount * attemptsPerProducer;
    FixedCaptureQueue<DialogueApplyRecord, 16U> queue;
    const DialogueApplyRecord base = dialogue_record(80U);
    std::barrier start(static_cast<std::ptrdiff_t>(producerCount + 2U));
    std::atomic<std::size_t> producersFinished{};
    std::atomic<std::size_t> poppedCount{};
    std::atomic<bool> sequenceOrdered{true};
    std::array<std::thread, producerCount> producers{};
    for (std::size_t producer = 0U; producer < producerCount; ++producer) {
        producers[producer] = std::thread([&, producer]() noexcept {
            start.arrive_and_wait();
            for (std::size_t item = 0U; item < attemptsPerProducer; ++item) {
                DialogueApplyRecord record = base;
                const std::uint64_t id = 1000U + producer * attemptsPerProducer + item;
                set_record_call(record, id, id * 3U);
                static_cast<void>(queue.try_push(record));
            }
            producersFinished.fetch_add(1U, std::memory_order_release);
        });
    }
    std::thread consumer([&]() noexcept {
        start.arrive_and_wait();
        std::uint64_t lastSequence{};
        while (producersFinished.load(std::memory_order_acquire) != producerCount
               || queue.counters().pending != 0U) {
            DialogueApplyRecord output{};
            if (queue.try_pop(output) == QueuePopResult::success) {
                if (output.sequence <= lastSequence) {
                    sequenceOrdered.store(false, std::memory_order_relaxed);
                }
                lastSequence = output.sequence;
                poppedCount.fetch_add(1U, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });
    start.arrive_and_wait();
    for (std::thread& producer : producers) {
        producer.join();
    }
    consumer.join();
    const QueueCounters counters = queue.counters();
    CHECK(counters.accepted == poppedCount.load(std::memory_order_relaxed));
    CHECK(counters.accepted + counters.dropped_busy + counters.dropped_full == attempts);
    CHECK(counters.semantic_duplicates == 0U && counters.rejected == 0U
          && counters.dropped_sequence_exhausted == 0U && counters.pending == 0U);
    CHECK(sequenceOrdered.load(std::memory_order_relaxed));
}

[[nodiscard]] ManagerSnapshot manager_snapshot(std::uint32_t count = 2U) noexcept {
    ManagerSnapshot snapshot{};
    snapshot.count = count;
    snapshot.readiness_true = true;
    snapshot.aligned_identity = true;
    snapshot.exact_owner_current = true;
    snapshot.valid = true;
    for (std::size_t index = 0U; index < snapshot.entries.size(); ++index) {
        snapshot.entries[index] = {kManagerEntryKind,
                                   0xAA000000U + static_cast<std::uint32_t>(index),
                                   static_cast<std::int32_t>(index), -1};
    }
    return snapshot;
}

void terminal_content_transition_and_manager_surfaces() {
    const CaptureContext context = exact_context();
    Type53TerminalEvent terminal{};
    terminal.metadata = metadata(CaptureWindow::type53_terminal_pre, 90U);
    terminal.context = context;
    terminal.static_consumer = kDialogueProvenance;
    terminal.record_index = 0U;
    terminal.record.mode = 1U;
    terminal.record.generation = 12U;
    terminal.selector = 4U;
    terminal.bank_handle = kDialogueBank;
    terminal.processed_generation_before = 11U;
    CHECK(valid_type53_terminal_event(terminal));
    terminal.metadata = metadata(CaptureWindow::type53_terminal_post, 90U);
    CHECK(!valid_type53_terminal_event(terminal));
    terminal.terminal_handle = 0x123400U;
    terminal.terminal_handle_valid = true;
    CHECK(valid_type53_terminal_event(terminal));
    terminal.metadata = metadata(CaptureWindow::type53_generation_consumed, 90U);
    terminal.generation_store_observed = true;
    terminal.processed_generation_after = terminal.record.generation;
    CHECK(valid_type53_terminal_event(terminal));
    terminal.processed_generation_after++;
    CHECK(!valid_type53_terminal_event(terminal));
    Type53PresentationEvent presentation{};
    presentation.metadata = metadata(CaptureWindow::type53_presentation_start, 91U);
    presentation.context = context;
    presentation.terminal_call_id = 90U;
    presentation.terminal_handle = 0x123400U;
    presentation.presentation_handle = 0x567800U;
    presentation.stage = PresentationStage::start;
    presentation.explicit_handle_correlation = true;
    CHECK(valid_type53_presentation_event(presentation));
    presentation.stage = PresentationStage::end_or_teardown_unrecovered;
    presentation.metadata = metadata(CaptureWindow::type53_presentation_end_unrecovered, 92U);
    CHECK(!valid_type53_presentation_event(presentation));
    Type68ContentEvent content{metadata(CaptureWindow::type68_content_return, 93U),
                               context, kDirectiveDefinition, kDirectiveBank, 0xBEEFU, true};
    CHECK(valid_type68_content_event(content));
    content.context = {};
    CHECK(!valid_type68_content_event(content));
    Type68TransitionEvent transition{};
    transition.metadata = metadata(CaptureWindow::type68_install_pre, 94U);
    transition.context = context;
    transition.kind = Type68TransitionKind::install;
    transition.record_index = 1U;
    transition.event_key = 0xC252E306U;
    transition.lifecycle = 0;
    transition.inside_exact_apply_call = true;
    CHECK(valid_type68_transition_event(transition));
    transition.metadata = metadata(CaptureWindow::type68_install_post, 94U);
    CHECK(!valid_type68_transition_event(transition));
    transition.outcome_observed = true;
    CHECK(valid_type68_transition_event(transition));
    transition.inside_exact_apply_call = false;
    CHECK(!valid_type68_transition_event(transition));

    ManagerSnapshot before = manager_snapshot();
    ManagerSnapshot after = before;
    after.entries[1].status = 4;
    CHECK(valid_manager_snapshot(before));
    ManagerSnapshot invalid = before;
    invalid.count = static_cast<std::uint32_t>(kManagerMaximumEntries + 1U);
    CHECK(!valid_manager_snapshot(invalid));
    invalid = before;
    invalid.entries[0].kind = 3U;
    CHECK(!valid_manager_snapshot(invalid));
    invalid = before;
    invalid.aligned_identity = false;
    CHECK(!valid_manager_snapshot(invalid));
    Type68ManagerDeltaEvent delta{};
    delta.metadata = metadata(CaptureWindow::type68_manager_add_post, 95U);
    delta.context = context;
    delta.before = before;
    delta.after = after;
    delta.event_hash = 0xC252E306U;
    delta.inside_exact_apply_call = true;
    delta.add_outcome_observed = true;
    CHECK(valid_type68_manager_delta_event(delta));
    delta.inside_exact_apply_call = false;
    CHECK(!valid_type68_manager_delta_event(delta));
    delta.inside_exact_apply_call = true;
    delta.add_outcome_observed = false;
    CHECK(!valid_type68_manager_delta_event(delta));
    delta.add_outcome_observed = true;
    delta.metadata = metadata(CaptureWindow::type68_alternate_add_post, 95U);
    CHECK(valid_type68_manager_delta_event(delta));
}

} // namespace

int main() {
    artifact_and_runtime_cohort_contract();
    exact_boundaries_windows_and_abi();
    context_and_canonical_contract();
    staged_resolver_and_lifetime_contract();
    unreadable_cross_page_and_phase_fail_closed();
    raw_canonical_default_and_privacy_lanes();
    queue_dedupe_loss_exhaustion_and_epoch();
    queue_real_mpsc_consumer_accounting();
    terminal_content_transition_and_manager_surfaces();
    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " Type-53/68 capture check(s) failed\n";
        return 1;
    }
    std::cout << "all remediated Type-53/68 capture checks passed\n";
    return 0;
}
