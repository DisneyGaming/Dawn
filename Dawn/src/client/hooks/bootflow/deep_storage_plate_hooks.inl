// Deep Storage extends the existing native 1006F20 timer interception.
namespace deep_plate {
namespace ds=state::activity::deep_storage;
struct Drive {ds::PlateReceipt plate{};std::uint32_t revision{};bool armed{};friend bool operator==(const Drive&,const Drive&)=default;};
std::array<Drive,3> applied{};std::array<std::uint64_t,3> nextContest{};
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
    if(!request.state.armed || !request.plate.valid() || request.plate.index==0 || request.state.charged) {return;}
    const auto i=request.plate.index;const auto now=GetTickCount64();
    if(applied[i].plate.owner!=request.plate.owner) {nextContest[i]=0;}
    if(now<nextContest[i]) {return;}nextContest[i]=now+100;
    const auto living=ds::living_enemies();if(living.owner!=request.owner) {return;}
    std::array<ds::EnemyPosition,256> positions{};std::size_t positionCount{};bool complete=true;
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
        positions[positionCount++]={enemy,{point[0],point[1],point[2]}};
    }
    if(current(request)) {ds::observe_contested_positions(request.plate,std::span(positions).first(positionCount),complete);}
}
bool present(const ds::PlateRequest& request,void* raw) noexcept {
    std::uintptr_t device{};server::runtime::activity::mission_device_pose::Sample sample{};
    if(!addresses(request,raw,device) || !plate_pose_sample(device,sample)
        || !addresses(request,raw,device))return false;
    ds::observe_plate_pose(request.plate,sample);return true;
}

ds::PlateRequest before(void* raw) noexcept {
    for(std::size_t i=0;i<3;++i) {
        auto request=ds::plate_request(i);std::uintptr_t device{};if(!addresses(request,raw,device)) {continue;}
        contest(request);request=ds::plate_request(i);if(!addresses(request,raw,device)) {return {};}
        if(!present(request,raw))return {};
        applied[i]={request.plate,request.state.revision,request.state.armed};
        // Return the authenticated request even while the timer publication is
        // pending so the post-original sample can report native pose drift.
        return request;
    }return {};
}
void after(void* raw,const ds::PlateRequest& request) noexcept {
    if(!request.enabled) {return;}
    // Native stopped/completed timers can choose idle. Report that drift to
    // server authority; the observer never writes the component or renderer.
    if(!present(request,raw) || !request.state.armed || request.state.charged)return;
    if(!request.state.occupied || request.state.contested) {return;}
    std::uintptr_t device{};if(!addresses(request,raw,device) || !plate_consumed(raw,request.capture,request.state.revision)) {return;}
    const auto p=reinterpret_cast<std::uintptr_t>(raw);std::uint8_t active{},latched{};float value{},remaining{};
    if(!read_at(p+0x30,active) || !read_at(p+0x79,latched) || !read_at(p+0x1B8,value) || !read_at(p+0x1BC,remaining) || !addresses(request,raw,device) || !plate_consumed(raw,request.capture,request.state.revision)) {return;}
    ds::observe_plate(request.plate,request.state.revision,value,plate_native::complete(active,latched,value,remaining));
    const auto completed=ds::plate_request(request.plate.index);
    if(completed.plate==request.plate && completed.state.charged) {static_cast<void>(present(completed,raw));}
}
}
