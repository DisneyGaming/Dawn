#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "native_capture_observer.h"
#include "../../hooking/call_gate.h"
#include "../../../core/logging/log.h"
#include "../../../server/runtime/activity/native_capture_bridge.h"

namespace dawn::client::hooks::bootflow {
namespace {
namespace bridge = server::runtime::activity::capture_bridge;
namespace feedback = server::runtime::activity::capture_feedback;

// Exact supported-build prefixes. The tick's first 16 bytes are complete,
// non-relative instructions. Original 1006F20 returns success in AL, not EAX.
constexpr std::array<std::uint8_t, 16> kTickPrefix{
    0x41,0x56,0x48,0x81,0xEC,0x80,0x00,0x00,0x00,0x0F,0xB6,0x41,0x30,0x4C,0x8B,0xF1};
constexpr std::array<std::uint8_t, 15> kEntitySourcePrefix{
    0x48,0x89,0x5C,0x24,0x20,0x56,0x48,0x83,0xEC,0x50,0x48,0x8B,0xF2,0x8B,0xD9};
constexpr std::array<std::uint8_t, 14> kScopedSourcePrefix{
    0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x0F,0xBE,0x41,0x04,0x48,0x8B,0xDA};
constexpr std::array<std::uint8_t, 15> kSourceClockPrefix{
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20};
constexpr std::uintptr_t kEntitySourceRva = 0x502350;
constexpr std::uintptr_t kScopedSourceRva = 0x501AD0;
constexpr std::uintptr_t kSourceClockRva = 0x4ECC50;
constexpr std::uintptr_t kLowestPointer = 0x10000;
constexpr std::uintptr_t kHighestPointer = 0x00007FFFFFFFFFFFULL;

struct ScopedSource final {
    std::uint32_t registry{0x811C9DC5};
    std::uint8_t type{}, reserved{};
    std::uint16_t slot{};
};
struct Descriptor final {
    std::uint32_t handle{UINT32_MAX}, type{UINT32_MAX};
    std::int64_t offset{};
};
static_assert(sizeof(ScopedSource) == 8 && sizeof(Descriptor) == 16);
using Tick = NativeCaptureTick;
using EntitySource = std::uint8_t(__fastcall*)(std::uint32_t, ScopedSource*);
using ResolveSource = std::uint8_t(__fastcall*)(const ScopedSource*, Descriptor*);
using SourceClock = std::uintptr_t(__fastcall*)(const std::byte*);

bool g_initialized{};
hooking::CallGate g_calls{};
std::atomic_uint64_t g_sequence{};
std::uintptr_t g_image{};

[[nodiscard]] bool pointer(std::uintptr_t at, std::size_t count = 1) noexcept {
    return at >= kLowestPointer && count && at <= kHighestPointer
        && count - 1 <= kHighestPointer - at;
}

// Only observation helpers catch unreadable native memory. The original tick
// runs outside every SEH handler and executes exactly once on every hook call.
[[nodiscard]] __declspec(noinline) bool copy(std::uintptr_t from, void* to,
                                           std::size_t bytes) noexcept {
    if (!to || !pointer(from, bytes)) return false;
    __try {
        std::memcpy(to, reinterpret_cast<const void*>(from), bytes);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}
template<class T> [[nodiscard]] bool read(std::uintptr_t from, T& value) noexcept {
    return copy(from, &value, sizeof value);
}
template<class T, std::size_t N>
[[nodiscard]] T field(const std::array<std::byte, N>& bytes, std::size_t at) noexcept {
    return feedback::field<T>(bytes, at);
}
template<std::size_t N> [[nodiscard]] bool prefix(
    std::uintptr_t at, const std::array<std::uint8_t, N>& expected) noexcept {
    std::array<std::uint8_t, N> actual{};
    return read(at, actual) && actual == expected;
}

/** Mirrors the native salted-handle directory, including its signed relocation. */
[[nodiscard]] std::uintptr_t resolve(std::uint32_t handle, std::int64_t offset = 0) noexcept {
    if (handle == UINT32_MAX) return 0;
    std::uintptr_t directory{}, tables{}, rows{};
    if (!read(g_image + 0x2439C70, directory) || !read(directory, tables)) return 0;
    const auto shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(handle) >> 13);
    const auto bucket = ((shifted | 0x0FFC0000U) >> 18) & (shifted & 0xFFFFU);
    const auto table = tables + static_cast<std::uintptr_t>(bucket) * 0x40;
    std::int32_t stride{}, mask{};
    if (!read(table + 8, rows) || !read(table + 0x30, stride)
        || !read(table + 0x34, mask) || stride <= 0 || stride > 0x100000) return 0;
    const auto row = rows + static_cast<std::uintptr_t>(handle & 0x1FFFU)
        * static_cast<std::uint32_t>(stride);
    std::uint64_t relocation{};
    if (!read(row + 8, relocation)) return 0;
    const auto correction = static_cast<std::uint64_t>(static_cast<std::int64_t>(mask)) & relocation;
    const auto result = row - correction + static_cast<std::uint64_t>(offset);
    return pointer(result) ? result : 0;
}

[[nodiscard]] __declspec(noinline) bool source_for_entity(
    std::uint32_t entity, ScopedSource& scoped, Descriptor& source) noexcept {
    __try {
        return reinterpret_cast<EntitySource>(g_image + kEntitySourceRva)(entity, &scoped) != 0
            && scoped.type == 4
            && reinterpret_cast<ResolveSource>(g_image + kScopedSourceRva)(&scoped, &source) != 0
            && source.handle != UINT32_MAX;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}
[[nodiscard]] __declspec(noinline) std::uintptr_t native_clock(std::uintptr_t source) noexcept {
    __try {
        return reinterpret_cast<SourceClock>(g_image + kSourceClockRva)(
            reinterpret_cast<const std::byte*>(source));
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return 0;
    }
}

/** Read the same source-clock mapping as 4ECC50/3C96C0; retain its full salt. */
[[nodiscard]] bool clock_context(std::uint32_t registry, std::uint32_t& handle,
                                 std::uintptr_t& address) noexcept {
    std::uint16_t selector{};
    std::uint64_t arenaOffset{};
    std::uint32_t arenaStride{};
    std::uintptr_t lookup{};
    std::int32_t count{};
    const auto arena = g_image + 0x2109B80;
    if (!read(g_image + 0x1F91FE8, selector) || !selector
        || !read(arena, arenaOffset) || !read(arena + 0x10, arenaStride)
        || !arenaStride || arenaStride > 0x100000
        || !read(arena + arenaOffset + static_cast<std::uint64_t>(selector) * arenaStride, lookup)
        || !read(lookup + 8, count) || count < 1 || count > 1024) return false;
    std::uint32_t id{UINT32_MAX};
    for (std::int32_t i = 0; i < count; ++i) {
        std::array<std::byte, 0x18> row{};
        if (!read(lookup + 0x10 + static_cast<std::uintptr_t>(i) * row.size(), row)) return false;
        if (field<std::uint32_t>(row, 4) != registry) continue;
        if (id != UINT32_MAX) return false;
        id = field<std::uint32_t>(row, 0xC);
    }
    if (id == UINT32_MAX) return false;
    std::array<std::byte, 0x40> pool{};
    if (!read(g_image + 0x1F8B090, pool)) return false;
    const auto index = id & 0x1FFFU;
    const auto stride = field<std::uint32_t>(pool, 0x20);
    if (!stride || stride > 0x100000) return false;
    std::uint32_t generation{};
    if (!read(field<std::uintptr_t>(pool, 8) + field<std::uint32_t>(pool, 0x1C)
        + static_cast<std::uint64_t>(stride) * index, generation)) return false;
    generation &= field<std::uint32_t>(pool, 0x24);
    const auto type = field<std::uint32_t>(pool, 0x34);
    handle = (type & 0x40000000U)
        ? (((((generation & 7U) | 0xFFFFFFF0U) << 14) | (type & 0x3FFFU)) << 13) | index
        : (((type & 0x3FFU) | ((generation & 0xFFU) << 10)) << 13) | index;
    address = resolve(handle);
    return address != 0;
}

struct Sample final {
    std::array<std::byte, feedback::kSourceBytes> source{};
    std::array<std::byte, 0x70> sourceDefinition{};
    std::array<std::byte, 0xC0> entity{};
    std::array<std::byte, 0x10> scene{};
    std::array<std::byte, feedback::kControllerBytes> controller{};
    std::array<std::byte, 0x80> context{};
    ScopedSource scoped{};
    std::uintptr_t sourceAddress{}, definitionAddress{}, entityAddress{}, sceneAddress{}, contextAddress{};
    std::uint32_t sourceHandle{}, commonHandle{}, entityHandle{}, sceneHandle{}, controllerHandle{}, clockHandle{};
};

struct SourceHandles final { std::uint32_t indexed{}, common{}; std::int64_t commonOffset{}; };
[[nodiscard]] SourceHandles source_handles(const std::array<std::byte, feedback::kSourceBytes>& source) noexcept {
    return {field<std::uint32_t>(source, 0x20), field<std::uint32_t>(source, 0x160),
            field<std::int64_t>(source, 0x168)};
}

enum class Rejection : std::size_t {
    none, controller_read, controller_handle, entity_source, source_read, source_common_identity,
    definition_or_entity_read, scene_read, definition_registry, clock_lookup,
    clock_pointer, clock_read, identity_changed, native_state, count
};
constexpr std::array kRejectionNames{
    "none", "controller_read", "controller_handle", "entity_source", "source_read", "source_common_identity",
    "definition_or_entity_read", "scene_read", "definition_registry", "clock_lookup",
    "clock_pointer", "clock_read", "identity_changed", "native_state"};
std::array<std::array<std::atomic_uint64_t, static_cast<std::size_t>(Rejection::count)>, 2> g_rejectedEpoch{};

// This global arm stamp is diagnostic only; receipts use the exact ticket read
// before the original callback. It also permits a bounded diagnostic when the
// native source lookup fails before an exact source ticket can be recovered.
[[nodiscard]] std::uint64_t diagnostic_epoch() noexcept {
    const std::lock_guard lock(bridge::mutex);
    return bridge::nextArm;
}
void reject(Rejection reason, bool after, std::uint64_t epoch, const Sample& sample) noexcept {
    if (!epoch || reason == Rejection::none) return;
    const auto index = static_cast<std::size_t>(reason);
    auto& stamp = g_rejectedEpoch[after ? 1 : 0][index];
    auto previous = stamp.load(std::memory_order_relaxed);
    do { if (previous >= epoch) return; }
    while (!stamp.compare_exchange_weak(previous, epoch, std::memory_order_relaxed));
    std::array<char, 512> message{};
    const int length = std::snprintf(message.data(), message.size(),
        "ev=native_capture stage=observer_sample result=rejected phase=%s reason=%s arm_epoch=%llu controller_def=0x%08X entity=0x%08X registry=0x%08X slot=%u clock=0x%08X clock_ready=%u active=%u running=%u progress=%.3f",
        after ? "after" : "before", kRejectionNames[index], static_cast<unsigned long long>(epoch),
        field<std::uint32_t>(sample.controller, 0), sample.entityHandle, sample.scoped.registry,
        static_cast<unsigned>(sample.scoped.slot), sample.clockHandle,
        static_cast<unsigned>(field<std::uint8_t>(sample.context, 0x10)),
        static_cast<unsigned>(field<std::uint8_t>(sample.controller, 0x30)),
        static_cast<unsigned>(field<std::uint8_t>(sample.controller, 0x38)),
        static_cast<double>(field<float>(sample.controller, 0x1B8)));
    if (length > 0 && static_cast<std::size_t>(length) < message.size())
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {message.data(), static_cast<std::size_t>(length)});
}

[[nodiscard]] Rejection sample(std::byte* controller, Sample& out) noexcept {
    const auto controllerAddress = reinterpret_cast<std::uintptr_t>(controller);
    if (!read(controllerAddress, out.controller)
        || field<std::uint32_t>(out.controller, 4) != 0x80804FCB) return Rejection::controller_read;
    out.entityHandle = field<std::uint32_t>(out.controller, 0x2C);
    out.controllerHandle = field<std::uint32_t>(out.controller, 0x24);
    // Scene subcomponents may need an offset in their typed descriptor. The
    // original callback supplies the actual controller pointer; its complete
    // header and salted handle are checked before/after and pinned by the bridge.
    if (out.controllerHandle == UINT32_MAX) return Rejection::controller_handle;
    Descriptor descriptor{};
    if (!source_for_entity(out.entityHandle, out.scoped, descriptor)) return Rejection::entity_source;
    out.sourceAddress = resolve(descriptor.handle, descriptor.offset);
    if (!read(out.sourceAddress, out.source)) return Rejection::source_read;
    // Sources use an indexed handle at +0x20. The +0x160 common
    // descriptor resolves to this source; it is not a registry metadata object.
    const auto handles = source_handles(out.source);
    out.sourceHandle = handles.indexed;
    out.commonHandle = handles.common;
    const auto commonAddress = resolve(out.commonHandle, handles.commonOffset);
    out.definitionAddress = resolve(field<std::uint32_t>(out.source, 0), field<std::int64_t>(out.source, 8));
    out.entityAddress = resolve(out.entityHandle);
    if (commonAddress != out.sourceAddress) return Rejection::source_common_identity;
    if (!read(out.definitionAddress, out.sourceDefinition) || !read(out.entityAddress, out.entity))
        return Rejection::definition_or_entity_read;
    out.sceneHandle = field<std::uint32_t>(out.entity, 0x4C);
    out.sceneAddress = resolve(out.sceneHandle);
    if (!read(out.sceneAddress, out.scene)) return Rejection::scene_read;
    // 4ECC50 reads the source's definition +0x30, not its runtime +0x30.
    if (field<std::uint32_t>(out.sourceDefinition, 0x30) != out.scoped.registry)
        return Rejection::definition_registry;
    if (!clock_context(out.scoped.registry, out.clockHandle, out.contextAddress)) return Rejection::clock_lookup;
    if (native_clock(out.sourceAddress) != out.contextAddress) return Rejection::clock_pointer;
    if (!read(out.contextAddress, out.context)) return Rejection::clock_read;
    return Rejection::none;
}

[[nodiscard]] bool same_identity(const Sample& a, const Sample& b) noexcept {
    return a.scoped.registry == b.scoped.registry && a.scoped.type == b.scoped.type && a.scoped.slot == b.scoped.slot
        && a.sourceAddress == b.sourceAddress && a.definitionAddress == b.definitionAddress
        && a.entityAddress == b.entityAddress && a.sceneAddress == b.sceneAddress && a.contextAddress == b.contextAddress
        && a.sourceHandle == b.sourceHandle && a.commonHandle == b.commonHandle
        && a.entityHandle == b.entityHandle && a.sceneHandle == b.sceneHandle
        && a.controllerHandle == b.controllerHandle && a.clockHandle == b.clockHandle
        && field<std::uint32_t>(a.entity, 0) == field<std::uint32_t>(b.entity, 0);
}

// Kept out of the no-bindings forwarding path so its snapshot buffers do not
// enlarge every unrelated native controller call's stack frame.
[[nodiscard]] __declspec(noinline) std::uint8_t observe(std::byte* controller, Tick original) {
    Sample before{};
    feedback::Ticket ticket{};
    const auto epoch = diagnostic_epoch();
    const auto beforeResult = sample(controller, before);
    if (beforeResult == Rejection::none) {
        ticket = bridge::lookup(field<std::uint32_t>(before.controller, 0),
                                before.scoped.registry, before.scoped.slot);
    } else reject(beforeResult, false, epoch, before);
    const auto result = original(controller);
    if (!feedback::valid(ticket)) return result;
    Sample after{};
    const auto afterResult = sample(controller, after);
    if (afterResult != Rejection::none) {
        reject(afterResult, true, ticket.armEpoch, after); return result;
    }
    if (!same_identity(before, after)) {
        reject(Rejection::identity_changed, true, ticket.armEpoch, after); return result;
    }
    const feedback::Capture capture{
        ticket, after.source, after.sourceDefinition, after.entity, after.scene, before.controller, after.controller, after.context,
        after.sourceHandle, after.commonHandle, after.entityHandle, after.sceneHandle, after.controllerHandle,
        before.clockHandle, after.clockHandle, feedback::kTickRva,
        g_sequence.fetch_add(1, std::memory_order_relaxed) + 1};
    feedback::Observation qualified{};
    if (feedback::qualify(ticket, capture, qualified)) (void)bridge::submit(capture);
    else reject(Rejection::native_state, true, ticket.armEpoch, after);
    return result;
}

} // namespace

__declspec(noinline) std::uint8_t observe_native_capture_tick(void* raw, NativeCaptureTick original) noexcept {
    const hooking::CallGate::Scope call(g_calls);
    auto* controller = static_cast<std::byte*>(raw);
    if (!call.accepts_side_effects() || !bridge::activeCount.load(std::memory_order_acquire))
        return original(controller);
    std::uint32_t definition{};
    if (!read(reinterpret_cast<std::uintptr_t>(controller), definition) || !bridge::interested(definition))
        return original(controller);
    return observe(controller, original);
}

bool install_native_capture_observer() noexcept {
    if (g_initialized) {
        g_calls.accept();
        return true;
    }
    g_image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!g_image || !prefix(g_image + feedback::kTickRva, kTickPrefix)
        || !prefix(g_image + kEntitySourceRva, kEntitySourcePrefix)
        || !prefix(g_image + kScopedSourceRva, kScopedSourcePrefix)
        || !prefix(g_image + kSourceClockRva, kSourceClockPrefix)) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                        "ev=native_capture stage=observer_install result=fail reason=target_prefix");
        return false;
    }
    g_initialized = true;
    g_calls.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
                    "ev=native_capture stage=observer_install result=ok mode=shared_arc_charge_tick tick_rva=0x1006F20");
    return true;
}

void quiesce_native_capture_observer() noexcept {
    g_calls.quiesce();
}

bool uninstall_native_capture_observer() noexcept {
    quiesce_native_capture_observer();
    // The physical hook remains with arc-charge. Its gate spans this wrapper,
    // including native forwarding, until that owner can detach safely.
    if (!g_initialized) return true;
    if (!g_calls.idle()) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                        "ev=native_capture stage=observer_uninstall result=retained reason=in_flight");
        return false;
    }
    g_initialized = false;
    g_image = 0;
    return true;
}

bool native_capture_observer_has_ownership() noexcept {
    return g_initialized;
}

} // namespace dawn::client::hooks::bootflow
