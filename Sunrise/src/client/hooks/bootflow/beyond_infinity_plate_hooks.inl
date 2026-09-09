// Runs on the original actor-timer world tick. Source-sense/apply hooks only
// bind ownership; they never invoke an animation setter on a network thread.
namespace plate_native=beyond_infinity_plate_timer;
namespace beyond=state::activity::beyond_infinity;
using PlateTick=bool(__fastcall*)(void*) noexcept;
using PlateApply=void(__fastcall*)(void*,const void*) noexcept;
using PlateClock=void*(__fastcall*)(void*) noexcept;
using PlateNow=std::uint64_t*(__fastcall*)(void*,std::uint64_t*) noexcept;
using PlateDuration=std::uint64_t*(__fastcall*)(std::uint64_t*,float) noexcept;
using PlateScenario=bool(__fastcall*)(std::uint32_t,void*) noexcept;
using PlateScope=bool(__fastcall*)(const void*,gateway_native::Ref*) noexcept;
using PlateDevice=void(__fastcall*)(std::byte*,float,char,std::uint32_t) noexcept;
std::atomic<PlateTick> g_plateTick{};
PlateApply g_plateApply{};PlateClock g_plateClock{};PlateNow g_plateNow{};
PlateDuration g_plateDuration{};PlateScenario g_plateScenario{};PlateScope g_plateScope{};PlateDevice g_plateDevice{};
struct PlateDrive {
    beyond::PlateReceipt plate{};std::uint32_t revision{};bool occupied{},destroyed{};
    friend bool operator==(const PlateDrive&,const PlateDrive&)=default;
};
PlateDrive g_plateDrive{};
unsigned g_plateLines{};
// Reference wzwJ69pzSIE ~107.25..114.5s. Reconstruction estimate, not a
// recovered retail script constant. The actual native timer owns progress.
constexpr float kWellChargeSeconds=7.F;
bool same_plate_request(const beyond::PlateRequest& a,const beyond::PlateRequest& b) noexcept {
    return b.enabled && a.owner==b.owner && a.plate==b.plate && a.revision==b.revision
        && a.occupied==b.occupied && a.destroyed==b.destroyed && a.charged==b.charged;
}
bool plate_addresses(const beyond::PlateRequest& request,void* raw,std::uintptr_t& device) noexcept {
    namespace gn=gateway_native;
    const auto& p=request.plate;if(!request.enabled || !p.valid()) { return false; }
    gn::Read read{g_image};std::array<std::byte,0x30> source{},timer{},generic{};
    std::uint32_t generation{},committed{};std::uint8_t active{};gn::Weak weak{};
    std::uintptr_t timerAddress{};
    if(!read.copy(p.source,std::span(source).first<16>()) || !prefix(source.data(),0x80F462ADU,0x80809928U,0x4C8)
        || !read.value(p.source+0x180,generation) || generation!=p.owner.value
        || !read.value(p.source+0x2F0,committed) || committed!=generation
        || !read.value(p.source+0x188,active) || active!=1
        || !read.value(p.source+0x440,weak) || weak.handle!=p.entity || weak.serial!=p.serial || !read.weak(weak)
        || !read.resolve(p.timer,timerAddress) || timerAddress!=reinterpret_cast<std::uintptr_t>(raw)
        || !read.copy(timerAddress,timer) || !prefix(timer.data(),0x815B8B3BU,0x80804FCBU,0x248)
        || at<std::uint32_t>(timer.data()+0x24)!=p.timer || at<std::uint32_t>(timer.data()+0x2C)!=p.entity
        || !read.resolve(p.device,device) || !read.copy(device,generic)
        || !prefix(generic.data(),0x80C7063BU,0x80803910U,0xA78)
        || at<std::uint32_t>(generic.data()+0x24)!=p.device || at<std::uint32_t>(generic.data()+0x2C)!=p.entity) { return false; }
    return same_plate_request(request,beyond::plate_request());
}
bool plate_command(const beyond::PlateRequest& request,void* raw,plate_native::Command& command) noexcept {
    command=plate_native::stopped();if(!request.occupied || request.destroyed) { return true; }
    gateway_native::Read read{g_image};float end{};
    if(!plate_native::authored_end(read,reinterpret_cast<std::uintptr_t>(raw),end)) { return false; }
    // Exact clock lookup used by 1006F20; never substitute GetTickCount64 or
    // a clock from another activity. Native scenario keys occupy eight bytes.
    std::array<std::byte,8> key{};const std::uint32_t unset=0x811C9DC5U;
    std::memcpy(key.data(),&unset,sizeof unset);gateway_native::Ref scope{};
    if(!g_plateScenario(request.plate.entity,key.data()) || !g_plateScope(key.data(),&scope)) { return false; }
    std::uintptr_t base{};
    if(!read.resolve(scope.handle,base) || scope.offset<0 || scope.offset>0x1000000
        || base>UINTPTR_MAX-static_cast<std::uintptr_t>(scope.offset)) { return false; }
    auto* clock=g_plateClock(reinterpret_cast<void*>(base+static_cast<std::uintptr_t>(scope.offset)));
    std::uint64_t now=UINT64_MAX,duration{};
    if(!clock || g_plateNow(clock,&now)!=&now || g_plateDuration(&duration,kWellChargeSeconds)!=&duration) { return false; }
    return plate_native::start(command,duration,now);
}
bool drive_plate(const beyond::PlateRequest& request,void* raw) noexcept {
    std::uintptr_t device{};if(!plate_addresses(request,raw,device)) { return false; }
    // Keep the native completed pose and timer until the box is destroyed.
    // An occupancy edge after completion must not restart or clear the effect.
    if(request.charged && !request.destroyed) { return true; }
    const PlateDrive desired{request.plate,request.revision,request.occupied,request.destroyed};
    AcquireSRWLockShared(&g_lock);const bool applied=g_plateDrive==desired;ReleaseSRWLockShared(&g_lock);
    if(applied) { return true; }
    plate_native::Command command{};
    if(!plate_command(request,raw,command) || !plate_addresses(request,raw,device)) { return false; }
    // Only .1 was confirmed to spawn this plate's authored hologram in the
    // live graph. .2 has unverified alternate capture semantics; do not use it.
    const float position=request.occupied && !request.destroyed?.1F:0.F;
    std::uint32_t revision{};
    if(!read_at(device+0x960,revision) || revision==UINT32_MAX-1U) { return false; }
    const auto next=revision==UINT32_MAX?0U:revision+1U;
    g_plateApply(raw,command.bytes.data());
    std::array<std::byte,plate_native::kStateBytes> state{};
    if(!plate_addresses(request,raw,device) || !copy(static_cast<std::byte*>(raw)+0x30,state)
        || std::memcmp(state.data(),command.bytes.data()+plate_native::kStateOffset,state.size())!=0) { return false; }
    g_plateDevice(reinterpret_cast<std::byte*>(device),position,1,next);
    float actual{};std::uint32_t accepted{};
    if(!plate_addresses(request,raw,device) || !read_at(device+0x370,actual) || actual!=position
        || !read_at(device+0x960,accepted) || accepted!=next) { return false; }
    AcquireSRWLockExclusive(&g_lock);g_plateDrive=desired;ReleaseSRWLockExclusive(&g_lock);
    report_capped(g_plateLines,96U,"ev=beyond_infinity stage=plate_native_%s run=%llu entity=%08X revision=%u position=%.1f charge_seconds=%.1f timing=reference_estimate",
        request.occupied && !request.destroyed?"charge":"reset",static_cast<unsigned long long>(request.plate.owner.run),request.plate.entity,request.revision,position,kWellChargeSeconds);
    return true;
}
__declspec(noinline) bool __fastcall beyond_plate_tick_hook(void* raw) noexcept {
    const hooking::CallGate::Scope guard{g_gate};
    // Fast identity rejection avoids querying the mission mutex for every
    // unrelated actor timer in the game.
    std::array<std::byte,16> header{};
    const bool candidate=guard.accepts_side_effects() && copy(raw,header)
        && prefix(header.data(),0x815B8B3BU,0x80804FCBU,0x248);
    const auto request=candidate?beyond::plate_request():beyond::PlateRequest{};
    const bool driven=candidate && request.enabled && drive_plate(request,raw);
    const bool result=hooking::await_original(g_plateTick)(raw);
    if(!driven || !guard.accepts_side_effects() || !request.occupied || request.destroyed) { return result; }
    std::uintptr_t device{};if(!plate_addresses(request,raw,device)) { return result; }
    std::uint8_t active{},latched{};float value{},remaining{};const auto address=reinterpret_cast<std::uintptr_t>(raw);
    if(!read_at(address+0x30,active) || active!=1 || !read_at(address+0x79,latched)
        || !read_at(address+0x1B8,value) || !read_at(address+0x1BC,remaining)
        || !plate_addresses(request,raw,device)) { return result; }
    beyond::observe_plate(request.plate,request.revision,value,plate_native::complete(active,latched,value,remaining));
    return result;
}
