#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#include <type_traits>
#include <vector>

#include "client/hooks/bootflow/opening_authority/type69_directive_producer_capture.h"

namespace {

using namespace dawn::client::hooks::bootflow::opening_authority::type69_capture;

static_assert(std::is_trivially_copyable_v<ResolverCaptureRecord>);
static_assert(std::is_trivially_copyable_v<DispatchCaptureRecord>);
static_assert(std::is_trivially_copyable_v<CallsiteCaptureRecord>);
static_assert(sizeof(Type69Payload) == 0x20U);

std::atomic_int gFailureCount{};
std::atomic_uint32_t gResolverCalls{};
std::atomic_uint32_t gDispatchCalls{};
std::atomic_uint32_t gFullSubscriberCalls{};
std::atomic_uint32_t gShortSubscriberCalls{};
std::atomic_uint32_t gFullBefore{};
std::atomic_uint32_t gFullAfter{};
std::array<float, 4U> gObservedFullPayload{};
std::uint8_t gObservedOperation{};
std::int32_t gObservedResolved{};
std::uint32_t gObservedAuxiliary{};

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    gFailureCount.fetch_add(1, std::memory_order_relaxed);
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

std::uint8_t __fastcall fake_resolver(std::uint16_t*,
                                     std::byte*,
                                     std::uint8_t prior) noexcept {
    gResolverCalls.fetch_add(1U, std::memory_order_relaxed);
    return static_cast<std::uint8_t>(prior + 1U);
}

void __fastcall fake_dispatch(void*, const std::byte*, void*) noexcept {
    gDispatchCalls.fetch_add(1U, std::memory_order_relaxed);
}

void __fastcall fake_full_subscriber(void*,
                                     std::uint8_t operation,
                                     std::int32_t resolved,
                                     const float* payload4,
                                     std::uint32_t auxiliary) noexcept {
    gFullSubscriberCalls.fetch_add(1U, std::memory_order_relaxed);
    gObservedOperation = operation;
    gObservedResolved = resolved;
    gObservedAuxiliary = auxiliary;
    std::memcpy(gObservedFullPayload.data(), payload4, sizeof gObservedFullPayload);
}

void __fastcall fake_short_subscriber(void*, std::uint32_t auxiliary) noexcept {
    gShortSubscriberCalls.fetch_add(1U, std::memory_order_relaxed);
    gObservedAuxiliary = auxiliary;
}

OwnerGenerationSet exact_generations(std::uint64_t registryGeneration = 11U) noexcept {
    const std::uint32_t presence =
        kNativeRequiredOwnerMask | static_cast<std::uint32_t>(OwnerField::authority)
        | static_cast<std::uint32_t>(OwnerField::component);
    return OwnerGenerationSet{presence,
                              17U,
                              19U,
                              41U,
                              7U,
                              5U,
                              9U,
                              registryGeneration,
                              13U,
                              15U,
                              23U,
                              29U,
                              31U,
                              ActivationSnapshotState::current};
}

CaptureTiming timing(std::uint64_t tick = 100U) noexcept {
    return CaptureTiming{tick, GetCurrentThreadId(), 0x1234U};
}

Type69Payload make_payload(std::int32_t endpointIndex = 2) noexcept {
    Type69Payload result{};
    result.endpoint_index = endpointIndex;
    result.auxiliary = 0xAABBCCDDU;
    result.operation = 7U;
    result.unclaimed_09 = {std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
    result.selectable_low = 17;
    result.selectable_high = 19;
    result.payload_xyz = {1.5F, 2.5F, 3.5F};
    return result;
}

void write_payload(std::byte* destination, const Type69Payload& value) noexcept {
    std::memcpy(destination, &value, sizeof value);
}

struct RuntimeFixture final {
    std::uint16_t runtime_id{0x1234U};
    RuntimeRecordImage record{};

    explicit RuntimeFixture(std::uint8_t type = kType69) noexcept {
        record[kRuntimeRecordTypeOffset] = std::byte{type};
        write_payload(record.data() + kRuntimeRecordBodyOffset, make_payload());
    }
};

struct DescriptorFixture final {
    std::array<std::byte, kActionTypeOffset + 1U> bytes{};

    explicit DescriptorFixture(std::int32_t endpointIndex = 2,
                               std::uint8_t type = kType69) noexcept {
        write_payload(bytes.data(), make_payload(endpointIndex));
        bytes[kActionTypeOffset] = std::byte{type};
    }
};

struct InterfaceFixture final {
    std::array<std::byte, 0xA0U> datum{};
    std::array<std::byte, 8U> object{};
    std::array<std::uintptr_t, 2U> pair{};

    InterfaceFixture() noexcept {
        const std::uintptr_t metadataOffset = 0x40U;
        std::memcpy(datum.data() + 0x18U, &metadataOffset, sizeof metadataOffset);
        const std::uintptr_t fullMethod =
            reinterpret_cast<std::uintptr_t>(&fake_full_subscriber);
        const std::uintptr_t shortMethod =
            reinterpret_cast<std::uintptr_t>(&fake_short_subscriber);
        std::memcpy(datum.data() + metadataOffset + kFullSubscriberMethodSlot,
                    &fullMethod,
                    sizeof fullMethod);
        std::memcpy(datum.data() + metadataOffset + kShortSubscriberMethodSlot,
                    &shortMethod,
                    sizeof shortMethod);
        pair = {reinterpret_cast<std::uintptr_t>(datum.data()),
                reinterpret_cast<std::uintptr_t>(object.data())};
    }
};

struct EndpointRegistrationFixture final {
    std::array<std::byte, 0xE0U> datum{};
    std::array<std::byte, 8U> object{};
    std::array<std::byte, 0x58U> row{};
    std::array<std::byte, 0x90U> owner{};

    EndpointRegistrationFixture() noexcept {
        for (std::size_t index = 0U; index < kEndpointRuntimeKeyBytes; ++index) {
            row[kEndpointRuntimeKeyOffset + index] =
                std::byte{static_cast<std::uint8_t>(0x40U + index)};
        }
        const std::uintptr_t metadataOffset = 0x40U;
        std::memcpy(datum.data() + 0x18U, &metadataOffset, sizeof metadataOffset);
        const std::uintptr_t registrationMethod =
            reinterpret_cast<std::uintptr_t>(&fake_dispatch);
        std::memcpy(datum.data() + metadataOffset + kEndpointRegistrationMethodSlot,
                    &registrationMethod,
                    sizeof registrationMethod);
        const std::array<std::uintptr_t, 2U> pair{
            reinterpret_cast<std::uintptr_t>(datum.data()),
            reinterpret_cast<std::uintptr_t>(object.data())};
        std::memcpy(row.data() + kEndpointInterfaceHalfOffset, pair.data(), sizeof pair);
        const std::uint32_t containerHandle = 0x10203040U;
        const std::uint32_t uncleared = 0xFFFFFFFFU;
        std::memcpy(owner.data() + kEndpointOwnerContainerHandleOffset,
                    &containerHandle,
                    sizeof containerHandle);
        std::memcpy(owner.data() + kEndpointOwnerClearedOffset,
                    &uncleared,
                    sizeof uncleared);
    }
};

std::uintptr_t original_for(NativeParticipant participant) noexcept {
    switch (participant) {
    case NativeParticipant::resolver:
        return reinterpret_cast<std::uintptr_t>(&fake_resolver);
    case NativeParticipant::full_dispatch:
    case NativeParticipant::short_dispatch:
        return reinterpret_cast<std::uintptr_t>(&fake_dispatch);
    case NativeParticipant::full_callsite:
        return reinterpret_cast<std::uintptr_t>(&fake_full_subscriber);
    case NativeParticipant::short_callsite:
        return reinterpret_cast<std::uintptr_t>(&fake_short_subscriber);
    default:
        return reinterpret_cast<std::uintptr_t>(&fake_dispatch);
    }
}

struct OwnerHarness final {
    CaptureCohortOwner owner{};

    explicit OwnerHarness(std::uint64_t epoch = 1U) noexcept {
        const ValidatedRuntimeCohort cohort = CaptureTestAccess::validated_cohort();
        CHECK(owner.begin_install(cohort, epoch) == OwnerResult::success);
        for (std::size_t index = 0U; index < kNativeParticipantCount; ++index) {
            const NativeParticipant participant = static_cast<NativeParticipant>(index);
            CHECK(owner.publish_original(participant, original_for(participant))
                  == OwnerResult::success);
        }
        CHECK(owner.publish_generations(exact_generations()) == OwnerResult::success);
        CHECK(owner.start_running() == OwnerResult::success);
    }

    ParticipantLease enter(NativeParticipant participant,
                           std::uint64_t tick = 100U) noexcept {
        ParticipantLease lease;
        CHECK(owner.try_enter(participant, timing(tick), lease) == OwnerResult::success);
        return lease;
    }
};

ResolverCaptureRecord capture_resolver(ParticipantLease& lease,
                                       RuntimeFixture& fixture,
                                       std::uint64_t postTick = 110U) noexcept {
    PendingResolverCapture pending;
    ResolverCaptureRecord output{};
    CHECK(prepare_resolver_capture(pending,
                                   lease,
                                   &fixture.runtime_id,
                                   fixture.record.data(),
                                   9U) == CaptureBuildResult::ready);
    CHECK(finish_resolver_capture(pending,
                                  lease,
                                  fixture.record.data(),
                                  1U,
                                  postTick,
                                  output) == CaptureBuildResult::complete);
    return output;
}

void artifact_backed_identity_and_every_surface_prefix() {
    FileDigestInspection packedDigest{};
    FileDigestInspection unpackedDigest{};
    FileDigestInspection packageDigest{};
    CHECK(inspect_file_digest(kPinnedPackedRuntimePath, packedDigest)
          == ArtifactInspectionResult::complete);
    CHECK(inspect_file_digest(kPinnedUnpackedProvenancePath, unpackedDigest)
          == ArtifactInspectionResult::complete);
    CHECK(inspect_file_digest(kPinnedOmegaPackagePath, packageDigest)
          == ArtifactInspectionResult::complete);
    CHECK(packedDigest.file_bytes == kPinnedPackedRuntimeBytes);
    CHECK(packedDigest.sha256 == kPinnedPackedRuntimeSha256);
    CHECK(unpackedDigest.file_bytes == kPinnedUnpackedProvenanceBytes);
    CHECK(unpackedDigest.sha256 == kPinnedUnpackedProvenanceSha256);
    CHECK(packageDigest.file_bytes == kPinnedOmegaPackageBytes);
    CHECK(packageDigest.sha256 == kPinnedOmegaPackageSha256);

    ArtifactInspection packedPe{};
    ArtifactInspection unpackedPe{};
    CHECK(inspect_file_artifact(kPinnedPackedRuntimePath, packedPe)
          == ArtifactInspectionResult::complete);
    CHECK(inspect_file_artifact(kPinnedUnpackedProvenancePath, unpackedPe)
          == ArtifactInspectionResult::complete);
    CHECK(packedPe.pe_size_of_image == kPinnedPcSizeOfImage);
    CHECK(unpackedPe.pe_size_of_image == kPinnedPcSizeOfImage);
    CHECK(packedPe.pe_machine == kPinnedPcMachineAmd64);
    CHECK(unpackedPe.optional_header_magic == kPinnedPe32PlusMagic);

    HANDLE file = CreateFileW(kPinnedUnpackedProvenancePath,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    CHECK(file != INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE) {
        for (std::size_t index = 0U; index < kPcSurfaceCount; ++index) {
            const PcSurface surface = static_cast<PcSurface>(index);
            const PcBoundaryDescriptor boundary = pc_boundary(surface);
            CHECK(boundary.rva != 0U);
            CHECK(!boundary.inline_detour_authorized);
            LARGE_INTEGER offset{};
            offset.QuadPart = static_cast<LONGLONG>(boundary.rva);
            CHECK(SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) != FALSE);
            std::array<std::byte, 24U> observed{};
            DWORD read{};
            CHECK(ReadFile(file,
                           observed.data(),
                           static_cast<DWORD>(boundary.prefix.size()),
                           &read,
                           nullptr) != FALSE);
            CHECK(read == boundary.prefix.size());
            CHECK(pc_prefix_matches(surface, {observed.data(), read}));
        }
        CHECK(CloseHandle(file) != FALSE);
    }

    ValidatedRuntimeCohort live{};
    CHECK(validate_live_pc_runtime_cohort(live)
          == LiveCohortValidationResult::packed_path_mismatch);
    CHECK(!live.valid());
}

void static_authored_metadata_never_becomes_a_live_binding() {
    CHECK(kOpeningDirectiveRow.name_hash == kOpeningDirectiveBank.event_key);
    CHECK(kOpeningDirectiveRow.slot == 5U);
    CHECK(kOpeningDirectiveRow.type == 69U);
    CHECK(kDownstreamType68Identity.authority_schema == 0x80804F67U);
    CHECK(kDownstreamType68Identity.fixed_wire_bits == 4'802U);
    CHECK(kShortFormTerminalClassification == EvidenceState::inferred_high);
    CHECK(kAuthoredSlotToMaterializedEndpointIndex == EvidenceState::unknown);
    CHECK(kConcreteSlot30SubscriberIdentity == EvidenceState::unknown);
    CHECK(kSlot30ToType68Publication == EvidenceState::unknown);
    CHECK(kOpeningType68Body == EvidenceState::unknown);
    const MaterializationObservationToken token{};
    CHECK(!token.valid());
    CHECK(token.value() == 0U);
    CHECK(!kWritesNativeState);
    CHECK(!kCallsType68ApplyDirectly);
    CHECK(!kCallsType68InstallDirectly);
    CHECK(!kGuessesUserInterface);
}

void owner_issued_generations_are_revalidated_at_queue_publication() {
    OwnerHarness harness;
    RuntimeFixture fixture;
    ParticipantLease lease = harness.enter(NativeParticipant::resolver);
    ResolverCaptureRecord record = capture_resolver(lease, fixture);
    CHECK(record.provenance.exact_owner_at_entry);
    CHECK(record.provenance.owner_validation == OwnerValidationState::not_published);
    CHECK(record.provenance.binding == BindingState::materialization_unobserved);
    CHECK(record.provenance.materialization_token == 0U);

    ResolverCaptureQueue queue;
    CHECK(queue.try_publish(record, lease, 111U) == QueuePushResult::enqueued);
    ResolverCaptureRecord drained{};
    CHECK(queue.try_pop(drained) == QueuePopResult::success);
    CHECK(drained.provenance.owner_validation == OwnerValidationState::exact_same_owner);
    CHECK(drained.provenance.same_owner_at_publication);
    CHECK(drained.sequence == 1U);
    ResolverTelemetry fields{};
    CHECK(default_telemetry(drained, fields));
    CHECK(fields.provenance.build_id != 0U);
    CHECK(fields.provenance.cohort_id != 0U);
    CHECK(fields.provenance.queue_sequence == 1U);
    CHECK(fields.provenance.entry_surface == PcSurface::runtime_resolver_entry);
    CHECK(fields.provenance.entry_rva
          == pc_boundary(PcSurface::runtime_resolver_entry).rva);
    CHECK(fields.provenance.return_rva == 0U);
    CHECK(fields.provenance.caller_rva == record.provenance.caller_rva);
    CHECK(fields.provenance.owner_entry == record.provenance.owner_entry);
    CHECK(fields.provenance.owner_exit == drained.provenance.owner_exit);
    CHECK(fields.provenance.platform == StaticInterfacePlatform::pc_win64);
    CHECK(fields.provenance.binding == BindingState::materialization_unobserved);
    CHECK(fields.provenance.duration_ticks == 11U);

    RuntimeFixture reused;
    ParticipantLease staleLease = harness.enter(NativeParticipant::resolver, 200U);
    ResolverCaptureRecord stale = capture_resolver(staleLease, reused, 210U);
    CHECK(harness.owner.publish_generations(exact_generations(12U)) == OwnerResult::success);
    CHECK(queue.try_publish(stale, staleLease, 211U) == QueuePushResult::enqueued);
    CHECK(queue.try_pop(drained) == QueuePopResult::success);
    CHECK(drained.provenance.owner_validation == OwnerValidationState::readable_but_stale);
    CHECK(!drained.provenance.same_owner_at_publication);
    CHECK(drained.provenance.owner_entry.registry_materialization_generation == 11U);
    CHECK(drained.provenance.owner_exit.registry_materialization_generation == 12U);
}

void exact_capture_windows_preserve_partial_and_negative_raw_evidence() {
    OwnerHarness harness;
    RuntimeFixture nonType69{0x44U};
    ParticipantLease resolverLease = harness.enter(NativeParticipant::resolver);
    PendingResolverCapture resolverPending;
    CHECK(prepare_resolver_capture(resolverPending,
                                   resolverLease,
                                   &nonType69.runtime_id,
                                   nonType69.record.data(),
                                   0U) == CaptureBuildResult::wrong_type);

    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    auto* const protectedRecord = static_cast<std::byte*>(VirtualAlloc(
        nullptr, info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    CHECK(protectedRecord != nullptr);
    if (protectedRecord != nullptr) {
        RuntimeFixture fixture;
        std::memcpy(protectedRecord, fixture.record.data(), fixture.record.size());
        CHECK(prepare_resolver_capture(resolverPending,
                                       resolverLease,
                                       &fixture.runtime_id,
                                       protectedRecord,
                                       0U) == CaptureBuildResult::ready);
        DWORD oldProtection{};
        CHECK(VirtualProtect(protectedRecord,
                             info.dwPageSize,
                             PAGE_NOACCESS,
                             &oldProtection) != FALSE);
        ResolverCaptureRecord partial{};
        CHECK(finish_resolver_capture(resolverPending,
                                      resolverLease,
                                      protectedRecord,
                                      1U,
                                      120U,
                                      partial) == CaptureBuildResult::partial);
        CHECK(partial.pre_valid);
        CHECK(!partial.post_valid);
        CHECK(valid_raw_record(partial));
        DWORD ignored{};
        CHECK(VirtualProtect(protectedRecord,
                             info.dwPageSize,
                             oldProtection,
                             &ignored) != FALSE);
        CHECK(VirtualFree(protectedRecord, 0U, MEM_RELEASE) != FALSE);
    }

    auto* const pages = static_cast<std::byte*>(VirtualAlloc(
        nullptr, 2U * info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    CHECK(pages != nullptr);
    if (pages != nullptr) {
        std::byte* const descriptor = pages + info.dwPageSize - 0x40U;
        write_payload(descriptor, make_payload());
        DWORD oldProtection{};
        CHECK(VirtualProtect(pages + info.dwPageSize,
                             info.dwPageSize,
                             PAGE_NOACCESS,
                             &oldProtection) != FALSE);
        ParticipantLease dispatchLease = harness.enter(NativeParticipant::full_dispatch);
        PendingDispatchCapture dispatchPending;
        CHECK(prepare_dispatch_capture(dispatchPending,
                                       dispatchLease,
                                       pages,
                                       descriptor,
                                       nullptr) == CaptureBuildResult::unreadable);
        DWORD ignored{};
        CHECK(VirtualProtect(pages + info.dwPageSize,
                             info.dwPageSize,
                             oldProtection,
                             &ignored) != FALSE);
        CHECK(VirtualFree(pages, 0U, MEM_RELEASE) != FALSE);
    }

    DescriptorFixture dispatchFixture;
    ParticipantLease dispatchLease = harness.enter(NativeParticipant::full_dispatch, 300U);
    PendingDispatchCapture pending;
    CHECK(prepare_dispatch_capture(pending,
                                   dispatchLease,
                                   &dispatchFixture,
                                   dispatchFixture.bytes.data(),
                                   nullptr) == CaptureBuildResult::ready);
    dispatchFixture.bytes[kActionTypeOffset] = std::byte{0x44U};
    DispatchCaptureRecord dispatch{};
    CHECK(finish_dispatch_capture(pending,
                                  dispatchLease,
                                  dispatchFixture.bytes.data(),
                                  310U,
                                  dispatch) == CaptureBuildResult::partial);
    CHECK(dispatch.pre_valid);
    CHECK(!dispatch.post_valid);
    CHECK(valid_raw_record(dispatch));
}

void callsite_candidates_are_surface_and_parent_bound_but_never_materialized() {
    OwnerHarness harness;
    InterfaceFixture interfaceFixture;
    DescriptorFixture descriptor{9};
    ParticipantLease dispatchLease = harness.enter(NativeParticipant::full_dispatch, 400U);
    PendingDispatchCapture dispatchPending;
    CHECK(prepare_dispatch_capture(dispatchPending,
                                   dispatchLease,
                                   &descriptor,
                                   descriptor.bytes.data(),
                                   nullptr) == CaptureBuildResult::ready);

    ParticipantLease callsiteLease = harness.enter(NativeParticipant::full_callsite, 401U);
    const std::array<float, 4U> payload4{1.5F, 2.5F, 3.5F, 1.0F};
    PendingCallsiteCapture callsitePending;
    CHECK(prepare_full_callsite_capture(callsitePending,
                                        callsiteLease,
                                        interfaceFixture.pair.data(),
                                        7U,
                                        21,
                                        payload4.data(),
                                        0xAABBCCDDU) == CaptureBuildResult::ready);
    CallsiteCaptureRecord callsite{};
    CHECK(finish_callsite_capture(callsitePending, callsiteLease, 405U, callsite)
          == CaptureBuildResult::complete);
    CHECK(callsite.provenance.parent_call_id
          == dispatchLease.entry_provenance().call_id);
    CHECK(callsite.parent_descriptor.payload.endpoint_index == 9);
    CHECK(callsite.arguments_match_parent);
    CHECK(callsite.resolution.method_slot == 0x30U);
    CHECK(callsite.resolution.concrete_method
          == reinterpret_cast<std::uintptr_t>(&fake_full_subscriber));
    CHECK(callsite.provenance.materialization_token == 0U);
    CHECK(callsite.provenance.binding == BindingState::materialization_unobserved);
    CHECK(callsite.parent_descriptor.payload.endpoint_index
          != static_cast<std::int32_t>(kOpeningDirectiveRow.slot));

    CallsiteCaptureQueue queue;
    CHECK(queue.try_publish(callsite, callsiteLease, 406U) == QueuePushResult::enqueued);
    CallsiteCaptureRecord drained{};
    CHECK(queue.try_pop(drained) == QueuePopResult::success);
    CallsiteTelemetry telemetry{};
    CHECK(default_telemetry(drained, telemetry));
    CHECK(telemetry.provenance.binding == BindingState::materialization_unobserved);
    CHECK(telemetry.provenance.materialization_token == 0U);
    CHECK(telemetry.endpoint_index == 9);
    CHECK(telemetry.resolution_validity_mask
          == (static_cast<std::uint32_t>(ResolutionValidity::pair_readable)
              | static_cast<std::uint32_t>(ResolutionValidity::interface_datum)
              | static_cast<std::uint32_t>(ResolutionValidity::endpoint_object)
              | static_cast<std::uint32_t>(ResolutionValidity::interface_metadata)
              | static_cast<std::uint32_t>(ResolutionValidity::adjusted_this)
              | static_cast<std::uint32_t>(ResolutionValidity::concrete_method)));

    DispatchCaptureRecord dispatchRecord{};
    CHECK(finish_dispatch_capture(dispatchPending,
                                  dispatchLease,
                                  descriptor.bytes.data(),
                                  410U,
                                  dispatchRecord) == CaptureBuildResult::complete);
}

void nested_reentrant_callsite_join_uses_the_owner_stack_not_time() {
    OwnerHarness harness;
    InterfaceFixture interfaceFixture;
    DescriptorFixture outerDescriptor{2};
    DescriptorFixture innerDescriptor{8};
    ParticipantLease outerLease = harness.enter(NativeParticipant::full_dispatch, 500U);
    PendingDispatchCapture outerPending;
    CHECK(prepare_dispatch_capture(outerPending,
                                   outerLease,
                                   &outerDescriptor,
                                   outerDescriptor.bytes.data(),
                                   nullptr) == CaptureBuildResult::ready);
    ParticipantLease innerLease = harness.enter(NativeParticipant::full_dispatch, 501U);
    PendingDispatchCapture innerPending;
    CHECK(prepare_dispatch_capture(innerPending,
                                   innerLease,
                                   &innerDescriptor,
                                   innerDescriptor.bytes.data(),
                                   nullptr) == CaptureBuildResult::ready);

    ParticipantLease callsiteLease = harness.enter(NativeParticipant::full_callsite, 502U);
    const std::array<float, 4U> payload4{1.5F, 2.5F, 3.5F, 1.0F};
    PendingCallsiteCapture callsitePending;
    CHECK(prepare_full_callsite_capture(callsitePending,
                                        callsiteLease,
                                        interfaceFixture.pair.data(),
                                        7U,
                                        1,
                                        payload4.data(),
                                        0xAABBCCDDU) == CaptureBuildResult::ready);
    CallsiteCaptureRecord innerCallsite{};
    CHECK(finish_callsite_capture(callsitePending, callsiteLease, 503U, innerCallsite)
          == CaptureBuildResult::complete);
    CHECK(innerCallsite.provenance.parent_call_id
          == innerLease.entry_provenance().call_id);
    CHECK(innerCallsite.parent_descriptor.payload.endpoint_index == 8);

    DispatchCaptureRecord innerRecord{};
    CHECK(finish_dispatch_capture(innerPending,
                                  innerLease,
                                  innerDescriptor.bytes.data(),
                                  504U,
                                  innerRecord) == CaptureBuildResult::complete);
    ParticipantLease secondCallsite = harness.enter(NativeParticipant::full_callsite, 505U);
    CHECK(prepare_full_callsite_capture(callsitePending,
                                        secondCallsite,
                                        interfaceFixture.pair.data(),
                                        7U,
                                        1,
                                        payload4.data(),
                                        0xAABBCCDDU) == CaptureBuildResult::ready);
    CallsiteCaptureRecord outerCallsite{};
    CHECK(finish_callsite_capture(callsitePending, secondCallsite, 506U, outerCallsite)
          == CaptureBuildResult::complete);
    CHECK(outerCallsite.provenance.parent_call_id
          == outerLease.entry_provenance().call_id);
    CHECK(outerCallsite.parent_descriptor.payload.endpoint_index == 2);
    DispatchCaptureRecord outerRecord{};
    CHECK(finish_dispatch_capture(outerPending,
                                  outerLease,
                                  outerDescriptor.bytes.data(),
                                  507U,
                                  outerRecord) == CaptureBuildResult::complete);

    ParticipantLease orphanCallsite = harness.enter(NativeParticipant::full_callsite, 508U);
    CHECK(prepare_full_callsite_capture(callsitePending,
                                        orphanCallsite,
                                        interfaceFixture.pair.data(),
                                        7U,
                                        1,
                                        payload4.data(),
                                        0xAABBCCDDU) == CaptureBuildResult::no_parent_dispatch);
}

void exact_abi_originals_forward_once_with_arguments_and_quiesced_lease() {
    OwnerHarness harness;
    InterfaceFixture interfaceFixture;
    const std::array<float, 4U> payload4{4.0F, 5.0F, 6.0F, 1.0F};
    ParticipantLease fullLease = harness.enter(NativeParticipant::full_callsite);
    gFullSubscriberCalls.store(0U, std::memory_order_relaxed);
    gFullBefore.store(0U, std::memory_order_relaxed);
    gFullAfter.store(0U, std::memory_order_relaxed);
    harness.owner.quiesce();
    forward_void_original_once<FullSubscriberTarget>(
        fullLease,
        []() noexcept { gFullBefore.fetch_add(1U, std::memory_order_relaxed); },
        []() noexcept { gFullAfter.fetch_add(1U, std::memory_order_relaxed); },
        interfaceFixture.object.data(),
        static_cast<std::uint8_t>(12U),
        static_cast<std::int32_t>(-7),
        payload4.data(),
        static_cast<std::uint32_t>(55U));
    CHECK(gFullSubscriberCalls.load(std::memory_order_relaxed) == 1U);
    CHECK(gFullBefore.load(std::memory_order_relaxed) == 1U);
    CHECK(gFullAfter.load(std::memory_order_relaxed) == 1U);
    CHECK(gObservedOperation == 12U);
    CHECK(gObservedResolved == -7);
    CHECK(gObservedAuxiliary == 55U);
    CHECK(gObservedFullPayload == payload4);
}

void owner_lifecycle_retains_protected_state_and_requires_drain_and_fresh_epoch() {
    const ValidatedRuntimeCohort cohort = CaptureTestAccess::validated_cohort();
    CaptureCohortOwner owner;
    CHECK(owner.begin_install(cohort, 10U) == OwnerResult::success);
    ParticipantLease premature;
    CHECK(owner.try_enter(NativeParticipant::resolver, timing(), premature)
          == OwnerResult::producer_closed);
    CHECK(owner.start_running() == OwnerResult::originals_incomplete);
    for (std::size_t index = 0U; index < kNativeParticipantCount; ++index) {
        const NativeParticipant participant = static_cast<NativeParticipant>(index);
        CHECK(owner.publish_original(participant, original_for(participant))
              == OwnerResult::success);
    }
    CHECK(owner.publish_generations(exact_generations()) == OwnerResult::success);
    CHECK(owner.start_running() == OwnerResult::success);
    ParticipantLease active;
    CHECK(owner.try_enter(NativeParticipant::resolver, timing(), active) == OwnerResult::success);
    owner.quiesce();
    owner.close_drain_admission();
    CaptureQueueAggregate queues{10U};
    queues.close_producers();
    queues.close_consumers();
    CHECK(owner.mark_drain_complete_if_empty(queues) == OwnerResult::active_calls);
    CHECK(owner.protected_detach(ProtectedDetachDisposition::removed)
          == OwnerResult::active_calls);
    active = ParticipantLease{};
    CHECK(owner.mark_drain_complete_if_empty(queues) == OwnerResult::success);
    CHECK(owner.protected_detach(ProtectedDetachDisposition::deferred)
          == OwnerResult::success);
    const OwnerSnapshot retained = owner.snapshot();
    CHECK(retained.phase == CohortPhase::protected_retained);
    CHECK(retained.published_original_mask == (1ULL << kNativeParticipantCount) - 1U);
    CHECK(owner.begin_install(cohort, 11U) == OwnerResult::wrong_phase);
    CHECK(owner.reopen_retained_for_detach() == OwnerResult::success);
    CHECK(owner.protected_detach(ProtectedDetachDisposition::removed)
          == OwnerResult::success);
    const OwnerSnapshot detached = owner.snapshot();
    CHECK(detached.phase == CohortPhase::detached);
    CHECK(detached.published_original_mask == 0U);
    CHECK(detached.last_detached_epoch == 10U);
    CHECK(owner.begin_install(cohort, 10U) == OwnerResult::invalid_epoch);
    CHECK(owner.begin_install(cohort, 11U) == OwnerResult::success);
}

void bounded_queue_has_real_mpsc_fifo_and_loss_accounting() {
    OwnerHarness harness;
    ResolverCaptureQueue queue;
    constexpr std::size_t producerCount = 4U;
    constexpr std::size_t perProducer = 50U;
    constexpr std::size_t total = producerCount * perProducer;
    std::barrier start{static_cast<std::ptrdiff_t>(producerCount + 1U)};
    std::atomic_uint32_t producersDone{};
    std::atomic_uint32_t unexpected{};
    std::vector<std::uint64_t> sequences;
    sequences.reserve(total);

    std::thread consumer([&]() {
        start.arrive_and_wait();
        while (producersDone.load(std::memory_order_acquire) != producerCount
               || !queue.empty()) {
            ResolverCaptureRecord output{};
            const QueuePopResult result = queue.try_pop(output);
            if (result == QueuePopResult::success) {
                sequences.push_back(output.sequence);
            } else if (result != QueuePopResult::empty && result != QueuePopResult::busy) {
                unexpected.fetch_add(1U, std::memory_order_relaxed);
            }
            std::this_thread::yield();
        }
    });

    std::array<std::thread, producerCount> producers;
    for (std::size_t producer = 0U; producer < producerCount; ++producer) {
        producers[producer] = std::thread([&, producer]() {
            start.arrive_and_wait();
            for (std::size_t index = 0U; index < perProducer; ++index) {
                RuntimeFixture fixture;
                ParticipantLease lease;
                const std::uint64_t tick =
                    1'000U + producer * perProducer * 10U + index * 10U;
                if (harness.owner.try_enter(NativeParticipant::resolver,
                                            timing(tick),
                                            lease) != OwnerResult::success) {
                    unexpected.fetch_add(1U, std::memory_order_relaxed);
                    continue;
                }
                ResolverCaptureRecord record = capture_resolver(lease, fixture, tick + 1U);
                for (;;) {
                    const QueuePushResult result = queue.try_publish(record, lease, tick + 2U);
                    if (result == QueuePushResult::enqueued) {
                        break;
                    }
                    if (result != QueuePushResult::busy && result != QueuePushResult::full) {
                        unexpected.fetch_add(1U, std::memory_order_relaxed);
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            producersDone.fetch_add(1U, std::memory_order_release);
        });
    }
    for (std::thread& producer : producers) {
        producer.join();
    }
    consumer.join();
    CHECK(unexpected.load(std::memory_order_relaxed) == 0U);
    CHECK(sequences.size() == total);
    for (std::size_t index = 0U; index < sequences.size(); ++index) {
        CHECK(sequences[index] == index + 1U);
    }
    const QueueCounters counters = queue.counters();
    CHECK(counters.accepted == total);
    CHECK(counters.losses() == counters.dropped_full + counters.dropped_busy);

    RuntimeFixture fixture;
    ParticipantLease lease = harness.enter(NativeParticipant::resolver, 5'000U);
    ResolverCaptureRecord record = capture_resolver(lease, fixture, 5'001U);
    ResolverCaptureQueue fullQueue;
    for (std::size_t index = 0U; index < kResolverQueueCapacity; ++index) {
        ParticipantLease itemLease = harness.enter(NativeParticipant::resolver, 6'000U + index);
        RuntimeFixture itemFixture;
        ResolverCaptureRecord item =
            capture_resolver(itemLease, itemFixture, 7'000U + index);
        CHECK(fullQueue.try_publish(item, itemLease, 8'000U + index)
              == QueuePushResult::enqueued);
    }
    CHECK(fullQueue.try_publish(record, lease, 9'000U) == QueuePushResult::full);
    fullQueue.close_producers();
    CHECK(fullQueue.try_publish(record, lease, 9'001U) == QueuePushResult::closed);
    ResolverCaptureRecord drained{};
    while (fullQueue.try_pop(drained) == QueuePopResult::success) {
    }
    fullQueue.close_consumer();
    CHECK(fullQueue.try_pop(drained) == QueuePopResult::closed);
    CHECK(!fullQueue.reset(1U));
    CHECK(fullQueue.reset(2U));
    fullQueue.close_producers();
    fullQueue.close_consumer();
    CHECK(fullQueue.empty());

    ResolverCaptureQueue exhausted;
    exhausted.testing_set_enqueue_position(
        (std::numeric_limits<std::uint64_t>::max)() - kResolverQueueCapacity);
    CHECK(exhausted.try_publish(record, lease, 9'002U)
          == QueuePushResult::sequence_exhausted);
}

void different_type_and_different_endpoint_are_negative_controls() {
    OwnerHarness harness;
    DescriptorFixture nonType{2, 0x44U};
    ParticipantLease fullLease = harness.enter(NativeParticipant::full_dispatch);
    PendingDispatchCapture pending;
    CHECK(prepare_dispatch_capture(pending,
                                   fullLease,
                                   &nonType,
                                   nonType.bytes.data(),
                                   nullptr) == CaptureBuildResult::wrong_type);

    DescriptorFixture nextEndpoint{6};
    ParticipantLease shortLease = harness.enter(NativeParticipant::short_dispatch, 900U);
    CHECK(prepare_dispatch_capture(pending,
                                   shortLease,
                                   &nextEndpoint,
                                   nextEndpoint.bytes.data(),
                                   nullptr) == CaptureBuildResult::ready);
    InterfaceFixture interfaceFixture;
    ParticipantLease shortCallsite = harness.enter(NativeParticipant::short_callsite, 901U);
    PendingCallsiteCapture callsitePending;
    CHECK(prepare_short_callsite_capture(callsitePending,
                                         shortCallsite,
                                         interfaceFixture.pair.data(),
                                         0xDEADBEEFU) == CaptureBuildResult::ready);
    CallsiteCaptureRecord record{};
    CHECK(finish_callsite_capture(callsitePending, shortCallsite, 902U, record)
          == CaptureBuildResult::complete);
    CHECK(!record.arguments_match_parent);
    CHECK(record.parent_descriptor.payload.endpoint_index == 6);
    CHECK(record.kind == SubscriberKind::short_slot_48);
    CHECK(record.provenance.binding == BindingState::materialization_unobserved);
    DispatchCaptureRecord dispatch{};
    CHECK(finish_dispatch_capture(pending,
                                  shortLease,
                                  nextEndpoint.bytes.data(),
                                  903U,
                                  dispatch) == CaptureBuildResult::complete);
}

void endpoint_binding_is_native_window_derived_and_never_authored_asserted() {
    OwnerHarness harness;
    EndpointRegistrationFixture fixture;

    ParticipantLease orphanLease =
        harness.enter(NativeParticipant::endpoint_registration_callsite, 1'000U);
    PendingEndpointRegistrationCapture orphanPending;
    CHECK(prepare_endpoint_registration_capture(
              orphanPending,
              orphanLease,
              fixture.row.data() + kEndpointInterfaceHalfOffset,
              0xABCDEF01U)
          == CaptureBuildResult::no_parent_dispatch);
    orphanLease = ParticipantLease{};

    ParticipantLease binderLease = harness.enter(NativeParticipant::endpoint_binder, 1'010U);
    PendingEndpointBinderCapture binderPending;
    CHECK(prepare_endpoint_binder_capture(
              binderPending, binderLease, fixture.owner.data(), 1U)
          == CaptureBuildResult::ready);
    ParticipantLease registrationLease =
        harness.enter(NativeParticipant::endpoint_registration_callsite, 1'011U);
    PendingEndpointRegistrationCapture registrationPending;
    CHECK(prepare_endpoint_registration_capture(
              registrationPending,
              registrationLease,
              fixture.row.data() + kEndpointInterfaceHalfOffset,
              0xABCDEF01U)
          == CaptureBuildResult::ready);

    EndpointRegistrationCaptureRecord registration{};
    CHECK(finish_endpoint_registration_capture(
              registrationPending, registrationLease, 1'012U, registration)
          == CaptureBuildResult::complete);
    CHECK(registration.row_identity
          == reinterpret_cast<std::uintptr_t>(fixture.row.data()));
    CHECK(registration.interface_pair
          == registration.row_identity + kEndpointInterfaceHalfOffset);
    CHECK(registration.registration_candidate.method_slot
          == kEndpointRegistrationMethodSlot);
    CHECK((registration.registration_candidate.validity_mask
           & static_cast<std::uint32_t>(ResolutionValidity::concrete_method)) != 0U);
    CHECK(registration.provenance.materialization_token == 0U);
    CHECK(registration.provenance.binding == BindingState::observed_candidate_not_joined);
    CHECK(registration.observed_ordinal == 0U);
    CHECK(registration.runtime_key_pre_hash == bounded_hash(registration.runtime_key_pre));

    PendingEndpointRegistrationCapture excessPending;
    ParticipantLease excessLease =
        harness.enter(NativeParticipant::endpoint_registration_callsite, 1'013U);
    CHECK(prepare_endpoint_registration_capture(
              excessPending,
              excessLease,
              fixture.row.data() + kEndpointInterfaceHalfOffset,
              0xABCDEF01U)
          == CaptureBuildResult::arguments_do_not_match_parent);
    excessLease = ParticipantLease{};

    const std::uint32_t cleared = 0U;
    const std::uint32_t count = 1U;
    std::memcpy(fixture.owner.data() + kEndpointOwnerClearedOffset,
                &cleared,
                sizeof cleared);
    std::memcpy(fixture.owner.data() + kEndpointOwnerCountOffset, &count, sizeof count);
    EndpointBinderCaptureRecord binder{};
    CHECK(finish_endpoint_binder_capture(
              binderPending, binderLease, fixture.owner.data(), true, 1'014U, binder)
          == CaptureBuildResult::complete);
    CHECK(binder.requested_count == 1U);
    CHECK(binder.cleared_field_pre == 0xFFFFFFFFU);
    CHECK(binder.cleared_field_post == 0U);
    CHECK(binder.owner_count_post == 1U);
    CHECK(binder.provenance.binding == BindingState::observed_candidate_not_joined);

    CaptureQueueAggregate queues{1U};
    CHECK(queues.endpoint_registration.try_publish(registration,
                                                   registrationLease,
                                                   1'015U)
          == QueuePushResult::enqueued);
    CHECK(queues.endpoint_binder.try_publish(binder, binderLease, 1'016U)
          == QueuePushResult::enqueued);
    registrationLease = ParticipantLease{};
    binderLease = ParticipantLease{};
    harness.owner.quiesce();
    queues.close_producers();
    harness.owner.close_drain_admission();
    CHECK(harness.owner.mark_drain_complete_if_empty(queues)
          == OwnerResult::drain_incomplete);
    EndpointRegistrationCaptureRecord registrationPopped{};
    EndpointBinderCaptureRecord binderPopped{};
    CHECK(queues.endpoint_registration.try_pop(registrationPopped) == QueuePopResult::success);
    CHECK(queues.endpoint_binder.try_pop(binderPopped) == QueuePopResult::success);
    queues.close_consumers();
    CHECK(harness.owner.mark_drain_complete_if_empty(queues) == OwnerResult::success);

    CHECK(kRuntimeEndpointBindingStructure == EvidenceState::proven);
    CHECK(kRuntimeEndpointToAuthoredRow == EvidenceState::unknown);
    CHECK(kDecodedType68Builder == EvidenceState::unknown);
    CHECK(kExactType68OutboundCallsite == EvidenceState::unknown);
    CHECK(kFirstOutboundType68Body == EvidenceState::unknown);
    CHECK(kPs4Type68Interface.type == 68U);
    CHECK(kPs4Type68Interface.schema == kType68AuthoritySchema);
    CHECK(kPs4Type69Interface.type == 69U);
    CHECK(exact_type68_serialization_shape(
        kType68AuthoritySchema, kType68SignificantBits, kType68RoundedBytes, false));
    CHECK(!exact_type68_serialization_shape(
        kType68AuthoritySchema, kType68SignificantBits - 1U, kType68RoundedBytes, false));
    CHECK(classify_type68_transport(0x12345678U, 0U)
          == Type68TransportInput::unrelated_schema);
    CHECK(classify_type68_transport(kType68AuthoritySchema, 0U)
          == Type68TransportInput::decoded_candidate);
    CHECK(classify_type68_transport(kType68AuthoritySchema, kSchemaTransportEncodeFlag)
          == Type68TransportInput::preencoded_publication_candidate);
}

void large_type68_serializer_capture_is_exact_filtered_hash_telemetry_and_mpsc() {
    OwnerHarness harness;
    Type68DecodedImage decoded{};
    Type68RoundedWireImage wire{};
    for (std::size_t index = 0U; index < decoded.size(); ++index) {
        decoded[index] = std::byte{static_cast<std::uint8_t>(index)};
    }
    for (std::size_t index = 0U; index < wire.size(); ++index) {
        wire[index] = std::byte{static_cast<std::uint8_t>(index * 3U)};
    }
    std::int32_t outputBytes = static_cast<std::int32_t>(kType68RoundedBytes);
    ParticipantLease lease = harness.enter(NativeParticipant::large_serializer, 2'000U);
    PendingType68SerializerCapture wrongSchema{};
    CHECK(prepare_type68_serializer_capture(wrongSchema,
                                            lease,
                                            kType68AuthoritySchema + 1U,
                                            decoded.data(),
                                            wire.data(),
                                            &outputBytes)
          == CaptureBuildResult::wrong_type);

    PendingType68SerializerCapture pending{};
    CHECK(prepare_type68_serializer_capture(pending,
                                            lease,
                                            kType68AuthoritySchema,
                                            decoded.data(),
                                            wire.data(),
                                            &outputBytes)
          == CaptureBuildResult::ready);
    CHECK(observe_type68_serializer_cursor(
              pending, lease, kType68SignificantBits, false)
          == CaptureBuildResult::complete);
    Type68SerializerCaptureRecord record{};
    CHECK(finish_type68_serializer_capture(pending,
                                           lease,
                                           decoded.data(),
                                           wire.data(),
                                           &outputBytes,
                                           true,
                                           2'001U,
                                           record)
          == CaptureBuildResult::complete);
    CHECK(record.exact_type68_shape);
    CHECK(record.provenance.parent_call_id == 0U);
    CHECK(record.provenance.binding == BindingState::observed_candidate_not_joined);
    CHECK(record.provenance.materialization_token == 0U);
    CHECK(record.rounded_wire_hash == bounded_hash(record.wire_post));
    CHECK(!kReflectedEncoderReturnIsOverflowOracle);
    CHECK(!kSerializerSurfaceIsCallableByCaptureModule);
    CHECK(!kSchemaTransportSurfaceIsCallableByCaptureModule);

    Type68SerializerCaptureQueue oneQueue{1U};
    CHECK(oneQueue.try_publish(record, lease, 2'002U) == QueuePushResult::enqueued);
    Type68SerializerCaptureRecord popped{};
    CHECK(oneQueue.try_pop(popped) == QueuePopResult::success);
    Type68SerializerTelemetry telemetry{};
    CHECK(default_telemetry(popped, telemetry));
    CHECK(telemetry.exact_type68_shape);
    CHECK(telemetry.rounded_wire_hash == record.rounded_wire_hash);
    CHECK(sizeof telemetry < sizeof popped);
    lease = ParticipantLease{};

    ParticipantLease partialLease =
        harness.enter(NativeParticipant::large_serializer, 2'010U);
    PendingType68SerializerCapture partialPending{};
    CHECK(prepare_type68_serializer_capture(partialPending,
                                            partialLease,
                                            kType68AuthoritySchema,
                                            decoded.data(),
                                            wire.data(),
                                            &outputBytes)
          == CaptureBuildResult::ready);
    CHECK(observe_type68_serializer_cursor(
              partialPending, partialLease, kType68SignificantBits - 1U, false)
          == CaptureBuildResult::complete);
    Type68SerializerCaptureRecord partial{};
    CHECK(finish_type68_serializer_capture(partialPending,
                                           partialLease,
                                           decoded.data(),
                                           wire.data(),
                                           &outputBytes,
                                           true,
                                           2'011U,
                                           partial)
          == CaptureBuildResult::partial);
    CHECK(!partial.exact_type68_shape);
    CHECK(valid_raw_record(partial));
    partialLease = ParticipantLease{};

    constexpr std::uint32_t producerCount = 4U;
    constexpr std::uint32_t itemsPerProducer = 32U;
    constexpr std::uint32_t expected = producerCount * itemsPerProducer;
    Type68SerializerCaptureQueue concurrent{};
    std::atomic_uint32_t producersDone{};
    std::atomic_uint32_t consumed{};
    std::atomic_bool sequenceFailure{};
    std::thread consumer([&]() noexcept {
        std::uint64_t lastSequence{};
        while (producersDone.load(std::memory_order_acquire) != producerCount
               || !concurrent.empty()) {
            Type68SerializerCaptureRecord item{};
            const QueuePopResult result = concurrent.try_pop(item);
            if (result == QueuePopResult::success) {
                if (!valid_raw_record(item) || item.sequence <= lastSequence) {
                    sequenceFailure.store(true, std::memory_order_relaxed);
                }
                lastSequence = item.sequence;
                consumed.fetch_add(1U, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });
    std::array<std::thread, producerCount> producers{};
    for (std::uint32_t producer = 0U; producer < producerCount; ++producer) {
        producers[producer] = std::thread([&, producer]() noexcept {
            for (std::uint32_t index = 0U; index < itemsPerProducer; ++index) {
                Type68DecodedImage localDecoded{};
                Type68RoundedWireImage localWire{};
                localDecoded[0] = std::byte{static_cast<std::uint8_t>(producer)};
                localDecoded[1] = std::byte{static_cast<std::uint8_t>(index)};
                localWire[0] = std::byte{static_cast<std::uint8_t>(producer + index)};
                std::int32_t localBytes = static_cast<std::int32_t>(kType68RoundedBytes);
                ParticipantLease localLease;
                const OwnerResult entered = harness.owner.try_enter(
                    NativeParticipant::large_serializer,
                    timing(3'000U + producer * itemsPerProducer + index),
                    localLease);
                if (entered != OwnerResult::success) {
                    sequenceFailure.store(true, std::memory_order_relaxed);
                    break;
                }
                PendingType68SerializerCapture localPending{};
                Type68SerializerCaptureRecord localRecord{};
                if (prepare_type68_serializer_capture(localPending,
                                                      localLease,
                                                      kType68AuthoritySchema,
                                                      localDecoded.data(),
                                                      localWire.data(),
                                                      &localBytes)
                        != CaptureBuildResult::ready
                    || observe_type68_serializer_cursor(localPending,
                                                        localLease,
                                                        kType68SignificantBits,
                                                        false)
                           != CaptureBuildResult::complete
                    || finish_type68_serializer_capture(localPending,
                                                        localLease,
                                                        localDecoded.data(),
                                                        localWire.data(),
                                                        &localBytes,
                                                        true,
                                                        4'000U + producer * itemsPerProducer + index,
                                                        localRecord)
                           != CaptureBuildResult::complete) {
                    sequenceFailure.store(true, std::memory_order_relaxed);
                    break;
                }
                for (;;) {
                    const QueuePushResult pushed = concurrent.try_publish(
                        localRecord,
                        localLease,
                        5'000U + producer * itemsPerProducer + index);
                    if (pushed == QueuePushResult::enqueued) {
                        break;
                    }
                    if (pushed != QueuePushResult::busy
                        && pushed != QueuePushResult::full) {
                        sequenceFailure.store(true, std::memory_order_relaxed);
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            producersDone.fetch_add(1U, std::memory_order_release);
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    CHECK(!sequenceFailure.load(std::memory_order_relaxed));
    CHECK(consumed.load(std::memory_order_relaxed) == expected);
    CHECK(concurrent.counters().accepted == expected);
}

} // namespace

int main() {
    artifact_backed_identity_and_every_surface_prefix();
    static_authored_metadata_never_becomes_a_live_binding();
    owner_issued_generations_are_revalidated_at_queue_publication();
    exact_capture_windows_preserve_partial_and_negative_raw_evidence();
    callsite_candidates_are_surface_and_parent_bound_but_never_materialized();
    nested_reentrant_callsite_join_uses_the_owner_stack_not_time();
    exact_abi_originals_forward_once_with_arguments_and_quiesced_lease();
    owner_lifecycle_retains_protected_state_and_requires_drain_and_fresh_epoch();
    bounded_queue_has_real_mpsc_fifo_and_loss_accounting();
    different_type_and_different_endpoint_are_negative_controls();
    endpoint_binding_is_native_window_derived_and_never_authored_asserted();
    large_type68_serializer_capture_is_exact_filtered_hash_telemetry_and_mpsc();

    const int failures = gFailureCount.load(std::memory_order_relaxed);
    if (failures != 0) {
        std::cerr << failures << " Type-69 capture remediation check(s) failed\n";
        return 1;
    }
    std::cout << "all Type-69 capture remediation checks passed\n";
    return 0;
}
