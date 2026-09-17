#include <Windows.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <latch>
#include <string_view>
#include <thread>
#include <type_traits>

#include "client/hooks/bootflow/opening_authority/type26_hold_capture.h"

namespace {

using namespace dawn::client::hooks::bootflow::opening_authority::type26_hold;

int gFailureCount = 0;
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
        ++gFailureCount;
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename Value>
void write_value(std::byte* destination, const Value& value) noexcept {
    std::memcpy(destination, &value, sizeof value);
}

[[nodiscard]] ActivationContext context(std::uint64_t activation = 9U) noexcept {
    ActivationContext result{};
    result.packed_runtime_sha256 = kPinnedPackedRuntimeSha256;
    result.module_generation = 4U;
    result.activation_generation = activation;
    result.source_session = 0x1122334455667788ULL;
    result.roster_epoch = 7U;
    result.roster_publication_sequence = 13U;
    result.native_activity_wrapper = 0x12340000U;
    result.full_activity_handle = 0xABC00055U;
    result.registry = kActivityRegistry;
    result.bubble = kOpeningBubble;
    result.local_player_datum = 0x420U;
    result.local_player_bit = 0x08U;
    result.state = ActivationState::current;
    return result;
}

[[nodiscard]] CaptureMetadata metadata(std::uint64_t call = 1U) noexcept {
    return CaptureMetadata{3U, 1000U + call, call, 17U, 0x7FF600001234ULL};
}

[[nodiscard]] AuthorityBody authority_body(std::int32_t seed) noexcept {
    AuthorityLayout layout{};
    layout.opaque_boolean_0 = static_cast<std::uint8_t>(seed & 1);
    layout.suppress_linked_enumeration = static_cast<std::uint8_t>((seed >> 1) & 1);
    layout.clear_generation = seed + 10;
    layout.subscriber_argument = seed + 20;
    layout.subscriber_generation = seed + 30;
    layout.sense_echo = seed + 40;
    layout.linked_selector = ObjectReference{kActivityRegistry, 34, std::byte{}, 22};
    for (std::size_t index = 0U; index < layout.runtime_nested_type34.size(); ++index) {
        layout.runtime_nested_type34[index] =
            std::byte{static_cast<std::uint8_t>(seed + static_cast<std::int32_t>(index))};
    }
    AuthorityBody result{};
    std::memcpy(result.data(), &layout, sizeof layout);
    return result;
}

[[nodiscard]] SenseBody sense_body(std::int32_t seed, std::uint8_t linked) noexcept {
    const SenseLayout layout{seed + 1, seed + 2, seed + 3, linked, {}};
    SenseBody result{};
    std::memcpy(result.data(), &layout, sizeof layout);
    return result;
}

class Page final {
public:
    explicit Page(DWORD protection = PAGE_READWRITE) noexcept {
        SYSTEM_INFO information{};
        GetSystemInfo(&information);
        bytes_ = information.dwPageSize;
        data_ = static_cast<std::byte*>(
            VirtualAlloc(nullptr, bytes_, MEM_COMMIT | MEM_RESERVE, protection));
    }
    ~Page() noexcept {
        if (data_ != nullptr) {
            (void)VirtualFree(data_, 0U, MEM_RELEASE);
        }
    }
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    [[nodiscard]] std::byte* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return bytes_; }
    [[nodiscard]] bool protect(DWORD protection) noexcept {
        DWORD previous{};
        return VirtualProtect(data_, bytes_, protection, &previous) != FALSE;
    }
private:
    std::byte* data_{};
    std::size_t bytes_{};
};

[[nodiscard]] AuthorityWireSnapshot authority_wire() noexcept {
    std::array<std::byte, 24U> bytes{};
    for (std::size_t index = 0U; index + 1U < bytes.size(); ++index) {
        bytes[index] = std::byte{static_cast<std::uint8_t>(0x40U + index)};
    }
    bytes.back() = std::byte{0xC0U}; // 186 MSB-first bits: low six storage bits are zero.
    AuthorityWireSnapshot snapshot{};
    const WireSourceEvidence source{
        bytes, 186U, 8U, WireBitOrder::most_significant_bit_first, true, true, true};
    CHECK(make_authority_wire_snapshot(source, snapshot) == WireSnapshotResult::complete);
    return snapshot;
}

[[nodiscard]] SenseWireSnapshot sense_wire() noexcept {
    std::array<std::byte, kSenseWireBytes> bytes{};
    for (std::size_t index = 0U; index + 1U < bytes.size(); ++index) {
        bytes[index] = std::byte{static_cast<std::uint8_t>(0x60U + index)};
    }
    bytes.back() = std::byte{0x80U}; // One meaningful MSB; seven unused bits are zero.
    SenseWireSnapshot snapshot{};
    const WireSourceEvidence source{
        bytes, 97U, 9U, WireBitOrder::most_significant_bit_first, true, true, false};
    CHECK(make_sense_wire_snapshot(source, snapshot) == WireSnapshotResult::complete);
    return snapshot;
}

class Fixture final {
public:
    Fixture() noexcept : opaque_payload_(PAGE_NOACCESS) {
        CHECK(instance_.data() != nullptr);
        CHECK(opaque_payload_.data() != nullptr);
        write_value(instance_.data(), kWeaponDownIdentity.definition);
        before_ = authority_body(10);
        resolved_ = authority_body(50);
        sense_ = sense_body(70, 1U);
        restore_before();
        packet_ = PacketReference{kHoldAuthoritySchema, 0xA5A5A5A5U, opaque_payload_.data()};
        sense_packet_ = PacketReference{kHoldSenseSchema,
                                        0U,
                                        instance_.data() + kSenseCacheOffset};
    }
    void restore_before() noexcept {
        std::memcpy(instance_.data() + kAuthorityCacheOffset, before_.data(), before_.size());
        std::memcpy(instance_.data() + kSenseCacheOffset, sense_.data(), sense_.size());
        instance_.data()[kDirtyByteOffset] = std::byte{};
    }
    void simulate_native_resolver_and_apply() noexcept {
        // The test's opaque PacketRef payload is PAGE_NOACCESS. This simulates only the native
        // resolver's externally visible post-state, never a direct payload dereference.
        std::memcpy(instance_.data() + kAuthorityCacheOffset,
                    resolved_.data(),
                    resolved_.size());
        instance_.data()[kDirtyByteOffset] = std::byte{1U};
    }
    [[nodiscard]] std::byte* instance() const noexcept { return instance_.data(); }
    [[nodiscard]] PacketReference* packet() noexcept { return &packet_; }
    [[nodiscard]] PacketReference* sense_packet() noexcept { return &sense_packet_; }
    [[nodiscard]] Page& instance_page() noexcept { return instance_; }
    [[nodiscard]] const AuthorityBody& before() const noexcept { return before_; }
    [[nodiscard]] const AuthorityBody& resolved() const noexcept { return resolved_; }
private:
    Page instance_{};
    Page opaque_payload_;
    AuthorityBody before_{};
    AuthorityBody resolved_{};
    SenseBody sense_{};
    PacketReference packet_{};
    PacketReference sense_packet_{};
};

[[nodiscard]] ApplyCaptureRecord make_apply_record(Fixture& fixture,
                                                   std::uint64_t call = 1U) {
    fixture.restore_before();
    PendingApplyCapture pending{};
    const AuthorityWireSnapshot wire = authority_wire();
    CHECK(prepare_apply_capture(pending,
                                context(),
                                kWeaponDownIdentity,
                                metadata(call),
                                41U,
                                fixture.instance(),
                                fixture.packet(),
                                wire) == CaptureBuildResult::ready);
    fixture.simulate_native_resolver_and_apply();
    ApplyCaptureRecord record{};
    CHECK(finish_apply_capture(pending, context(), fixture.instance(), record)
          == CaptureBuildResult::complete);
    CHECK(valid_raw_record(record));
    return record;
}

[[nodiscard]] ReconcileCaptureInput reconcile_observation() noexcept {
    ReconcileCaptureInput input{};
    input.branches = ReconcileBranch::enumerate_materialize
                     | ReconcileBranch::attach_materialize | ReconcileBranch::subscriber;
    input.selected_linked_reference = ObjectReference{kActivityRegistry, 34, std::byte{}, 22};
    input.live_object_count = 2U;
    input.live_object_datums[0] = 0x101U;
    input.live_object_datums[1] = 0x202U;
    input.subscriber = SubscriberObservation{
        0x1000U, 0x2000U, 0x7FF6009F28ACULL, 11U, 12U, 13, true};
    return input;
}

[[nodiscard]] ReconcileCaptureRecord make_reconcile_record(Fixture& fixture,
                                                           std::uint64_t call = 2U) {
    PendingReconcileCapture pending{};
    CHECK(prepare_reconcile_capture(pending,
                                    context(),
                                    kWeaponDownIdentity,
                                    metadata(call),
                                    fixture.instance()) == CaptureBuildResult::ready);
    ReconcileCaptureRecord record{};
    CHECK(finish_reconcile_capture(pending,
                                   context(),
                                   fixture.instance(),
                                   reconcile_observation(),
                                   record) == CaptureBuildResult::complete);
    CHECK(valid_raw_record(record));
    return record;
}

[[nodiscard]] SenseCaptureRecord make_sense_record(Fixture& fixture,
                                                   std::uint64_t call = 3U) {
    PendingSenseCapture pending{};
    CHECK(prepare_sense_capture(pending,
                                context(),
                                kWeaponDownIdentity,
                                metadata(call),
                                99U,
                                101U,
                                fixture.instance()) == CaptureBuildResult::ready);
    SenseCaptureRecord record{};
    CHECK(finish_sense_capture(pending,
                               context(),
                               fixture.instance(),
                               fixture.sense_packet(),
                               sense_wire(),
                               record) == CaptureBuildResult::complete);
    CHECK(valid_raw_record(record));
    return record;
}

[[nodiscard]] constexpr std::uint8_t nibble(char value) noexcept {
    return value >= '0' && value <= '9'
               ? static_cast<std::uint8_t>(value - '0')
               : static_cast<std::uint8_t>(value - 'A' + 10);
}
[[nodiscard]] bool prefix_equals(std::span<const std::byte> prefix,
                                 std::string_view hex) noexcept {
    if (hex.size() != prefix.size() * 2U) {
        return false;
    }
    for (std::size_t index = 0U; index < prefix.size(); ++index) {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (nibble(hex[index * 2U]) << 4U) | nibble(hex[index * 2U + 1U]));
        if (prefix[index] != std::byte{value}) {
            return false;
        }
    }
    return true;
}

void exact_static_contract_and_live_admission() {
    static_assert(kObservationOnly && !kOwnsNativeDetour);
    static_assert(!kProvidesAuthorityEncoder && !kProvidesAuthorityWriter);
    static_assert(!kProvidesGuessedBooleanPredicate);
    static_assert(!kDefaultTelemetryContainsRawBodies);
    static_assert(!kDefaultTelemetryContainsAbsoluteAddresses);
    static_assert(kActiveAuthorityDisposition
                  == ActiveAuthorityDisposition::unknown_do_not_encode);
    static_assert(sizeof(AuthorityLayout) == 0x70U);
    static_assert(sizeof(SenseLayout) == 0x10U);
    static_assert(kSenseWireBits == 97U && kSenseWireBytes == 13U);
    static_assert(std::is_same_v<Apply,
                                 void(__fastcall*)(void*, const PacketReference*)>);

    CHECK(admitted_hold_definition(kWeaponDownIdentity));
    CHECK(admitted_hold_definition(kNoCombatAbilitiesIdentity));
    CHECK(kWeaponDownIdentity.definition == 0x80F47B7FU);
    CHECK(kNoCombatAbilitiesIdentity.definition == 0x80F47B82U);

    constexpr std::array<std::string_view, kNativeSurfaceCount> prefixes{
        "48895C24084889742410574883EC30448B02488BF14C8B4A08",
        "40534883EC20488BD9E86226000080BB81010000007508488BCB",
        "48895C2410574881EC20010000488B0534A16B014833C44889842410010000",
        "4889742420574883EC20448B19488BF14C8B4908418BD3488B059276A401",
        "4883EC28448B01418BC0488B49084181E0FF1F0000C1F80D",
        "48895C2418574883EC208BC28BDA25FF1F0000488BF9448BC0",
        "4055535741544156488DAC2420F0FFFFB8E0100000E856CFE800",
        "48895C2410574883EC20448B09488BD9418BC1488BFAC1F80D"};
    for (std::size_t index = 0U; index < prefixes.size(); ++index) {
        CHECK(prefix_equals(native_boundary(static_cast<NativeSurface>(index)).mapped_prefix,
                            prefixes[index]));
    }

    PackedRuntimeIdentity measured{};
    CHECK(measure_pinned_packed_runtime(measured));
    CHECK(matches_pinned_packed_runtime(measured));
    CHECK(measured.sha256 != kPinnedUnpackedProvenanceSha256);

    LiveRuntimeAddressGroup output{};
    const HMODULE testModule = GetModuleHandleW(nullptr);
    CHECK(testModule != nullptr);
    CHECK(validate_live_runtime_group(testModule, 1U, output)
          == LiveRuntimeValidation::module_path_mismatch);
    CHECK((output.addresses == std::array<std::uintptr_t, kNativeSurfaceCount>{}));
    CHECK(validate_live_runtime_group(reinterpret_cast<void*>(1U), 1U, output)
          == LiveRuntimeValidation::wrong_process_main_module);
}

void wire_snapshots_are_guarded_and_bit_exact() {
    CHECK(authority_wire().bit_count == 186U);
    CHECK(sense_wire().bit_count == 97U);

    std::array<std::byte, 13U> bytes{};
    bytes.back() = std::byte{0x80U};
    for (const std::size_t bits : {0U, 96U, 98U, 99U, 104U}) {
        SenseWireSnapshot snapshot{};
        const WireSourceEvidence source{
            bytes, bits, 1U, WireBitOrder::most_significant_bit_first, true, true, false};
        CHECK(make_sense_wire_snapshot(source, snapshot) == WireSnapshotResult::invalid_length);
    }
    bytes.back() = std::byte{0x81U};
    SenseWireSnapshot nonzeroPadding{};
    CHECK(make_sense_wire_snapshot(
              WireSourceEvidence{bytes,
                                 97U,
                                 1U,
                                 WireBitOrder::most_significant_bit_first,
                                 true,
                                 true,
                                 false},
              nonzeroPadding) == WireSnapshotResult::nonzero_unused_bits);

    Page inaccessible(PAGE_NOACCESS);
    CHECK(inaccessible.data() != nullptr);
    SenseWireSnapshot sense{};
    const std::span<const std::byte> badSense{inaccessible.data(), kSenseWireBytes};
    CHECK(make_sense_wire_snapshot(
              WireSourceEvidence{badSense,
                                 97U,
                                 2U,
                                 WireBitOrder::most_significant_bit_first,
                                 true,
                                 true,
                                 false},
              sense) == WireSnapshotResult::unreadable);
    AuthorityWireSnapshot authority{};
    const std::span<const std::byte> badAuthority{inaccessible.data(), 24U};
    CHECK(make_authority_wire_snapshot(
              WireSourceEvidence{badAuthority,
                                 186U,
                                 2U,
                                 WireBitOrder::most_significant_bit_first,
                                 true,
                                 true,
                                 true},
              authority) == WireSnapshotResult::unreadable);
}

void resolver_timing_and_guarded_apply_reads() {
    Fixture fixture{};
    const ApplyCaptureRecord record = make_apply_record(fixture);
    CHECK(record.packet_payload_was_nonnull);
    CHECK(record.packet_pad == 0xA5A5A5A5U);
    CHECK(record.cache_before == fixture.before());
    CHECK(record.cache_after == fixture.resolved());
    CHECK(record.incoming_decoded_after_resolver == fixture.resolved());
    CHECK(!record.equal_before);
    CHECK(record.dirty_before == 0U && record.dirty_after == 1U);
    CHECK(record.entry_context == record.exit_context);

    Page badWrapper(PAGE_NOACCESS);
    PendingApplyCapture pending{};
    CHECK(prepare_apply_capture(pending,
                                context(),
                                kWeaponDownIdentity,
                                metadata(10U),
                                1U,
                                fixture.instance(),
                                reinterpret_cast<const PacketReference*>(badWrapper.data()),
                                authority_wire()) == CaptureBuildResult::unreadable);

    Fixture badPre{};
    CHECK(badPre.instance_page().protect(PAGE_NOACCESS));
    CHECK(prepare_apply_capture(pending,
                                context(),
                                kWeaponDownIdentity,
                                metadata(11U),
                                1U,
                                badPre.instance(),
                                badPre.packet(),
                                authority_wire()) == CaptureBuildResult::unreadable);

    Fixture badPost{};
    CHECK(prepare_apply_capture(pending,
                                context(),
                                kWeaponDownIdentity,
                                metadata(12U),
                                1U,
                                badPost.instance(),
                                badPost.packet(),
                                authority_wire()) == CaptureBuildResult::ready);
    CHECK(badPost.instance_page().protect(PAGE_NOACCESS));
    ApplyCaptureRecord ignored{};
    CHECK(finish_apply_capture(pending, context(), badPost.instance(), ignored)
          == CaptureBuildResult::unreadable);

    Fixture reopened{};
    CHECK(prepare_apply_capture(pending,
                                context(),
                                kWeaponDownIdentity,
                                metadata(13U),
                                1U,
                                reopened.instance(),
                                reopened.packet(),
                                authority_wire()) == CaptureBuildResult::ready);
    CHECK(reopened.instance_page().protect(PAGE_NOACCESS));
    // Same wrapper/handle, new activation generation: reject before touching protected post-state.
    CHECK(finish_apply_capture(pending, context(10U), reopened.instance(), ignored)
          == CaptureBuildResult::activation_changed);
}

void reconcile_and_sense_have_exact_entry_exit_fences() {
    Fixture fixture{};
    ReconcileCaptureRecord reconcile = make_reconcile_record(fixture);
    CHECK(reconcile.entry_context == reconcile.exit_context);
    CHECK(reconcile.observation.live_object_datums[1] == 0x202U);

    PendingReconcileCapture pendingReconcile{};
    CHECK(prepare_reconcile_capture(pendingReconcile,
                                    context(),
                                    kWeaponDownIdentity,
                                    metadata(20U),
                                    fixture.instance()) == CaptureBuildResult::ready);
    CHECK(fixture.instance_page().protect(PAGE_NOACCESS));
    CHECK(finish_reconcile_capture(pendingReconcile,
                                   context(10U),
                                   fixture.instance(),
                                   reconcile_observation(),
                                   reconcile) == CaptureBuildResult::activation_changed);

    Fixture senseFixture{};
    const SenseCaptureRecord sense = make_sense_record(senseFixture);
    CHECK(sense.entry_context == sense.exit_context);
    CHECK(sense.outbound_wire.bit_count == 97U);
    CHECK(sense.packet_points_to_sense_cache);

    PendingSenseCapture pendingSense{};
    CHECK(prepare_sense_capture(pendingSense,
                                context(),
                                kWeaponDownIdentity,
                                metadata(21U),
                                1U,
                                0U,
                                senseFixture.instance()) == CaptureBuildResult::ready);
    Page badPacket(PAGE_NOACCESS);
    SenseCaptureRecord ignored{};
    CHECK(finish_sense_capture(pendingSense,
                               context(10U),
                               senseFixture.instance(),
                               reinterpret_cast<const PacketReference*>(badPacket.data()),
                               sense_wire(),
                               ignored) == CaptureBuildResult::activation_changed);

    CHECK(prepare_sense_capture(pendingSense,
                                context(),
                                kWeaponDownIdentity,
                                metadata(22U),
                                1U,
                                0U,
                                senseFixture.instance()) == CaptureBuildResult::ready);
    CHECK(finish_sense_capture(pendingSense,
                               context(),
                               senseFixture.instance(),
                               reinterpret_cast<const PacketReference*>(badPacket.data()),
                               sense_wire(),
                               ignored) == CaptureBuildResult::unreadable);

    Fixture badSenseBody{};
    CHECK(prepare_sense_capture(pendingSense,
                                context(),
                                kWeaponDownIdentity,
                                metadata(23U),
                                1U,
                                0U,
                                badSenseBody.instance()) == CaptureBuildResult::ready);
    PacketReference external{kHoldSenseSchema, 0U, badPacket.data()};
    CHECK(finish_sense_capture(pendingSense,
                               context(),
                               badSenseBody.instance(),
                               &external,
                               sense_wire(),
                               ignored) == CaptureBuildResult::unreadable);
}

std::atomic_uint32_t gNativeCalls{};
std::atomic_uint32_t gBeforeCalls{};
std::atomic_uint32_t gAfterCalls{};
std::atomic_bool gNativeEntered{};
std::atomic_bool gReleaseNative{};
void __fastcall immediate_unary(void*) { gNativeCalls.fetch_add(1U); }
void __fastcall blocking_unary(void*) {
    gNativeCalls.fetch_add(1U);
    gNativeEntered.store(true, std::memory_order_release);
    gNativeEntered.notify_all();
    gReleaseNative.wait(false, std::memory_order_acquire);
}
std::uintptr_t __fastcall attach(void* instance, std::uint32_t datum) {
    gNativeCalls.fetch_add(1U);
    return reinterpret_cast<std::uintptr_t>(instance) + datum;
}
void reset_calls() noexcept {
    gNativeCalls = 0U;
    gBeforeCalls = 0U;
    gAfterCalls = 0U;
    gNativeEntered = false;
    gReleaseNative = false;
}
template <typename Predicate>
[[nodiscard]] bool wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    return predicate();
}

void epoch_gate_and_single_assignment_originals_cover_reopen() {
    reset_calls();
    FullCallGate publicationGate{};
    CHECK(publicationGate.begin_activation());
    OriginalSlot<Unary> publicationSlot{};
    std::thread duringCommit([&]() noexcept {
        forward_void_original_once(
            publicationGate,
            publicationSlot,
            5U,
            []() noexcept { gBeforeCalls.fetch_add(1U); },
            []() noexcept { gAfterCalls.fetch_add(1U); },
            static_cast<void*>(nullptr));
    });
    CHECK(wait_until([&] { return publicationGate.active_calls() == 1U; }));
    CHECK(publicationSlot.publish(5U, &immediate_unary) == OriginalPublishResult::published);
    duringCommit.join();
    CHECK(gNativeCalls == 1U && gBeforeCalls == 1U && gAfterCalls == 1U);
    CHECK(publicationSlot.publish(5U, &immediate_unary)
          == OriginalPublishResult::already_published);
    CHECK(publicationSlot.publish(6U, nullptr) == OriginalPublishResult::invalid);

    reset_calls();
    forward_void_original_once(publicationGate,
                               publicationSlot,
                               999U,
                               []() noexcept { gBeforeCalls.fetch_add(1U); },
                               []() noexcept { gAfterCalls.fetch_add(1U); },
                               static_cast<void*>(nullptr));
    CHECK(gNativeCalls == 1U && gBeforeCalls == 0U && gAfterCalls == 0U);

    reset_calls();
    FullCallGate reopened{};
    CHECK(reopened.begin_activation());
    OriginalSlot<Unary> blockingSlot{};
    CHECK(blockingSlot.publish(7U, &blocking_unary) == OriginalPublishResult::published);
    std::thread oldCall([&]() noexcept {
        forward_void_original_once(
            reopened,
            blockingSlot,
            7U,
            []() noexcept { gBeforeCalls.fetch_add(1U); },
            []() noexcept { gAfterCalls.fetch_add(1U); },
            static_cast<void*>(nullptr));
    });
    CHECK(wait_until([] { return gNativeEntered.load(std::memory_order_acquire); }));
    const std::uint64_t oldEpoch = reopened.epoch();
    reopened.quiesce();
    CHECK(reopened.begin_activation());
    CHECK(reopened.epoch() > oldEpoch);
    gReleaseNative.store(true, std::memory_order_release);
    gReleaseNative.notify_all();
    oldCall.join();
    CHECK(gNativeCalls == 1U && gBeforeCalls == 1U && gAfterCalls == 0U);

    reset_calls();
    FullCallGate falseThenTrue{};
    OriginalSlot<Unary> falseEntrySlot{};
    CHECK(falseEntrySlot.publish(8U, &blocking_unary) == OriginalPublishResult::published);
    std::thread quiescedCall([&]() noexcept {
        forward_void_original_once(
            falseThenTrue,
            falseEntrySlot,
            8U,
            []() noexcept { gBeforeCalls.fetch_add(1U); },
            []() noexcept { gAfterCalls.fetch_add(1U); },
            static_cast<void*>(nullptr));
    });
    CHECK(wait_until([] { return gNativeEntered.load(std::memory_order_acquire); }));
    CHECK(falseThenTrue.begin_activation());
    gReleaseNative.store(true, std::memory_order_release);
    gReleaseNative.notify_all();
    quiescedCall.join();
    CHECK(gNativeCalls == 1U && gBeforeCalls == 0U && gAfterCalls == 0U);

    reset_calls();
    FullCallGate valueGate{};
    CHECK(valueGate.begin_activation());
    OriginalSlot<HoldAttach> attachSlot{};
    CHECK(attachSlot.publish(9U, &attach) == OriginalPublishResult::published);
    CHECK(forward_value_original_once(valueGate,
                                      attachSlot,
                                      9U,
                                      []() noexcept { gBeforeCalls.fetch_add(1U); },
                                      []() noexcept { gAfterCalls.fetch_add(1U); },
                                      reinterpret_cast<void*>(0x1000U),
                                      0x55U) == 0x1055U);
    CHECK(gNativeCalls == 1U);
    CHECK(attachSlot.clear_after_confirmed_removal(9U));

    FullCallGate exhausted{};
    exhausted.testing_set_epoch((std::numeric_limits<std::uint64_t>::max)());
    CHECK(!exhausted.begin_activation());
    CHECK(exhausted.epoch_exhausted());

    FullCallGate changing{};
    constexpr std::uint32_t transitionCount = 20'000U;
    std::latch transitionStart{2};
    std::atomic_bool transitionFailure{};
    std::thread lifecycleTransitions([&]() noexcept {
        transitionStart.arrive_and_wait();
        for (std::uint32_t index = 0U; index < transitionCount; ++index) {
            if (!changing.begin_activation()) {
                transitionFailure.store(true, std::memory_order_relaxed);
                return;
            }
            changing.quiesce();
        }
    });
    transitionStart.arrive_and_wait();
    for (std::uint32_t index = 0U; index < transitionCount; ++index) {
        FullCallGate::Scope boundedSnapshot{changing};
        (void)boundedSnapshot.entry_observation_eligible();
        (void)boundedSnapshot.post_observation_eligible();
    }
    lifecycleTransitions.join();
    CHECK(!transitionFailure.load(std::memory_order_relaxed));
    CHECK(changing.active_calls() == 0U);
}

void producer_rings_are_spsc_isolated_and_concurrent() {
    Fixture fixture{};
    const ApplyCaptureRecord applyPrototype = make_apply_record(fixture, 30U);
    const ReconcileCaptureRecord reconcilePrototype = make_reconcile_record(fixture, 31U);
    const SenseCaptureRecord sensePrototype = make_sense_record(fixture, 32U);

    ApplyCaptureRing isolation{101U};
    ApplyCaptureRecord source = applyPrototype;
    CHECK(isolation.try_push(source, 101U) == RingPushResult::enqueued);
    const AuthorityBody expected = source.cache_after;
    source.cache_after.fill(std::byte{0xFFU});
    ApplyCaptureRecord isolated{};
    CHECK(isolation.try_pop(isolated) == RingPopResult::success);
    CHECK(isolated.cache_after == expected);
    CHECK(isolation.empty());
    CHECK(isolation.try_push(applyPrototype, 999U) == RingPushResult::wrong_producer);

    ApplyCaptureRing full{102U};
    for (std::size_t index = 0U; index < kApplyRingCapacity; ++index) {
        CHECK(full.try_push(applyPrototype, 102U) == RingPushResult::enqueued);
    }
    CHECK(full.try_push(applyPrototype, 102U) == RingPushResult::full);
    CHECK(full.counters().dropped_full == 1U);

    ApplyCaptureRing exhausted{103U};
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted.try_push(applyPrototype, 103U) == RingPushResult::sequence_exhausted);

    constexpr std::uint64_t records = 200U;
    ApplyCaptureRing applyRing{201U};
    ReconcileCaptureRing reconcileRing{202U};
    SenseCaptureRing senseRing{203U};
    std::latch start{4};
    std::atomic_bool concurrentFailure{};

    auto applyProducer = [&]() noexcept {
        start.count_down();
        start.wait();
        for (std::uint64_t index = 1U; index <= records; ++index) {
            ApplyCaptureRecord record = applyPrototype;
            record.metadata.call_id = index;
            record.authority_publication_sequence = index;
            while (applyRing.try_push(record, 201U) == RingPushResult::full) {
                std::this_thread::yield();
            }
        }
    };
    auto reconcileProducer = [&]() noexcept {
        start.count_down();
        start.wait();
        for (std::uint64_t index = 1U; index <= records; ++index) {
            ReconcileCaptureRecord record = reconcilePrototype;
            record.metadata.call_id = index;
            while (reconcileRing.try_push(record, 202U) == RingPushResult::full) {
                std::this_thread::yield();
            }
        }
    };
    auto senseProducer = [&]() noexcept {
        start.count_down();
        start.wait();
        for (std::uint64_t index = 1U; index <= records; ++index) {
            SenseCaptureRecord record = sensePrototype;
            record.metadata.call_id = index;
            record.report_sequence = index;
            while (senseRing.try_push(record, 203U) == RingPushResult::full) {
                std::this_thread::yield();
            }
        }
    };
    auto consumer = [&]() noexcept {
        start.count_down();
        start.wait();
        std::uint64_t applyExpected = 1U;
        std::uint64_t reconcileExpected = 1U;
        std::uint64_t senseExpected = 1U;
        while (applyExpected <= records || reconcileExpected <= records
               || senseExpected <= records) {
            ApplyCaptureRecord apply{};
            if (applyExpected <= records
                && applyRing.try_pop(apply) == RingPopResult::success) {
                if (apply.sequence != applyExpected || apply.metadata.call_id != applyExpected
                    || apply.authority_publication_sequence != applyExpected) {
                    concurrentFailure = true;
                }
                ++applyExpected;
            }
            ReconcileCaptureRecord reconcile{};
            if (reconcileExpected <= records
                && reconcileRing.try_pop(reconcile) == RingPopResult::success) {
                if (reconcile.sequence != reconcileExpected
                    || reconcile.metadata.call_id != reconcileExpected) {
                    concurrentFailure = true;
                }
                ++reconcileExpected;
            }
            SenseCaptureRecord sense{};
            if (senseExpected <= records
                && senseRing.try_pop(sense) == RingPopResult::success) {
                if (sense.sequence != senseExpected || sense.metadata.call_id != senseExpected
                    || sense.report_sequence != senseExpected) {
                    concurrentFailure = true;
                }
                ++senseExpected;
            }
            std::this_thread::yield();
        }
    };

    std::thread first{applyProducer};
    std::thread second{reconcileProducer};
    std::thread third{senseProducer};
    std::thread drain{consumer};
    first.join();
    second.join();
    third.join();
    drain.join();
    CHECK(!concurrentFailure.load());
    CHECK(applyRing.empty() && reconcileRing.empty() && senseRing.empty());
    CHECK(applyRing.counters().accepted == records);
    CHECK(reconcileRing.counters().accepted == records);
    CHECK(senseRing.counters().accepted == records);
}

[[nodiscard]] constexpr KeyedDigest128 digest(std::uint64_t seed) noexcept {
    return KeyedDigest128{0xA000000000000000ULL | seed, 0xB000000000000000ULL | seed};
}
template <typename Telemetry>
[[nodiscard]] bool contains_u64(const Telemetry& telemetry, std::uint64_t value) noexcept {
    std::array<std::byte, sizeof(Telemetry)> bytes{};
    std::memcpy(bytes.data(), &telemetry, sizeof telemetry);
    std::array<std::byte, sizeof value> needle{};
    std::memcpy(needle.data(), &value, sizeof value);
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end();
}

void telemetry_preserves_provenance_without_raw_or_addresses() {
    Fixture fixture{};
    const ApplyCaptureRecord apply = make_apply_record(fixture, 40U);
    const ReconcileCaptureRecord reconcile = make_reconcile_record(fixture, 41U);
    const SenseCaptureRecord sense = make_sense_record(fixture, 42U);
    const TelemetryProjectionContext projection{77U, 0x901U, 0x902U, 3U};
    const ApplyTelemetryDigests applyDigests{digest(1U), digest(2U), digest(3U), digest(4U)};
    const ReconcileTelemetryDigests reconcileDigests{
        digest(5U), digest(6U), digest(7U), digest(8U), digest(9U)};
    const SenseTelemetryDigests senseDigests{digest(10U), digest(11U)};

    ApplyTelemetry applyTelemetry{};
    CHECK(default_telemetry(apply, projection, applyDigests, applyTelemetry));
    CHECK(applyTelemetry.provenance.build_id == kPackedBuildTelemetryId);
    CHECK(applyTelemetry.provenance.module_generation == context().module_generation);
    CHECK(applyTelemetry.provenance.activation_generation == context().activation_generation);
    CHECK(applyTelemetry.provenance.source_session_pseudonym == 0x901U);
    CHECK(applyTelemetry.digests.incoming == digest(1U));
    CHECK(!contains_u64(applyTelemetry, apply.instance_identity));
    CHECK(!contains_u64(applyTelemetry, context().native_activity_wrapper));
    CHECK(!contains_u64(applyTelemetry, context().source_session));
    CHECK(!contains_u64(applyTelemetry, apply.metadata.caller_rva));

    ReconcileTelemetry reconcileTelemetry{};
    CHECK(!default_telemetry(
        reconcile, projection, reconcileDigests, 0U, false, reconcileTelemetry));
    CHECK(default_telemetry(
        reconcile, projection, reconcileDigests, 0x9F28ACU, true, reconcileTelemetry));
    CHECK(reconcileTelemetry.verified_subscriber_target_rva == 0x9F28ACU);
    CHECK(!contains_u64(reconcileTelemetry,
                        reconcile.observation.subscriber.slot_1a8_target));

    SenseTelemetry senseTelemetry{};
    CHECK(default_telemetry(sense, projection, senseDigests, senseTelemetry));
    CHECK(senseTelemetry.outbound_wire_bits == 97U);
    CHECK(senseTelemetry.provenance.roster_epoch == context().roster_epoch);
}

void protected_detach_and_claim_validators_remain_fail_closed() {
    constexpr std::array<std::uintptr_t, kNativeSurfaceCount> originals{
        0x1000U, 0x2000U, 0x3000U, 0x4000U,
        0x5000U, 0x6000U, 0x7000U, 0x8000U};
    std::array<FullCallGate, kNativeSurfaceCount> gates{};
    std::array<FullCallGate*, kNativeSurfaceCount> gatePointers{};
    for (std::size_t index = 0U; index < gates.size(); ++index) {
        gatePointers[index] = &gates[index];
    }
    HookGroupState group{};
    CHECK(group.begin_install(10U));
    CHECK(group.complete_install(originals));
    CHECK(group.quiesce());
    {
        FullCallGate::Scope active{gates[3]};
        CHECK(group.record_protected_detach(gatePointers,
                                            ProtectedDetachDisposition::removed)
              == ProtectedDetachResult::protected_code_active);
    }
    CHECK(group.record_protected_detach(gatePointers,
                                        ProtectedDetachDisposition::deferred)
          == ProtectedDetachResult::adapter_deferred);
    CHECK(group.snapshot().originals == originals);
    CHECK(group.record_protected_detach(gatePointers,
                                        ProtectedDetachDisposition::failed)
          == ProtectedDetachResult::adapter_failed);
    CHECK(group.snapshot().originals == originals);
    CHECK(group.record_protected_detach(gatePointers,
                                        ProtectedDetachDisposition::removed)
          == ProtectedDetachResult::removed);
    CHECK(group.snapshot().phase == HookGroupPhase::detached);

    Fixture fixture{};
    const ApplyCaptureRecord apply = make_apply_record(fixture, 50U);
    ActiveBodyClaim body{true, true, true, true, true, true, true};
    CHECK(validate_active_body_claim(apply, body)
          == ActiveBodyClaimResult::accepted_observation_only);
    body.externally_proven_decode_encode_round_trip_bit_exact = false;
    CHECK(validate_active_body_claim(apply, body) == ActiveBodyClaimResult::rejected);
    CHECK(kActiveAuthorityDisposition
          == ActiveAuthorityDisposition::unknown_do_not_encode);

    RoleClassificationClaim role{};
    role.context = context();
    role.identity = kWeaponDownIdentity;
    role.hold_publication_generation = 41U;
    role.scene_publication_generation = 42U;
    role.weapon_down_completed_trials = kAllRequiredTrials;
    role.no_combat_abilities_completed_trials = kAllRequiredTrials;
    role.scene_proceeded = true;
    CHECK(validate_role_classification_claim(role)
          == HoldRoleClassification::unused_in_captured_opening);
    RoleClassificationClaim unusedWithReadEdge = role;
    unusedWithReadEdge.scene_read_edge = ExplicitSceneReadEdgeEvidence{
        context(),
        kWeaponDownIdentity,
        41U,
        42U,
        0xDEADBEEFU,
        ReadEdgeProvenance::mission_vm_instruction,
        HoldReadSource::acknowledged_sense_generation,
        true,
        true,
        true};
    CHECK(validate_role_classification_claim(unusedWithReadEdge)
          == HoldRoleClassification::inconclusive);
    role.schema_complete_non_neutral_authority_apply = true;
    role.materialize_or_subscriber_transition = true;
    role.matching_sense_host_acknowledgement = true;
    role.correlated_player_facing_restriction = true;
    role.repeated_hold_before_scene_same_publication_generation = true;
    CHECK(validate_role_classification_claim(role)
          == HoldRoleClassification::parallel_restriction_lane);
    role.host_publication_dag_explicitly_orders_hold_before_scene = true;
    CHECK(validate_role_classification_claim(role)
          == HoldRoleClassification::ordered_presentation_predecessor);

    RoleClassificationClaim missingOrderedFact = role;
    missingOrderedFact.scene_proceeded = false;
    CHECK(validate_role_classification_claim(missingOrderedFact)
          == HoldRoleClassification::parallel_restriction_lane);
    missingOrderedFact = role;
    missingOrderedFact.hold_publication_generation = 0U;
    CHECK(validate_role_classification_claim(missingOrderedFact)
          == HoldRoleClassification::parallel_restriction_lane);
    missingOrderedFact = role;
    missingOrderedFact.scene_publication_generation = 0U;
    CHECK(validate_role_classification_claim(missingOrderedFact)
          == HoldRoleClassification::parallel_restriction_lane);
    missingOrderedFact = role;
    missingOrderedFact.hold_publication_generation = 0U;
    missingOrderedFact.scene_publication_generation = 0U;
    CHECK(validate_role_classification_claim(missingOrderedFact)
          == HoldRoleClassification::parallel_restriction_lane);

    role.scene_read_edge = ExplicitSceneReadEdgeEvidence{context(),
                                                         kWeaponDownIdentity,
                                                         41U,
                                                         42U,
                                                         0xDEADBEEFU,
                                                         ReadEdgeProvenance::mission_vm_instruction,
                                                         HoldReadSource::acknowledged_sense_generation,
                                                         true,
                                                         true,
                                                         true};
    CHECK(validate_role_classification_claim(role) == HoldRoleClassification::strict_scene_gate);
    RoleClassificationClaim missingStrictFact = role;
    missingStrictFact.scene_proceeded = false;
    CHECK(validate_role_classification_claim(missingStrictFact)
          == HoldRoleClassification::inconclusive);
    missingStrictFact = role;
    missingStrictFact.hold_publication_generation = 0U;
    CHECK(validate_role_classification_claim(missingStrictFact)
          == HoldRoleClassification::inconclusive);
    missingStrictFact = role;
    missingStrictFact.scene_publication_generation = 0U;
    CHECK(validate_role_classification_claim(missingStrictFact)
          == HoldRoleClassification::inconclusive);
    missingStrictFact = role;
    missingStrictFact.hold_publication_generation = 0U;
    missingStrictFact.scene_publication_generation = 0U;
    CHECK(validate_role_classification_claim(missingStrictFact)
          == HoldRoleClassification::inconclusive);

    role.scene_read_edge.hold_publication_generation = 999U;
    CHECK(validate_role_classification_claim(role)
          == HoldRoleClassification::ordered_presentation_predecessor);

    role = {};
    role.context = context();
    role.identity = kWeaponDownIdentity;
    role.weapon_down_completed_trials = kAllRequiredTrials;
    role.no_combat_abilities_completed_trials =
        kAllRequiredTrials
        & ~static_cast<std::uint16_t>(RequiredTrial::reentry_volume_60_29);
    role.scene_proceeded = true;
    CHECK(validate_role_classification_claim(role) == HoldRoleClassification::inconclusive);
}

} // namespace

int main() {
    exact_static_contract_and_live_admission();
    wire_snapshots_are_guarded_and_bit_exact();
    resolver_timing_and_guarded_apply_reads();
    reconcile_and_sense_have_exact_entry_exit_fences();
    epoch_gate_and_single_assignment_originals_cover_reopen();
    producer_rings_are_spsc_isolated_and_concurrent();
    telemetry_preserves_provenance_without_raw_or_addresses();
    protected_detach_and_claim_validators_remain_fail_closed();

    if (gFailureCount != 0) {
        std::cerr << gFailureCount << " Type-26 remediation check(s) failed\n";
        return 1;
    }
    std::cout << "all Type-26 remediation checks passed\n";
    return 0;
}
