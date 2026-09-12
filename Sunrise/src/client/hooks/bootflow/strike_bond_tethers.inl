// Only the three explicitly reserved Garden World anomaly sources are adapted.
// Authored placement is restored before the existing create callback returns.
namespace garden_tether {
namespace mission=state::activity::strike_bond;
namespace gn=gateway_native;
namespace coo=state::activity::coo;
bool replace(std::uintptr_t address,std::span<const std::byte> before,std::span<const std::byte> after) noexcept {
    if(before.size()!=after.size() || before.size()>48) return false;
    std::array<std::byte,48> current{};gn::Read read{g_image};SIZE_T written{};
    return read.copy(address,std::span(current).first(before.size()))
        && std::equal(before.begin(),before.end(),current.begin())
        && WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),after.data(),after.size(),&written)
        && written==after.size();
}
struct Placement {
    std::uintptr_t address{};DWORD protection{};
    std::array<std::byte,48> before{},after{};bool changed{};
    bool apply() noexcept {
        if(!VirtualProtect(reinterpret_cast<void*>(address),before.size(),PAGE_READWRITE,&protection)) return false;
        changed=replace(address,before,after);
        if(!changed) {DWORD unused{};VirtualProtect(reinterpret_cast<void*>(address),before.size(),protection,&unused);}
        return changed;
    }
    ~Placement() {
        if(!changed) return;
        const bool restored=replace(address,after,before);DWORD unused{};
        const bool protectedAgain=VirtualProtect(reinterpret_cast<void*>(address),before.size(),protection,&unused)!=FALSE;
        if(!restored || !protectedAgain) report("ev=strike_bond stage=tether_restore_failed address=%016llX restored=%u protection=%u",
            static_cast<unsigned long long>(address),restored?1U:0U,protectedAgain?1U:0U);
    }
};
bool point(std::uint32_t entity,mission::Point& result) noexcept {
    constexpr std::array<std::uint8_t,16> expected{0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0x83,0xC1,0xA0,0xB8,0,0,0,0,0x48};
    gn::Read read{g_image};std::array<std::uint8_t,16> actual{};std::uintptr_t rows{};std::uint32_t stride{},self{},flags{};
    if(entity==UINT32_MAX || !read.value(g_image+0x558330,actual) || actual!=expected
        || !read.value(g_image+0x1F93428,rows) || !read.value(g_image+0x1F93430,stride) || stride<0xE0 || stride>0x1000) return false;
    const auto row=rows+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
    if(!read.value(row+0xC,self) || self!=entity || !read.value(row+4,flags) || (flags&5U)) return false;
    alignas(16) std::array<float,4> value{};
    // 558330 receives the world row's embedded interface at +60, not its base.
    reinterpret_cast<void(__fastcall*)(std::uintptr_t,void*) noexcept>(g_image+0x558330)(row+0x60,value.data());
    if(!read.value(row+0xC,self) || self!=entity || !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) return false;
    result={value[0],value[1],value[2]};return true;
}
bool guardian(const mission::TetherBinding& b,coo::Generation owner,mission::Point& result) noexcept {
    const auto enemy=mission::guardian_enemy(b.source.registry,b.guardian);
    if(!enemy.valid() || enemy.run!=owner.run || enemy.generation!=owner.value) return false;
    gn::Read read{g_image};std::uintptr_t table{},source{};std::uint32_t stride{},self{},entity{},parent{},generation{},committed{};std::int64_t offset{};
    if(!read.value(g_image+0x1F9D7F8,table) || !read.value(g_image+0x1F9D800,stride) || stride<0x70 || stride>0x100000) return false;
    const auto actor=table+static_cast<std::uintptr_t>(enemy.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x48,self) || self!=enemy.actor || !read.value(actor+0x38,parent) || parent!=enemy.owner
        || !read.value(actor+0x4C,entity) || !read.value(actor+0x40,offset) || offset<0 || offset>0x1000000
        || !read.resolve(parent,source)) return false;
    source+=static_cast<std::uintptr_t>(offset);std::array<std::byte,16> header{};
    const auto* binding=mission::find(b.source.registry,1,b.guardian);
    if(!binding || !read.copy(source,header) || !prefix(header.data(),binding->asset.definition,0x8080948FU,binding->offset)
        || !read.value(source+0x1FC,generation) || generation!=enemy.generation || !read.value(source+0x244,committed) || committed!=generation
        || !point(entity,result) || !read.value(actor+0x48,self) || self!=enemy.actor || !read.value(actor+0x4C,self) || self!=entity) return false;
    return mission::guardian_enemy(b.source.registry,b.guardian)==enemy;
}
bool create(void* raw,Create original,bool enabled) noexcept {
    if(!enabled || !mission::native_run()) return original(raw);
    const auto source=reinterpret_cast<std::uintptr_t>(raw);gn::Read read{g_image};std::array<std::byte,16> header{};
    if(!read.copy(source,header)) return original(raw);
    const mission::TetherBinding* binding{};
    for(const auto& b:mission::kRouteTethers) if(prefix(header.data(),b.source.definition,0x80809928U,0x4C8)) {binding=&b;break;}
    if(!binding) return original(raw);
    const auto& b=*binding;const auto request=mission::request();const auto& state=request.frame.native[mission::asset_index(b.source)];
    if(!request.owner.valid() || !request.frame.enabled || !state.managed) return original(raw);
    // A pending create can arrive after the cube death retired its source.
    // Do not fall through and create the original anomaly in that case.
    if(!state.active || !state.desired) return false;
    const auto lens=mission::lens_request(b.lens);std::uint32_t generation{};std::uint8_t active{};
    if(lens.owner!=request.owner || !lens.enabled || lens.destroyed || !lens.lens.valid()
        || !read.value(source+0x180,generation) || generation!=state.generation || !read.value(source+0x188,active) || active!=1) return false;
    const auto resource=mission::tether_resource(b);
    mission::Point start{},end{};mission::TetherPose pose{};
    if(!read.weak({lens.lens.serial,lens.lens.entity}) || !point(lens.lens.entity,start)
        || !guardian(b,request.owner,end) || !mission::tether_pose(start,end,pose,resource.length)) return false;
    // The shared constructor dereferences the replacement entity definition.
    // An anomaly's dependency set does not guarantee that the beam is loaded.
    std::uintptr_t beam{};
    std::uintptr_t skeleton{};std::uint64_t bytes{};float length{};
    if(!read.resolve(resource.entity,beam) || !read.value(beam,bytes) || bytes!=7636
        || !read.resolve(resource.skeleton,skeleton) || !read.value(skeleton,bytes) || bytes!=848
        || !read.value(skeleton+0x300,length) || std::abs(length-resource.length)>.001F) return false;
    std::uintptr_t asset{};std::array<std::byte,8> scope{};
    if(!read.resolve(b.source.definition,asset) || !read.copy(asset+0x4F8,scope)
        || at<std::uint32_t>(scope.data())!=b.source.registry || at<std::uint16_t>(scope.data()+4)!=4
        || at<std::uint16_t>(scope.data()+6)!=b.source.slot) return false;
    Placement placement{};placement.address=asset+0x580;
    if(!read.copy(placement.address,placement.before) || at<std::uint32_t>(placement.before.data())!=0x80C00F38U) return false;
    placement.after=placement.before;const std::uint32_t beamClass=resource.entity;
    std::memcpy(placement.after.data(),&beamClass,4);
    std::memcpy(placement.after.data()+0x10,pose.rotation.data(),16);
    std::memcpy(placement.after.data()+0x20,&pose.position,12);
    std::memcpy(placement.after.data()+0x2C,&pose.scale,4);
    if(mission::request().owner!=request.owner || mission::lens_request(b.lens).destroyed || !placement.apply()) return false;
    const bool created=original(raw);
    if(created) report("ev=strike_bond stage=tether_created run=%llu slot=%u cube=%u guardian=%u length=%.3f scale=%.3f",
        static_cast<unsigned long long>(request.owner.run),b.source.slot,b.cube,b.guardian,pose.scale*resource.length,pose.scale);
    return created;
}
// Use the same native local-pose setter used by hierarchy movement. For these
// unparented visual entities local pose is world pose. It encodes translation,
// dirties the entity and notifies its components; raw world-row writes do not.
bool follow(const mission::TetherBinding& b,coo::Generation owner,std::uint32_t bundle,gn::Weak entity) noexcept {
    const auto current=mission::request();const auto lens=mission::lens_request(b.lens);
    if(mission::tether_visibility(b,current,lens,owner)!=1.F || !lens.lens.valid()) return false;
    gn::Read read{g_image};std::uintptr_t row{};std::uint32_t parent{},actualBundle{},flags{};
    if(!read.entity_row(entity,row) || !read.value(row+0x3C,parent) || parent!=UINT32_MAX
        || !read.value(row+4,flags) || (flags&5U) || !read.value(row+0x4C,actualBundle) || actualBundle!=bundle
        || !read.weak({lens.lens.serial,lens.lens.entity})) return false;
    mission::Point start{},end{};mission::TetherPose pose{};
    if(!point(lens.lens.entity,start) || !guardian(b,owner,end)
        || !mission::tether_pose(start,end,pose,mission::tether_resource(b).length)) return false;
    constexpr std::array<std::uint8_t,11> wrapper{0x41,0xB8,0x02,0,0,0,0xE9,0xF5,0x01,0,0};
    constexpr std::array<std::uint8_t,16> setter{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x0F};
    std::array<std::uint8_t,11> entry{};std::array<std::uint8_t,16> body{};
    if(!read.value(g_image+0x559A00,entry) || entry!=wrapper
        || !read.value(g_image+0x559C00,body) || body!=setter) return false;
    const auto fresh=mission::lens_request(b.lens);std::uintptr_t again{};
    if(fresh.owner!=lens.owner || fresh.lens!=lens.lens
        || mission::tether_visibility(b,mission::request(),fresh,owner)!=1.F
        || !read.entity_row(entity,again) || again!=row || !read.value(row+0x3C,parent) || parent!=UINT32_MAX
        || !read.value(row+0x4C,actualBundle) || actualBundle!=bundle) return false;
    static_assert(sizeof(mission::TetherPose)==32);
    alignas(16) const auto aligned=pose;
    using SetPose=void(__fastcall*)(std::uintptr_t,const void*) noexcept;
    reinterpret_cast<SetPose>(g_image+0x559A00)(row,&aligned);
    return true;
}
struct Retained {coo::Generation owner{};std::uint32_t bundle{};gn::Weak entity{};bool following{};};
SRWLOCK retainedLock=SRWLOCK_INIT;
std::array<Retained,3> retained{};
void visible(const mission::TetherBinding& b,coo::Generation owner,std::uint32_t bundle,gn::Weak entity) noexcept {
    const auto request=mission::lens_request(b.lens);
    const auto current=mission::request();
    const auto visibility=mission::tether_visibility(b,current,request,owner);
    if(!visibility) return;
    gn::Read read{g_image};std::uintptr_t component{};std::array<std::byte,0x30> header{};
    if(!read.weak(entity) || !coo_native::component<gn::Read,1024>(read,bundle,entity.handle,0x80803910U,component)
        || !read.copy(component,header) || !prefix(header.data(),0x80C7063BU,0x80803910U,0xA78)
        || at<std::uint32_t>(header.data()+0x2C)!=entity.handle) return;
    AcquireSRWLockExclusive(&retainedLock);
    for(std::size_t i=0;i<retained.size();++i) if(mission::kRouteTethers[i].source==b.source) {
        const bool same=retained[i].owner==owner && retained[i].bundle==bundle && retained[i].entity==entity;
        retained[i]={owner,bundle,entity,same && retained[i].following};
    }
    ReleaseSRWLockExclusive(&retainedLock);
    if(*visibility==1.F && follow(b,owner,bundle,entity)) {
        bool first{};AcquireSRWLockExclusive(&retainedLock);
        for(std::size_t i=0;i<retained.size();++i) if(mission::kRouteTethers[i].source==b.source
            && retained[i].owner==owner && retained[i].entity==entity) {
            first=!retained[i].following;retained[i].following=true;
        }
        ReleaseSRWLockExclusive(&retainedLock);
        if(first) report("ev=strike_bond stage=tether_following run=%llu slot=%u guardian=%u entity=%08X",
            static_cast<unsigned long long>(owner.run),b.source.slot,b.guardian,entity.handle);
    }
    const auto self=at<std::uint32_t>(header.data()+0x24);std::uintptr_t fresh{};float before{};const float after=*visibility;
    // Same bounded visibility channels used in the confirmed live test. No
    // health, immunity or actor state is changed by the presentation adapter.
    for(const auto offset:{0x37CU,0x370U}) {
        gn::Read check{g_image};std::array<std::byte,0x30> again{};
        if(!check.resolve(self,fresh) || fresh!=component || !check.copy(fresh,again) || again!=header
            || !check.weak(entity) || !check.value(fresh+offset,before) || !std::isfinite(before) || before==after
            || mission::tether_visibility(b,mission::request(),mission::lens_request(b.lens),owner)!=visibility) continue;
        static_cast<void>(replace(fresh+offset,std::as_bytes(std::span{&before,std::size_t{1}}),std::as_bytes(std::span{&after,std::size_t{1}})));
    }
}
// The cube's native death callback can precede the retired source's next sense
// callback. Hide verified retained entities here too; a cached pointer alone
// is never used as identity evidence.
void retire_destroyed() noexcept {
    AcquireSRWLockShared(&retainedLock);const auto entries=retained;ReleaseSRWLockShared(&retainedLock);
    for(std::size_t i=0;i<entries.size();++i) {
        const auto& entry=entries[i];const auto& binding=mission::kRouteTethers[i];
        if(entry.owner.valid() && mission::lens_request(binding.lens).destroyed)
            visible(binding,entry.owner,entry.bundle,entry.entity);
    }
}
}
