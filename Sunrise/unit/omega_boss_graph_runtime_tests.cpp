// Exercises the actual production callback implementation, including its datum
// resolver and owner chain. Native actor lookup/queue/request dispatch are modeled;
// Original lookup/constructor/event/scalar/frame routines execute unchanged in
// private memory. Distinct AI actor and world entity domains are mandatory.
#include <Windows.h>
#include "client/hooks/bootflow/omega_native_readable.h"
#include "state/activity/omega/omega_mission_devices.h"
#include "state/activity/omega/omega_transit_authority.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include "client/hooking/call_gate.h"
#include "client/hooks/bootflow/omega_boss_graph.h"
#include "client/hooks/bootflow/omega_boss_graph_observation.h"
#include "client/hooks/bootflow/omega_boss_spawn.h"
#include "client/hooks/bootflow/omega_boss_combat_start.h"
#include "client/hooks/graphics/omega_frame_timing.h"
#include "state/activity/omega/omega_lair_start.h"
#include "state/activity/omega/omega_mission_runtime.h"
#include "state/activity/omega/omega_ending_runtime.h"
#include "client/hooks/bootflow/omega_mission_characters.h"
#include "client/hooks/bootflow/omega_mission_motion.h"
#include "client/hooks/bootflow/omega_mission_crown.h"
#include "client/hooks/bootflow/omega_cannon_delivery.h"

namespace sunrise::state::activity {
std::uint64_t mission_run_generation() noexcept { return 7; }
namespace omega {
Progress fixtureProgress = [] { Progress value{}; value.route = Route::crownEntrance;
    value.bossDoorReached = true; value.loadedBubble = 14; return value; }();
Progress presentation_progress(std::uint64_t) noexcept { return fixtureProgress; }
namespace lair_start {
unsigned starts{};
State receiptState;
void note_left_started(std::uint64_t run, std::uint32_t generation, std::uint32_t actor,
                       std::uint32_t character, std::uint32_t biped) noexcept {
    if (run != 7 || generation != 8 || actor != 0x53F4200E || character != 0x76F9EA22 || biped != 0x2DF9ECFD)
        std::abort();
    if (receiptState.left_started({run, generation, actor, character, biped})) ++starts;
}
}
}
}
namespace sunrise::client::hooks::bootflow::fixture {
namespace omega = state::activity::omega;
namespace graph = omega_boss_graph;
namespace observation = omega_boss_graph_observation;
namespace combat = omega_boss_combat_start;
namespace frame_timing = graphics::omega_frame_timing;
namespace motion=omega_mission_motion;
namespace crown=omega_mission_crown;
void observe_mission_health(const hooking::CallGate::Scope&) noexcept;
void observe_mission_boss_death(std::byte*,std::uint32_t) noexcept;
void pump_mission_motion(const hooking::CallGate::Scope&) noexcept;
void pump_mission_crown(const hooking::CallGate::Scope&) noexcept;
void observe_mission_crown(std::span<const std::byte>,bool,const hooking::CallGate::Scope&) noexcept;
// Read-only eye telemetry has its own production-helper fixture. It neither
// starts nor gates the animation/condition pipeline exercised here.
void observe_eye_inputs(const graph::Owner&, const observation::Snapshot&) noexcept {}
unsigned checks{}, nativeQueues{}, nativeAdds{}, nativeRemoves{}, memberCallbacks{};
void check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
using Tick = void(__fastcall*)(std::byte*) noexcept;
using CharacterInitialize = void(__fastcall*)(std::byte*, const std::uint32_t*) noexcept;
using GraphUpdate = bool(__fastcall*)(float, const void*, std::byte*, bool*) noexcept;
using FullBodyUpdate = bool(__fastcall*)(std::byte*, const void*, float, std::uint8_t, const void*, void*) noexcept;
void tick_original(std::byte*) noexcept { ++memberCallbacks; }
void cinematic_original(std::byte*) noexcept {}
void observe_tick(bool, std::byte*, bool, bool) noexcept {}
void observe_mission_character(std::byte*,const std::uint32_t*) noexcept;
void character_original(std::byte*, const std::uint32_t*) noexcept {}
bool graph_original(float, const void*, std::byte*, bool*) noexcept { return true; }
bool fullbody_original(std::byte*, const void*, float, std::uint8_t, const void*, void*) noexcept { return true; }
std::atomic<Tick> memberOriginal{tick_original};
std::atomic<Tick> cinematicOriginal{cinematic_original};
unsigned motionCallbacks{},cleanupCallbacks{};
bool motion_original(void*,const void*,std::byte* raw) noexcept {++motionCallbacks;auto stage=std::to_integer<std::uint8_t>(raw[0xA4]);if(stage<3)raw[0xA4]=static_cast<std::byte>(stage+1);return false;}
void cleanup_original(void*,std::byte*,void*) noexcept {++cleanupCallbacks;}
using MotionUpdate=bool(__fastcall*)(void*,const void*,std::byte*) noexcept;
using MotionCleanup=void(__fastcall*)(void*,std::byte*,void*) noexcept;
std::atomic<MotionUpdate> motionUpdateOriginal{motion_original};
std::atomic<MotionCleanup> motionCleanupOriginal{cleanup_original};

std::atomic<CharacterInitialize> characterOriginal{character_original};
using CharacterDeath=bool(__fastcall*)(std::byte*,std::uint32_t) noexcept;
unsigned deathCallbacks{};
bool death_original(std::byte*,std::uint32_t) noexcept {++deathCallbacks;return true;}
std::atomic<CharacterDeath> deathOriginal{death_original};
std::atomic<GraphUpdate> graphOriginal{graph_original};
std::atomic<FullBodyUpdate> fullBodyOriginal{fullbody_original};
std::atomic<std::uint64_t> observedCharacter{UINT64_MAX};
hooking::CallGate callGate;
std::byte* image{};
std::mutex mutex;
struct RunState {
    std::uint64_t run{UINT64_MAX};
    graph::Owner graphOwner{};
    graph::EventLease summonLease{};
    bool queueClaimed{}, queueAccepted{}, graphRetired{}, flightSeen{}, bossSeen{}, summonSeen{}, introIdleSeen{};
    bool cinematicStarted{}, cinematicFinished{}, cinematicObserved{}, cinematicMissingLogged{};
    unsigned cinematicAttempts{};
    std::uint64_t nextCinematic{}, cinematicCompletedAt{};
    observation::Phase graphPhase{};
    unsigned memberWaits{}, graphRejectSamples{};
    std::uint64_t ownerWaits{};
    motion::Track departure{};crown::Track crown{};crown::HealthTrack health{};
    combat::Track left{};
    omega::mission::Token armToken{};
    std::array<omega_cannon_delivery::Reference,omega::rescue::sources.size()> rescueReferences{};
} runState;
std::vector<std::string> receipts;
template<class... A> void log(const char* format, A... args) noexcept {
    std::array<char, 1024> buffer{};
    std::snprintf(buffer.data(), buffer.size(), format, args...);
    receipts.emplace_back(buffer.data());
}
template<class T> T read(const std::byte* bytes, std::size_t offset) noexcept {
    T result{}; std::memcpy(&result, bytes + offset, sizeof result); return result;
}
template<class T> void put(std::byte* bytes, std::size_t offset, T value) noexcept {
    std::memcpy(bytes + offset, &value, sizeof value);
}
bool readable(const void* pointer, std::size_t size) noexcept {
    return omega_native_memory::readable(pointer, size);
}
bool active() noexcept { return true; }
void reset(std::uint64_t run) noexcept {
    if (runState.run != run) { runState = {}; runState.run = run; }
}
constexpr std::uint32_t memberHandle = 0x37F92001, actorHandle = 0x53F4200E;
constexpr std::uint32_t parentHandle = 0x14F9EA25, characterHandle = 0x76F9EA22;
constexpr std::uint32_t animationHandle = 0x05F3A00E, bipedHandle = 0x2DF9ECFD;
constexpr std::uint32_t fullbodyHandle = 0x12F9EAB3, scalarHandle = 0x21F9EAB4, entityHandle = 0x045CE005;
constexpr std::uint32_t bipedType = 0x808036CF, graphMetadataType = 0x80801234;
std::byte* worldRows{};
std::uint32_t fixtureWorldEntity = entityHandle;
std::array<std::byte, 0x108> fixtureAuth{};
std::byte *characterBody{}, *animationBody{}, *memberBody{}, *fixtureBiped{};
std::byte *fullbodyBody{}, *scalarBody{};
std::byte* ordinaryScalarBody{};
bool ordinaryScalarLookup{};
std::int32_t ordinaryScalarIndex = 1;
unsigned scalarWrites{};
bool scalarSetAccepted = true;
bool scalar_find(std::uint32_t entity, const std::uint32_t* hash, std::byte** body, std::int32_t* index) noexcept {
    check(entity == entityHandle && (*hash == combat::kLeftProperty || *hash == combat::kRightProperty), "ordinary lookup uses entity and left export name");
    if (ordinaryScalarLookup) { *body = ordinaryScalarBody; *index = ordinaryScalarIndex; return true; }
    return false; // actual authored left variable is the bound fallback
}
bool scalar_bound_find(std::uint32_t entity, const std::uint32_t* hash, std::uint32_t* self, std::int32_t* index) noexcept {
    check(entity == entityHandle && (*hash == combat::kLeftProperty || *hash == combat::kRightProperty), "bound lookup uses full entity and exact export");
    *self = scalarHandle; *index = *hash==combat::kRightProperty?30:31; return true;
}
bool scalar_set(std::uint32_t entity, const std::uint32_t* hash, const void* value) noexcept {
    check(entity == entityHandle && (*hash == combat::kLeftProperty || *hash == combat::kRightProperty), "scalar setter uses entity not character or actor");
    check((reinterpret_cast<std::uintptr_t>(value) & 15U) == 0, "native setter receives aligned float4");
    ++scalarWrites;
    if (!scalarSetAccepted) return false;
    auto* row = scalarBody + 0x58 + read<std::ptrdiff_t>(scalarBody, 0x58) + 0x10 + (*hash==combat::kRightProperty?30:31) * 16;
    std::memcpy(row, value, 16); return true;
}
std::vector<std::pair<const std::byte*,std::uint32_t>> fixtureDeviceMembers;
std::uint32_t* member_self(const std::byte* component, std::uint32_t* out) noexcept {
    for(const auto& pair:fixtureDeviceMembers) if(pair.first==component) {*out=pair.second;return out;}
    *out = memberHandle; return out;
}
void portal_visual_apply(std::byte*,const std::byte*) noexcept;
const std::byte* cannon_authority(std::byte* object,std::uint32_t) noexcept {return read<const std::byte*>(object,0x40);}
void observe_mission_device(std::byte*) noexcept;
const std::byte* member_auth(const std::byte*) noexcept { return fixtureAuth.data(); }
void parent_context(void* out, std::uint32_t actor) noexcept {
    put(static_cast<std::byte*>(out), 0, parentHandle);
    put(static_cast<std::byte*>(out), 4, actor);
}
// Only native component enumeration is modeled. DCBF30 itself performs the
// original entity-index lookup and generic component-handle resolution.
bool find_entity_component(std::byte* row, std::uint32_t type, std::byte* out, bool) noexcept {
    if (row != worldRows + (fixtureWorldEntity & 0x1FFF) * 0x100 || type != bipedType) return false;
    put(out, 0x18, bipedHandle); put(out, 0x20, std::int64_t{0}); return true;
}
const std::uint64_t fixtureSeed = 0x12345678;
std::uint64_t* graph_seed(std::uint64_t* out) noexcept { *out = fixtureSeed; return out; }
std::uint32_t* unrelated_component(std::uint32_t* out, std::uint32_t) noexcept { *out = UINT32_MAX; return out; }
bool missionNativeFixture{};
float fixtureEye=1.F,fixtureBody=1.F;
float fraction_get(std::byte*,std::int32_t region) noexcept {return region?fixtureEye:fixtureBody;}
const std::byte* named_find(const std::byte* config,const std::uint32_t* hash) noexcept {
    for(unsigned i=0;i<7;++i) if(read<std::uint32_t>(config,0x60+i*24)==*hash) return config+0x60+i*24;
    return nullptr;
}
bool named_start(std::byte* channel,const std::byte* entry,bool) noexcept {
    const auto hash=read<std::uint32_t>(entry,0);
    for(unsigned i=0;i<8;++i) if(read<std::uint32_t>(channel,0x20+i*0x108)==0x811C9DC5) {
        put(channel,0x20+i*0x108,hash);put(channel,0x120+i*0x108,std::uint8_t{1});return true;
    }
    return false;
}

std::byte* endingDirectory{};
void* cinema_lookup(const std::uint32_t* selector) noexcept { return *selector==0x2FECC6FD?endingDirectory:image; }
bool cinema_start(std::byte* component) noexcept { put(component, 0x260, std::uint8_t{1}); return true; }
unsigned vfxAttempts{};
bool invalidateVfxOwner{};
bool prepare_intro_vfx(const graph::Owner& owner, const hooking::CallGate::Scope& call) noexcept {
    ++vfxAttempts;
    check(call.accepts_side_effects() && owner.actor == actorHandle && nativeQueues == 0,
          "VFX candidate attempted on validated owner before intro queue");
    check(mutex.try_lock(), "VFX native call is outside reveal mutex"); mutex.unlock();
    if (invalidateVfxOwner) put(animationBody, 4, std::uint32_t{0x80F45197});
    return false; // exercise unavailable VFX without suppressing the working intro
}
void queue_submit(std::byte* member, const void* queue, std::int32_t head) noexcept {
    ++nativeQueues; std::memcpy(member + 0x230, queue, 0x808); put(member, 0x228, head);
}
void condition_add(std::byte* character, const void* request, const void*) noexcept {
    check(mutex.try_lock(), "native condition call is outside the reveal mutex"); mutex.unlock();
    check(character == characterBody, "condition receives real character, not parent");
    if(missionNativeFixture) {
        auto* bytes=static_cast<const std::byte*>(request);
        const auto event=read<std::uint32_t>(bytes,8),sequence=read<std::uint32_t>(bytes,4);
        for(const auto& cycle:crown::cycles) if(cycle.sequence==sequence) {
            using Event=void(__fastcall*)(std::byte*,std::int32_t,std::int32_t,const std::uint32_t*) noexcept;
            reinterpret_cast<Event>(image+0xC66110)(animationBody+0x34,0,cycle.ordinal,&event);++nativeAdds;return;
        }
        check(false,"unknown Crown sequence");
    }
    const auto expected = graph::make_condition_request();
    check(std::memcmp(request, expected.data(), expected.size()) == 0, "exact opcode5E request");
    using Event = void(__fastcall*)(std::byte*, std::int32_t, std::int32_t, const std::uint32_t*) noexcept;
    reinterpret_cast<Event>(image + 0xC66110)(animationBody + 0x34, 0, 1, &graph::kSummonEvent);
    ++nativeAdds;
}
void condition_remove(std::byte* character, const void* request, const void*) noexcept {
    check(mutex.try_lock(), "native condition removal is outside the reveal mutex"); mutex.unlock();
    check(character == characterBody, "condition removal receives character");
    if(missionNativeFixture) {
        auto* bytes=static_cast<const std::byte*>(request);
        if(bytes[0x60]==std::byte{0x5D}) {put(memberBody,0x228,std::int32_t{1});return;}
        const auto event=read<std::uint32_t>(bytes,8),sequence=read<std::uint32_t>(bytes,4);
        for(const auto& cycle:crown::cycles) if(cycle.sequence==sequence) {
            using Event=void(__fastcall*)(std::byte*,std::int32_t,std::int32_t,const std::uint32_t*) noexcept;
            reinterpret_cast<Event>(image+0xC717E0)(animationBody+0x34,0,cycle.ordinal,&event);++nativeRemoves;return;
        }
        check(false,"unknown Crown remove sequence");
    }
    using Event = void(__fastcall*)(std::byte*, std::int32_t, std::int32_t, const std::uint32_t*) noexcept;
    reinterpret_cast<Event>(image + 0xC717E0)(animationBody + 0x34, 0, 1, &graph::kSummonEvent);
    ++nativeRemoves;
}
std::uint32_t sourceSense=8;
const std::byte* source_sense(std::byte*) noexcept {return reinterpret_cast<const std::byte*>(&sourceSense);}
void source_weak(const void* weak,std::uint32_t* actor) noexcept {
    const auto* bytes=static_cast<const std::byte*>(weak);
    *actor=read<std::uint32_t>(bytes,0)==0xABCDU?read<std::uint32_t>(bytes,4):UINT32_MAX;
}
template<class Fn> Fn native(std::uintptr_t rva) noexcept {
    void* fn{};
    switch (rva) {
    case 0xA89430: fn=reinterpret_cast<void*>(&named_find);break;
    case 0xC66590: fn=reinterpret_cast<void*>(&named_start);break;
    case 0xCD6C20: fn=reinterpret_cast<void*>(&fraction_get);break;
    case 0x4E2630: fn=reinterpret_cast<void*>(&source_sense);break;
    case 0x352310: fn=reinterpret_cast<void*>(&source_weak);break;
    case 0x4E5C60: fn = reinterpret_cast<void*>(&member_self); break;
    case 0xAB27C0: fn = reinterpret_cast<void*>(&member_auth); break;
    case 0xA8CB20: fn = reinterpret_cast<void*>(&parent_context); break;
    case 0x558330: fn=image+rva;break; // original encoded world-position getter
    case 0xDCBF30: fn = image + rva; break; // original world-entity biped lookup
    case 0xC4C1A0: fn = reinterpret_cast<void*>(&cinema_lookup); break;
    case 0x1069CC0: fn = reinterpret_cast<void*>(&cinema_start); break;
    case 0xAB6C60: fn = reinterpret_cast<void*>(&queue_submit); break;
    case 0xC620F0: fn = reinterpret_cast<void*>(&condition_add); break;
    case 0xC693F0: fn = reinterpret_cast<void*>(&condition_remove); break;
    case 0xA889E0: fn = image + rva; break; // unchanged original lookup
    case 0x583DF0: fn = reinterpret_cast<void*>(&scalar_find); break;
    case 0x583C70: fn = reinterpret_cast<void*>(&scalar_bound_find); break;
    case 0x576420: fn = reinterpret_cast<void*>(&scalar_set); break;
    case 0x57A0A0: case 0x57A770: fn = image + rva; break; // unchanged original scalar getters
    case 0x9FEC30: fn=reinterpret_cast<void*>(&cannon_authority); break;
    case 0x9F19F0: fn=reinterpret_cast<void*>(&portal_visual_apply); break;
    case 0x10699C0: fn=image+rva; break;
    case 0xB41330: fn=image+rva; break;
    default: check(false, "unexpected native entry");
    }
    return reinterpret_cast<Fn>(fn);
}
void observe_eye_clip_cursor(const void*,std::span<const std::byte>,const hooking::CallGate::Scope&) noexcept;
#include "client/hooks/bootflow/omega_boss_graph_runtime.inl"
#include "client/hooks/bootflow/omega_eye_clip_trace.inl"
#include "client/hooks/bootflow/omega_mission_receipts.inl"
bool mission_crown_queue(const graph::Owner&,const MemberView&,unsigned) noexcept;
#include "client/hooks/bootflow/omega_mission_motion.inl"
#include "client/hooks/bootflow/omega_mission_crown.inl"
#include "client/hooks/bootflow/omega_mission_health.inl"
#include "client/hooks/bootflow/omega_eye_resource_trace.inl"
#include "client/hooks/bootflow/omega_eye_execution_trace.inl"
#include "client/hooks/bootflow/omega_boss_combat_runtime.inl"
#include "client/hooks/bootflow/omega_mission_ending.inl"
#include "client/hooks/bootflow/omega_cinematic_runtime.inl"

std::vector<std::byte*> allocations;
std::byte* allocate(std::size_t size) {
    auto* result = static_cast<std::byte*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(result != nullptr, "private fixture allocation"); allocations.push_back(result); return result;
}
std::array<std::byte, 0x100000> directory{};
std::byte* tables = directory.data();
struct Pool { std::uint64_t bucket{}; std::byte* rows{}; };
std::vector<Pool> pools;
std::byte* bind(std::uint32_t handle, std::size_t size) {
    const auto shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(handle) >> 13);
    const auto bucket = ((std::uint64_t{shifted} | 0xFFC0000ULL) >> 18) & (shifted & 0xFFFFU);
    check(bucket < 16384, "native bucket in directory");
    std::byte* rows{};
    for (const auto& pool : pools) if (pool.bucket == bucket) rows = pool.rows;
    if (!rows) {
        rows = allocate(0x20000); pools.push_back({bucket, rows});
        put(tables + bucket * 0x40, 8, rows);
        put(tables + bucket * 0x40, 0x30, std::int32_t{16});
        put(tables + bucket * 0x40, 0x34, std::int32_t{-1});
    }
    auto* object = allocate(size);
    auto* row = rows + (handle & 0x1FFF) * 16;
    const auto relocation = reinterpret_cast<std::uintptr_t>(row) - reinterpret_cast<std::uintptr_t>(object);
    put(row, 8, relocation);
    check(resolve_handle(handle) == object, "production resolver matches allocated datum");
    return object;
}
void copy_file(const char* file, std::byte* output, std::size_t count, std::size_t offset = 0) {
    std::ifstream stream(file, std::ios::binary);
    check(static_cast<bool>(stream), "fixture evidence file available");
    stream.seekg(static_cast<std::streamoff>(offset));
    stream.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(count));
    check(stream.gcount() == static_cast<std::streamsize>(count), "complete evidence read");
}
void source(std::byte* body, std::uint32_t tag, std::uint32_t kind, std::int64_t offset) {
    put(body, 0, tag); put(body, 4, kind); put(body, 8, offset);
}
void fixture_thunk(std::uintptr_t rva, void* function) {
    image[rva] = std::byte{0x48}; image[rva + 1] = std::byte{0xB8};
    put(image, rva + 2, function); image[rva + 10] = std::byte{0xFF}; image[rva + 11] = std::byte{0xE0};
}
// Only the process-specific encoding keys are modeled. The getter executes
// the original decode/mask instructions against encoded world-row bytes.
std::uint32_t world_key_xy() noexcept {return 0x12345678;}
std::uint32_t world_key_zw() noexcept {return 0x76543210;}
void world_position(const motion::Point& position) {
    auto* row=worldRows+(entityHandle&0x1FFF)*0x100;
    put(row,4,std::uint32_t{0});put(row,0xC,entityHandle);
    for(unsigned i=0;i<4;++i) {
        std::uint32_t bits{};std::memcpy(&bits,&position[i],4);
        put(row,0xD0+4*i,bits^(i<2?world_key_xy():world_key_zw()));
    }
}
void setup_native_fullbody(const char* nativeImage);
void setup(const char* nativeImage, const char* configFile, const char* bankFile,
           const char* fullbodyFile, const char* scalarFile, const char* actionsFile, const char* evaluationFile) {
    image = allocate(0x6270000);
    put(image, 0x2439C70, &tables);
    copy_file(nativeImage,image+0x558330,0xBB,0x558330);
    copy_file(nativeImage,image+0xCDCB60,0x12,0xCDCB60);
    copy_file(nativeImage,image+0x1B9E420,0x20,0x1B9E420);
    copy_file(nativeImage, image + 0xF62F80, 0x45, 0xF62F80);
    copy_file(nativeImage, image + 0xA889E0, 0xCE, 0xA889E0);
    copy_file(nativeImage, image + 0xC66110, 0xA5, 0xC66110);
    copy_file(nativeImage, image + 0xC717E0, 0xD4, 0xC717E0);
    copy_file(nativeImage, image + 0x57A0A0, 0x4E, 0x57A0A0);
    copy_file(nativeImage, image + 0x57A770, 0x19, 0x57A770);
    copy_file(nativeImage, image + 0xF5D3C0, 0x14C, 0xF5D3C0);
    copy_file(nativeImage, image + 0xDCBF30, 0xB8, 0xDCBF30);
    copy_file(nativeImage, image + 0xF51CF0, 0x63, 0xF51CF0);
    copy_file(nativeImage, image + 0xF48F70, 0x16B, 0xF48F70);
    copy_file(nativeImage, image + 0xF46C60, 0x9F, 0xF46C60);
    copy_file(nativeImage, image + 0xF50AC0, 0x59, 0xF50AC0);
    copy_file(nativeImage, image + 0xF57250, 7, 0xF57250);
    copy_file(nativeImage, image + 0x10174F0, 0xC, 0x10174F0);
    copy_file(nativeImage, image + 0x4A61C0, 0x40, 0x4A61C0);
    copy_file(nativeImage, image + 0xAB6CE0, 0x134, 0xAB6CE0);
    fixture_thunk(0x557470, reinterpret_cast<void*>(&find_entity_component));
    fixture_thunk(0x35F320, reinterpret_cast<void*>(&graph_seed));
    for (auto rva : {0x464EC0U, 0xD653E0U, 0xC65500U})
        fixture_thunk(rva, reinterpret_cast<void*>(&unrelated_component));
    worldRows = allocate(0x200000);
    put(image, 0x1F93428, worldRows); put(image, 0x1F93430, std::uint32_t{0x100});
    put(image, 0x2098918, &bipedType); put(image, 0x206B9C0, &graphMetadataType);
    copy_file(nativeImage, image + 0x1BA2B80, 4, 0x1BA2B80);
    copy_file(nativeImage, image + 0x1BA64E4, 4, 0x1BA64E4);
    DWORD old{};
    check(VirtualProtect(image + 0xCDC000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original damage flag gate");
    check(VirtualProtect(image + 0xF62000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original resolver");
    check(VirtualProtect(image + 0xA88000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original lookup");
    check(VirtualProtect(image + 0xC66000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original condition add");
    check(VirtualProtect(image + 0xC71000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original condition remove");
    check(VirtualProtect(image + 0x57A000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original scalar getters");
    check(VirtualProtect(image + 0xF5D000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original weighted frame builder");
    for (auto rva : {0x558000U, 0xDCB000U, 0xF51000U, 0xF48000U, 0xF49000U, 0xF46000U, 0xF50000U,
                    0xF57000U, 0x1017000U, 0x4A6000U, 0x557000U, 0x35F000U, 0x464000U, 0xD65000U, 0xC65000U, 0xAB6000U})
        check(VirtualProtect(image + rva, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original owner constructors and bounded enum thunks");
    FlushInstructionCache(GetCurrentProcess(), image, 0x243A000);
    auto* memberDatum = bind(memberHandle, 0xC00); memberBody = memberDatum + 0x80;
    characterBody = bind(characterHandle, 0x1000); animationBody = bind(animationHandle, 0x3000);
    fixtureBiped = bind(bipedHandle, 0x900); (void)bind(parentHandle, 0x40);
    (void)bind(0x80F4517C, 0x180);
    auto* config = bind(0x80F4519A, 1225); copy_file(configFile, config, 1225);
    auto* bank = bind(0x80F45190, 0x440); copy_file(bankFile, bank, 0x440);
    auto* fullbodyResource = bind(combat::kFullBodyAsset, 6928); copy_file(fullbodyFile, fullbodyResource, 6928);
    fullbodyBody = bind(fullbodyHandle, 0x3000); scalarBody = bind(scalarHandle, 0x1000);
    auto* actions = bind(combat::kActionAsset, 0x9EC); copy_file(actionsFile, actions, 0x9EC);
    auto* evaluation = bind(0x80F45169, 0x40); copy_file(evaluationFile, evaluation, 0x40);
    ordinaryScalarBody = bind(0x22F9EAB5, 0x140);
    put(ordinaryScalarBody, 0x24, std::uint32_t{0x22F9EAB5}); put(ordinaryScalarBody, 0x2C, entityHandle);
    put(ordinaryScalarBody, 0x30, std::int32_t{2}); put(ordinaryScalarBody, 0x40, std::int32_t{2});
    put(ordinaryScalarBody, 0x38, std::uintptr_t{0x48}); put(ordinaryScalarBody, 0x48, std::uintptr_t{0x58});
    const ScalarValue ordinaryValue{{0.25F, 0.5F, 0.75F, 1.F}};
    std::memcpy(ordinaryScalarBody + 0xE0, &ordinaryValue, 16);
    std::memcpy(ordinaryScalarBody + 0x100, &ordinaryValue, 16);
    copy_file(scalarFile, scalarBody, 0xEC0, 0x4A0);
    put(scalarBody, 0x24, scalarHandle); put(scalarBody, 0x2C, entityHandle);
    source(fullbodyBody, combat::kFullBodyAsset, 0x80803640, 0x15B8);
    put(fullbodyBody, 0x24, fullbodyHandle); put(fullbodyBody, 0x2C, entityHandle);
    put(fullbodyBody, 0xB29, std::uint8_t{1}); put(fullbodyBody, 0xB20, 1.F);
    put(fullbodyBody, 0x500, std::int32_t{6}); put(fullbodyBody, 0x508, std::uintptr_t{0x1600 - 0x518});
    source(memberBody, 0x80F4756D, 0x80807D9D, 0xB58);
    put(memberBody, 0x180, std::uint32_t{8}); put(memberBody, 0x21C, actorHandle);
    put(memberBody, 0x220, memberHandle); put(memberBody, 0x224, actorHandle);
    graph::write(fixtureAuth, 0, std::uint32_t{8}); fixtureAuth[6] = std::byte{1};
    source(characterBody, 0x80F6690B, 0x80806832, 0x738);
    put(characterBody, 0x24, characterHandle); put(characterBody, 0xC0, actorHandle);
    put(characterBody, 0x2C, entityHandle);
    put(characterBody, 0x5C0, animationHandle);
    put(animationBody, 4, std::uint32_t{0x80F4519A}); put(animationBody, 0x30, characterHandle);
    put(animationBody, 0xC0, parentHandle); put(animationBody, 0xC4, actorHandle);
    source(fixtureBiped, 0x80F66907, 0x808036CF, 0x1B48);
    put(fixtureBiped, 0x24, bipedHandle); put(fixtureBiped, 0xD0, std::uint32_t{0x80F45190});
    put(fixtureBiped, 0x2C, entityHandle);
    put(fixtureBiped, 0x888, UINT32_MAX);
    setup_native_fullbody(nativeImage);
    callGate.accept();
}
std::array<std::byte, observation::kGraphBytes> snapshot(int node) {
    std::array<std::byte, 0x120> initialized{};
    using Initialize = bool(__fastcall*)(std::byte*, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::int32_t, std::int32_t, std::int32_t) noexcept;
    check(reinterpret_cast<Initialize>(image + 0xF48F70)(initialized.data(), entityHandle, bipedHandle,
          characterHandle, 0, 1, 0), "original F48F70 accepts independent world entity/biped/character");
    using InitializePlayback = void(__fastcall*)(std::byte*, std::uint32_t, std::uint32_t) noexcept;
    reinterpret_cast<InitializePlayback>(image + 0xF57250)(initialized.data() + 0x18, entityHandle, bipedHandle);
    std::array<std::byte, observation::kGraphBytes> result{};
    std::memcpy(result.data(), initialized.data(), result.size());
    constexpr std::array rows{2, 5, 1, 1, 13};
    constexpr std::array loops{true, false, true, true, false};
    result[0x20] = loops[node] ? std::byte{1} : std::byte{0}; result[0x21] = std::byte{1};
    graph::write(result, 0x28, UINT32_MAX); graph::write(result, 0x2C, UINT32_MAX);
    graph::write(result, 0x30, rows[node]); graph::write(result, 0x38, 10.F);
    graph::write(result, 0x3C, 0.2F); graph::write(result, 0xA4, node); graph::write(result, 0xB0, node);
    graph::write(result, 0xA8, std::int32_t{0}); graph::write(result, 0xB4, std::int32_t{0});
    return result;
}
void original_resolver_test() {
    // Force the row BELOW the object, reproducing the legal negative-relative
    // representation rejected by revision9 regardless of allocator addresses.
    auto* region = allocate(0x30000); auto* row = region + 0x10; auto* object = region + 0x21000;
    constexpr std::uint32_t handle = 0x80F44001; // resource bucket 7A2, index1
    const auto shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(handle) >> 13);
    const auto bucket = ((std::uint64_t{shifted} | 0xFFC0000ULL) >> 18) & (shifted & 0xFFFFU);
    std::array<std::byte, 0x40> saved{};
    std::memcpy(saved.data(), tables + bucket * 0x40, saved.size());
    put(tables + bucket * 0x40, 8, region); put(tables + bucket * 0x40, 0x30, std::int32_t{16});
    put(tables + bucket * 0x40, 0x34, std::int32_t{-1});
    const auto relocation = reinterpret_cast<std::uintptr_t>(row) - reinterpret_cast<std::uintptr_t>(object);
    put(row, 8, relocation);
    check(relocation > reinterpret_cast<std::uintptr_t>(row), "legacy unsigned guard rejects valid relocation");
    std::array<std::byte, 0x24> evaluator{}; graph::write(evaluator, 0x20, handle);
    using Getter = std::byte*(__fastcall*)(const std::byte*);
    check(reinterpret_cast<Getter>(image + 0xF62F80)(evaluator.data()) == object,
          "unchanged original F62F80 accepts negative relocation");
    check(resolve_handle(handle) == object, "production resolver agrees with original negative relocation");
    put(row, 8, std::uintptr_t{0});
    check(resolve_handle(handle) == row, "zero relocation retains inline datum");
    check(reinterpret_cast<Getter>(image + 0xF62F80)(evaluator.data()) == row, "original zero relocation agrees");
    std::memcpy(tables + bucket * 0x40, saved.data(), saved.size());
}
#include "omega_boss_fullbody_native_fixture.inl"
std::array<std::byte, combat::kFrameBytes> native_frame(std::uint32_t clip, float time,
                                                       float weight = 1.F, int layer = 1) {
    std::array<std::byte, combat::kFrameBytes> frame{};
    const std::array refs{clip, UINT32_MAX, UINT32_MAX};
    using Build = void(__fastcall*)(std::byte*, const std::uint32_t*, float, float, int) noexcept;
    reinterpret_cast<Build>(image + 0xF5D3C0)(frame.data(), refs.data(), weight, time, layer);
    return frame;
}
void observe_fullbody(const void* frame) {
    (void)fullbody_update(fullbodyBody, frame, 1.F / 60.F, 0, nullptr, nullptr);
}
void combat_start_test() {
    check(omega::lair_start::receiptState.prepare(7, 8).generation == 8
        && !omega::lair_start::receiptState.prepare(7, 8).leftStarted,
        "actual Lair State prepares dormant native member generation");
    std::array<std::byte, combat::kFrameBytes> empty{};
    ScalarView baseline{};
    ordinaryScalarLookup = true;
    for (auto index : {1, 3}) {
        ordinaryScalarIndex = index;
        ScalarView ordinary{};
        check(left_scalar(entityHandle, ordinary) && !ordinary.bound && ordinary.value.lanes[0] == 0.25F
            && ordinary.value.lanes[3] == 1.F, "original 57A0A0 reads both validated native property arrays");
    }
    ordinaryScalarLookup = false;
    check(left_scalar(entityHandle, baseline) && baseline.bound && baseline.index == 31
        && scalar_equals(baseline.value, 0.F), "original 57A770 reads authored left31 zero baseline");
    auto* scalarRow = scalarBody + 0x58 + read<std::ptrdiff_t>(scalarBody, 0x58) + 0x10 + 31 * 16;
    for (auto offset : {0, 4, 8, 0x24, 0x2C}) {
        const auto saved = read<std::uint32_t>(fullbodyBody, offset);
        put(fullbodyBody, offset, UINT32_MAX);
        observe_fullbody(empty.data());
        check(scalarWrites == 0 && omega::lair_start::starts == 0, "wrong fullbody field cannot issue action");
        put(fullbodyBody, offset, saved);
    }
    put(fixtureBiped, 0x2C, entityHandle ^ 0x01000000U);
    observe_fullbody(empty.data());
    check(scalarWrites == 0, "recycled entity on biped blocks fullbody action");
    put(fixtureBiped, 0x2C, entityHandle);
    const auto relative = read<std::uintptr_t>(scalarBody, 0x58);
    put(scalarBody, 0x58, relative + 1);
    observe_fullbody(empty.data());
    check(scalarWrites == 0, "misaligned native getter source rejected before SIMD read");
    put(scalarBody, 0x58, relative);
    const ScalarValue one{{1.F, 1.F, 1.F, 1.F}}, zero{};
    std::memcpy(scalarRow, &one, sizeof one); observe_fullbody(empty.data());
    check(scalarWrites == 0, "preexisting scalar ownership is not borrowed");
    std::memcpy(scalarRow, &zero, sizeof zero);
    auto alreadyPlaying = native_frame(combat::kLeftClip, 0.1F);
    put(left_group(), 0x70, std::int32_t{1}); put(left_group(), 4, 1.F);
    put(left_group(), 0xC, combat::kLeftClipIndex); put(left_group(), 0x13, std::uint8_t{1});
    put(left_group(), 0x14, std::uint8_t{1}); put(left_group(), 0x18, 0.1F);
    observe_fullbody(alreadyPlaying.data());
    check(scalarWrites == 0, "preexisting left clip is not borrowed");
    std::memset(left_group(), 0, combat::kActionGroupBytes);
    scalarSetAccepted = false;
    observe_fullbody(empty.data());
    check(scalarWrites == 1 && runState.left.stage == combat::Stage::uncertain,
        "rejected native setter is reserved and cannot claim playback");
    observe_fullbody(alreadyPlaying.data());
    check(scalarWrites == 1 && omega::lair_start::starts == 0, "uncertain setter is never retried or promoted");
    runState.left = {}; runState.armToken={}; omega::mission::runtime::state={}; scalarWrites = 0; scalarSetAccepted = true;
    observe_fullbody(empty.data());
    check(scalarWrites == 1 && runState.left.stage == combat::Stage::requested,
        "terminal idle submits exact authored left scalar through native API");
    check(omega::lair_start::starts == 0, "scalar readback cannot start enemy wave");
    check(!omega::lair_start::receiptState.prepare(7, 8).leftStarted,
        "request-only callback leaves actual Lair State dormant");
    observe_fullbody(empty.data());
    auto right = native_frame(0x80F4518A, 0.1F); observe_fullbody(right.data());
    observe_fullbody(alreadyPlaying.data());
    check(omega::lair_start::starts == 0 && scalarWrites == 1,
        "even a direct left clip in the incoming base frame cannot claim fullbody output");
    std::array<std::byte, combat::kFrameBytes> baseIdle{};
    const std::array idleRefs{UINT32_MAX, UINT32_MAX, std::uint32_t{1}};
    using Build = void(__fastcall*)(std::byte*, const std::uint32_t*, float, float, int) noexcept;
    reinterpret_cast<Build>(image + 0xF5D3C0)(baseIdle.data(), idleRefs.data(), 1.F, 0.01F, 1);
    using ResolveClip = void(__fastcall*)(std::byte*, const std::uint32_t*, std::uint32_t) noexcept;
    std::array<std::byte, 0x18> resolvedIdle{};
    reinterpret_cast<ResolveClip>(image + 0xC886C0)(resolvedIdle.data(), idleRefs.data(), bipedHandle);
    check(read<std::byte*>(resolvedIdle.data(), 0) == resolve_handle(0x80F4517C)
        && read<std::uint32_t>(resolvedIdle.data(), 0x10) == 3,
        "original C886C0 proves base graph row1 resolves clip-bank3 idle, not left");
    prepare_native_fullbody();
    (void)native_fullbody_producer(fullbodyBody, baseIdle.data(), 1.F / 60.F, 255, nullptr, nullptr);
    (void)native_fullbody_producer(fullbodyBody, baseIdle.data(), 0.25F, 0, nullptr, nullptr);
    LeftOutput blended{};
    check(left_output(fullbodyBody, blended) && !blended.enabled && blended.blend == 0.F,
        "original mode0 fades custom output to zero using native rate5");
    (void)native_fullbody_producer(fullbodyBody, baseIdle.data(), 0.25F, 255, nullptr, nullptr);
    check(left_output(fullbodyBody, blended) && blended.enabled && blended.blend == 1.F,
        "original mode255 restores custom output using native rate5");
    std::array<std::byte, combat::kActionGroupBytes> generated{};
    std::memcpy(generated.data(), left_group(), generated.size());
    fullBodyOriginal.store(fullbody_original);
    for (const auto& bad : std::array{
        std::pair{0x70U, std::uint32_t{5}}, std::pair{0xCU, std::uint32_t{19}},
        std::pair{4U, std::uint32_t{0}}, std::pair{4U, std::uint32_t{0x7FC00000}},
        std::pair{0x18U, std::uint32_t{0x7FC00000}}, std::pair{0x18U, std::uint32_t{0xBF800000}}}) {
        put(left_group(), bad.first, bad.second); observe_fullbody(baseIdle.data());
        check(omega::lair_start::starts == 0 && runState.left.stage == combat::Stage::requested,
            "malformed/right/unweighted native output cannot start enemies");
        std::memcpy(left_group(), generated.data(), generated.size());
    }
    for (const auto& bad : std::array{std::pair{0x13U, std::uint8_t{0}}, std::pair{0x14U, std::uint8_t{3}}}) {
        put(left_group(), bad.first, bad.second); observe_fullbody(baseIdle.data());
        check(omega::lair_start::starts == 0, "unprocessed or unknown-layer output cannot start enemies");
        std::memcpy(left_group(), generated.data(), generated.size());
    }
    for (auto disabled : {true, false}) {
        if (disabled) put(fullbodyBody, 0xB29, std::uint8_t{0}); else put(fullbodyBody, 0xB20, 0.F);
        observe_fullbody(baseIdle.data());
        check(omega::lair_start::starts == 0, "disabled/blended-out stale custom rows cannot start enemies");
        put(fullbodyBody, 0xB29, std::uint8_t{1}); put(fullbodyBody, 0xB20, 1.F);
    }
    prepare_native_fullbody();
    auto nativeStep = [&](float dt) {
        (void)fullbody_update(fullbodyBody, baseIdle.data(), dt, 255, nullptr, nullptr);
    };
    nativeStep(1.F / 60.F);
    LeftOutput sampled{};
    combat::Playback receipt{};
    check(left_output(fullbodyBody, sampled) && combat::left_playback(sampled.group, sampled.duration, receipt),
        "production decoder accepts actual original104B7A0 output while input remains indexed idle");
    check(omega::lair_start::starts == 1 && runState.left.stage == combat::Stage::playing,
        "original fullbody output starts first enemies once with complete owned receipt");
    check(omega::lair_start::receiptState.prepare(7, 8).leftStarted,
        "original evaluated output activates actual prepared Lair State");
    combat::Track wrapGuard{};
    wrapGuard.previousValid = true; wrapGuard.previousTime = 0.9F;
    wrapGuard.layer = receipt.layer;
    check(combat::wrapped(wrapGuard, receipt), "same output identity admits a native high-to-low wrap");
    wrapGuard.previousValid = false;
    check(!combat::wrapped(wrapGuard, receipt), "observation gap prevents assumed wrap");
    wrapGuard.previousValid = true; wrapGuard.layer = receipt.layer == 1 ? 2 : 1;
    check(!combat::wrapped(wrapGuard, receipt), "changed output layer prevents assumed wrap");
    check(read<std::uint32_t>(baseIdle.data(), 0) == UINT32_MAX
        && read<std::uint32_t>(baseIdle.data(), 4) == UINT32_MAX
        && read<std::uint32_t>(baseIdle.data(), 8) == 1,
        "original fullbody producer does not replace base input refs with action clip");
    render_output_test();
    nativeStep(1.F / 60.F);
    check(omega::lair_start::starts == 1 && scalarWrites == 1, "repeated clip receipt cannot replay scalar or wave");
    // All clock samples below are advanced/wrapped by unchanged104C9D0.
    unsigned ticks{};
    while (runState.left.stage != combat::Stage::complete && ticks++ < 360) nativeStep(1.F / 60.F);
    check(scalarWrites == 2 && runState.left.stage == combat::Stage::complete,
        "original5.5-second clock wrap releases owned scalar once without host-time fallback");
    check(ticks >= 325 && ticks <= 335, "release follows original native clock rather than input idle phase");
    ScalarView released{};
    check(left_scalar(entityHandle, released) && scalar_equals(released.value, 0.F),
        "original native getter confirms released scalar zero");
    nativeStep(1.F / 60.F);
    check(scalarWrites == 3 && omega::lair_start::starts == 1, "completed left advances to right without replaying left");
    check(omega::mission::runtime::snapshot(7).command.action==omega::mission::Action::right,
        "production driver schedules documented right batch after native left wrap");
    nativeStep(1.F/60.F);
    LeftOutput rightOutput{}; combat::Playback rightPlayback{};
    check(left_output(fullbodyBody,rightOutput,true)
        && combat::left_playback(rightOutput.group,rightOutput.duration,rightPlayback,true),
        "original fullbody emits authored right bank19 from scalar30");
    check(omega::mission::runtime::snapshot(7).requested[0][0]==1,
        "native right output activates Achronos member budget");
    ticks=0;
    while(runState.left.stage!=combat::Stage::complete && ticks++<360) nativeStep(1.F/60.F);
    check(scalarWrites==4 && omega::mission::runtime::snapshot(7).phase==omega::mission::Phase::clearance,
        "right native wrap releases scalar and waits for all qualified deaths");
    nativeStep(1.F/60.F);
    check(scalarWrites==4,"both summons finish without an animation loop");
    check(omega::lair_start::receiptState.prepare(7, 8).leftStarted
        && !omega::lair_start::receiptState.left_started({7, 8, actorHandle, characterHandle, bipedHandle}),
        "actual Lair State retains one cumulative first wave after native scalar release");
    fullBodyOriginal.store(fullbody_original);
}
void enemy_receipt_test() {
    namespace mission=omega::mission;
    constexpr std::uint32_t actor=0x30F42041,character=0x32F9EA65,health=0x33F9EA66,syncHandle=0x34F9EA67,entity=0x045CE042;
    auto* characterBytes=bind(character,0x600);auto* healthBytes=bind(health,0x400);auto* sync=bind(syncHandle,0x70);
    source(characterBytes,0x815B305B,0x80806832,0x7D8);
    put(characterBytes,0x24,character);put(characterBytes,0x2C,entity);put(characterBytes,0xC0,actor);
    put(characterBytes,0x2E8,health);put(characterBytes,0x2EC,std::uint32_t{0x80804BEE});
    put(healthBytes,4,std::uint32_t{0x80804B8A});put(healthBytes,0x24,health);put(healthBytes,0x2C,entity);
    character_initialize(characterBytes,&actor);
    std::array<std::byte,0x690> component{};
    source(component.data(),0x80F4791F,0x8080948F,0x728);
    put(component.data(),0x5D8,std::uint32_t{0xF4D0E0B2});put(component.data(),0x5DC,std::uint8_t{1});put(component.data(),0x5DE,std::uint16_t{3});
    put(component.data(),0x170,syncHandle);put(component.data(),0x1FC,std::uint32_t{8});put(component.data(),0x244,std::uint32_t{8});
    put(component.data(),0x314,std::int32_t{1});put(component.data(),0x318,std::uint32_t{0xABCD});put(component.data(),0x31C,actor);
    put(sync,0,std::uint32_t{0xF4D0E0B2});put(sync,4,std::uint8_t{1});put(sync,6,std::uint16_t{3});put(sync,0xC,std::uint32_t{0x80807EC9});
    const auto admissions=[&] {return std::count(missionAdmitted.begin(),missionAdmitted.end(),true);};
    sourceSense=9;observe_mission_source(component.data());check(admissions()==0,"wrong sense generation cannot admit actor");sourceSense=8;
    put(healthBytes,0x24,health^0x01000000U);observe_mission_source(component.data());check(admissions()==0,"recycled health salt cannot admit actor");put(healthBytes,0x24,health);
    put(component.data(),0x318,std::uint32_t{0xABCE});observe_mission_source(component.data());check(admissions()==0,"invalid native weak serial cannot admit actor");put(component.data(),0x318,std::uint32_t{0xABCD});
    observe_mission_source(component.data());check(admissions()==1,"actual production source observer joins native actor to full character and health owners");
    observe_mission_source(component.data());check(admissions()==1,"repeated source list cannot duplicate admission");
    mission_character_death(characterBytes,0x12345678);check(mission::runtime::state.deaths(0)==0,"death callback without typed dead bit cannot clear wave");
    put(healthBytes,0x338,std::uint8_t{1});put(healthBytes,0x2C,entity^0x01000000U);
    mission_character_death(characterBytes,0x12345678);check(mission::runtime::state.deaths(0)==0,"dead bit from recycled entity cannot clear wave");put(healthBytes,0x2C,entity);
    mission_character_death(characterBytes,0x12345678);check(mission::runtime::state.deaths(0)==1,"qualified pre-original death reaches controller synchronously");
    mission_character_death(characterBytes,0x12345678);check(mission::runtime::state.deaths(0)==1&&deathCallbacks==4,"duplicate death ignored and original called exactly once per callback");
}
void pipeline_test() {
    MemberView member{}; graph::Owner owner{};
    using BipedLookup = std::uint32_t*(__fastcall*)(std::uint32_t*, std::uint32_t) noexcept;
    std::uint32_t resolvedBiped{};
    check(native<BipedLookup>(0xDCBF30)(&resolvedBiped, actorHandle) == &resolvedBiped
        && resolvedBiped == UINT32_MAX, "original DCBF30 reproduces live invalid biped when passed AI actor");
    for (const auto wrongDomain : {characterHandle, bipedHandle})
        check(native<BipedLookup>(0xDCBF30)(&resolvedBiped, wrongDomain) == &resolvedBiped
            && resolvedBiped == UINT32_MAX, "original DCBF30 rejects character and biped as world entity inputs");
    check(native<BipedLookup>(0xDCBF30)(&resolvedBiped, entityHandle) == &resolvedBiped
        && resolvedBiped == bipedHandle, "original DCBF30 resolves biped from distinct world entity");
    using CacheOwners = void(__fastcall*)(std::byte*) noexcept;
    reinterpret_cast<CacheOwners>(image + 0xF51CF0)(characterBody);
    check(read<std::uint32_t>(characterBody, 0x148) == bipedHandle,
        "original native caller passes component+2C to original biped lookup");
    check(member_view(memberBody, member), "production member binding");
    check(character_owner(member, 7, characterHandle, owner), "production complete owner chain");
    check(owner.entity == entityHandle && owner.entity != owner.actor && owner.entity != owner.character,
        "production owner distinguishes full actor, entity and character handles");
    // Independent pools may use the same numeric handle. Native lookup and
    // backlinks establish identity; numeric inequality across pools does not.
    for (const auto coincident : {actorHandle, characterHandle}) {
        fixtureWorldEntity = coincident;
        put(characterBody, 0x2C, coincident); put(fixtureBiped, 0x2C, coincident);
        check(character_owner(member, 7, characterHandle, owner) && owner.entity == coincident,
            "coincident world and actor/character bits pass original lookup and valid backlinks");
        put(fixtureBiped, 0x2C, entityHandle);
        check(!character_owner(member, 7, characterHandle, owner),
            "coincident numeric handles cannot bypass full native entity backlink");
    }
    fixtureWorldEntity = entityHandle;
    put(characterBody, 0x2C, entityHandle); put(fixtureBiped, 0x2C, entityHandle);
    check(character_owner(member, 7, characterHandle, owner), "distinct domain fixture restored after coincidence controls");
    put(characterBody, 0x2C, entityHandle ^ 0x01000000U);
    check(!character_owner(member, 7, characterHandle, owner), "same-index recycled entity fails full biped owner check");
    put(characterBody, 0x2C, entityHandle);
    check(character_owner(member, 7, characterHandle, owner), "restored entity owner revalidates");
    check(named_pair(owner), "unchanged original A889E0 resolves extracted graph pair");
    auto* config = resolve_handle(0x80F4519A);
    const std::uint32_t absentSequence = 0x811C9DC5;
    std::int32_t wildcardGroup = -2, wildcardSequence = -2;
    using Lookup = bool(__fastcall*)(const void*, const std::uint32_t*, const std::uint32_t*, std::int32_t*, std::int32_t*) noexcept;
    check(native<Lookup>(0xA889E0)(config, &graph::kGroup, &absentSequence, &wildcardGroup, &wildcardSequence)
        && wildcardGroup == 0 && wildcardSequence == -1, "original lookup permits native sequence wildcard");
    graph::EventTable wildcard{}; wildcard.count = 1; wildcard.rows[0] = {0x12345678, -1, 0, 1};
    check(graph::valid_table(wildcard), "unrelated valid native wildcard does not block summon lease");
    std::memcpy(animationBody + 0x34, &wildcard, sizeof wildcard);
    const auto groupOffset = read<std::uint64_t>(config, 0x28);
    put(config, 0x28, UINT64_MAX);
    check(!named_pair(owner), "malformed native config rejected before unbounded lookup");
    put(config, 0x28, groupOffset);
    put(memberBody, 0x180, std::uint32_t{9});
    check(!member_view(memberBody, member), "authority generation cannot relabel older member actor");
    put(memberBody, 0x180, std::uint32_t{8});
    check(member_view(memberBody, member), "matching generation restores member binding");
    constexpr std::array<std::size_t, 7> offsets{0, 4, 8, 0x24, 0x2C, 0xC0, 0x5C0};
    for (auto offset : offsets) {
        const auto before = read<std::uint32_t>(characterBody, offset);
        put(characterBody, offset, UINT32_MAX);
        check(!character_owner(member, 7, characterHandle, owner), "reject wrong character field");
        put(characterBody, offset, before);
    }
    for (auto offset : {4, 0x30, 0xC0, 0xC4}) {
        const auto before = read<std::uint32_t>(animationBody, offset);
        put(animationBody, offset, UINT32_MAX);
        check(!character_owner(member, 7, characterHandle, owner), "reject wrong animation backlink");
        put(animationBody, offset, before);
    }
    for (auto offset : {0, 4, 8, 0x24, 0xD0}) {
        const auto before = read<std::uint32_t>(fixtureBiped, offset);
        put(fixtureBiped, offset, UINT32_MAX);
        check(!character_owner(member, 7, characterHandle, owner), "reject wrong biped field");
        put(fixtureBiped, offset, before);
    }
    check(!receipts.empty(), "specific owner diagnostic receipts emitted");
    member_tick(memberBody);
    check(nativeQueues == 0, "missing initializer cannot claim native graph");
    character_initialize(characterBody, &actorHandle);
    check(static_cast<std::uint32_t>(observedCharacter.load()) == characterHandle, "initializer retains full character");
    put(animationBody, 4, std::uint32_t{0x80F45197});
    member_tick(memberBody);
    check(nativeQueues == 0 && !runState.queueClaimed, "wrong actual animation config cannot claim native graph");
    put(animationBody, 4, std::uint32_t{0x80F4519A});
    invalidateVfxOwner = true;
    member_tick(memberBody);
    check(vfxAttempts == 1 && nativeQueues == 0 && !runState.queueClaimed,
          "owner changed by VFX call blocks intro without consuming queue claim");
    invalidateVfxOwner = false;
    put(animationBody, 4, std::uint32_t{0x80F4519A});
    member_tick(memberBody);
    check(vfxAttempts == 2, "fresh owner can continue after unconfirmed visual initialization");
    if (!runState.queueAccepted) for (const auto& receipt : receipts) std::puts(receipt.c_str());
    check(nativeQueues == 1 && runState.queueAccepted, "native graph submitted once after full owner validation");
    check(runState.bossSeen && !runState.flightSeen, "queue receipt cannot release camera");
    member_tick(memberBody); check(nativeQueues == 1, "duplicate member tick does not replay graph");
    auto earlyIdle = snapshot(2); graph_update(0.01F, nullptr, earlyIdle.data(), nullptr);
    check(!runState.introIdleSeen, "idle without real summon cannot release fight startup");
    std::array<std::byte, combat::kFrameBytes> emptyFrame{}; observe_fullbody(emptyFrame.data());
    check(scalarWrites == 0, "native fullbody callback cannot bypass intro summon and idle");
    auto lead = snapshot(0); graph_update(0.01F, nullptr, lead.data(), nullptr);
    check(!runState.flightSeen, "authored leadin cannot release camera");
    auto fly = snapshot(1); graph_update(0.01F, nullptr, fly.data(), nullptr);
    check(runState.flightSeen && runState.graphPhase == observation::Phase::fly, "owned native fly releases camera latch");
    std::array<std::byte, 0x262> camera{};
    source(camera.data(), 0x80F478D3, 0x80804F07, 0x2E8);
    cinematic_tick(camera.data());
    check(runState.cinematicStarted && runState.cinematicObserved, "actual cinematic callback starts after native fly");
    put(camera.data(), 0x260, std::uint8_t{0}); cinematic_tick(camera.data());
    check(runState.cinematicFinished && nativeAdds == 0, "camera completion before hover does not synthesize summon");
    auto actorDomainGraph = snapshot(4);
    graph::write(actorDomainGraph, 0, actorHandle); graph::write(actorDomainGraph, 0x18, actorHandle);
    graph_update(0.01F, nullptr, actorDomainGraph.data(), nullptr);
    check(runState.graphPhase == observation::Phase::fly, "AI actor substituted into native entity graph fields is rejected");
    auto foreign = snapshot(4); graph::write(foreign, 4, characterHandle ^ 0x01000000U);
    graph_update(0.01F, nullptr, foreign.data(), nullptr);
    check(runState.graphPhase == observation::Phase::fly, "recycled same-index character cannot own graph receipt");
    omega::fixtureProgress.route = omega::Route::reveal;
    member_tick(memberBody); check(nativeAdds == 0, "close approach cannot shorten flyin");
    omega::fixtureProgress.route = omega::Route::crownEntrance;
    auto hover = snapshot(3); graph_update(0.01F, nullptr, hover.data(), nullptr);
    check(nativeAdds == 0, "native hover holds until later approach");
    const auto earlyMemberCallbacks = memberCallbacks;
    // AB6CE0 only forwards message-kind3 into AB69A0 -> AB6600. Execute the
    // untouched early-return branches: unrelated messages cannot supply a tick.
    using NativeMessage = void(__fastcall*)(void*, std::uint32_t, const void*) noexcept;
    std::array<std::byte, 16> message{};
    for (const auto kind : {0U, 1U, 2U, 4U, 255U}) {
        message[8] = std::byte{static_cast<unsigned char>(kind)};
        reinterpret_cast<NativeMessage>(image + 0xAB6CE0)(nullptr, 0, message.data());
        check(memberCallbacks == earlyMemberCallbacks, "original native unrelated-message path has no member callback");
    }
    omega::fixtureProgress.route = omega::Route::reveal;
    graph::EventTable beforeApproach{}; std::memcpy(&beforeApproach, animationBody + 0x34, sizeof beforeApproach);
    graph::EventTable capacity{}; capacity.count = 16;
    for (std::size_t i = 0; i < capacity.rows.size(); ++i) capacity.rows[i] = {0x70000000U + static_cast<unsigned>(i), 1, 0, 1};
    std::memcpy(animationBody + 0x34, &capacity, sizeof capacity);
    graph_update(0.01F, nullptr, hover.data(), nullptr);
    check(nativeAdds == 0 && runState.summonLease.state() == graph::LeaseState::idle,
        "full native condition table blocks mutation without consuming lease");
    capacity.count = 17; std::memcpy(animationBody + 0x34, &capacity, sizeof capacity);
    graph_update(0.01F, nullptr, hover.data(), nullptr);
    check(nativeAdds == 0, "invalid native event table cannot mutate");
    std::memcpy(animationBody + 0x34, &beforeApproach, sizeof beforeApproach);
    graph_update(0.01F, nullptr, hover.data(), nullptr);
    check(nativeAdds == 1 && memberCallbacks == earlyMemberCallbacks,
        "later approach adds native summon through graph callback with no late member callback");
    check(std::any_of(receipts.begin(), receipts.end(), [](const auto& line) { return line.find("condition_lease_preflight") != std::string::npos; }),
        "blocked condition capacity has a bounded diagnostic");
    graph_update(0.01F, nullptr, hover.data(), nullptr);
    check(nativeAdds == 1 && memberCallbacks == earlyMemberCallbacks, "repeated graph ticks do not duplicate condition add");
    auto summon = snapshot(4); graph_update(0.01F, nullptr, summon.data(), nullptr);
    check(runState.graphPhase == observation::Phase::summon, "production observer reaches true summon");
    auto idle = snapshot(2); graph_update(0.01F, nullptr, idle.data(), nullptr);
    check(runState.graphPhase == observation::Phase::idle, "summon ends in persistent native idle");
    check(runState.introIdleSeen, "real summon then idle releases next native action");
    combat_start_test();
    put(characterBody, 0x2C, entityHandle ^ 0x01000000U);
    graph_update(0.01F, nullptr, idle.data(), nullptr);
    check(!runState.graphRetired && nativeRemoves == 0, "recycled entity never receives owned lease removal");
    put(characterBody, 0x2C, entityHandle);
    put(memberBody, 0x228, std::int32_t{1});
    graph_update(0.01F, nullptr, idle.data(), nullptr);
    check(runState.graphRetired && nativeRemoves == 1 && nativeAdds == 1 && memberCallbacks == earlyMemberCallbacks,
        "graph callback retires exactly owned condition on native queue completion without member tick");
    graph::EventTable retained{}; std::memcpy(&retained, animationBody + 0x34, sizeof retained);
    check(retained.count == 1 && retained.rows[0] == wildcard.rows[0], "owned cleanup preserves native wildcard lease");
    member_tick(memberBody); check(nativeRemoves == 1 && nativeQueues == 1, "retired command never replays");
    check(callGate.idle(), "all actual callback call gates exited");
}
using DeviceApply=void(__fastcall*)(std::byte*,const std::byte*) noexcept;
unsigned deviceApplyCalls{};
void device_apply_original(std::byte* component,const std::byte* stateKey) noexcept {
    ++deviceApplyCalls;
    if(read<std::uint32_t>(stateKey,0)==0x8080992F) std::memcpy(component+0x180,read<const std::byte*>(stateKey,8),0x150);
    else put(component,0x180,read<std::uint32_t>(stateKey,0));
}
std::atomic<DeviceApply> g_portalApplyOriginal{device_apply_original};
void observe_mission_arc_source(std::byte*) noexcept {} // independently covered by Arc fixtures
unsigned arcPolls{};
void poll_mission_arc_carry() noexcept {++arcPolls;} // native carry proof covered by captured Arc fixtures
void mission_scene_sense(std::byte*) noexcept {} // independently covered by rescue fixtures
#include "client/hooks/bootflow/omega_cannon_delivery.inl"
#include "client/hooks/bootflow/omega_mission_devices.inl"
namespace omega_reveal_native {void observe_mission_device(std::byte* c) noexcept {fixture::observe_mission_device(c);}}
void portal_effect_observe(const char*,unsigned,std::byte*) noexcept {}
#include "client/hooks/bootflow/omega_portal_visual_apply.inl"
#include "omega_full_mission_native_fixture.inl"
#include "omega_eye_resource_trace_fixture.inl"
#include "omega_eye_clip_trace_fixture.inl"
#include "omega_eye_execution_trace_fixture.inl"
#include "omega_cannon_native_fixture.inl"
#include "omega_rescue_native_fixture.inl"
#include "omega_platform_native_fixture.inl"
#include "omega_arc_interface_native_fixture.inl"
#include "omega_arc_eligibility_native_fixture.inl"
#include "omega_arc_unlock_native_fixture.inl"
}
int main(int argc, char** argv) {
    using namespace sunrise::client::hooks::bootflow::fixture;
    AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS failure) -> LONG {
        std::fprintf(stderr, "native fixture exception=%08lX rva=%llX rcx=%llX rdx=%llX rax=%llX\n",
            failure->ExceptionRecord->ExceptionCode,
            failure->ContextRecord->Rip - reinterpret_cast<std::uintptr_t>(image),
            failure->ContextRecord->Rcx, failure->ContextRecord->Rdx, failure->ContextRecord->Rax);
        ExitProcess(99);
    });
    if (argc != 8) { std::fprintf(stderr, "usage: runtime_tests native-image config-80F4519A bank-80F45190 fullbody-815B5A41 scalar-80F6695E actions-80F45197 evaluation-80F45169\n"); return 2; }
    setup(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]); original_resolver_test(); pipeline_test(); enemy_receipt_test(); full_mission_native_test(); eye_resource_trace_test(); eye_clip_trace_test(); eye_execution_trace_test(); cannon_delivery_test(argv[1]); rescue_delivery_test(argv[1]); platform_native_test(argv[1]); arc_interface_native_test(argv[1]);
    arc_eligibility_native_test(argv[1]);
    arc_unlock_native_test(argv[1]);
    std::printf("%u checks passed: production intro, departures, three Crown cycles, health and ending callbacks; original resolver, lookup, event, scalar-getter and frame-builder instructions.\n", checks);
    for (auto* allocation : allocations) VirtualFree(allocation, 0, MEM_RELEASE);
    return 0;
}


