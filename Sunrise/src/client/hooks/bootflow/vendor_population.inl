// Uses the existing admission, area-unload and world-step integration.
namespace vendorPopulation {
namespace lifetime=state::activity::vendors::lifetime;
namespace native=vendor_lifetime_native;
std::atomic_uint lines{},unloading{};
template<class... Args> void report(const char* format,Args... args) noexcept {
    if(lines.fetch_add(1,std::memory_order_relaxed)>=256) return;
    std::array<char,320> line{};const auto count=std::snprintf(line.data(),line.size(),format,args...);
    if(count>0 && static_cast<std::size_t>(count)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(count)});
}
struct Read : gateway_native::Read {
    explicit Read(std::uintptr_t base) noexcept : gateway_native::Read{base} {}
    bool make_weak(std::uint32_t handle,gateway_native::Weak& out) noexcept {
        out={};if(handle==UINT32_MAX) return false;
        std::uintptr_t directory{},registry{},metadata{},head{},elements{};std::int32_t size{};
        if(!value(image+0x2439C70,directory) || !value(directory,registry)
            || !value(directory+0x10,size) || size<=0 || size>0x1000) return false;
        const auto index=((static_cast<std::int32_t>(handle)>>31&0x3C00U)|0x3FFU)&(handle>>13)&0xFFFFU;
        std::uint16_t count{};std::uint32_t offset{},stride{},serial{};
        return value(registry+static_cast<std::uintptr_t>(index)*size+0x10,metadata)
            && value(metadata,head) && value(metadata+8,elements) && value(head+0x1C,count)
            && (handle&0x1FFFU)<count && value(metadata+0x1C,offset) && value(metadata+0x20,stride)
            && stride>0 && stride<=0x100000
            && value(elements+offset+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride,serial)
            && (out={serial,handle},true);
    }
};

struct Entry {
    nativeEvents::Event birth{};vendor_network::Root<gateway_native::Weak> root{};
    gateway_native::Weak source{};nativeEvents::Lease waiting{};
    native::Retirement<gateway_native::Weak> removal{};const char* lastReason{};unsigned reports{};
};
std::array<Entry,64> roots;
// Caller owns g_pendingMutex. Admission already qualified source and entity.
void remember(const nativeEvents::Event& birth) noexcept {
    if(!lifetime::owns(birth.lease)) return;
    Entry* empty{};
    for(auto& entry:roots) {
        if(entry.birth.actor==birth.actor) return;
        if(!entry.birth.lease.activity && !empty) empty=&entry;
    }
    if(!empty) {report("ev=vendor_population stage=retain result=capacity limit=%u",64U);return;}
    empty->birth=birth;Read read{g_image};
    static_cast<void>(read.make_weak(birth.sourceHandle,empty->source));
    static_cast<void>(vendor_network::attach(read,birth.actor.entity,empty->root));
    static_cast<void>(vendor_network::capture(read,empty->root));
    report("ev=vendor_population stage=admitted registry=%08X slot=%u generation=%u actor=%08X entity=%08X networked=%u",
        birth.lease.source.source.registry,birth.lease.source.source.slot,birth.lease.source.generation,
        birth.actor.actor,birth.actor.entity,empty->root.networked?1U:0U);
}
void poll() noexcept {
    for(auto& entry:roots) {
        if(!entry.birth.lease.activity) continue;
        if(!lifetime::owns(entry.waiting.activity?entry.waiting:entry.birth.lease)) {entry={};continue;}
        if(entry.waiting.activity) continue;
        Read read{g_image};
        if(!entry.root.networked) static_cast<void>(vendor_network::capture(read,entry.root));
        Read current{g_image};
        if(vendor_network::presence(current,entry.root)!=vendor_network::Presence::removed) continue;
        Read check{g_image};
        if(entry.removal.count && !native::gone(check,entry.removal)) continue;
        if(lifetime::removed(entry.birth,&entry.waiting))
            report("ev=vendor_population stage=removed registry=%08X slot=%u generation=%u next_generation=%u source_blocked=1",
                entry.birth.lease.source.source.registry,entry.birth.lease.source.source.slot,
                entry.birth.lease.source.generation,entry.waiting.source.generation);
    }
}
struct Identity {std::uint32_t key{};std::uint8_t type{},pad{};std::int16_t slot{};};
static_assert(sizeof(Identity)==8 && sizeof(Ref)==16);
struct Native {
    bool lookup(Identity id,Ref& out) noexcept {
        Read read{g_image};std::uint16_t selector{};std::uintptr_t context{};std::uint32_t count{};
        if(!read.value(g_image+0x1F91FE8,selector) || !selector) return false;
        const auto cell=reinterpret_cast<std::uintptr_t(__fastcall*)()>(g_image+0x4EA560)();
        return cell && read.value(cell,context) && context && read.value(context+8,count) && count && count<=128
            && reinterpret_cast<bool(__fastcall*)(const Identity*,std::uint32_t,Ref*)>(g_image+0x4EA140)(&id,0,&out);
    }
    std::uintptr_t context() noexcept {
        return reinterpret_cast<std::uintptr_t(__fastcall*)()>(g_image+0x16FC600)();
    }
    void remove(const native::Retirement<gateway_native::Weak>& r) noexcept {
        reinterpret_cast<void(__fastcall*)(std::uintptr_t,std::uint32_t)>(g_image+0x170FC90)(r.manager,r.parts[0].facet);
    }
};
bool boundaries() noexcept {
    struct Boundary {std::uint32_t rva;std::array<std::uint8_t,12> bytes;};
    constexpr Boundary expected[]{
        {0x4EA560,{0x0F,0xB7,0x0D,0x81,0x7A,0xAA,0x01,0x66,0x85,0xC9,0x0F,0x85}},
        {0x4EA140,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48}},
        {0x16FC600,{0x40,0x53,0x48,0x83,0xEC,0x20,0x33,0xDB,0x38,0x1D,0xE2,0xB4}},
        {0x170FC90,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x18,0x48,0x89}},
    };
    Read read{g_image};std::array<std::uint8_t,12> bytes{};
    for(const auto& b:expected) if(!read.value(g_image+b.rva,bytes) || bytes!=b.bytes) return false;
    return true;
}
// Runs only AFTER native424D10 has completed, with the existing unload
// integration's allocator validation. No observer/state lock spans native calls.
void finish_unload(bool allocatorReady) noexcept {
    std::array<Entry,64> saved;{std::lock_guard lock(g_pendingMutex);saved=roots;}
    if(std::none_of(saved.begin(),saved.end(),[](const auto& e){return e.birth.lease.activity && !e.waiting.activity && !e.removal.count;})) return;
    if(!allocatorReady || !boundaries()) {report("%s","ev=vendor_population stage=unload result=native_context_unavailable");return;}
    Native engine;
    for(const auto& entry:saved) {
        if(!entry.birth.lease.activity || entry.waiting.activity || entry.removal.count || !lifetime::owns(entry.birth.lease)) continue;
        native::Retirement<gateway_native::Weak> before{},after{};native::Stage stage{};Read read{g_image},check{g_image};
        if(!native::sample(read,entry.root,engine.context(),before,stage)) {
            if(stage!=native::Stage::entity)
                report("ev=vendor_population stage=unload result=%s registry=%08X slot=%u facet=%08X",
                    native::name(stage),entry.birth.lease.source.source.registry,entry.birth.lease.source.source.slot,entry.root.facet);
            continue;
        }
        if(!native::sample(check,entry.root,engine.context(),after,stage) || before!=after) continue;
        bool owned{};
        {
            std::lock_guard lock(g_pendingMutex);
            for(auto& current:roots) if(current.birth.actor==entry.birth.actor && current.birth.lease==entry.birth.lease
                && !current.waiting.activity && !current.removal.count && lifetime::owns(current.birth.lease)) {
                current.removal=after;owned=true;break;
            }
        }
        if(!owned) continue;
        engine.remove(after);
        Read result{g_image};const bool removed=native::gone(result,after);
        report("ev=vendor_population stage=network_retired registry=%08X slot=%u facet=%08X parts=%zu confirmed=%u",
            entry.birth.lease.source.source.registry,entry.birth.lease.source.source.slot,entry.root.facet,after.count,removed?1U:0U);
    }
    std::lock_guard lock(g_pendingMutex);poll();
}
// Wait for a different native source allocation with the exact zero-request
// generation. Reusing an old source while it is unloading can create a duplicate.
bool reloaded(const Entry& entry,gateway_native::Weak& source,const char*& reason) noexcept {
    const auto asset=entry.waiting.source.source;
    const state::activity::newlight::launchpad::AssetBinding* binding{};
    using Binding=state::activity::newlight::launchpad::AssetBinding;
    for(auto assets:{std::span<const Binding>(state::activity::newlight::launchpad::welcome::kAssets),std::span<const Binding>(state::activity::vendors::kAssets)})
        for(const auto& b:assets) if(b.asset==asset) binding=&b;
    reason="source_lookup";Native engine;Ref ref{},definition{};Read read{g_image};std::uintptr_t address{};
    if(!binding || !engine.lookup({asset.registry,1,0,static_cast<std::int16_t>(asset.slot)},ref)
        || ref.kind!=0x80809A3BU || ref.offset || !read.make_weak(ref.handle,source) || !read.resolve(ref.handle,address)) return false;
    reason="old_source";if(source==entry.source || entry.source.handle==UINT32_MAX) return false;
    reason="source_identity";
    if(!read.value(address,definition) || definition.handle!=asset.definition || definition.kind!=0x8080948FU
        || definition.offset!=binding->offset) return false;
    reason="source_generation";std::uint32_t authority{},sense{};
    if(!read.value(address+0x1FC,authority) || !read.value(address+0x244,sense)
        || authority!=entry.waiting.source.generation || sense!=authority || !read.weak(source)) return false;
    reason="ready";return true;
}
void resume() noexcept {
    static std::atomic_flag busy;static std::uint64_t next{};
    if(busy.test_and_set(std::memory_order_acquire)) return;
    struct Release {std::atomic_flag& busy;~Release(){busy.clear(std::memory_order_release);}} release{busy};
    const auto now=GetTickCount64();if(now<next || unloading.load(std::memory_order_acquire)) return;next=now+100;
    if(!boundaries()) return;
    std::array<Entry,64> saved;{std::lock_guard lock(g_pendingMutex);saved=roots;}
    for(const auto& entry:saved) {
        if(!entry.waiting.activity || !lifetime::owns(entry.waiting)) continue;
        gateway_native::Weak before{},after{};const char* reason{};
        const bool ready=reloaded(entry,before,reason) && reloaded(entry,after,reason) && before==after;
        std::lock_guard lock(g_pendingMutex);
        for(auto& current:roots) if(current.waiting==entry.waiting) {
            if(ready && !unloading.load(std::memory_order_acquire) && lifetime::resume(entry.waiting)) {
                report("ev=vendor_population stage=source_resumed registry=%08X slot=%u generation=%u old_source=%08X new_source=%08X",
                    entry.waiting.source.source.registry,entry.waiting.source.source.slot,entry.waiting.source.generation,
                    entry.source.handle,before.handle);current={};
            } else if(current.lastReason!=reason && current.reports<8) {
                current.lastReason=reason;++current.reports;
                report("ev=vendor_population stage=source_wait result=%s registry=%08X slot=%u generation=%u",
                    reason,entry.waiting.source.source.registry,entry.waiting.source.source.slot,entry.waiting.source.generation);
            }
            break;
        }
    }
}
}
