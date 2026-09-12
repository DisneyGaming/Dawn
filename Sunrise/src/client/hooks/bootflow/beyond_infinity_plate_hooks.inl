// Runs on the original actor-timer world tick. Source-sense/apply hooks only
// bind ownership; they never invoke an animation setter on a network thread.
namespace plate_native=beyond_infinity_plate_timer;
namespace beyond=state::activity::beyond_infinity;
using PlateTick=NativeCaptureTick;
std::atomic<PlateTick> g_plateTick{};
// Read-only native pose feedback. Native DF6510 now applies the server's
//80805063 dynamic record and owns DF6C70; no host animation setter remains.
bool plate_pose_sample(std::uintptr_t device,server::runtime::activity::mission_device_pose::Sample& sample) noexcept {
    return read_at(device+0x370,sample.actual) && read_at(device+0x37C,sample.target)
        && read_at(device+0x960,sample.revision) && read_at(device+0x964,sample.snapRevision);
}
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
bool plate_consumed(void* raw,const server::runtime::activity::mission_capture::Publication& publication,std::uint32_t revision) noexcept {
    gateway_native::Read read{g_image};float end{};
    if(!plate_native::authored_end(read,reinterpret_cast<std::uintptr_t>(raw),end)) {return false;}
    std::array<std::byte,plate_native::kStateBytes> state{};
    return copy(static_cast<std::byte*>(raw)+0x30,state)
        && server::runtime::activity::mission_capture::matches(publication,revision,state);
}
bool observe_beyond_plate_pose(const beyond::PlateRequest& request,void* raw) noexcept {
    std::uintptr_t device{};server::runtime::activity::mission_device_pose::Sample sample{};
    if(!plate_addresses(request,raw,device) || !plate_pose_sample(device,sample)
        || !plate_addresses(request,raw,device))return false;
    beyond::observe_plate_pose(request.plate,sample);return true;
}
bool drive_plate(const beyond::PlateRequest& request,void* raw) noexcept {
    return observe_beyond_plate_pose(request,raw) && plate_consumed(raw,request.capture,request.revision);
}
#include "deep_storage_plate_hooks.inl"
#include "hijacked_plate_hooks.inl"
__declspec(noinline) std::uint8_t __fastcall beyond_plate_tick_hook(void* raw) noexcept {
    const hooking::CallGate::Scope guard{g_gate};
    // Fast identity rejection avoids querying the mission mutex for every
    // unrelated actor timer in the game.
    std::array<std::byte,16> header{};
    const bool candidate=guard.accepts_side_effects() && copy(raw,header)
        && prefix(header.data(),0x815B8B3BU,0x80804FCBU,0x248);
    const auto request=candidate?beyond::plate_request():beyond::PlateRequest{};
    const bool driven=candidate && request.enabled && drive_plate(request,raw);
    const auto deepRequest=candidate?deep_plate::before(raw):state::activity::deep_storage::PlateRequest{};
    const auto hijackedRequest=candidate && guard.accepts_side_effects()?hijacked_plate::before(raw):state::activity::hijacked::PlateRequest{};
    // One physical timer detour brackets exactly one original tick and
    // preserves its AL. Samples on both sides retain native pose drift.
    const auto result=observe_native_capture_tick(raw,hooking::await_original(g_plateTick));
    if(guard.accepts_side_effects()) {
        deep_plate::after(raw,deepRequest);hijacked_plate::after(raw,hijackedRequest);
        if(request.enabled) {static_cast<void>(observe_beyond_plate_pose(request,raw));}
    }
    if(!driven || !guard.accepts_side_effects() || !request.occupied || request.destroyed) { return result; }
    std::uintptr_t device{};if(!plate_addresses(request,raw,device) || !plate_consumed(raw,request.capture,request.revision)) { return result; }
    std::uint8_t active{},latched{};float value{},remaining{};const auto address=reinterpret_cast<std::uintptr_t>(raw);
    if(!read_at(address+0x30,active) || active!=1 || !read_at(address+0x79,latched)
        || !read_at(address+0x1B8,value) || !read_at(address+0x1BC,remaining)
        || !plate_addresses(request,raw,device) || !plate_consumed(raw,request.capture,request.revision)) { return result; }
    beyond::observe_plate(request.plate,request.revision,value,plate_native::complete(active,latched,value,remaining));
    return result;
}
