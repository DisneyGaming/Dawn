#include "forest_candy_drops.h"
#include "forest_drop_slot_policy.h"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <optional>

#include "gateway_native_read.h"
#include "native_placement_pose_probe.h"
#include "../../../state/activity/runtime.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../server/web_service/forest_loot_pickups.h"

namespace dawn::client::hooks::bootflow::forest_candy_drops {
namespace {
namespace forest_loot = server::web_service::forest_loot;

// Native anchors (RVAs), traced statically on build 86657 (rewards-research-20260914/bauble-injection).
constexpr std::uintptr_t kDispatchRva = 0x4AA1C0;         // dispatch(sink, killRecord): slot 3 of the sink, unconditionally
constexpr std::uintptr_t kLootSinkVtableRva = 0x1BEBF18;  // sink whose slot 3 is 0x4CE790 (the only caller of the roll)
constexpr std::uintptr_t kPresentationSinkVtableRva = 0x1C21E68; // sink whose slot 3 is 0xD7EA90 (native ring push)
constexpr std::uintptr_t kLootStateRva = 0xBE1CB0;        // void* loot_state(void), may return null
constexpr std::uintptr_t kSequenceRva = 0x523880;         // i32 next_sequence(lootState): returns pre-increment
constexpr std::uintptr_t kComponentRva = 0xFCA3F0;        // void* singleton(descriptor)
constexpr std::uintptr_t kPushRowRva = 0xBE2820;          // void push_tracked_source(const Row48*)
constexpr std::uintptr_t kMarkDirtyRva = 0xBE2850;        // void mark_bauble_sweep_dirty(void)
constexpr std::uintptr_t kInventoryResourcesRva = 0xFA4780;
constexpr std::uintptr_t kItemResourcesRva = 0xF27510;
constexpr std::uintptr_t kBaubleDescriptorRva = 0x1FB7698; // the bauble component's singleton descriptor
constexpr std::uintptr_t kWorldPositionRva = 0x3F7C30;    // float* world_position(entityRecord, float[4]) - lair cinematic's read
constexpr std::uintptr_t kZoneGetterRva = 0x429BA0;       // void* current_zone(world, scratch): dword [result] = zone id
constexpr std::uintptr_t kWorldGlobalRva = 0x1F8DD00;     // the world singleton 0x4294C0 returns (lea rax,[rip+...])
constexpr std::uintptr_t kComponentDirtyOffset = 0x1A30;  // sweep-dirty byte the walker consumes
constexpr std::uintptr_t kTableOffset = 0x4C20;           // pending-drop table: 64 records x 200 bytes
constexpr std::size_t kRecordBytes = 0xC8;
constexpr std::size_t kRecordCount = 64;
constexpr std::size_t kFallbackRowBase = 0x4B;            // fallback source row i lives at component+(i+0x4B)*48
constexpr std::uint32_t kSyntheticSourceTag = 0x0B;       // walker skips records with this tag
constexpr std::uint8_t kBucketDefault = 0xFF;

// Kill record fields (the dispatcher's rdx): entry count, the source block at +0x408.
constexpr std::uintptr_t kRecordEntryCountOffset = 0x00;
constexpr std::uintptr_t kRecordEntriesOffset = 0x08;      // 16-byte entries {i32 defHash, i32 defIdx, i64 defRow}
constexpr std::uintptr_t kRecordHasSourceOffset = 0x408;   // u8
constexpr std::uintptr_t kRecordSourceKindOffset = 0x40C;  // i32, 1 = world object at +0x420
constexpr std::uintptr_t kRecordSourceObjectOffset = 0x420; // the object the record is about
constexpr std::int32_t kSourceKindWorldObject = 1;

// Source object fields and the entity record the native ring builder reads position/zone from.
constexpr std::uintptr_t kSourceTagOffset = 0x00;          // per-event serial stamped by 0x4AA20B just before dispatch
constexpr std::uintptr_t kSourceHandleOffset = 0x10;
constexpr std::uintptr_t kSourceEntityWeakOffset = 0x60;   // {u32 serial, u32 handle}
constexpr std::uintptr_t kEntityRowPositionOffset = 0x2B0; // 16 bytes on the object-table record (raw, may be stale)
constexpr std::uintptr_t kEntityRowZoneOffset = 0x2D0;     // i32 on the object-table record

// Synthetic source references for server-requested drops (chest coins): never 0x0B, never a real serial range.
constexpr std::int32_t kSyntheticTagBase = 0x4E430000;
constexpr std::uint64_t kSyntheticHandleBase = 0xC0FFEE0000000000ULL;

constexpr std::array<std::uint8_t, 16> kDispatchPrefix{0x40, 0x57, 0x41, 0x56, 0x48, 0x83, 0xEC, 0x28,
                                                        0x4C, 0x8B, 0xF1, 0x48, 0x89, 0x5C, 0x24, 0x50};
constexpr std::array<std::uint8_t, 16> kLootStatePrefix{0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8D,
                                                         0x0D, 0x33, 0x59, 0x3D, 0x01, 0x33, 0xDB, 0xE8};
constexpr std::array<std::uint8_t, 16> kSequencePrefix{0x8B, 0x81, 0x18, 0x4C, 0x00, 0x00, 0x8D, 0x50,
                                                        0x01, 0x89, 0x91, 0x18, 0x4C, 0x00, 0x00, 0xC3};
constexpr std::array<std::uint8_t, 16> kComponentPrefix{0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
                                                         0x59, 0x08, 0x48, 0x85, 0xDB, 0x0F, 0x84, 0x3E};
constexpr std::array<std::uint8_t, 16> kPushRowPrefix{0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
                                                       0xD9, 0x48, 0x8D, 0x0D, 0x68, 0x4E, 0x3D, 0x01};
constexpr std::array<std::uint8_t, 16> kMarkDirtyPrefix{0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x0D, 0x3D,
                                                         0x4E, 0x3D, 0x01, 0xE8, 0x90, 0x7B, 0x3E, 0x00};
constexpr std::array<std::uint8_t, 16> kWorldPositionPrefix{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                             0xEC, 0x40, 0x0F, 0x29, 0x74, 0x24, 0x30, 0x48};
constexpr std::array<std::uint8_t, 16> kZoneGetterPrefix{0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83,
                                                          0xEC, 0x20, 0x48, 0x8B, 0x59, 0x08, 0x48, 0x8B};


using Dispatch = std::uint64_t(__fastcall*)(void*, void*, void*, void*) noexcept;
using LootState = void*(__fastcall*)() noexcept;
using NextSequence = std::int32_t(__fastcall*)(void*) noexcept;
using Component = void*(__fastcall*)(void*) noexcept;
using PushRow = void(__fastcall*)(const void*) noexcept;
using MarkDirty = void(__fastcall*)() noexcept;
using InventoryResources = void(__fastcall*)(void*, void*) noexcept;
using ItemResources = void(__fastcall*)(std::uint16_t, const void*, void*,
                                       std::uint32_t*, std::uint32_t*, bool*) noexcept;
using WorldPosition = float*(__fastcall*)(void*, float*) noexcept;
using ZoneGetter = void*(__fastcall*)(void*, void*) noexcept;

/** The 48-byte tracked-source row the bauble spawner resolves a record's source through. */
struct alignas(16) Row48 final {
    std::int32_t tag{};
    std::int32_t pad{};
    std::uint64_t handle{};
    std::array<std::byte, 16> position{};
    std::int32_t zone{};
    std::array<std::uint8_t, 12> reserved{};
};
static_assert(sizeof(Row48) == 48);

/** One dispatched world object, remembered until its entity dies. */
struct Seen final {
    std::uint32_t entity{UINT32_MAX};
    gateway_native::Weak weak{};
    std::int32_t tag{};
    std::uint64_t handle{};
    std::array<std::byte, 16> position{};
    std::int32_t zone{};
    bool rowResolved{};
    bool injected{};
};

/** One drop to write: where, under which source reference, and what the server should pay. */
struct DropRequest final {
    std::int32_t tag{};
    std::uint64_t handle{};
    std::array<std::byte, 16> position{};
    std::int32_t zone{};
    std::uint32_t entity{};
    std::uint32_t itemHash{};
    std::int32_t quantity{1};
    const char* origin{"death"};
};

struct Injected final {
    std::int32_t tag{};
    std::uint64_t handle{};
    std::int32_t sequence{};
    std::size_t slot{};
    std::uint16_t item{};
};

hooking::CallGate g_gate;
std::array<hooking::detour::Handle, 2> g_handles{};
std::atomic<Dispatch> g_original{};
std::atomic<InventoryResources> g_resourcesOriginal{};
std::uintptr_t g_image{};
std::atomic<DWORD> g_gameThread{};
std::atomic<std::uint32_t> g_lines{};
std::atomic<std::uint32_t> g_injectedTotal{};
SRWLOCK g_lock = SRWLOCK_INIT;
std::array<Injected, kRecordCount> g_injected{};
std::size_t g_injectedCount{};
SRWLOCK g_seenLock = SRWLOCK_INIT;
std::array<Seen, 128> g_seen{};
std::size_t g_seenCursor{};
std::uint32_t g_rng{};
std::uint32_t g_synthetic{};
std::uint64_t g_lastProbeTick{};
std::uint8_t g_lastDirty{0xFF};
std::uint64_t g_lastHandlePair{UINT64_MAX};

template <class... Args> void report(const char* format, Args... args) noexcept {
    if (g_lines.fetch_add(1, std::memory_order_relaxed) >= 4096) { return; }
    std::array<char, 512> line{};
    const auto count = std::snprintf(line.data(), line.size(), format, args...);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

void skipped(const char* reason, std::uint32_t entity) noexcept {
    report("ev=forest_candy stage=skipped reason=%s entity=%08X", reason, entity);
}

template <std::size_t N>
bool prefix_matches(std::uintptr_t rva, const std::array<std::uint8_t, N>& expected) noexcept {
    std::array<std::byte, N> bytes{};
    gateway_native::Read read{g_image};
    return read.copy(g_image + rva, bytes)
           && std::memcmp(bytes.data(), expected.data(), expected.size()) == 0;
}

// Each native call sits in its own frame so the SEH handler never has C++ objects to unwind.
void* call_loot_state() noexcept {
    if (!prefix_matches(kLootStateRva, kLootStatePrefix)) { return nullptr; }
    __try { return reinterpret_cast<LootState>(g_image + kLootStateRva)(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool call_next_sequence(void* lootState, std::int32_t& sequence) noexcept {
    if (!prefix_matches(kSequenceRva, kSequencePrefix)) { return false; }
    __try { sequence = reinterpret_cast<NextSequence>(g_image + kSequenceRva)(lootState); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void* call_component() noexcept {
    if (!prefix_matches(kComponentRva, kComponentPrefix)) { return nullptr; }
    __try {
        return reinterpret_cast<Component>(g_image + kComponentRva)(
            reinterpret_cast<void*>(g_image + kBaubleDescriptorRva));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool call_push_row(const Row48* row) noexcept {
    if (!prefix_matches(kPushRowRva, kPushRowPrefix)) { return false; }
    __try { reinterpret_cast<PushRow>(g_image + kPushRowRva)(row); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool call_mark_dirty() noexcept {
    if (!prefix_matches(kMarkDirtyRva, kMarkDirtyPrefix)) { return false; }
    __try { reinterpret_cast<MarkDirty>(g_image + kMarkDirtyRva)(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

/** Use the callback's native request context, which owns the requested asset lifetimes. */
bool request_item_resources(void* context, std::uint16_t item,
                            std::uint32_t* definitions, std::uint32_t* presentations,
                            bool& ready) noexcept {
    __try {
        reinterpret_cast<ItemResources>(g_image + kItemResourcesRva)(
            item, nullptr, context, definitions, presentations, &ready);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

/** Same context preparation as FA47A5/FA47AA, on its original callback thread. */
bool begin_item_resources(void* context) noexcept {
    if (context == nullptr) { return false; }
    __try {
        auto** table = *reinterpret_cast<void***>(context);
        reinterpret_cast<void(__fastcall*)(void*)>(table[8])(context);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

/**
 * Declare event pickup assets during native resource collection, before drops exist.
 * Pending records appear later during play; conditioning this callback on that ledger
 * missed the collection window and left every bauble handle at -1. The native context
 * retains both assets and performs asynchronous residency checks; no foreign loader.
 */
__declspec(noinline) void __fastcall inventory_resources_hook(void* owner, void* context) noexcept {
    const hooking::CallGate::Scope call{g_gate};
    const auto original=hooking::await_original(g_resourcesOriginal);
    if(!call.accepts_side_effects()) {original(owner,context);return;}
    const bool prepared=begin_item_resources(context);
    original(owner,context);
    if(!prepared) {
        static std::atomic<bool> reported{};
        if(!reported.exchange(true))report("ev=forest_candy stage=resources result=context_unavailable");
        return;
    }
    std::array<std::uint32_t,1024> definitions{},presentations{};
    constexpr std::array<std::uint16_t,2> items{
        forest_loot::kCandyDefinitionIndex,forest_loot::kChocolateStrangeCoinIndex};
    for(std::size_t i=0;i<items.size();++i) {
        bool ready{};
        const bool requested=request_item_resources(context,items[i],definitions.data(),presentations.data(),ready);
        static std::atomic<std::uint32_t> reported{};
        const auto bit=1U << (static_cast<unsigned>(i)*3U+(requested?(ready?2U:1U):0U));
        if(!(reported.fetch_or(bit,std::memory_order_relaxed)&bit))
            report("ev=forest_candy stage=resources item=%u requested=%u ready=%u requester=native context=inventory_callback timing=preload",
                static_cast<unsigned>(items[i]),requested?1U:0U,ready?1U:0U);
    }
}
/** The lair cinematic's world-transform read: the record must still name the entity around the call. */
bool call_world_position(std::uintptr_t record, std::uint32_t entity, float* out4) noexcept {
    if (record < 0x10000 || !prefix_matches(kWorldPositionRva, kWorldPositionPrefix)) { return false; }
    gateway_native::Read read{g_image};
    std::uint32_t handleField{}, tail{};
    if (!read.value(record + 0xC, handleField) || !read.value(record + 0x3C, tail) || handleField != entity
        || tail != UINT32_MAX) {
        return false;
    }
    __try { reinterpret_cast<WorldPosition>(g_image + kWorldPositionRva)(reinterpret_cast<void*>(record), out4); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!read.value(record + 0xC, handleField) || handleField != entity) { return false; }
    return out4[3] == 1.F && std::isfinite(out4[0]) && std::isfinite(out4[1]) && std::isfinite(out4[2]);
}
bool call_current_zone(std::int32_t& zone) noexcept {
    if (!prefix_matches(kZoneGetterRva, kZoneGetterPrefix)) { return false; }
    alignas(16) std::array<std::byte, 64> scratch{};
    void* result{};
    __try {
        result = reinterpret_cast<ZoneGetter>(g_image + kZoneGetterRva)(
            reinterpret_cast<void*>(g_image + kWorldGlobalRva), scratch.data());
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    gateway_native::Read read{g_image};
    return reinterpret_cast<std::uintptr_t>(result) >= 0x10000
           && read.value(reinterpret_cast<std::uintptr_t>(result), zone);
}
bool finite_position(const std::array<std::byte, 16>& position) noexcept {
    float xyz[3]{};
    std::memcpy(xyz, position.data(), sizeof xyz);
    return std::isfinite(xyz[0]) && std::isfinite(xyz[1]) && std::isfinite(xyz[2]);
}

/** Compare-then-write: the destination must still hold `before` when the store lands. */
bool replace(std::uintptr_t address, std::span<const std::byte> before, std::span<const std::byte> after) noexcept {
    if (address < 0x10000 || before.size() != after.size() || after.empty()) { return false; }
    std::array<std::byte, kRecordBytes> current{};
    if (after.size() > current.size()) { return false; }
    gateway_native::Read read{g_image};
    if (!read.copy(address, std::span(current).first(after.size()))
        || std::memcmp(current.data(), before.data(), before.size()) != 0) {
        return false;
    }
    SIZE_T written{};
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), after.data(),
                              after.size(), &written) != 0
           && written == after.size();
}

bool write_bytes(std::uintptr_t address, std::span<const std::byte> bytes) noexcept {
    if (address < 0x10000 || bytes.empty()) { return false; }
    SIZE_T written{};
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), bytes.data(),
                              bytes.size(), &written) != 0
           && written == bytes.size();
}

bool drop_rolls() noexcept {
    if (forest_loot::kCandyDropPercent >= 100) { return true; }
    if (forest_loot::kCandyDropPercent <= 0) { return false; }
    if (g_rng == 0) { g_rng = static_cast<std::uint32_t>(GetTickCount64()) | 1U; }
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return static_cast<std::int32_t>(g_rng % 100U) < forest_loot::kCandyDropPercent;
}

/** Writes one record and its tracked-source row; the server learns the payout through the registry. */
void inject(const DropRequest& drop) noexcept {
    gateway_native::Read read{g_image};
    // The item index is identity, not a replaceable visual. The resource callback
    // requests both item tags through F27510 even without the native inventory provider.
    // Publish even before residency: BE4490 leaves the handle unset and retries until ready.
    const std::uint16_t item = forest_loot::pickup_definition_index(drop.itemHash);
    if (item == UINT16_MAX) { skipped("unsupported_item", drop.entity); return; }
    const auto loot = reinterpret_cast<std::uintptr_t>(call_loot_state());
    if (loot < 0x10000) { skipped("no_loot_state", drop.entity); return; }
    const std::uintptr_t table = loot + kTableOffset;
    const auto component = reinterpret_cast<std::uintptr_t>(call_component());
    if (component < 0x10000) { skipped("no_component", drop.entity); return; }

    // Never merge, never evict a native record: a duplicate of a live record is refused; a free
    // record is one marked 0xFFFF or still zero-filled (nothing native writes this table here).
    std::size_t free = kRecordCount;
    std::array<std::byte, 2> freeMark{std::byte{0xFF}, std::byte{0xFF}};
    unsigned freeMarked{}, zeroHeads{}, others{}, awaitingTeardown{};
    for (std::size_t i = 0; i < kRecordCount; ++i) {
        std::array<std::byte, 0x28> head{};
        if (!read.copy(table + i * kRecordBytes, head)) { skipped("table_read", drop.entity); return; }
        std::uint16_t recordItem{}; std::uint8_t kind{}; std::int32_t recordTag{}; std::uint64_t recordHandle{};
        std::memcpy(&recordItem, head.data(), sizeof recordItem);
        std::memcpy(&kind, head.data() + 0x08, sizeof kind);
        std::memcpy(&recordTag, head.data() + 0x10, sizeof recordTag);
        std::memcpy(&recordHandle, head.data() + 0x18, sizeof recordHandle);
        bool zero = true;
        for (const std::byte b : head) { if (b != std::byte{}) { zero = false; break; } }
        if (recordItem == 0xFFFF || zero) {
            if (recordItem == 0xFFFF) { ++freeMarked; } else { ++zeroHeads; }
            std::uint64_t handlePair{};
            if (!read.value(component + i * 8, handlePair)) { skipped("handle_read", drop.entity); return; }
            if (drop_slot::reusable(zero, recordItem, handlePair)) {
                if (free == kRecordCount) { free = i; std::memcpy(freeMark.data(), head.data(), freeMark.size()); }
            } else { ++awaitingTeardown; }
            continue;
        }
        ++others;
        if (kind == forest_loot::kRewardSheetDropKind && recordTag == drop.tag && recordHandle == drop.handle) {
            skipped("duplicate", drop.entity); return;
        }
    }
    const char* slotSource = "free";
    if (free == kRecordCount) {
        // The walker must destroy the previous bauble and clear its handle before reuse.
        // Overwriting a live handle or dropping it from the ledger orphans that bauble.
        report("ev=forest_candy stage=skipped reason=table_full entity=%08X free_marked=%u zero=%u other=%u awaiting_teardown=%u",
               drop.entity, freeMarked, zeroHeads, others, awaitingTeardown);
        return;
    }

    std::int32_t sequence{};
    if (!call_next_sequence(reinterpret_cast<void*>(loot), sequence)) { skipped("sequence", drop.entity); return; }

    // 1. The tracked-source row, through the native push, so the spawner resolves the position.
    Row48 rowData{};
    rowData.tag = drop.tag; rowData.handle = drop.handle; rowData.position = drop.position; rowData.zone = drop.zone;
    if (!call_push_row(&rowData)) { skipped("push_row", drop.entity); return; }

    // 2. The record body, then its item index last so no reader sees a partial record.
    std::array<std::byte, kRecordBytes> record{};
    const std::uint8_t kind = static_cast<std::uint8_t>(forest_loot::kRewardSheetDropKind);
    const std::int32_t socketCount = 0;
    std::memcpy(record.data() + 0x04, &drop.quantity, sizeof drop.quantity);
    std::memcpy(record.data() + 0x08, &kind, sizeof kind);
    std::memcpy(record.data() + 0x10, &drop.tag, sizeof drop.tag);
    std::memcpy(record.data() + 0x18, &drop.handle, sizeof drop.handle);
    std::memcpy(record.data() + 0x20, &sequence, sizeof sequence);
    record[0x30] = std::byte{kBucketDefault};
    std::memcpy(record.data() + 0x34, &socketCount, sizeof socketCount);
    const std::uintptr_t slot = table + free * kRecordBytes;
    if (!replace(slot, freeMark, freeMark)) { skipped("slot_taken", drop.entity); return; }
    if (!write_bytes(slot + 2, std::span(record).subspan(2))) { skipped("record_write", drop.entity); return; }
    std::memcpy(record.data(), &item, sizeof item);
    if (!replace(slot, freeMark, std::span(record).first(2))) { skipped("publish", drop.entity); return; }

    // 3. Initialize a virgin zero-filled slot only; a reused slot was already cleared by
    // the native walker. Never overwrite a live spawn handle.
    if (freeMark[0] == std::byte{} && freeMark[1] == std::byte{}) {
        const std::uint64_t unset = UINT64_MAX;
        static_cast<void>(write_bytes(component + free * 8, std::as_bytes(std::span{&unset, 1})));
    }
    static_cast<void>(write_bytes(component + (free + kFallbackRowBase) * sizeof(Row48),
                                  std::as_bytes(std::span{&rowData, 1})));
    const bool dirty = call_mark_dirty();

    forest_loot::remember_injected_drop(drop.tag, drop.handle, sequence, drop.itemHash, drop.quantity);
    AcquireSRWLockExclusive(&g_lock);
    bool stored = false;
    for (std::size_t i = 0; i < g_injectedCount; ++i) {
        if (g_injected[i].slot == free) { g_injected[i] = {drop.tag, drop.handle, sequence, free, item}; stored = true; break; }
    }
    if (!stored && g_injectedCount < g_injected.size()) { g_injected[g_injectedCount++] = {drop.tag, drop.handle, sequence, free, item}; }
    ReleaseSRWLockExclusive(&g_lock);
    g_injectedTotal.fetch_add(1, std::memory_order_relaxed);
    float position[3]{};
    std::memcpy(position, drop.position.data(), sizeof position);
    report("ev=forest_candy stage=injected origin=%s slot=%zu(%s) item=%u pay=%08X qty=%d kind=%u tag=%d handle=%016llX seq=%d "
           "zone=%d entity=%08X pos=%.2f,%.2f,%.2f dirty=%u total=%u free_marked=%u zero=%u other=%u",
           drop.origin, free, slotSource, static_cast<unsigned>(item), drop.itemHash, drop.quantity,
           static_cast<unsigned>(kind), drop.tag, static_cast<unsigned long long>(drop.handle), sequence, drop.zone,
           drop.entity, position[0], position[1], position[2], dirty ? 1U : 0U,
           g_injectedTotal.load(std::memory_order_relaxed), freeMarked, zeroHeads, others);
}

/** Remembers the damaged world object named by a dispatched record. */
void remember(std::uintptr_t source) noexcept {
    gateway_native::Read read{g_image};
    Seen seen{};
    if (!read.value(source + kSourceTagOffset, seen.tag) || !read.value(source + kSourceHandleOffset, seen.handle)
        || !read.value(source + kSourceEntityWeakOffset, seen.weak)) {
        return;
    }
    if (static_cast<std::uint32_t>(seen.tag) == kSyntheticSourceTag || seen.handle == 0 || seen.handle == UINT64_MAX
        || seen.weak.handle == UINT32_MAX) {
        return;
    }
    seen.entity = seen.weak.handle;
    std::uintptr_t row{};
    if (read.entity_row(seen.weak, row)) {
        alignas(16) float sampled[4]{};
        if (call_world_position(row, seen.entity, sampled)) {
            std::memcpy(seen.position.data(), sampled, sizeof sampled);
            seen.rowResolved = true;
        } else {
            seen.rowResolved = read.copy(row + kEntityRowPositionOffset, seen.position) && finite_position(seen.position);
        }
        static_cast<void>(read.value(row + kEntityRowZoneOffset, seen.zone));
    }
    AcquireSRWLockExclusive(&g_seenLock);
    for (Seen& existing : g_seen) {
        if (existing.entity == seen.entity) {
            const bool injected = existing.injected;
            existing = seen; existing.injected = injected;
            ReleaseSRWLockExclusive(&g_seenLock);
            return;
        }
    }
    g_seen[g_seenCursor] = seen;
    g_seenCursor = (g_seenCursor + 1U) % g_seen.size();
    ReleaseSRWLockExclusive(&g_seenLock);
}

/** Logs every dispatched kill record and remembers the ones that name a world object. */
void observe_dispatch(void* sink, void* record) noexcept {
    const auto sinkAddress = reinterpret_cast<std::uintptr_t>(sink);
    const auto recordAddress = reinterpret_cast<std::uintptr_t>(record);
    gateway_native::Read read{g_image};
    std::uintptr_t vtable{};
    std::int32_t count{}; std::uint8_t hasSource{}; std::int32_t sourceKind{}; std::uintptr_t source{};
    std::array<std::uint32_t, 2> entries{};
    if (sinkAddress < 0x10000 || recordAddress < 0x10000 || !read.value(sinkAddress, vtable)
        || !read.value(recordAddress + kRecordEntryCountOffset, count)
        || !read.value(recordAddress + kRecordHasSourceOffset, hasSource)
        || !read.value(recordAddress + kRecordSourceKindOffset, sourceKind)
        || !read.value(recordAddress + kRecordSourceObjectOffset, source)) {
        return;
    }
    if (hasSource == 0 || sourceKind != kSourceKindWorldObject || source < 0x10000) { return; }
    for (std::size_t i = 0; i < entries.size() && static_cast<std::int32_t>(i) < count; ++i) {
        static_cast<void>(read.value(recordAddress + kRecordEntriesOffset + i * 16, entries[i]));
    }
    const char* sinkName = vtable == g_image + kLootSinkVtableRva ? "loot"
                           : vtable == g_image + kPresentationSinkVtableRva ? "presentation" : "other";
    std::uint32_t entity{UINT32_MAX};
    static_cast<void>(read.value(source + kSourceEntityWeakOffset + 4, entity));
    report("ev=forest_candy stage=dispatch sink=%s count=%d source=%p entity=%08X entry0=%08X entry1=%08X",
           sinkName, count, reinterpret_cast<void*>(source), entity, entries[0], entries[1]);
    remember(source);
}

__declspec(noinline) std::uint64_t __fastcall dispatch_hook(void* sink, void* record, void* third,
                                                            void* fourth) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const std::uint64_t result = hooking::await_original(g_original)(sink, record, third, fourth);
    if (scope.accepts_side_effects()) { observe_dispatch(sink, record); }
    return result;
}

bool idle() noexcept { return g_gate.idle(); }

void* dispatch_target() noexcept {
    return prefix_matches(kDispatchRva, kDispatchPrefix) ? reinterpret_cast<void*>(g_image + kDispatchRva) : nullptr;
}

/** Frees the records this hook wrote so a quiesced run leaves no bauble the server would pay for. */
void retire_injected() noexcept {
    const auto loot = reinterpret_cast<std::uintptr_t>(call_loot_state());
    if (loot < 0x10000) { return; }
    AcquireSRWLockExclusive(&g_lock);
    gateway_native::Read read{g_image};
    for (std::size_t i = 0; i < g_injectedCount; ++i) {
        const Injected& entry = g_injected[i];
        const std::uintptr_t slot = loot + kTableOffset + entry.slot * kRecordBytes;
        std::array<std::byte, 0x28> head{};
        if (!read.copy(slot, head)) { continue; }
        std::uint16_t item{}; std::int32_t tag{}; std::uint64_t handle{}; std::int32_t sequence{};
        std::memcpy(&item, head.data(), sizeof item); std::memcpy(&tag, head.data() + 0x10, sizeof tag);
        std::memcpy(&handle, head.data() + 0x18, sizeof handle); std::memcpy(&sequence, head.data() + 0x20, sizeof sequence);
        if (item != entry.item || tag != entry.tag || handle != entry.handle || sequence != entry.sequence) { continue; }
        const std::array<std::byte, 2> freeMark{std::byte{0xFF}, std::byte{0xFF}};
        static_cast<void>(replace(slot, std::span(head).first(2), freeMark));
    }
    g_injectedCount = 0;
    ReleaseSRWLockExclusive(&g_lock);
}

/** Once a second: did the walker consume the dirty flag, and did the last record get a spawn handle? */
void probe_walker() noexcept {
    const auto now = GetTickCount64();
    if (now - g_lastProbeTick < 1000U) { return; }
    g_lastProbeTick = now;
    Injected last{};
    bool have = false;
    AcquireSRWLockExclusive(&g_lock);
    if (g_injectedCount > 0) { last = g_injected[g_injectedCount - 1]; have = true; }
    ReleaseSRWLockExclusive(&g_lock);
    if (!have) { return; }
    const auto component = reinterpret_cast<std::uintptr_t>(call_component());
    if (component < 0x10000) { return; }
    gateway_native::Read read{g_image};
    std::uint8_t dirty{}; std::uint64_t pair{};
    if (!read.value(component + kComponentDirtyOffset, dirty) || !read.value(component + last.slot * 8, pair)) { return; }
    if (dirty == g_lastDirty && pair == g_lastHandlePair) { return; }
    g_lastDirty = dirty; g_lastHandlePair = pair;
    report("ev=forest_candy stage=walker dirty=%u last_slot=%zu handle=%08X:%08X", static_cast<unsigned>(dirty), last.slot,
           static_cast<std::uint32_t>(pair & 0xFFFFFFFFU), static_cast<std::uint32_t>(pair >> 32));
}

/** Frees the record of a paid pickup so the walker's teardown removes the bauble. */
void drain_retires() noexcept {
    for (int n = 0; n < 16; ++n) {
        forest_loot::RetireRequest request{};
        // The client's reply handler must find the record; free it only after the reply had time.
        if (!forest_loot::take_retire(request, 2000U)) { return; }
        Injected entry{};
        bool found = false;
        AcquireSRWLockExclusive(&g_lock);
        for (std::size_t i = 0; i < g_injectedCount; ++i) {
            const Injected& candidate = g_injected[i];
            if (candidate.tag == request.sourceTag && candidate.handle == request.sourceHandle
                && candidate.sequence == request.sequence) {
                entry = candidate; found = true;
                g_injected[i] = g_injected[--g_injectedCount];
                break;
            }
        }
        ReleaseSRWLockExclusive(&g_lock);
        if (!found) {
            report("ev=forest_candy stage=retire result=unknown tag=%d seq=%d", request.sourceTag, request.sequence);
            continue;
        }
        const auto loot = reinterpret_cast<std::uintptr_t>(call_loot_state());
        if (loot < 0x10000) { report("ev=forest_candy stage=retire result=no_loot_state slot=%zu", entry.slot); continue; }
        const std::uintptr_t slot = loot + kTableOffset + entry.slot * kRecordBytes;
        gateway_native::Read read{g_image};
        std::array<std::byte, 0x28> head{};
        std::uint16_t item{}; std::int32_t tag{}; std::uint64_t handle{}; std::int32_t sequence{};
        if (!read.copy(slot, head)) { continue; }
        std::memcpy(&item, head.data(), sizeof item); std::memcpy(&tag, head.data() + 0x10, sizeof tag);
        std::memcpy(&handle, head.data() + 0x18, sizeof handle); std::memcpy(&sequence, head.data() + 0x20, sizeof sequence);
        if (item != entry.item || tag != entry.tag || handle != entry.handle || sequence != entry.sequence) {
            report("ev=forest_candy stage=retire result=mismatch slot=%zu", entry.slot); continue;
        }
        const std::array<std::byte, 2> freeMark{std::byte{0xFF}, std::byte{0xFF}};
        const bool freed = replace(slot, std::span(head).first(2), freeMark);
        const bool dirty = freed && call_mark_dirty();
        report("ev=forest_candy stage=retire result=%s slot=%zu seq=%d dirty=%u", freed ? "freed" : "write_failed",
               entry.slot, entry.sequence, dirty ? 1U : 0U);
    }
}

/** Drains server-requested drops (chest coins) into records at the requested world position. */
void drain_chest_drops() noexcept {
    static std::optional<forest_loot::ChestDrop> pending{};
    for (int n = 0; n < 8; ++n) {
        if (!pending) {
            forest_loot::ChestDrop next{};
            if (!forest_loot::take_chest_drop(next)) { return; }
            pending=next;
        }
        const auto& chest=*pending;
        if (!state::activity::contains(chest.owner)
            || state::activity::newest_joined_activity()!=chest.owner) {
            pending.reset();continue;
        }
        gateway_native::Read read{g_image};
        // Shared read-only pose probe; no animation setter, delay, or new hook.
        const native_local_placement_probe::Request lid{
            0x34D23982U,55,0x815500EEU,0x4C8,chest.chestGeneration,true};
        native_placement_pose_probe::Trace pose{};
        const bool ready=native_placement_pose_probe::complete(read,lid,1.F,2,&pose);
        static unsigned lastStage=UINT32_MAX;
        if(lastStage!=pose.stage) {
            lastStage=pose.stage;
            report("ev=forest_chest stage=lid_probe gate=%u actual=%.3f target=%.3f revision=%d",
                pose.stage,pose.actual,pose.target,pose.revision);
        }
        if (!ready) { return; }
        std::int32_t zone{};
        if (!call_current_zone(zone)) { return; }
        const std::uint32_t ordinal = ++g_synthetic;
        DropRequest drop{};
        drop.tag = static_cast<std::int32_t>(kSyntheticTagBase + ordinal);
        drop.handle = kSyntheticHandleBase | ordinal;
        const float spread[4] = {chest.x + 0.35F * static_cast<float>(ordinal % 5U) - 0.7F,
                                 chest.y + 0.35F * static_cast<float>((ordinal / 5U) % 3U) - 0.35F,
                                 chest.z + 0.4F, 1.F};
        std::memcpy(drop.position.data(), spread, sizeof spread);
        drop.zone = zone;
        drop.entity = 0;
        drop.itemHash = chest.itemDefinitionHash;
        drop.quantity = chest.quantity;
        drop.origin = "chest";
        inject(drop);
        report("ev=forest_chest stage=lid_open item=%08X quantity=%d",chest.itemDefinitionHash,chest.quantity);
        pending.reset();
    }
}
} // namespace

void observe_death(std::uint32_t entity, std::uintptr_t characterAddress) noexcept {
    if (!g_handles[0].attached || !g_gate.accepting() || entity == UINT32_MAX) { return; }
    if (GetCurrentThreadId() != g_gameThread.load(std::memory_order_acquire)) { skipped("thread", entity); return; }
    if (!forest_loot::candy_drop_armed()) { skipped("not_armed", entity); return; }
    Seen seen{};
    bool found = false;
    AcquireSRWLockExclusive(&g_seenLock);
    for (Seen& existing : g_seen) {
        if (existing.entity != entity) { continue; }
        found = true;
        if (existing.injected) { ReleaseSRWLockExclusive(&g_seenLock); skipped("already_dropped", entity); return; }
        existing.injected = true;
        seen = existing;
        break;
    }
    ReleaseSRWLockExclusive(&g_seenLock);
    if (!found) {
        report("ev=forest_candy stage=skipped reason=no_dispatch entity=%08X character=%p", entity,
               reinterpret_cast<void*>(characterAddress));
        return;
    }
    // Position: the live world transform through the native getter, else the hit-time snapshot, else
    // the record's raw field if finite. Zone: the game's own current zone, which is what the bauble
    // walker compares the row against; the corpse is in it by construction.
    gateway_native::Read read{g_image};
    std::uintptr_t row{};
    const char* positionSource = "none";
    alignas(16) float sampled[4]{};
    std::array<std::byte, 16> raw{};
    const bool rowOk = read.entity_row(seen.weak, row);
    if (rowOk && call_world_position(row, entity, sampled)) {
        std::memcpy(seen.position.data(), sampled, sizeof sampled); positionSource = "getter";
    } else if (seen.rowResolved && finite_position(seen.position)) {
        positionSource = "hit";
    } else if (rowOk && read.copy(row + kEntityRowPositionOffset, raw) && finite_position(raw)) {
        seen.position = raw; positionSource = "raw";
    } else {
        report("ev=forest_candy stage=skipped reason=no_position entity=%08X row=%u", entity, rowOk ? 1U : 0U);
        return;
    }
    const char* zoneSource = "getter";
    std::int32_t currentZone{};
    if (call_current_zone(currentZone)) { seen.zone = currentZone; }
    else if (seen.zone != 0) { zoneSource = "hit"; }
    else if (rowOk && read.value(row + kEntityRowZoneOffset, currentZone) && currentZone != 0) { seen.zone = currentZone; zoneSource = "raw"; }
    else { zoneSource = "unknown"; }
    report("ev=forest_candy stage=corpse entity=%08X pos_src=%s zone_src=%s zone=%d", entity, positionSource,
           zoneSource, seen.zone);
    if (!drop_rolls()) { skipped("chance", entity); return; }
    DropRequest drop{};
    drop.tag = seen.tag; drop.handle = seen.handle; drop.position = seen.position; drop.zone = seen.zone;
    drop.entity = entity; drop.itemHash = forest_loot::kCandyDefinitionHash; drop.quantity = forest_loot::kCandyPerPickup;
    drop.origin = "death";
    inject(drop);
}

bool install() noexcept {
    if (g_handles[0].attached) { return g_gate.accepting(); }
    g_image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (g_image == 0) { return false; }
    const std::array<hooking::detour::Spec, 2> specs{{
        {dispatch_target(), reinterpret_cast<void*>(&dispatch_hook)},
        {reinterpret_cast<void*>(g_image + kInventoryResourcesRva), reinterpret_cast<void*>(&inventory_resources_hook)},
    }};
    constexpr std::array<std::uint8_t, 16> resourcePrefix{
        0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0xB9,0x04,0x00,0x00,0x00,0x48};
    constexpr std::array<std::uint8_t, 16> itemResourcePrefix{
        0x4C,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x55,0x57,0x48,0x83,0xEC,0x48};
    const bool anchors = prefix_matches(kLootStateRva, kLootStatePrefix) && prefix_matches(kSequenceRva, kSequencePrefix)
                         && prefix_matches(kComponentRva, kComponentPrefix) && prefix_matches(kPushRowRva, kPushRowPrefix)
                         && prefix_matches(kMarkDirtyRva, kMarkDirtyPrefix)
                         && prefix_matches(kInventoryResourcesRva, resourcePrefix)
                         && prefix_matches(kItemResourcesRva, itemResourcePrefix);
    if (specs[0].target == nullptr || !anchors || !hooking::detour::install(specs, g_handles)) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                         "ev=forest_candy stage=install result=fail dispatch=4AA1C0 loot_state=BE1CB0 sequence=523880 component=FCA3F0 push=BE2820 dirty=BE2850");
        return false;
    }
    hooking::publish_original(g_original, reinterpret_cast<Dispatch>(g_handles[0].original));
    hooking::publish_original(g_resourcesOriginal, reinterpret_cast<InventoryResources>(g_handles[1].original));
    g_gate.accept();
    report("ev=forest_candy stage=install result=ok dispatch=4AA1C0 witness=death item=%u qty=%d kind=%u percent=%d resources=native_inventory_callback mutation=client_pending_drop_record",
           static_cast<unsigned>(forest_loot::kCandyDefinitionIndex), forest_loot::kCandyPerPickup,
           static_cast<unsigned>(forest_loot::kRewardSheetDropKind), forest_loot::kCandyDropPercent);
    return true;
}

void quiesce() noexcept { g_gate.quiesce(); }

bool uninstall() noexcept {
    quiesce();
    if (!g_handles[0].attached) { return true; }
    retire_injected();
    const std::array<hooking::detour::ProtectedCodeEntry, 11> protectedCode{{
        {reinterpret_cast<void*>(&dispatch_hook)}, {reinterpret_cast<void*>(&observe_dispatch)},
        {reinterpret_cast<void*>(&observe_death)}, {reinterpret_cast<void*>(&inject)},
        {reinterpret_cast<void*>(&drain_chest_drops)}, {reinterpret_cast<void*>(&drain_retires)},
        {reinterpret_cast<void*>(&inventory_resources_hook)},
        {reinterpret_cast<void*>(&request_item_resources)}, {reinterpret_cast<void*>(&begin_item_resources)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    }};
    if (hooking::detour::uninstall(g_handles, protectedCode, idle) != hooking::detour::UninstallResult::removed) {
        return false;
    }
    g_original.store(nullptr, std::memory_order_release);
    g_resourcesOriginal.store(nullptr, std::memory_order_release);
    g_image = 0;
    return true;
}

void poll() noexcept {
    g_gameThread.store(GetCurrentThreadId(), std::memory_order_release);
    if (!g_handles[0].attached || !g_gate.accepting()) { return; }
    probe_walker();
    drain_retires();
    drain_chest_drops();
}
} // namespace dawn::client::hooks::bootflow::forest_candy_drops
