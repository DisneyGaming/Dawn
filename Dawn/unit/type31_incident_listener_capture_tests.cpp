#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "client/hooks/bootflow/opening_authority/type31_incident_listener_capture.h"

namespace capture = dawn::client::hooks::bootflow::opening_authority::type31_incident;
using namespace capture;

namespace {

int g_failure_count{};
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " #condition << '\n';     \
            ++g_failure_count;                                                                     \
        }                                                                                          \
    } while (false)

template <typename T>
void write_value(std::byte* base, std::size_t offset, const T& value) noexcept {
    std::memcpy(base + offset, &value, sizeof value);
}

[[nodiscard]] CaptureMetadata meta(std::uint64_t tick, std::uintptr_t rva) noexcept {
    return {tick, GetCurrentThreadId(), rva};
}
[[nodiscard]] OwnerToken owner(std::uint64_t id, std::uint64_t generation) noexcept {
    return {Presence::present, id, generation};
}

enum class MutatedOwner : std::uint8_t {
    none,
    activation,
    authority,
    membership,
    manager,
    listener
};
struct OwnerFixture final {
    OwnershipSnapshot stable{};
    std::atomic<std::uint32_t> reads{};
    std::uint32_t mutate_from{(std::numeric_limits<std::uint32_t>::max)()};
    MutatedOwner mutated{MutatedOwner::none};
    bool fail{};

    OwnerFixture() noexcept = default;
    OwnerFixture(const OwnerFixture& other) noexcept
        : stable(other.stable), reads(other.reads.load(std::memory_order_relaxed)),
          mutate_from(other.mutate_from), mutated(other.mutated), fail(other.fail) {}
    OwnerFixture& operator=(const OwnerFixture&) = delete;

    static bool read(void* context, OwnershipSnapshot& output) noexcept {
        auto& self = *static_cast<OwnerFixture*>(context);
        const std::uint32_t n = self.reads.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        std::atomic_thread_fence(std::memory_order_acquire);
        output = self.stable;
        if (n >= self.mutate_from) {
            OwnerToken* changed{};
            switch (self.mutated) {
            case MutatedOwner::activation:
                changed = &output.activation;
                break;
            case MutatedOwner::authority:
                changed = &output.authority;
                break;
            case MutatedOwner::membership:
                changed = &output.membership;
                break;
            case MutatedOwner::manager:
                changed = &output.incident_manager;
                break;
            case MutatedOwner::listener:
                changed = &output.listener_table;
                break;
            case MutatedOwner::none:
                break;
            }
            if (changed != nullptr) {
                ++changed->generation;
            }
        }
        std::atomic_thread_fence(std::memory_order_release);
        return !self.fail;
    }
    [[nodiscard]] OwnershipSnapshotProvider provider() noexcept {
        return {this, &read};
    }
};

class NativeFixture final {
public:
    explicit NativeFixture(const PointIdentity& point = kVignettePoint) noexcept : point_(point) {
        for (std::size_t i = 0U; i < root_.size(); ++i)
            root_[i] = std::byte{static_cast<std::uint8_t>(0x20U + i)};
        for (std::size_t i = 0U; i < dynamic_.size(); ++i)
            dynamic_[i] = std::byte{static_cast<std::uint8_t>(0x80U + i)};
        for (std::size_t i = 0U; i < table_.size(); ++i)
            table_[i] = std::byte{static_cast<std::uint8_t>(i ^ 0x5AU)};
        for (std::size_t i = 0U; i < row_.size(); ++i)
            row_[i] = std::byte{static_cast<std::uint8_t>(i ^ 0xC3U)};
        write_value(instance_.data(), 0U, point_.definition);
        set_authority({kUnsetGeneration, 42U, 0xABCDEF1234ULL, 1U});
        constexpr std::uint64_t member = 1ULL << 7U;
        write_value(volume_.data(), kRuntimeVolumeMembershipOffset, member);
        write_value(root_.data(), kRootObjectReferenceOffset, point_.volume);
        constexpr std::uint32_t resolved = 0x44556677U;
        write_value(root_.data(), kRootResolvedReferenceOffset, resolved);
    }
    void set_authority(const AuthorityState& state) noexcept {
        write_value(instance_.data(), kConsumedGenerationOffset, state.consumed_generation);
        write_value(instance_.data(), kAuthorityActiveOffset, state.active);
        write_value(instance_.data(), kPendingGenerationOffset, state.pending_generation);
        write_value(instance_.data(), kAuthorityCompanionOffset, state.companion);
    }
    void commit() noexcept {
        set_authority({42U, 42U, 0xABCDEF1234ULL, 1U});
        for (std::byte& value : table_)
            value ^= std::byte{0x0FU};
        for (std::byte& value : row_)
            value ^= std::byte{0x33U};
    }
    [[nodiscard]] void* instance() noexcept {
        return instance_.data();
    }
    [[nodiscard]] void* volume() noexcept {
        return volume_.data();
    }
    [[nodiscard]] const ObjectReference* matched() const noexcept {
        return &point_.volume;
    }
    [[nodiscard]] auto& root() noexcept {
        return root_;
    }
    [[nodiscard]] auto& dynamic() noexcept {
        return dynamic_;
    }
    [[nodiscard]] auto& table() noexcept {
        return table_;
    }
    [[nodiscard]] auto& row() noexcept {
        return row_;
    }

private:
    PointIdentity point_{};
    std::array<std::byte, 0x1A0U> instance_{};
    std::array<std::byte, 0x10U> volume_{};
    std::array<std::byte, kIncidentRootDecodedBytes> root_{};
    std::array<std::byte, 96U> dynamic_{};
    std::array<std::byte, 160U> table_{};
    std::array<std::byte, 80U> row_{};
};

[[nodiscard]] OwnerFixture
make_owners(const RuntimeCohortToken& cohort, std::uint64_t epoch, NativeFixture& native) noexcept {
    OwnerFixture result{};
    result.stable.cohort = cohort;
    result.stable.capture_epoch = epoch;
    result.stable.activation = owner(0x101U, 11U);
    result.stable.activity = owner(0x102U, 12U);
    result.stable.component = owner(reinterpret_cast<std::uintptr_t>(native.instance()), 13U);
    result.stable.authority = owner(0x104U, 14U);
    result.stable.runtime_volume = owner(reinterpret_cast<std::uintptr_t>(native.volume()), 15U);
    result.stable.membership = owner(0x106U, 16U);
    result.stable.incident_manager = owner(0x107U, 17U);
    result.stable.listener_table = owner(0x108U, 18U);
    result.stable.local_player_datum = 0xA5B6C7D8U;
    result.stable.local_player_bit_index = 7U;
    return result;
}

struct TraceOutcome final {
    RestrictedIncidentRecord record{};
    TraceResult begin{TraceResult::invalid_input};
    TraceResult root{TraceResult::invalid_input};
    TraceResult listener_entry{TraceResult::invalid_input};
    TraceResult listener_exit{TraceResult::invalid_input};
    TraceFinishResult finish{};
    bool popped{};
};

[[nodiscard]] TraceOutcome run_trace(TraceCoordinator& coordinator,
                                     OwnerFixture& owners,
                                     NativeFixture& native,
                                     std::uint64_t epoch) {
    TraceOutcome out{};
    EpochCallGate gate{};
    CHECK(gate.open(epoch));
    EpochCallGate::Scope scope{gate};
    ThreadTraceStack stack{};
    RestrictedEvidenceQueue queue{};
    out.begin = coordinator.begin_terminal(stack,
                                           scope,
                                           owners.provider(),
                                           {native.instance(),
                                            native.matched(),
                                            native.volume(),
                                            meta(1U, kTerminalEvaluatorCallsiteRva)});
    constexpr std::uint32_t rootId = kIncidentRootSchema;
    out.root = coordinator.observe_root_dispatch(
        stack,
        owners.provider(),
        {&rootId,
         kIncidentSObjectKind,
         native.root().data(),
         {native.dynamic().data(), native.dynamic().size(), {Presence::present, 35U}},
         meta(2U, native_boundary(NativeSurface::sobject_validate_dispatch).mapped_rva)});
    (void)coordinator.observe_recursive_materialize_submit(stack, owners.provider());
    (void)coordinator.observe_manager_submit(stack, owners.provider());
    (void)coordinator.observe_recursive_visitor(stack, owners.provider());
    (void)coordinator.observe_visitor_callback(stack, owners.provider());
    (void)coordinator.observe_normal_route(stack, owners.provider());
    const std::uintptr_t listener =
        native_boundary(NativeSurface::dynamic_listener_enumeration).mapped_rva;
    out.listener_entry = coordinator.observe_listener_entry(
        stack,
        owners.provider(),
        {{native.table().data(), native.table().size(), {Presence::present, 0U}},
         {native.row().data(), native.row().size(), {Presence::present, 0U}},
         meta(8U, listener)});
    native.commit();
    out.listener_exit = coordinator.observe_listener_exit(
        stack,
        owners.provider(),
        {{native.table().data(), native.table().size(), {Presence::present, 0U}},
         {native.row().data(), native.row().size(), {Presence::present, 0U}},
         meta(9U, listener)});
    out.finish = coordinator.finish_terminal(stack,
                                             scope,
                                             owners.provider(),
                                             native.instance(),
                                             meta(10U, kTerminalEvaluatorReturnRva),
                                             queue);
    out.popped = queue.try_pop(out.record) == QueuePopResult::success;
    CHECK(stack.depth() == 0U);
    return out;
}

[[nodiscard]] RestrictedIncidentRecord template_record(std::uint64_t epoch = 51U) {
    NativeFixture native{};
    OwnerFixture owners = make_owners(testing_runtime_cohort_token(epoch), epoch, native);
    TraceCoordinator coordinator{};
    TraceOutcome out = run_trace(coordinator, owners, native, epoch);
    CHECK(out.popped);
    out.record.sequence = 0U;
    return out.record;
}

void rewrite_epoch(RestrictedIncidentRecord& record, std::uint64_t epoch) noexcept {
    record.capture_epoch = epoch;
    auto rewrite = [epoch](PhaseFence& fence) noexcept {
        fence.before.capture_epoch = epoch;
        fence.after.capture_epoch = epoch;
    };
    rewrite(record.terminal_entry_fence);
    rewrite(record.root.fence);
    for (PhaseFence& fence : record.route_fences)
        rewrite(fence);
    rewrite(record.listener.entry_fence);
    rewrite(record.listener.exit_fence);
    rewrite(record.terminal_exit_fence);
}

class MappedFile final {
public:
    explicit MappedFile(const wchar_t* path) noexcept {
        file_ = CreateFileW(path,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            nullptr,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            nullptr);
        LARGE_INTEGER fileSize{};
        if (file_ != INVALID_HANDLE_VALUE && GetFileSizeEx(file_, &fileSize) != FALSE
            && fileSize.QuadPart > 0) {
            size_ = static_cast<std::size_t>(fileSize.QuadPart);
            mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0U, 0U, nullptr);
            if (mapping_ != nullptr)
                view_ = static_cast<const std::byte*>(
                    MapViewOfFile(mapping_, FILE_MAP_READ, 0U, 0U, 0U));
        }
    }
    ~MappedFile() noexcept {
        if (view_ != nullptr) UnmapViewOfFile(view_);
        if (mapping_ != nullptr) CloseHandle(mapping_);
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
    }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return view_ == nullptr ? std::span<const std::byte>{}
                                : std::span<const std::byte>{view_, size_};
    }

private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE mapping_{};
    const std::byte* view_{};
    std::size_t size_{};
};

[[nodiscard]] bool contains_bytes(std::span<const std::byte> haystack,
                                  std::span<const std::byte> needle) noexcept {
    return !needle.empty()
           && std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end())
                  != haystack.end();
}

void test_runtime_and_prefix_admission() {
    PackedRuntimeIdentity packed{};
    CHECK(measure_pinned_packed_runtime(packed) == FileMeasurementResult::measured);
    CHECK(matches_pinned_packed_runtime(packed));
    ValidatedRuntimeCohort live{};
    CHECK(admit_live_runtime(live) == LiveAdmissionResult::packed_file_mismatch);
    CHECK(!live.token().valid());

    MappedFile unpacked{kPinnedUnpackedProvenancePath};
    CHECK(unpacked.bytes().size() == kPinnedUnpackedProvenanceBytes);
    std::array<std::uintptr_t, kNativeSurfaceCount> addresses{};
    CHECK(validate_mapped_prefixes_untrusted(unpacked.bytes(), addresses)
          == PrefixCohortResult::valid);
    constexpr std::array<std::uintptr_t, kNativeSurfaceCount> expected{0xB202F0U,
                                                                       0xB20640U,
                                                                       0xB20B00U,
                                                                       0xB20820U,
                                                                       0x4CFA00U,
                                                                       0x4AA0B0U,
                                                                       0x4E5200U,
                                                                       0xD83660U,
                                                                       0x4AA1C0U,
                                                                       0xD82730U,
                                                                       0xD82AE0U,
                                                                       0xD82B60U};
    for (std::size_t i = 0U; i < addresses.size(); ++i)
        CHECK(addresses[i]
              == reinterpret_cast<std::uintptr_t>(unpacked.bytes().data()) + expected[i]);
    for (std::size_t i = 0U; i < kNativeSurfaceCount; ++i) {
        const auto surface = static_cast<NativeSurface>(i);
        const NativeBoundaryDescriptor boundary = native_boundary(surface);
        CHECK(boundary.mapped_rva == expected[i]);
        CHECK(native_prefix_matches(surface, unpacked.bytes().subspan(boundary.mapped_rva)));
    }
    std::vector<std::byte> damaged(unpacked.bytes().begin(), unpacked.bytes().end());
    damaged[expected.back()] ^= std::byte{1U};
    CHECK(validate_mapped_prefixes_untrusted(damaged, addresses)
          == PrefixCohortResult::prefix_mismatch);
    CHECK(validate_mapped_prefixes_untrusted(unpacked.bytes().first(64U), addresses)
          == PrefixCohortResult::target_out_of_range);
}

void test_joined_evidence_and_projection() {
    NativeFixture native{};
    OwnerFixture owners = make_owners(testing_runtime_cohort_token(1U), 41U, native);
    const auto rootBefore = native.root();
    const auto dynamicBefore = native.dynamic();
    const auto tableBefore = native.table();
    const auto rowBefore = native.row();
    TraceCoordinator coordinator{};
    TraceOutcome out = run_trace(coordinator, owners, native, 41U);
    CHECK(out.begin == TraceResult::complete);
    CHECK(out.root == TraceResult::complete);
    CHECK(out.listener_entry == TraceResult::complete);
    CHECK(out.listener_exit == TraceResult::complete);
    CHECK(out.finish.trace == TraceResult::complete);
    CHECK(out.finish.queue == QueuePushResult::enqueued);
    CHECK(out.popped);
    CHECK(owners.reads.load(std::memory_order_acquire) == 20U);
    CHECK(valid_restricted_record(out.record));
    CHECK(complete_generic_listener_trace(out.record));
    CHECK(one_shot_commit_observed(out.record));
    CHECK(out.record.phases == kCompleteTracePhases);
    CHECK(out.record.root.sobject_id_value == kIncidentRootSchema);
    CHECK(out.record.root.descriptor_type == kIncidentSObjectKind);
    CHECK(out.record.root.root_bytes == rootBefore);
    CHECK(std::equal(dynamicBefore.begin(),
                     dynamicBefore.end(),
                     out.record.root.dynamic_type35.captured().begin()));
    CHECK(std::equal(tableBefore.begin(),
                     tableBefore.end(),
                     out.record.listener.table_before.captured().begin()));
    CHECK(std::equal(rowBefore.begin(),
                     rowBefore.end(),
                     out.record.listener.selected_row_before.captured().begin()));
    CHECK(!std::equal(tableBefore.begin(),
                      tableBefore.end(),
                      out.record.listener.table_after.captured().begin()));
    CHECK(out.record.listener.entry_fence.before.listener_table == owners.stable.listener_table);

    DefaultProjection projection{};
    CHECK(project_default(out.record, projection));
    Sha256 expectedDigest{};
    CHECK(sha256_bytes(rootBefore, expectedDigest));
    CHECK(projection.root_digest == expectedDigest);
    CHECK(projection.consumer == RetailConsumerDisposition::unknown_dynamic_listener);
    CHECK((static_cast<std::uint16_t>(projection.validity)
           & static_cast<std::uint16_t>(ProjectionValidity::complete_path))
          != 0U);
    const auto projectedBytes = std::as_bytes(std::span{&projection, 1U});
    const std::uintptr_t address = out.record.instance_identity;
    const std::uint32_t player = owners.stable.local_player_datum;
    CHECK(!contains_bytes(projectedBytes, std::as_bytes(std::span{&address, 1U})));
    CHECK(!contains_bytes(projectedBytes, std::as_bytes(std::span{&player, 1U})));

    native.root().fill(std::byte{0xEEU});
    native.dynamic().fill(std::byte{0xDDU});
    CHECK(out.record.root.root_bytes == rootBefore);
    DefaultProjection afterMutation{};
    CHECK(project_default(out.record, afterMutation));
    CHECK(afterMutation.root_digest == expectedDigest);

    RestrictedIncidentRecord wrongRoot = out.record;
    wrongRoot.sequence = 0U;
    wrongRoot.root.sobject_id_value ^= 1U;
    RestrictedEvidenceQueue rejected{};
    CHECK(!valid_restricted_record(wrongRoot));
    CHECK(rejected.try_push(wrongRoot) == QueuePushResult::rejected);
}

void test_presence_predicates_and_faults() {
    CHECK(valid_presence(OptionalU32{Presence::present, 0U}));
    CHECK(valid_presence(OptionalU32{Presence::absent, 0U}));
    CHECK(!valid_presence(OptionalU32{Presence::absent, 7U}));
    CHECK(valid_presence(owner(1U, 1U)));
    CHECK(!valid_presence(OwnerToken{Presence::present, 0U, 1U}));
    CHECK(!valid_presence(OwnerToken{Presence::absent, 1U, 0U}));

    NativeFixture native{};
    OwnerFixture owners = make_owners(testing_runtime_cohort_token(2U), 42U, native);
    CHECK(valid_ownership_snapshot(
        owners.stable, OwnerRequirement::activation | OwnerRequirement::listener_table));
    OwnershipSnapshot badPresence = owners.stable;
    badPresence.listener_table = {Presence::absent, 1U, 0U};
    CHECK(!valid_ownership_snapshot(badPresence, OwnerRequirement::listener_table));

    EpochCallGate gate{};
    CHECK(gate.open(42U));
    EpochCallGate::Scope scope{gate};
    TraceCoordinator coordinator{};
    native.set_authority({kUnsetGeneration, kUnsetGeneration, 0U, 1U});
    ThreadTraceStack unresolved{};
    CHECK(coordinator.begin_terminal(unresolved,
                                     scope,
                                     owners.provider(),
                                     {native.instance(),
                                      native.matched(),
                                      native.volume(),
                                      meta(1U, kTerminalEvaluatorCallsiteRva)})
          == TraceResult::partial);

    NativeFixture nonmember{};
    constexpr std::uint64_t none{};
    write_value(static_cast<std::byte*>(nonmember.volume()), kRuntimeVolumeMembershipOffset, none);
    OwnerFixture nonmemberOwners = make_owners(testing_runtime_cohort_token(2U), 42U, nonmember);
    ThreadTraceStack nonmemberStack{};
    CHECK(coordinator.begin_terminal(nonmemberStack,
                                     scope,
                                     nonmemberOwners.provider(),
                                     {nonmember.instance(),
                                      nonmember.matched(),
                                      nonmember.volume(),
                                      meta(2U, kTerminalEvaluatorCallsiteRva)})
          == TraceResult::partial);

    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    auto* pages = static_cast<std::byte*>(
        VirtualAlloc(nullptr, system.dwPageSize * 2U, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    CHECK(pages != nullptr);
    if (pages != nullptr) {
        DWORD oldProtection{};
        CHECK(VirtualProtect(
                  pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &oldProtection)
              != FALSE);
        void* crossing = pages + system.dwPageSize - kConsumedGenerationOffset - 8U;
        write_value(static_cast<std::byte*>(crossing), 0U, kVignettePoint.definition);
        OwnerFixture faultOwners = make_owners(testing_runtime_cohort_token(2U), 42U, native);
        faultOwners.stable.component.identity = reinterpret_cast<std::uintptr_t>(crossing);
        ThreadTraceStack faultStack{};
        CHECK(coordinator.begin_terminal(faultStack,
                                         scope,
                                         faultOwners.provider(),
                                         {crossing,
                                          native.matched(),
                                          native.volume(),
                                          meta(3U, kTerminalEvaluatorCallsiteRva)})
              == TraceResult::unreadable);
        CHECK(faultStack.depth() == 0U);
        CHECK(VirtualFree(pages, 0U, MEM_RELEASE) != FALSE);
    }
}

void test_torn_owner_generations() {
    constexpr std::array cases{std::pair{MutatedOwner::activation, 2U},
                               std::pair{MutatedOwner::authority, 2U},
                               std::pair{MutatedOwner::membership, 2U},
                               std::pair{MutatedOwner::manager, 5U},
                               std::pair{MutatedOwner::listener, 17U}};
    std::uint64_t generation = 20U;
    for (const auto& [mutated, read] : cases) {
        NativeFixture native{};
        const std::uint64_t epoch = 100U + generation;
        OwnerFixture owners = make_owners(testing_runtime_cohort_token(generation), epoch, native);
        owners.mutated = mutated;
        owners.mutate_from = read;
        TraceCoordinator coordinator{};
        TraceOutcome out = run_trace(coordinator, owners, native, epoch);
        CHECK(out.popped);
        CHECK(out.finish.trace == TraceResult::partial);
        CHECK(!complete_generic_listener_trace(out.record));
        DefaultProjection projection{};
        CHECK(project_default(out.record, projection));
        CHECK((static_cast<std::uint16_t>(projection.validity)
               & static_cast<std::uint16_t>(ProjectionValidity::torn_candidate))
              != 0U);
        ++generation;
    }
}

std::atomic<std::uint32_t> g_original_calls{};
void forwarded_original(std::uint32_t* value) noexcept {
    g_original_calls.fetch_add(1U, std::memory_order_relaxed);
    ++*value;
}

void test_gate_and_original_once() {
    EpochCallGate gate{};
    CHECK(!gate.open(0U));
    CHECK(gate.open(7U));
    CHECK(!gate.open(8U));
    {
        EpochCallGate::Scope first{gate};
        CHECK(first.admitted() && first.epoch() == 7U);
        CHECK(gate.active_calls() == 1U);
        CHECK(gate.quiesce(7U));
        CHECK(!gate.open(8U));
        EpochCallGate::Scope afterQuiesce{gate};
        CHECK(!afterQuiesce.admitted());
        CHECK(gate.active_calls() == 2U);
    }
    CHECK(gate.idle());
    CHECK(!gate.open(7U));
    CHECK(gate.open(8U));

    using Original = void (*)(std::uint32_t*) noexcept;
    OriginalSlot<Original> slot{};
    CHECK(!slot.publish(nullptr));
    std::uint32_t value{};
    const auto before = [](const EpochCallGate::Scope&) noexcept {};
    const auto after = [](const EpochCallGate::Scope&) noexcept {};
    CHECK(forward_void_original_once(gate, slot, before, after, &value)
          == OriginalForwardResult::missing_original);
    CHECK(value == 0U);
    CHECK(slot.publish(&forwarded_original));
    CHECK(!slot.publish(&forwarded_original));
    CHECK(forward_void_original_once(gate, slot, before, after, &value)
          == OriginalForwardResult::forwarded);
    CHECK(value == 1U);
    CHECK(g_original_calls.load(std::memory_order_relaxed) == 1U);
    slot.clear_after_confirmed_detach();
    CHECK(slot.load() == nullptr);
}

void test_queue_limits_and_concurrency() {
    RestrictedIncidentRecord record = template_record();
    auto isolated = std::make_unique<RestrictedEvidenceQueue>();
    const auto rootBefore = record.root.root_bytes;
    CHECK(isolated->try_push(record) == QueuePushResult::enqueued);
    record.root.root_bytes.fill(std::byte{0xEEU});
    RestrictedIncidentRecord popped{};
    CHECK(isolated->try_pop(popped) == QueuePopResult::success);
    CHECK(popped.sequence == 1U && popped.root.root_bytes == rootBefore);
    record.root.root_bytes = rootBefore;

    auto locked = std::make_unique<RestrictedEvidenceQueue>();
    CHECK(locked->testing_lock());
    CHECK(locked->try_push(record) == QueuePushResult::busy);
    CHECK(locked->try_pop(popped) == QueuePopResult::busy);
    CHECK(!locked->empty_quiesced());
    locked->testing_unlock();

    auto full = std::make_unique<RestrictedEvidenceQueue>();
    for (std::size_t i = 0U; i < kRestrictedQueueCapacity; ++i)
        CHECK(full->try_push(record) == QueuePushResult::enqueued);
    CHECK(full->try_push(record) == QueuePushResult::full);
    RestrictedIncidentRecord otherEpoch = record;
    rewrite_epoch(otherEpoch, record.capture_epoch + 1U);
    CHECK(full->try_pop(popped) == QueuePopResult::success);
    CHECK(full->try_push(otherEpoch) == QueuePushResult::wrong_epoch);

    auto exhausted = std::make_unique<RestrictedEvidenceQueue>();
    exhausted->testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted->try_push(record) == QueuePushResult::sequence_exhausted);

    auto concurrent = std::make_unique<RestrictedEvidenceQueue>();
    constexpr std::uint32_t producerCount = 4U;
    constexpr std::uint32_t perProducer = 16U;
    constexpr std::uint32_t total = producerCount * perProducer;
    std::atomic<bool> start{};
    std::atomic<bool> workerFailure{};
    std::atomic<std::uint32_t> finished{};
    std::array<std::thread, producerCount> producers{};
    for (std::uint32_t p = 0U; p < producerCount; ++p) {
        producers[p] = std::thread([&, p]() {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (std::uint32_t i = 0U; i < perProducer; ++i) {
                RestrictedIncidentRecord candidate = record;
                candidate.terminal_call_id = 1000U + p * perProducer + i;
                for (;;) {
                    const QueuePushResult result = concurrent->try_push(candidate);
                    if (result == QueuePushResult::enqueued) break;
                    if (result != QueuePushResult::busy && result != QueuePushResult::full) {
                        workerFailure.store(true, std::memory_order_release);
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            finished.fetch_add(1U, std::memory_order_release);
        });
    }
    std::vector<std::uint64_t> sequences{};
    sequences.reserve(total);
    start.store(true, std::memory_order_release);
    while ((sequences.size() < total || finished.load(std::memory_order_acquire) != producerCount)
           && !workerFailure.load(std::memory_order_acquire)) {
        RestrictedIncidentRecord item{};
        const QueuePopResult result = concurrent->try_pop(item);
        if (result == QueuePopResult::success)
            sequences.push_back(item.sequence);
        else
            std::this_thread::yield();
    }
    for (std::thread& producer : producers)
        producer.join();
    CHECK(!workerFailure.load(std::memory_order_acquire));
    CHECK(sequences.size() == total);
    CHECK(std::is_sorted(sequences.begin(), sequences.end()));
    CHECK(std::adjacent_find(sequences.begin(), sequences.end()) == sequences.end());
    if (sequences.size() == total) {
        CHECK(sequences.front() == 1U);
        CHECK(sequences.back() == total);
    }
    CHECK(concurrent->counters().accepted == total);
    CHECK(concurrent->empty_quiesced());
}

void test_aggregate_lifecycle_and_reset() {
    const RuntimeCohortToken token = testing_runtime_cohort_token(61U);
    HookGroupState group{};
    CHECK(group.begin_install(token, 61U));
    CHECK(group.record_attached(NativeSurface::point_terminal, 0x111U, 0x222U, 61U)
          == ParticipantResult::recorded);
    CHECK(group.retain_partial_install_failure());
    CHECK(group.record_participant_detach(NativeSurface::point_terminal,
                                          ProtectedDetachDisposition::deferred)
          == ParticipantResult::recorded);
    DetachReceipt receipt{};
    AggregateParticipantSnapshot idle{61U, 61U, 61U, 0U, 0U, true, true};
    CHECK(group.finalize_detach(idle, receipt) == FinalDetachResult::participants_remain);
    CHECK(group.record_participant_detach(NativeSurface::point_terminal,
                                          ProtectedDetachDisposition::failed)
          == ParticipantResult::recorded);
    CHECK(group.snapshot().attached_mask != 0U);
    CHECK(group.record_participant_detach(NativeSurface::point_terminal,
                                          ProtectedDetachDisposition::removed)
          == ParticipantResult::recorded);
    auto active = idle;
    active.active_producers = 1U;
    CHECK(group.finalize_detach(active, receipt) == FinalDetachResult::aggregate_not_idle);
    auto wrongEpoch = idle;
    wrongEpoch.drain_epoch = 60U;
    CHECK(group.finalize_detach(wrongEpoch, receipt) == FinalDetachResult::wrong_epoch);
    CHECK(group.finalize_detach(idle, receipt) == FinalDetachResult::detached);
    CHECK(receipt.valid_for(61U));
    CHECK(group.snapshot().phase == HookGroupPhase::detached);

    RestrictedIncidentRecord record = template_record(61U);
    RestrictedEvidenceQueue queue{};
    CHECK(queue.try_push(record) == QueuePushResult::enqueued);
    CHECK(!queue.try_reset(receipt));
    RestrictedIncidentRecord drained{};
    CHECK(queue.try_pop(drained) == QueuePopResult::success);
    CHECK(queue.try_reset(receipt));
    CHECK(queue.counters().accepted == 0U);
    CHECK(queue.try_push(record) == QueuePushResult::enqueued);
    CHECK(queue.try_pop(drained) == QueuePopResult::success);
    CHECK(drained.sequence == 1U);
}

void test_nested_stack_and_phase_negative() {
    NativeFixture outer{};
    NativeFixture inner{kOuterDialoguePoint};
    OwnerFixture owners = make_owners(testing_runtime_cohort_token(71U), 71U, outer);
    EpochCallGate gate{};
    CHECK(gate.open(71U));
    EpochCallGate::Scope scope{gate};
    TraceCoordinator coordinator{};
    ThreadTraceStack stack{};
    CHECK(coordinator.begin_terminal(stack,
                                     scope,
                                     owners.provider(),
                                     {outer.instance(),
                                      outer.matched(),
                                      outer.volume(),
                                      meta(1U, kTerminalEvaluatorCallsiteRva)})
          == TraceResult::complete);
    owners.stable.component.identity = reinterpret_cast<std::uintptr_t>(inner.instance());
    owners.stable.runtime_volume.identity = reinterpret_cast<std::uintptr_t>(inner.volume());
    CHECK(coordinator.begin_terminal(stack,
                                     scope,
                                     owners.provider(),
                                     {inner.instance(),
                                      inner.matched(),
                                      inner.volume(),
                                      meta(2U, kTerminalEvaluatorCallsiteRva)})
          == TraceResult::complete);
    CHECK(stack.depth() == 2U);
    CHECK(coordinator.observe_manager_submit(stack, owners.provider())
          == TraceResult::phase_order_error);
    RestrictedEvidenceQueue queue{};
    CHECK(coordinator
              .finish_terminal(stack,
                               scope,
                               owners.provider(),
                               inner.instance(),
                               meta(3U, kTerminalEvaluatorReturnRva),
                               queue)
              .trace
          == TraceResult::partial);
    CHECK(stack.depth() == 1U);
    owners.stable.component.identity = reinterpret_cast<std::uintptr_t>(outer.instance());
    owners.stable.runtime_volume.identity = reinterpret_cast<std::uintptr_t>(outer.volume());
    CHECK(coordinator
              .finish_terminal(stack,
                               scope,
                               owners.provider(),
                               outer.instance(),
                               meta(4U, kTerminalEvaluatorReturnRva),
                               queue)
              .trace
          == TraceResult::partial);
    CHECK(stack.depth() == 0U);
    RestrictedIncidentRecord a{};
    RestrictedIncidentRecord b{};
    CHECK(queue.try_pop(a) == QueuePopResult::success);
    CHECK(queue.try_pop(b) == QueuePopResult::success);
    CHECK(a.terminal_call_id != b.terminal_call_id);
}

void test_no_claim_surface() {
    std::ifstream stream("D:\\Dawn-port\\Dawn\\src\\client\\hooks\\bootflow\\opening_"
                         "authority\\type31_incident_listener_capture.h",
                         std::ios::binary);
    const std::string source{std::istreambuf_iterator<char>{stream},
                             std::istreambuf_iterator<char>{}};
    CHECK(!source.empty());
    constexpr std::array prohibited{"incident_ordinal",
                                    "observed_path",
                                    "callback_identity",
                                    "route_identity",
                                    "root_payload_hash",
                                    "strict_ghost_gate"};
    for (const char* field : prohibited)
        CHECK(source.find(field) == std::string::npos);
    CHECK(!kOwnsNativeDetour && kObservationOnly && !kPerformsCapturePathIo);
    CHECK(!kProvidesAuthorityEncoder && !kProvidesAuthorityWriter && !kProvidesStrictGate);
    CHECK(!kDynamicRetailListenerKnown);
    for (const CandidateConsumer candidate : {CandidateConsumer::ghost_opening_vo,
                                              CandidateConsumer::grounded_ikora_dwell,
                                              CandidateConsumer::ikora_outer_dialogue,
                                              CandidateConsumer::type26_hold,
                                              CandidateConsumer::scene_or_lift})
        CHECK(!may_be_used_as_strict_gate(candidate));
}

} // namespace

int main() {
    test_runtime_and_prefix_admission();
    test_joined_evidence_and_projection();
    test_presence_predicates_and_faults();
    test_torn_owner_generations();
    test_gate_and_original_once();
    test_queue_limits_and_concurrency();
    test_aggregate_lifecycle_and_reset();
    test_nested_stack_and_phase_negative();
    test_no_claim_surface();
    if (g_failure_count == 0) std::cout << "type31 incident-listener capture tests passed\n";
    return g_failure_count == 0 ? 0 : 1;
}
