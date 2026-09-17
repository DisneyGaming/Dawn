// Included in the existing Forest hook translation unit. Every pointer is
// re-resolved on this native tick; no movable worker or sensor pointer is kept.
namespace beyond_forest_runtime {
namespace mission=state::activity::beyond_infinity;
namespace selection=mission::forest;
struct Ref { std::uint32_t handle{UINT32_MAX},kind{UINT32_MAX};std::int64_t offset{}; };
inline bool selected() noexcept {
    state::activity::forced::ForcedDestination destination{};
    state::activity::forced::snapshot(destination);
    return state::activity::forced::override_active()
        && destination.packageNameLength<=destination.packageName.size()
        && std::string_view(destination.packageName.data(),destination.packageNameLength)=="adventure_vod"
        && state::activity::mission_seed_armed()
        && state::activity::world_phase()!=state::activity::WorldPhase::idle;
}
inline bool worker(void* instance) noexcept {
    auto* bytes=static_cast<const std::byte*>(instance);
    return bytes && readable(bytes,beyond_forest::kWorkerBytes)
        && beyond_forest::matches({bytes,beyond_forest::kWorkerBytes});
}
inline bool prefix(std::uintptr_t image,std::uintptr_t rva,std::span<const unsigned char> expected) noexcept {
    auto* bytes=reinterpret_cast<const std::byte*>(image+rva);
    return image && readable(bytes,expected.size()) && std::memcmp(bytes,expected.data(),expected.size())==0;
}
inline bool lookup_sensor(std::uintptr_t image,Ref& output) noexcept {
    // 10059A0 calls 103E700 to resolve its current type-37 sensor.
    constexpr std::array<unsigned char,16> expected{0x40,0x53,0x48,0x83,0xEC,0x30,0x33,0xD2,0x48,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48};
    if(!prefix(image,0x103E700,expected)) { return false; }
    using Lookup=Ref*(__fastcall*)(Ref*) noexcept;
    __try { reinterpret_cast<Lookup>(image+0x103E700)(&output); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return output.handle!=UINT32_MAX && output.kind==0x80804EF6U;
}
inline std::byte* sensor(std::uintptr_t image) noexcept {
    Ref ref{};if(!lookup_sensor(image,ref)) { return nullptr; }
    const auto root=omega_teardown_native::resolve(teardown_source(),ref.handle);
    std::uintptr_t address{};
    if(!root.address || !omega_teardown_native::add_signed(root.address,ref.offset,address)) { return nullptr; }
    auto* bytes=reinterpret_cast<std::byte*>(address);
    if(!readable(bytes,0x1E0)) { return nullptr; }
    const auto native=read_value<Ref>(bytes);
    // Recovered 8E70632B / 37 / 3 descriptor; F/G and Omega sensors are excluded.
    if(!beyond_forest::sensor_identity(native.handle,native.kind,native.offset)) { return nullptr; }
    const auto definition=omega_teardown_native::resolve(teardown_source(),native.handle);
    if(!definition.address) { return nullptr; }
    auto* scope=reinterpret_cast<const std::byte*>(definition.address+0xD98);
    return readable(scope,8) && read_value<std::uint32_t>(scope)==0x8E70632BU
        && read_value<std::uint16_t>(scope+4)==37 && read_value<std::uint16_t>(scope+6)==3?bytes:nullptr;
}
inline bool switch_value(std::uintptr_t image,std::uint32_t key,std::uint32_t desired) noexcept {
    // A55BB0 is the native current-context typed switch getter. It performs no
    // publication. Require the class as well as the payload before generating.
    constexpr std::array<unsigned char,16> expected{0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x8B,0x19,0xE8,0x2F,0x9D,0xAA};
    if(!prefix(image,0xA55BB0,expected)) { return false; }
    using Lookup=const std::byte*(__fastcall*)(const std::uint32_t*) noexcept;
    const std::byte* value{};
    __try { value=reinterpret_cast<Lookup>(image+0xA55BB0)(&key); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return value && readable(value,24) && read_value<std::uint32_t>(value)==key
        && read_value<std::uint32_t>(value+4)==selection::kHashClass
        && read_value<std::uint32_t>(value+8)==desired;
}
inline void prepare(void* instance) noexcept {
    if(!selected() || !worker(instance)) { return; }
    const auto request=mission::request();
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto* source=sensor(image);if(!source) { return; }
    const auto pass=request.frame.forestPass;
    const bool wanted=request.owner.valid() && request.frame.enabled && !request.frame.finished && pass>=1 && pass<=2;
    const bool ready=wanted && switch_value(image,selection::kVex,selection::value(pass,selection::kVex))
        && switch_value(image,selection::kFallen,selection::value(pass,selection::kFallen));
    const auto seed=beyond_forest::seed(request.owner.run,request.owner.value,pass);
    const auto current=mission::request();
    if(current.owner!=request.owner || current.frame.forestPass!=pass || !selected() || !worker(instance) || sensor(image)!=source) { return; }
    // Match Omega's native owner repair. This permits replicated gate creation;
    // native encounter-clear and interaction predicates still open the gates.
    if(ready) {
        const auto owner=read_value<std::uint32_t>(static_cast<std::byte*>(instance)+0x2C);
        const auto setter=g_forestOwnerAuthoritySetter.load(std::memory_order_acquire);
        auto* word=reinterpret_cast<const std::byte*>(image+kObjectAuthorityTableRva+((owner&0x1FFFU)>>5U)*4U);
        if(owner==UINT32_MAX || !setter || !readable(word,4)) { return; }
        if((read_value<std::uint32_t>(word)&(1U<<(owner&31U)))==0) { setter(owner,1U); }
    }
    mission::observe_forest_readiness(request.owner,pass,ready);
    static std::atomic_uint64_t reported{UINT64_MAX};
    const auto stamp=(static_cast<std::uint64_t>(seed)<<1)|(ready?1ULL:0ULL);
    if(reported.exchange(stamp,std::memory_order_relaxed)!=stamp) {
        std::array<char,256> line{};
        const int length=std::snprintf(line.data(),line.size(),
            "ev=beyond_forest run=%llu generation=%u pass=%u ready=%u seed=%u config=80F4D0F1 worker=%p sensor=%p observation=switch_readiness",
            static_cast<unsigned long long>(request.owner.run),request.owner.value,unsigned(pass),ready?1U:0U,seed,instance,static_cast<void*>(source));
        if(length>0 && static_cast<std::size_t>(length)<line.size()) { core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(length)}); }
    }
}
}
