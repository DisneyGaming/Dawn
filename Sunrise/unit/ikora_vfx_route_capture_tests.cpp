#define SUNRISE_IKORA_VFX_ROUTE_CAPTURE_TEST 1

#include "client/hooks/bootflow/opening_authority/ikora_vfx_route_capture.h"
#include "client/hooks/bootflow/opening_authority/ikora_vfx_route_lifecycle.h"

#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <span>
#include <thread>
#include <vector>

namespace route = sunrise::client::hooks::bootflow::opening_authority::ikora_vfx_route;

namespace {

int g_failures = 0;

void check(bool condition, const char* expression, int line) noexcept {
    if (!condition) {
        std::fprintf(stderr, "line %d: check failed: %s\n", line, expression);
        ++g_failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

class FileView final {
public:
    explicit FileView(const wchar_t* path) noexcept {
        file_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) { return; }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_, &size) || size.QuadPart <= 0) { return; }
        size_ = static_cast<std::size_t>(size.QuadPart);
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0U, 0U, nullptr);
        if (mapping_ == nullptr) { return; }
        data_ = static_cast<const std::byte*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0U, 0U, 0U));
    }
    ~FileView() noexcept {
        if (data_ != nullptr) { UnmapViewOfFile(data_); }
        if (mapping_ != nullptr) { CloseHandle(mapping_); }
        if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); }
    }
    FileView(const FileView&) = delete;
    FileView& operator=(const FileView&) = delete;
    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return {data_, size_}; }

private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE mapping_{};
    const std::byte* data_{};
    std::size_t size_{};
};

class CopyFileView final {
public:
    explicit CopyFileView(const wchar_t* path) noexcept {
        file_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) { return; }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_, &size) || size.QuadPart <= 0) { return; }
        size_ = static_cast<std::size_t>(size.QuadPart);
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_WRITECOPY, 0U, 0U, nullptr);
        if (mapping_ == nullptr) { return; }
        data_ = static_cast<std::byte*>(MapViewOfFile(mapping_, FILE_MAP_COPY, 0U, 0U, 0U));
    }
    ~CopyFileView() noexcept {
        if (data_ != nullptr) { UnmapViewOfFile(data_); }
        if (mapping_ != nullptr) { CloseHandle(mapping_); }
        if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); }
    }
    CopyFileView(const CopyFileView&) = delete;
    CopyFileView& operator=(const CopyFileView&) = delete;
    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
    [[nodiscard]] std::span<std::byte> bytes() const noexcept { return {data_, size_}; }

private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    HANDLE mapping_{};
    std::byte* data_{};
    std::size_t size_{};
};

class MappedRuntimeFixture final {
public:
    MappedRuntimeFixture(const FileView& packed, const FileView& unpacked) noexcept
        : packed_(packed.bytes()), unpacked_(unpacked.bytes()) {
        base_ = static_cast<std::byte*>(VirtualAlloc(nullptr, route::kPinnedSizeOfImage,
                                                     MEM_RESERVE, PAGE_NOACCESS));
        if (base_ == nullptr || packed.bytes().size() < route::kPinnedSizeOfHeaders
            || unpacked_.size() != route::kPinnedSizeOfImage) { return; }
        void* header = VirtualAlloc(base_, 0x1000U, MEM_COMMIT, PAGE_READWRITE);
        if (header == nullptr) { return; }
        std::memcpy(base_, packed.bytes().data(), route::kPinnedSizeOfHeaders);

        for (std::size_t index = 0U; index < route::kNativeSurfaceCount; ++index) {
            const auto target = route::native_target(static_cast<route::NativeSurface>(index));
            add_pages(target.rva, target.function_bytes);
        }
        for (const auto& edge : route::direct_edge_manifest()) {
            add_pages(edge.callsite_rva, 5U);
            add_pages(edge.target_rva, 1U);
        }
        const auto copy = route::candidate_result_bank_copy_window();
        add_pages(copy.rva, copy.bytes.size());

        for (const auto page : pages_) {
            if (page > unpacked_.size() || 0x1000U > unpacked_.size() - page) { return; }
            void* committed = VirtualAlloc(base_ + page, 0x1000U, MEM_COMMIT, PAGE_READWRITE);
            if (committed == nullptr) { return; }
            std::memcpy(base_ + page, unpacked_.data() + page, 0x1000U);
        }
        for (const auto page : pages_) {
            DWORD previous{};
            if (!VirtualProtect(base_ + page, 0x1000U, PAGE_EXECUTE_READ, &previous)) { return; }
        }
        ready_ = true;
    }

    ~MappedRuntimeFixture() noexcept {
        if (base_ != nullptr) { VirtualFree(base_, 0U, MEM_RELEASE); }
    }
    MappedRuntimeFixture(const MappedRuntimeFixture&) = delete;
    MappedRuntimeFixture& operator=(const MappedRuntimeFixture&) = delete;
    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] const std::byte* data() const noexcept { return base_; }

    [[nodiscard]] bool protect_page(std::uint32_t rva, DWORD protect) noexcept {
        DWORD previous{};
        return VirtualProtect(base_ + page_of(rva), 0x1000U, protect, &previous) != FALSE;
    }
    [[nodiscard]] bool write_byte(std::uint32_t rva, std::byte value) noexcept {
        if (!protect_page(rva, PAGE_READWRITE)) { return false; }
        base_[rva] = value;
        return protect_page(rva, PAGE_EXECUTE_READ);
    }
    [[nodiscard]] bool restore_page(std::uint32_t rva) noexcept {
        const auto page = page_of(rva);
        if (!protect_page(rva, PAGE_READWRITE)) { return false; }
        std::memcpy(base_ + page, unpacked_.data() + page, 0x1000U);
        return protect_page(rva, PAGE_EXECUTE_READ);
    }
    [[nodiscard]] bool corrupt_mapped_header(std::uint32_t offset) noexcept {
        if (offset >= route::kPinnedSizeOfHeaders) { return false; }
        base_[offset] ^= std::byte{0x01};
        return true;
    }
    void restore_mapped_header() noexcept {
        std::memcpy(base_, packed_.data(), route::kPinnedSizeOfHeaders);
    }

private:
    [[nodiscard]] static constexpr std::uint32_t page_of(std::uint32_t rva) noexcept {
        return rva & ~0xFFFU;
    }
    void add_pages(std::uint32_t rva, std::size_t bytes) {
        const auto first = page_of(rva);
        const auto last = page_of(static_cast<std::uint32_t>(rva + bytes - 1U));
        for (auto page = first;; page += 0x1000U) {
            pages_.insert(page);
            if (page == last) { break; }
        }
    }

    std::span<const std::byte> packed_{};
    std::span<const std::byte> unpacked_{};
    std::byte* base_{};
    std::set<std::uint32_t> pages_{};
    bool ready_{};
};

template <typename T, std::size_t N>
void store(std::array<std::byte, N>& bytes, std::size_t offset, T value) noexcept {
    CHECK(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset);
    if (offset <= bytes.size() && sizeof(T) <= bytes.size() - offset) {
        std::memcpy(bytes.data() + offset, &value, sizeof(T));
    }
}

route::Transform32 transform(float base) noexcept {
    route::Transform32 bytes{};
    std::array<float, 8U> values{};
    for (std::size_t index = 0U; index < values.size(); ++index) {
        values[index] = base + static_cast<float>(index);
    }
    std::memcpy(bytes.data(), values.data(), bytes.size());
    return bytes;
}

route::RouteCaptureRecord make_closed_record(route::OwnerScopeToken scope) noexcept {
    route::RouteCaptureRecord record{};
    record.metadata = {1U, 1000U, 77U, 88U, 99U,
                       route::NativeSurface::source_writer, 0x58FAB5U};
    record.entry_scope = scope;
    record.exit_scope = scope;
    record.subject = {0x80806E28U, route::EffectResourceDefinition{0x80C71D8EU}, 0U, 0U};
    record.presentation = {scope, 10U, 11U, 0x12345678U, 12U};

    auto& provider = record.provider;
    provider.scope = scope;
    provider.owner = {scope, route::kSuccessorVisibleSchedulerTag, route::kSharedEntityDefinition,
                      route::kSuccessorVisibleFullHandle, route::kReusedVisibleLowIndex,
                      0x71000000U, 101U, 102U, 0U};
    provider.component_definition = route::kProviderComponentDefinition;
    provider.component_full_handle = 0x11112222U;
    provider.component_allocation = {0x20000000U, 0x20001000U};
    provider.component_generation = 103U;
    provider.runtime_provider_handle = {0x22223333U, 0x30000000U, 104U, true};
    provider.runtime_provider_relative = -0x0FFFFF00LL;
    provider.provider_identity = 0x20000100U;
    provider.definition_handle = {0x33334444U, 0x40000000U, 105U, true};
    provider.definition_relative = 0x1000;
    provider.definition_target_identity = 0x40001000U;
    provider.definition_target_generation = 106U;
    provider.definition_table_relative = 0x0FA8;
    provider.definition_table_identity = 0x40002000U;
    provider.definition_table_generation = 107U;
    provider.definition_table_allocation = {0x40002000U, 0x40003000U};
    provider.pose_dispatch_handle = {0x44445555U, 0x50000000U, 108U, true};
    provider.pose_dispatch_base_identity = 0x50000000U;
    provider.pose_object_handle = {0x55556666U, 0x60000000U, 109U, true};
    provider.pose_object_relative = 0x1000;
    provider.concrete_pose_object_identity = 0x60001000U;
    provider.filter_relative = 0x90;
    provider.filter_context_identity = 0x20000200U;
    provider.pose_resource_definition = route::kPoseResourceDefinition;
    provider.pose_resource_generation = 110U;
    provider.provider_generation = 111U;
    provider.provider_read_complete = true;
    provider.definition_target_read_complete = true;
    store(provider.provider_bytes, 0x00U, provider.definition_handle.full_handle);
    store(provider.provider_bytes, 0x08U, provider.definition_relative);
    store(provider.provider_bytes, 0x50U, provider.pose_dispatch_handle.full_handle);
    store(provider.provider_bytes, 0x58U, provider.pose_object_handle.full_handle);
    store(provider.provider_bytes, 0x60U, provider.pose_object_relative);
    store(provider.provider_bytes, 0x70U, provider.filter_relative);
    store(provider.definition_target_bytes, 0x58U, provider.definition_table_relative);

    record.ownership.observed_provider_factory_ordinal =
        route::kSourceRetainedProviderFactoryOrdinal;
    record.ownership.concurrent_carrier = {
        scope, route::kCarrierSchedulerTag, route::kSharedEntityDefinition,
        route::kCarrierFullHandle, 0x0007U, 0x72000000U, 201U, 202U, 0U};
    record.ownership.carrier_concurrent = true;
    record.ownership.carrier_presentation_correlated = true;
    record.ownership.carrier_exact_provider_owner_proven = false;

    record.source.machine_definition = route::kPresentationMachine;
    record.source.machine_class = route::kPresentationClass;
    record.source.descriptor = route::kExactOutput1Descriptor;
    record.source.source_row = route::kRuntimeSourceRow;
    record.source.output_start = route::kOutputSlot;
    record.source.output_count = 1U;
    record.source.source_kind = 2U;
    record.source.descriptor_read_complete = true;
    record.source.runtime_read_complete = true;
    store(record.source.runtime, 0x00U, provider.runtime_provider_handle.full_handle);
    store(record.source.runtime, 0x08U, provider.runtime_provider_relative);
    const std::uint32_t selector = 2U;
    store(record.source.runtime, 0x10U, selector);

    auto& bank = record.bank;
    bank.bank_identity = 0x10000000U;
    bank.transform_relative = 0x0FC8;
    bank.transform_count = 4U;
    bank.transform_base_identity = 0x10001000U;
    bank.transform_allocation = {0x10001010U, 0x10001090U};
    bank.validity_relative = 0x1FB8;
    bank.validity_base_identity = 0x10002000U;
    bank.validity_allocation = {0x10002010U, 0x10002018U};
    bank.runtime_relative = 0x2FA8;
    bank.runtime_base_identity = 0x10003000U;
    bank.runtime_rows_identity = 0x10003010U;
    bank.runtime_allocation = {0x10003010U, 0x10003070U};
    bank.selected_output_identity = 0x10001030U;
    bank.selected_validity_identity = 0x10002012U;
    bank.selected_runtime_identity = 0x10003028U;
    bank.output_before = transform(1.0F);
    bank.output_after = transform(20.0F);
    bank.validity_before = 0U;
    bank.validity_after = 1U;
    bank.before_read_complete = true;
    bank.after_read_complete = true;

    auto& resolver = record.resolver;
    resolver.runtime_selector = selector;
    resolver.selector_mode = route::RuntimeSelectorMode::ordinary;
    resolver.resolver_stride = 0x40U;
    resolver.selection_address = 0x400020C0U;
    resolver.selection_value = route::kPurpleSelection;
    resolver.candidate_interval_begin = 7U;
    resolver.candidate_interval_end = 8U;
    resolver.candidate_ordinal = 7U;
    resolver.candidate_row_identity = 0x400021D0U;
    resolver.binding_key = route::kSourceRetainedBindingKey;
    resolver.candidate_selection_key = route::kPurpleSelection;
    resolver.candidate_local = transform(2.0F);
    store(resolver.candidate_row, 0x04U, resolver.binding_key);
    std::copy(resolver.candidate_local.begin(), resolver.candidate_local.end(),
              resolver.candidate_row.begin() + 0x10U);
    store(resolver.candidate_row, 0x30U, resolver.candidate_selection_key);
    resolver.pose_interface_dispatch_base = provider.pose_dispatch_base_identity;
    resolver.pose_interface_object = provider.concrete_pose_object_identity;
    resolver.pose_header_identity = route::kPoseHeaderIdentity;
    resolver.pose_object_bytes = route::kPoseObjectBytes;
    resolver.pose_resource_generation = provider.pose_resource_generation;
    resolver.socket_flags = 1U;
    resolver.allow_fallback = false;
    resolver.provider_selection_callsite_rva = route::kProviderSelectionExactCallsiteRva;
    resolver.provider_route_ordinal_for_actor = 1U;
    resolver.native_result_count = 1;
    resolver.output_capacity = 1U;
    resolver.result_status = route::NativeResultStatus::results_returned;
    std::copy(bank.output_after.begin(), bank.output_after.end(),
              resolver.candidate_result_after.begin() + 0x10U);
    resolver.pose_output_after = transform(3.0F);
    resolver.candidate_row_read_complete = true;
    resolver.candidate_result_after_read_complete = true;
    resolver.pose_output_after_read_complete = true;
    record.original = {record.metadata.call_id, 1U};
    record.provider_owner_handle_current_at_entry = true;
    record.provider_owner_handle_current_at_exit = true;
    return record;
}

route::EffectCreateCapture make_effect_create(route::OwnerScopeToken scope,
                                              std::uint32_t handle,
                                              std::uint64_t serial) noexcept {
    route::EffectCreateCapture capture{};
    capture.metadata = {20U, 2000U, serial, serial + 1U, 9U,
                        route::NativeSurface::effect_create_one, 0x1212CB0U};
    capture.scope = scope;
    capture.resource = route::kEarlyEffectDefinitions[static_cast<std::size_t>(handle % 4U)];
    capture.out_pair_before = 0U;
    capture.out_pair_after = (static_cast<std::uint64_t>(handle) << 32U)
                           | static_cast<std::uint32_t>(serial);
    capture.resolved_effect_identity = 0x80000000U + handle * 0x100U;
    capture.pool_generation = 500U + handle;
    capture.create_serial = 600U + serial;
    capture.owner_actor_full_handle = route::kSuccessorVisibleFullHandle;
    capture.owner_actor_record_generation = 101U;
    capture.owner_provider_generation = 111U;
    capture.first_or_child_call = true;
    capture.native_success = true;
    capture.out_pair_before_read_complete = true;
    capture.out_pair_after_read_complete = true;
    capture.original = {capture.metadata.call_id, 1U};
    return capture;
}

route::EffectTerminalCapture make_effect_terminal(const route::EffectCreateCapture& create,
                                                  route::EffectTerminalKind kind,
                                                  std::uint64_t serial) noexcept {
    route::EffectTerminalCapture terminal{};
    terminal.metadata = {21U, 2100U, serial, serial + 1U, 9U,
                         route::NativeSurface::effect_explicit_destroy, 0x120B540U};
    terminal.scope = create.scope;
    terminal.kind = kind;
    terminal.full_handle = route::effect_handle_from_out_pair(create.out_pair_after);
    terminal.resolved_effect_identity = create.resolved_effect_identity;
    terminal.pool_generation = create.pool_generation;
    terminal.terminal_serial = 900U + serial;
    switch (kind) {
    case route::EffectTerminalKind::explicit_destroy:
        terminal.metadata.surface = route::NativeSurface::effect_explicit_destroy;
        terminal.terminal_path_rva = 0x120B540U;
        break;
    case route::EffectTerminalKind::priority_eviction_destroy:
        terminal.metadata.surface = route::NativeSurface::effect_priority_eviction_destroy;
        terminal.terminal_path_rva = 0x120B420U;
        break;
    case route::EffectTerminalKind::automatic_expiry_path_a:
        terminal.metadata.surface = route::NativeSurface::effect_update_and_expiry;
        terminal.terminal_path_rva = route::kEffectExpiryPathARva;
        break;
    case route::EffectTerminalKind::automatic_expiry_path_b:
        terminal.metadata.surface = route::NativeSurface::effect_update_and_expiry;
        terminal.terminal_path_rva = route::kEffectExpiryPathBRva;
        break;
    }
    terminal.original = {terminal.metadata.call_id, 1U};
    return terminal;
}

route::ActorInvalidationCapture make_invalidation(route::OwnerScopeToken scope,
                                                  const route::RouteCaptureRecord& retiring) noexcept {
    route::ActorInvalidationCapture capture{};
    capture.metadata = {30U, 3000U, 301U, 302U, 9U,
                        route::NativeSurface::actor_terminal_invalidation,
                        route::kActorTerminalInvalidationCallsiteRva};
    capture.retiring_scope = scope;
    capture.retiring_actor = retiring.provider.owner;
    capture.wrapper_scene_tag = capture.retiring_actor.scheduler_tag;
    capture.wrapper_full_actor_handle = capture.retiring_actor.full_handle;
    capture.terminal_flags_before = 0U;
    capture.terminal_flags_after = route::kSceneWrapperTerminalBit;
    capture.invalidated_provider_generation = retiring.provider.provider_generation;
    capture.invalidated_bank_generation = retiring.presentation.bank_generation;
    capture.rooted_generation_invalidated_before_original = true;
    capture.original = {capture.metadata.call_id, 1U};
    return capture;
}

void test_runtime_admission(const FileView& packed,
                            const FileView& unpacked,
                            MappedRuntimeFixture& mapped,
                            route::RuntimeCohortToken& token) noexcept {
    CHECK(packed.valid());
    CHECK(unpacked.valid());
    CHECK(packed.bytes().size() == route::kPinnedPackedRuntimeBytes);
    CHECK(unpacked.bytes().size() == route::kPinnedUnpackedProvenanceBytes);
    CHECK(mapped.ready());
    route::RuntimeImageView view{packed.bytes(), mapped.data(), route::kPinnedSizeOfImage};
    const auto admitted = route::validate_runtime_admission(view, token);
    CHECK(admitted.result == route::RuntimeAdmissionResult::admitted);
    CHECK(token.valid());

    const auto invalidation = route::capture_boundary(
        route::NativeSurface::actor_terminal_invalidation);
    CHECK(invalidation.boundary_rva == route::kActorTerminalInvalidationRva);
    CHECK(invalidation.timing == route::CaptureTiming::pre_native_entry);
    CHECK(invalidation.exact_callsite_count == 1U);
    CHECK(invalidation.exact_callsite_rvas[0] == route::kActorTerminalInvalidationCallsiteRva);
    const auto create = route::capture_boundary(route::NativeSurface::effect_create_one);
    CHECK(create.payload == route::CapturePayload::effect_create_out_pair);
    CHECK(create.exact_callsite_count == 2U);
    CHECK(create.exact_callsite_rvas[0] == 0x1212CB0U);
    CHECK(create.exact_callsite_rvas[1] == 0x1212DF7U);
    const auto explicit_destroy = route::capture_boundary(
        route::NativeSurface::effect_explicit_destroy);
    CHECK(explicit_destroy.boundary_rva == route::kEffectExplicitDestroyRva);
    CHECK(explicit_destroy.terminal_path_rvas[0] == 0x120B540U);
    const auto eviction = route::capture_boundary(
        route::NativeSurface::effect_priority_eviction_destroy);
    CHECK(eviction.boundary_rva == route::kEffectPriorityEvictionDestroyRva);
    CHECK(eviction.terminal_path_rvas[0] == 0x120B420U);
    const auto expiry = route::capture_boundary(route::NativeSurface::effect_update_and_expiry);
    CHECK(expiry.boundary_rva == route::kEffectUpdateAndExpiryRva);
    CHECK(expiry.terminal_path_count == 2U);
    CHECK(expiry.terminal_path_rvas[0] == route::kEffectExpiryPathARva);
    CHECK(expiry.terminal_path_rvas[1] == route::kEffectExpiryPathBRva);

    route::RuntimeCohortToken rejected{};
    auto short_view = view;
    short_view.packed_file = packed.bytes().first(packed.bytes().size() - 1U);
    CHECK(route::validate_runtime_admission(short_view, rejected).result
          == route::RuntimeAdmissionResult::packed_size_mismatch);
    CHECK(!rejected.valid());

    const auto function = route::native_target(route::NativeSurface::cache_rebuild);
    CHECK(mapped.protect_page(function.rva, PAGE_READONLY));
    CHECK(route::validate_runtime_admission(view, rejected).result
          == route::RuntimeAdmissionResult::page_not_committed_read_execute);
    CHECK(mapped.protect_page(function.rva, PAGE_EXECUTE_READ));

    const std::uint32_t hash_corrupt = function.rva + static_cast<std::uint32_t>(function.prefix.size()) + 8U;
    CHECK(mapped.write_byte(hash_corrupt, std::byte{0xCC}));
    CHECK(route::validate_runtime_admission(view, rejected).result
          == route::RuntimeAdmissionResult::function_hash_mismatch);
    CHECK(mapped.restore_page(hash_corrupt));

    CHECK(mapped.write_byte(0x56D9A1U, std::byte{0x90}));
    CHECK(route::validate_runtime_admission(view, rejected).result
          == route::RuntimeAdmissionResult::callsite_mismatch);
    CHECK(mapped.restore_page(0x56D9A1U));

    constexpr std::array<std::byte, 64U> golden_copy{
        std::byte{0x41},std::byte{0x0F},std::byte{0xB6},std::byte{0x46},std::byte{0x01},std::byte{0x41},std::byte{0x0F},std::byte{0x10},
        std::byte{0x00},std::byte{0x4D},std::byte{0x8D},std::byte{0x40},std::byte{0x50},std::byte{0x41},std::byte{0x03},std::byte{0xC1},
        std::byte{0x41},std::byte{0xFF},std::byte{0xC1},std::byte{0x48},std::byte{0x63},std::byte{0xC8},std::byte{0x48},std::byte{0x8B},
        std::byte{0x46},std::byte{0x38},std::byte{0x48},std::byte{0x8B},std::byte{0xD1},std::byte{0x48},std::byte{0x03},std::byte{0xC6},
        std::byte{0x48},std::byte{0xC1},std::byte{0xE2},std::byte{0x05},std::byte{0x0F},std::byte{0x11},std::byte{0x44},std::byte{0x02},
        std::byte{0x48},std::byte{0x41},std::byte{0x0F},std::byte{0x10},std::byte{0x48},std::byte{0xC0},std::byte{0x0F},std::byte{0x11},
        std::byte{0x4C},std::byte{0x02},std::byte{0x58},std::byte{0x48},std::byte{0x8B},std::byte{0x46},std::byte{0x48},std::byte{0x48},
        std::byte{0x03},std::byte{0xC6},std::byte{0x66},std::byte{0x83},std::byte{0x4C},std::byte{0x48},std::byte{0x58},std::byte{0x01}};
    const auto copy = route::candidate_result_bank_copy_window();
    CHECK(copy.rva == 0x58F030U);
    CHECK(copy.bytes.size() == golden_copy.size());
    CHECK(std::equal(golden_copy.begin(), golden_copy.end(), unpacked.bytes().begin() + copy.rva));

    for (const auto& edge : route::direct_edge_manifest()) {
        const auto* bytes = unpacked.bytes().data() + edge.callsite_rva;
        std::int32_t displacement{};
        std::memcpy(&displacement, bytes + 1U, sizeof(displacement));
        const auto target = static_cast<std::int64_t>(edge.callsite_rva) + 5 + displacement;
        CHECK(std::to_integer<std::uint8_t>(bytes[0]) == edge.opcode);
        CHECK(target == static_cast<std::int64_t>(edge.target_rva));
    }
}

void test_route_equations(route::OwnerScopeToken scope) noexcept {
    const auto record = make_closed_record(scope);
    CHECK(route::validate_raw_observation(record) == route::RawObservationResult::retained);
    CHECK(route::close_route(record, scope) == route::RouteClosureResult::closed);
    CHECK(route::resolves_exact_left_hand_joint(record));
    CHECK(route::fnv1_name_hash(std::span{route::kLeftHandJointName,
                                         sizeof(route::kLeftHandJointName) - 1U})
          == route::kLeftHandJointNameFnv1);

    auto corrupt = record;
    corrupt.bank.transform_base_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::bank_address_equation_mismatch);
    corrupt = record; corrupt.bank.selected_validity_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::bank_address_equation_mismatch);
    corrupt = record; corrupt.bank.selected_runtime_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::bank_address_equation_mismatch);
    corrupt = record; corrupt.bank.bank_identity = 1U;
    corrupt.bank.transform_relative = (std::numeric_limits<std::int64_t>::min)();
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::bank_address_equation_mismatch);

    corrupt = record; corrupt.provider.provider_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::provider_address_equation_mismatch);
    corrupt = record; corrupt.provider.definition_target_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::provider_address_equation_mismatch);
    corrupt = record; corrupt.provider.definition_table_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::table_address_equation_mismatch);
    corrupt = record; corrupt.resolver.selection_address++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::table_address_equation_mismatch);
    corrupt = record; corrupt.resolver.candidate_row_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::table_address_equation_mismatch);
    corrupt = record; corrupt.resolver.resolver_stride = 0x50U;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::table_address_equation_mismatch);
    corrupt = record; corrupt.provider.pose_dispatch_base_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::dispatch_or_pose_equation_mismatch);
    corrupt = record; corrupt.provider.concrete_pose_object_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::dispatch_or_pose_equation_mismatch);
    corrupt = record; corrupt.provider.filter_context_identity++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::dispatch_or_pose_equation_mismatch);
    corrupt = record; store(corrupt.source.runtime, 0x00U, 0xDEADBEEFU);
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::provider_address_equation_mismatch);
    corrupt = record; store(corrupt.provider.provider_bytes, 0x50U, 0xDEADBEEFU);
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::provider_address_equation_mismatch);

    corrupt = record; corrupt.resolver.selector_mode = route::RuntimeSelectorMode::sentinel_f00dfeed;
    CHECK(route::validate_raw_observation(corrupt) == route::RawObservationResult::retained);
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::sentinel_or_failed_result);
    corrupt = record; corrupt.resolver.result_status = route::NativeResultStatus::zero_result;
    corrupt.resolver.native_result_count = 0;
    CHECK(route::validate_raw_observation(corrupt) == route::RawObservationResult::retained);
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::sentinel_or_failed_result);
    corrupt = record; corrupt.resolver.candidate_result_after_read_complete = false;
    CHECK(route::validate_raw_observation(corrupt) == route::RawObservationResult::retained);
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::incomplete_nested_read);

    corrupt = record; corrupt.resolver.binding_key = 0U;
    store(corrupt.resolver.candidate_row, 0x04U, 0U);
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::closed);
    CHECK(!route::resolves_exact_left_hand_joint(corrupt));
    corrupt = record; corrupt.resolver.selection_value = 0x01020304U;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::closed);
    corrupt = record; corrupt.ownership.observed_provider_factory_ordinal = 99U;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::closed);

    corrupt = record; corrupt.provider.pose_resource_definition++;
    CHECK(route::close_route(corrupt, scope) == route::RouteClosureResult::closed);
    CHECK(!route::resolves_exact_left_hand_joint(corrupt));
    corrupt = record; corrupt.resolver.pose_resource_generation++;
    CHECK(!route::resolves_exact_left_hand_joint(corrupt));

    corrupt = record; corrupt.entry_scope = {};
    CHECK(route::validate_raw_observation(corrupt) == route::RawObservationResult::invalid_scope_token);
}

void test_static_exclusion() noexcept {
    route::RouteSubject subject{};
    subject.effect_class = route::kPlacedVisualClass;
    CHECK(route::permanently_excluded_static_visual(subject));
    subject = {}; subject.placed_anchor_or_target = route::kStaticFanAnchor;
    CHECK(route::permanently_excluded_static_visual(subject));
    subject = {}; subject.placed_anchor_or_target = route::kStaticPortalTarget;
    CHECK(route::permanently_excluded_static_visual(subject));
    for (const auto definition : route::kPlacedVisualDefinitions) {
        subject = {}; subject.effect_resource = {definition};
        CHECK(route::permanently_excluded_static_visual(subject));
    }
    for (const auto handle : route::kPlacedVisualFullHandles) {
        subject = {}; subject.placed_visual_full_handle = handle;
        CHECK(route::permanently_excluded_static_visual(subject));
    }
}

void test_effect_ledgers(route::OwnerScopeToken scope) noexcept {
    route::EffectLifetimeLedger ledger{};
    constexpr std::array<route::EffectTerminalKind, 4U> kinds{
        route::EffectTerminalKind::explicit_destroy,
        route::EffectTerminalKind::priority_eviction_destroy,
        route::EffectTerminalKind::automatic_expiry_path_a,
        route::EffectTerminalKind::automatic_expiry_path_b};
    for (std::size_t index = 0U; index < kinds.size(); ++index) {
        auto create = make_effect_create(scope, static_cast<std::uint32_t>(0x100U + index), 700U + index);
        CHECK(route::validate_effect_create(create) == route::AuxiliaryValidationResult::valid);
        CHECK(route::effect_serial_from_out_pair(create.out_pair_after) == static_cast<std::uint32_t>(700U + index));
        CHECK(route::effect_handle_from_out_pair(create.out_pair_after).value == 0x100U + index);
        CHECK(ledger.try_record_create(create) == route::EffectLedgerResult::recorded);
        CHECK(ledger.try_record_create(create) == route::EffectLedgerResult::duplicate_live_handle);
        auto terminal = make_effect_terminal(create, kinds[index], 800U + index);
        CHECK(route::validate_effect_terminal(terminal) == route::AuxiliaryValidationResult::valid);
        CHECK(ledger.try_record_terminal(terminal) == route::EffectLedgerResult::recorded);
        CHECK(ledger.try_record_terminal(terminal) == route::EffectLedgerResult::already_terminal);
        route::EffectLifetime lifetime{};
        CHECK(ledger.try_find(terminal.full_handle, terminal.pool_generation, lifetime));
        CHECK(lifetime.terminal_observed);
        CHECK(lifetime.terminal_kind == kinds[index]);
        CHECK(route::early_effect_definition(lifetime.resource));
        CHECK(lifetime.owner_actor_full_handle == route::kSuccessorVisibleFullHandle);
        CHECK(lifetime.owner_provider_generation == 111U);
    }
    auto failed = make_effect_create(scope, 0x200U, 900U);
    failed.native_success = false;
    failed.out_pair_after = 0U;
    failed.resolved_effect_identity = 0U;
    failed.pool_generation = 0U;
    failed.create_serial = 0U;
    CHECK(route::validate_effect_create(failed) == route::AuxiliaryValidationResult::valid);
    CHECK(ledger.try_record_create(failed) == route::EffectLedgerResult::invalid_capture);
    CHECK(ledger.testing_lock());
    CHECK(ledger.try_record_create(make_effect_create(scope, 0x300U, 1000U))
          == route::EffectLedgerResult::busy);
    ledger.testing_unlock();
}

void test_successor(route::OwnerScopeToken scope) noexcept {
    auto successor = make_closed_record(scope);
    auto retiring = successor;
    retiring.provider.owner.scheduler_tag = route::kInitialVisibleSchedulerTag;
    retiring.provider.owner.full_handle = route::kInitialVisibleFullHandle;
    retiring.provider.owner.object_record_generation = 90U;
    retiring.provider.provider_generation = 91U;
    retiring.presentation.bank_generation = 92U;
    auto invalidation = make_invalidation(scope, retiring);
    const auto retiring_key = route::provider_freshness_key(retiring);
    CHECK(route::validate_actor_invalidation(invalidation) == route::AuxiliaryValidationResult::valid);
    CHECK(route::evaluate_successor_eligibility(invalidation, retiring_key, successor, scope)
          == route::SuccessorEligibilityResult::eligible_capture_only_emission_not_observed);
    CHECK(!route::kFrozenSuccessorRouteObserved);
    CHECK(!route::kFrozenSuccessorEmissionObserved);
    CHECK(!route::kCaptureBeforeEnableOraclePassed);
    CHECK(!route::kSuccessorEmissionEnabled);
    CHECK(!route::kCarrierFallbackAllowed);

    auto corrupt = successor;
    corrupt.provider.owner.scheduler_tag = route::kCarrierSchedulerTag;
    corrupt.provider.owner.full_handle = route::kCarrierFullHandle;
    CHECK(route::evaluate_successor_eligibility(invalidation, retiring_key, corrupt, scope)
          == route::SuccessorEligibilityResult::carrier_fallback_rejected);
    corrupt = successor; corrupt.resolver.provider_route_ordinal_for_actor = 2U;
    CHECK(route::evaluate_successor_eligibility(invalidation, retiring_key, corrupt, scope)
          == route::SuccessorEligibilityResult::not_first_exact_route);
    corrupt = successor; corrupt.ownership.carrier_exact_provider_owner_proven = true;
    CHECK(route::evaluate_successor_eligibility(invalidation, retiring_key, corrupt, scope)
          == route::SuccessorEligibilityResult::carrier_fallback_rejected);
    corrupt = successor; corrupt.resolver.candidate_ordinal = 8U;
    CHECK(route::evaluate_successor_eligibility(invalidation, retiring_key, corrupt, scope)
          != route::SuccessorEligibilityResult::eligible_capture_only_emission_not_observed);
    auto late = invalidation; late.rooted_generation_invalidated_before_original = false;
    CHECK(route::validate_actor_invalidation(late)
          == route::AuxiliaryValidationResult::invalidation_not_pre_native);

    const route::ProbeSamplePair samples{22'744'656U, 22'744'687U,
        route::ProbeBoundary::after_transition_wrapper_and_schedule,
        route::ProbeBoundary::before_successor_factory};
    CHECK(route::coarse_probe_sample_delta(samples) == 31U);
}

void test_queue_and_telemetry(route::OwnerScopeToken scope) {
    const auto source = make_closed_record(scope);
    route::CaptureQueue queue{};
    auto caller = source;
    CHECK(queue.try_push(caller) == route::QueuePushResult::enqueued);
    caller.metadata.call_id = 999999U;
    caller.bank.output_after.fill(std::byte{0xCC});
    route::RouteCaptureRecord popped{};
    CHECK(queue.try_pop(popped) == route::QueuePopResult::success);
    CHECK(popped.sequence == 1U);
    CHECK(popped.metadata.call_id == source.metadata.call_id);
    CHECK(popped.bank.output_after == source.bank.output_after);

    CHECK(queue.testing_lock());
    CHECK(queue.try_push(source) == route::QueuePushResult::busy);
    CHECK(queue.try_pop(popped) == route::QueuePopResult::busy);
    queue.testing_unlock();
    queue.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(queue.try_push(source) == route::QueuePushResult::sequence_exhausted);

    route::CaptureQueue full{};
    for (std::size_t index = 0U; index < route::kCaptureQueueCapacity; ++index) {
        CHECK(full.try_push(source) == route::QueuePushResult::enqueued);
    }
    CHECK(full.try_push(source) == route::QueuePushResult::full);

    auto excluded = source;
    excluded.subject.placed_anchor_or_target = route::kStaticPortalTarget;
    CHECK(full.try_push(excluded) == route::QueuePushResult::static_fan_excluded);

    route::OpaqueIdProjector projector{};
    route::RouteTelemetry telemetry{};
    auto unpublished = source;
    CHECK(!route::default_telemetry(unpublished, scope, 0U, projector, telemetry));
    CHECK(route::default_telemetry(popped, scope, queue.counters().losses(), projector, telemetry));
    CHECK(telemetry.sequence != 0U);
    CHECK(telemetry.cohort_id == scope.cohort_id());
    CHECK(telemetry.status == route::TelemetryRecordStatus::route_closed);
    CHECK(telemetry.actor_opaque_id != 0U);
    CHECK(telemetry.actor_opaque_id != source.provider.owner.full_handle);
    CHECK(telemetry.left_hand_joint_valid);
    CHECK(telemetry.observed_provider_factory_ordinal
          == route::kSourceRetainedProviderFactoryOrdinal);
    CHECK(telemetry.carrier_concurrent);
    CHECK(telemetry.carrier_presentation_correlated);
    CHECK(!telemetry.carrier_exact_provider_owner_proven);

    route::CaptureQueue concurrent{};
    constexpr std::size_t producer_count = 8U;
    constexpr std::size_t per_producer = 400U;
    std::atomic<std::size_t> done{};
    std::atomic<std::size_t> popped_count{};
    std::vector<std::thread> producers{};
    producers.reserve(producer_count);
    std::thread consumer([&]() {
        route::RouteCaptureRecord item{};
        while (done.load(std::memory_order_acquire) != producer_count) {
            if (concurrent.try_pop(item) == route::QueuePopResult::success) {
                popped_count.fetch_add(1U, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        while (concurrent.try_pop(item) == route::QueuePopResult::success) {
            popped_count.fetch_add(1U, std::memory_order_relaxed);
        }
    });
    for (std::size_t producer = 0U; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer]() {
            auto local = source;
            local.metadata.producer_thread_id = static_cast<std::uint32_t>(producer + 1U);
            for (std::size_t index = 0U; index < per_producer; ++index) {
                local.metadata.call_id = 1U + producer * per_producer + index;
                local.original.call_id = local.metadata.call_id;
                (void)concurrent.try_push(local);
            }
            done.fetch_add(1U, std::memory_order_release);
        });
    }
    for (auto& producer : producers) { producer.join(); }
    consumer.join();
    const auto counters = concurrent.counters();
    CHECK(counters.accepted + counters.losses() == producer_count * per_producer);
    CHECK(popped_count.load(std::memory_order_relaxed) == counters.accepted);
}

void increment_original(int* calls) noexcept { ++(*calls); }
int add_original(int left, int right, int* calls) noexcept { ++(*calls); return left + right; }

void blocking_original(std::atomic_bool* entered,
                       std::atomic_bool* release,
                       std::atomic<int>* calls) noexcept {
    calls->fetch_add(1, std::memory_order_relaxed);
    entered->store(true, std::memory_order_release);
    while (!release->load(std::memory_order_acquire)) { std::this_thread::yield(); }
}

void test_lifecycle_and_tls(route::RuntimeCohortToken cohort,
                            route::LifecycleTokenOwner& owner,
                            route::OwnerScopeToken scope) {
    route::SourceOnlyCohortState state{};
    CHECK(state.remember_admission(cohort, scope));
    CHECK(state.phase() == route::SourceOnlyCohortPhase::admission_validated);
    CHECK(!state.can_attach());
    CHECK(!state.integration_ready());
    CHECK(state.quiesce());

    route::RouteArmToken base{scope, 1U, 2U, 0x1000U, 0x2000U,
                              route::kSuccessorVisibleFullHandle, 5U};
    route::g_route_arm_stack.invalidate();
    std::array<route::RouteArmToken, route::kRouteArmDepth> tokens{};
    for (std::size_t index = 0U; index < tokens.size(); ++index) {
        tokens[index] = base;
        tokens[index].call_id += index;
        tokens[index].nonce += index;
        CHECK(route::g_route_arm_stack.push(tokens[index]));
    }
    auto overflow = base; overflow.call_id = 99U; overflow.nonce = 100U;
    CHECK(!route::g_route_arm_stack.push(overflow));
    for (std::size_t index = tokens.size(); index > 0U; --index) {
        CHECK(route::g_route_arm_stack.pop(tokens[index - 1U]));
    }
    CHECK(route::g_route_arm_stack.depth() == 0U);
    {
        route::ScopedRouteArm arm{base};
        CHECK(arm.armed());
        CHECK(route::g_route_arm_stack.current() != nullptr);
    }
    CHECK(route::g_route_arm_stack.current() == nullptr);

    std::atomic_bool ready1{}; std::atomic_bool ready2{}; std::atomic_bool release{};
    std::atomic_bool isolated1{}; std::atomic_bool isolated2{};
    auto tls_worker = [&](std::uint64_t id, std::atomic_bool& ready, std::atomic_bool& isolated) {
        auto token = base; token.call_id = id; token.nonce = id + 10U;
        route::ScopedRouteArm arm{token};
        ready.store(true, std::memory_order_release);
        while (!release.load(std::memory_order_acquire)) { std::this_thread::yield(); }
        const auto* current = route::g_route_arm_stack.current();
        isolated.store(arm.armed() && current != nullptr && *current == token,
                       std::memory_order_release);
    };
    std::thread first(tls_worker, 100U, std::ref(ready1), std::ref(isolated1));
    std::thread second(tls_worker, 200U, std::ref(ready2), std::ref(isolated2));
    while (!ready1.load(std::memory_order_acquire) || !ready2.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    CHECK(route::g_route_arm_stack.current() == nullptr);
    release.store(true, std::memory_order_release);
    first.join(); second.join();
    CHECK(isolated1.load(std::memory_order_acquire));
    CHECK(isolated2.load(std::memory_order_acquire));

    route::FullCallGate gate{};
    gate.accept();
    int calls = 0;
    const auto void_outcome = route::invoke_void_original_once(gate, 10U, &increment_original, &calls);
    CHECK(calls == 1);
    CHECK(void_outcome.receipt.valid_for(10U));
    CHECK(void_outcome.observation_admitted);
    const auto value_outcome = route::invoke_value_original_once(gate, 11U, &add_original, 2, 3, &calls);
    CHECK(value_outcome.value == 5);
    CHECK(value_outcome.receipt.valid_for(11U));
    CHECK(calls == 2);

    std::atomic_bool entered{}; std::atomic_bool unblock{}; std::atomic<int> concurrent_calls{};
    route::OriginalCallOutcome concurrent_outcome{};
    std::thread active([&]() {
        concurrent_outcome = route::invoke_void_original_once(
            gate, 12U, &blocking_original, &entered, &unblock, &concurrent_calls);
    });
    while (!entered.load(std::memory_order_acquire)) { std::this_thread::yield(); }
    gate.quiesce();
    CHECK(!gate.accepting());
    CHECK(!gate.idle());
    int quiesced_calls = 0;
    const auto quiesced = route::invoke_void_original_once(gate, 13U, &increment_original, &quiesced_calls);
    CHECK(quiesced_calls == 1);
    CHECK(!quiesced.observation_admitted);
    CHECK(quiesced.receipt.valid_for(13U));
    unblock.store(true, std::memory_order_release);
    active.join();
    CHECK(concurrent_calls.load(std::memory_order_relaxed) == 1);
    CHECK(concurrent_outcome.observation_admitted);
    CHECK(gate.idle());

    owner.publish(2U, 2U);
    CHECK(!owner.is_current(scope));
    route::OwnerScopeToken replacement{};
    CHECK(owner.try_snapshot(replacement));
    CHECK(replacement != scope);
}

} // namespace

int main() {
    FileView packed{route::kPinnedPackedRuntimePath};
    FileView unpacked{route::kPinnedUnpackedProvenancePath};
    MappedRuntimeFixture mapped{packed, unpacked};
    route::RuntimeCohortToken cohort{};
    test_runtime_admission(packed, unpacked, mapped, cohort);
    if (!cohort.valid()) {
        std::fprintf(stderr, "runtime evidence unavailable; remaining tests cannot issue owner tokens\n");
        return 1;
    }

    route::LifecycleTokenOwner owner{cohort, 1U, 1U, 1U};
    route::OwnerScopeToken scope{};
    CHECK(owner.try_snapshot(scope));
    CHECK(owner.is_current(scope));
    test_route_equations(scope);
    test_static_exclusion();
    test_effect_ledgers(scope);
    test_successor(scope);
    test_queue_and_telemetry(scope);
    test_lifecycle_and_tls(cohort, owner, scope);

    CHECK(sizeof(route::RouteCaptureRecord) <= 2048U);
    CHECK(sizeof(route::CaptureQueue) <= 160U * 1024U);
    CHECK(!route::kIntegrationReady);
    CHECK(!route::kProvidesVfxWriter);
    CHECK(!route::kProvidesAttachmentRebind);
    CHECK(!route::kProvidesCandidateSubstitution);
    CHECK(!route::kProvidesPoseSwap);
    CHECK(!route::kProvidesSuppression);

    if (g_failures != 0) {
        std::fprintf(stderr, "%d Ikora VFX route capture check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all evidence-derived Ikora VFX route capture checks passed "
                "(record=%zu bytes queue=%zu bytes)\n",
                sizeof(route::RouteCaptureRecord), sizeof(route::CaptureQueue));
    return 0;
}
