// Hijacked extends the existing native 1006F20 timer interception.
namespace hijacked_plate {
namespace ds=state::activity::hijacked;
struct Drive {ds::PlateReceipt plate{};std::uint32_t revision{};bool armed{};friend bool operator==(const Drive&,const Drive&)=default;};
std::array<Drive,std::size(ds::kPlates)> applied{};std::array<std::uint64_t,std::size(ds::kPlates)> nextContest{};
bool current(const ds::PlateRequest& a) noexcept {
    const auto b=ds::plate_request(a.plate.index);return b.enabled && a.owner==b.owner && a.plate==b.plate
        && a.state.revision==b.state.revision && a.state.armed==b.state.armed && a.state.occupied==b.state.occupied
        && a.state.contested==b.state.contested && a.state.charged==b.state.charged;
}
bool addresses(const ds::PlateRequest& request,void* raw,std::uintptr_t& device) noexcept {
    namespace gn=gateway_native;const auto& p=request.plate;if(!request.enabled || !p.valid()) {return false;}
    gn::Read read{g_image};std::array<std::byte,0x30> timer{},generic{},source{};gn::Weak weak{};
    std::uint32_t generation{},committed{};std::uint8_t active{};std::uintptr_t timerAddress{};
    const auto& binding=ds::kPlates[p.index];
    if(!read.copy(p.source,std::span(source).first<16>()) || !prefix(source.data(),binding.source.definition,0x80809928U,0x4C8)
        || !read.value(p.source+0x180,generation) || generation!=p.owner.value || !read.value(p.source+0x2F0,committed) || committed!=generation
        || !read.value(p.source+0x188,active) || active!=1 || !read.value(p.source+0x440,weak) || weak.handle!=p.entity || weak.serial!=p.serial || !read.weak(weak)
        || !read.resolve(p.timer,timerAddress) || timerAddress!=reinterpret_cast<std::uintptr_t>(raw)
        || !read.copy(timerAddress,timer) || !prefix(timer.data(),0x815B8B3BU,0x80804FCBU,0x248)
        || at<std::uint32_t>(timer.data()+0x24)!=p.timer || at<std::uint32_t>(timer.data()+0x2C)!=p.entity
        || !read.resolve(p.device,device) || !read.copy(device,generic) || !prefix(generic.data(),0x80C7063BU,0x80803910U,0xA78)
        || at<std::uint32_t>(generic.data()+0x24)!=p.device || at<std::uint32_t>(generic.data()+0x2C)!=p.entity) {return false;}
    return current(request);
}
void contest(const ds::PlateRequest& request) noexcept {
    if(!request.state.armed || !request.plate.valid() || request.state.charged) {return;}
    const auto i=request.plate.index;const auto now=GetTickCount64();
    if(applied[i].plate.owner!=request.plate.owner) {nextContest[i]=0;}
    if(now<nextContest[i]) {return;}nextContest[i]=now+100;
    const auto living=ds::living_enemies();if(living.owner!=request.owner) {return;}
    const ds::Volume* volume{};for(const auto& v:ds::kVolumes) {if(v.asset==ds::kPlates[i].volume) {volume=&v;break;}}if(!volume) {return;}
    bool occupied{},complete=true;
    // This accessor is already used for authenticated local/native positions.
    // It runs here only on the original actor-timer world thread.
    constexpr std::array<std::uint8_t,16> signature{0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0x83,0xC1,0xA0,0xB8,0,0,0,0,0x48};
    gateway_native::Read entry{g_image};std::array<std::uint8_t,16> bytes{};
    if(!entry.value(g_image+0x558330,bytes) || bytes!=signature) {return;}
    for(std::size_t n=0;n<living.count;++n) {
        const auto& enemy=living.actors[n];gateway_native::Read read{g_image};std::uintptr_t table{},actor{},entities{},row{};
        std::uint32_t stride{},self{},entity{},owner{},flags{};
        if(!read.value(g_image+0x1F9D7F8,table) || !read.value(g_image+0x1F9D800,stride) || stride<0x70 || stride>0x100000) {complete=false;break;}
        actor=table+static_cast<std::uintptr_t>(enemy.actor&0x1FFFU)*stride;
        if(!read.value(actor+0x48,self) || self!=enemy.actor || !read.value(actor+0x38,owner) || owner!=enemy.owner
            || !read.value(actor+0x4C,entity) || entity==UINT32_MAX || !read.value(g_image+0x1F93428,entities)
            || !read.value(g_image+0x1F93430,stride) || stride<0x50 || stride>0x100000) {complete=false;continue;}
        std::int64_t sourceOffset{};std::uintptr_t source{};gateway_native::Ref sourceRef{};std::uint32_t generation{},committed{};
        const ds::Spawn* spawn{};for(const auto& binding:ds::kSpawns) {if(binding.registry==enemy.registry && binding.source==enemy.source) {spawn=&binding;break;}}
        if(!spawn || !read.value(actor+0x40,sourceOffset) || sourceOffset<0 || sourceOffset>0x1000000 || !read.resolve(owner,source)
            || source>UINTPTR_MAX-static_cast<std::uintptr_t>(sourceOffset)) {complete=false;continue;}
        source+=static_cast<std::uintptr_t>(sourceOffset);
        if(!read.value(source,sourceRef) || sourceRef.handle!=spawn->definition || sourceRef.kind!=0x8080948FU || sourceRef.offset!=spawn->offset
            || !read.value(source+0x1FC,generation) || generation!=enemy.generation || !read.value(source+0x244,committed) || committed!=generation) {complete=false;continue;}
        row=entities+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
        if(!read.value(row+12,self) || self!=entity || !read.value(row+4,flags) || (flags&4U)) {complete=false;continue;}
        std::array<float,4> point{};reinterpret_cast<void(__fastcall*)(std::uintptr_t,std::array<float,4>*)>(g_image+0x558330)(row,&point);
        if(!read.value(actor+0x48,self) || self!=enemy.actor || !read.value(actor+0x4C,self) || self!=entity || !read.value(row+12,self) || self!=entity || !read.value(actor+0x38,self) || self!=enemy.owner
            || !read.value(source+0x1FC,generation) || generation!=enemy.generation || !read.value(source+0x244,committed) || committed!=generation) {complete=false;continue;}
        if(ds::contains(*volume,{point[0],point[1],point[2]})) {occupied=true;break;}
    }
    if((occupied || complete) && current(request)) {ds::observe_contested(request.plate,occupied);}
}
bool command(const ds::PlateRequest& request,void* raw,plate_native::Command& out) noexcept {
    out=plate_native::stopped();if(!request.state.armed || !request.state.occupied || request.state.contested) {return true;}
    gateway_native::Read read{g_image};float end{};if(!plate_native::authored_end(read,reinterpret_cast<std::uintptr_t>(raw),end)) {return false;}
    std::array<std::byte,8> key{};const std::uint32_t unset=0x811C9DC5U;std::memcpy(key.data(),&unset,sizeof unset);gateway_native::Ref scope{};
    if(!g_plateScenario(request.plate.entity,key.data()) || !g_plateScope(key.data(),&scope)) {return false;}
    std::uintptr_t base{};if(!read.resolve(scope.handle,base) || scope.offset<0 || scope.offset>0x1000000 || base>UINTPTR_MAX-static_cast<std::uintptr_t>(scope.offset)) {return false;}
    auto* clock=g_plateClock(reinterpret_cast<void*>(base+static_cast<std::uintptr_t>(scope.offset)));std::uint64_t now=UINT64_MAX,duration{};
    if(!clock || g_plateNow(clock,&now)!=&now || g_plateDuration(&duration,ds::kPlates[request.plate.index].chargeSeconds)!=&duration) {return false;}
    return plate_native::start(out,duration,now);
}
bool present(const ds::PlateRequest& request,void* raw) noexcept {
    std::uintptr_t device{};if(!addresses(request,raw,device)) {return false;}
    gateway_native::Read read{g_image};
    return ds::plate_presentation::reconcile(request,read,device,[&] {
        std::uintptr_t checked{};return addresses(request,raw,checked) && checked==device;
    },[&](float position,std::uint32_t revision) {
        g_plateDevice(reinterpret_cast<std::byte*>(device),position,1,revision);
    });
}

ds::PlateRequest before(void* raw) noexcept {
    for(std::size_t i=0;i<std::size(ds::kPlates);++i) {
        auto request=ds::plate_request(i);std::uintptr_t device{};if(!addresses(request,raw,device)) {continue;}
        contest(request);request=ds::plate_request(i);if(!addresses(request,raw,device)) {return {};}
        // Keep checking the native pose after completion, including after departure.
        // The timer stays completed; only a drifted visual channel is repaired.
        if(request.state.charged) {return present(request,raw)?request:ds::PlateRequest{};}
        const Drive desired{request.plate,request.state.revision,request.state.armed};
        if(applied[i]==desired) {return request;}
        plate_native::Command body{};if(!command(request,raw,body) || !addresses(request,raw,device)) {return {};}
        g_plateApply(raw,body.bytes.data());std::array<std::byte,plate_native::kStateBytes> state{};
        if(!addresses(request,raw,device) || !copy(static_cast<std::byte*>(raw)+0x30,state)
            || std::memcmp(state.data(),body.bytes.data()+plate_native::kStateOffset,state.size())) {return {};}
        if(!present(request,raw)) {return {};}
        applied[i]=desired;return request;
    }return {};
}
void after(void* raw,const ds::PlateRequest& request) noexcept {
    if(!request.enabled) {return;}
    // The original stopped timer may select idle; restore the preloaded red pose.
    if(!request.state.armed) {static_cast<void>(present(request,raw));return;}
    if(request.state.charged) {static_cast<void>(present(request,raw));return;}
    if(!request.state.occupied || request.state.contested) {return;}
    std::uintptr_t device{};if(!addresses(request,raw,device)) {return;}
    const auto p=reinterpret_cast<std::uintptr_t>(raw);std::uint8_t active{},latched{};float value{},remaining{};
    if(!read_at(p+0x30,active) || !read_at(p+0x79,latched) || !read_at(p+0x1B8,value) || !read_at(p+0x1BC,remaining) || !addresses(request,raw,device)) {return;}
    ds::observe_plate(request.plate,request.state.revision,value,plate_native::complete(active,latched,value,remaining));
    const auto completed=ds::plate_request(request.plate.index);
    if(completed.plate==request.plate && completed.state.charged) {static_cast<void>(present(completed,raw));}
}
}
