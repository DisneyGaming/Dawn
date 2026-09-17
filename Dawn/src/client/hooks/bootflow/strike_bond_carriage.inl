// Runs on the existing native character-update boundary. The engine's attach
// routine owns hierarchy registration, relative transform and subsequent motion.
namespace garden_carriage {
namespace policy=strike_bond_carriage;
namespace gn=gateway_native;
using Lookup=std::uint64_t(__fastcall*)(std::uintptr_t,const std::uint32_t*,std::uint32_t,void*,const std::uint32_t*) noexcept;
using Interface=void(__fastcall*)(const void*,void*) noexcept;
using Attach=void(__fastcall*)(std::uint32_t,const void*,std::uint32_t) noexcept;
SRWLOCK lock=SRWLOCK_INIT;garden::EnemyReceipt observed{};unsigned reports{};bool busy{};std::uint64_t nextCheck{};
void reset() noexcept {observed={};reports=0;busy=false;nextCheck=0;}
void report(const garden::BossRequest& r,const char* stage,std::uint32_t parent=UINT32_MAX) noexcept {
    bool emit{};AcquireSRWLockExclusive(&lock);if(observed==r.enemy && reports<8){++reports;emit=true;}ReleaseSRWLockExclusive(&lock);
    if(!emit)return;
    std::array<char,384> line{};
    std::snprintf(line.data(),line.size(),"ev=garden_carriage stage=%s run=%llu generation=%u actor=%08X platform=%08X parent=%08X bone=1 socket=9F6DB313",
        stage,r.owner.run,r.owner.value,r.enemy.actor,r.platform.entity,parent);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
struct Binding {
    strike_bond_fire_trace::Identity character{};
    std::uintptr_t bossRow{},plateRow{},provider{},body{};
    std::uint32_t providerSelf{},bodySelf{},parent{UINT32_MAX},bone{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
bool sample(void* character,const garden::BossRequest& r,Binding& b) noexcept {
    gn::Read read{g_image};std::uint32_t value{},flags{},bundle{},stride{};std::uintptr_t rows{};
    if(!policy::wanted(r) || !strike_bond_fire_trace::sample(read,g_image,reinterpret_cast<std::uintptr_t>(character),false,r,b.character)
        || !read.value(g_image+0x1F93428,rows) || !read.value(g_image+0x1F93430,stride) || stride<0xE0 || stride>0x1000) return false;
    b.bossRow=rows+static_cast<std::uintptr_t>(b.character.entity&0x1FFFU)*stride;
    if(b.character.entity==r.platform.entity || !read.value(b.bossRow+0xC,value) || value!=b.character.entity
        || !read.value(b.bossRow+4,flags) || (flags&5U) || !read.value(b.bossRow+0x3C,b.parent)
        || !read.value(b.bossRow+0x38,b.bone)
        || !read.entity_row({r.platform.serial,r.platform.entity},b.plateRow)
        || !read.value(b.plateRow+0xC,value) || value!=r.platform.entity
        || !read.value(b.plateRow+4,flags) || (flags&5U)
        || !read.value(b.plateRow+0x4C,bundle)) return false;
    gn::Read components{g_image};
    if(!coo_native::component<gn::Read,1024>(components,bundle,r.platform.entity,0x80808507U,b.provider)
        || !coo_native::component<gn::Read,1024>(components,bundle,r.platform.entity,0x80808546U,b.body)) return false;
    std::array<std::byte,0x30> h{};
    if(!read.copy(b.provider,h) || policy::at<std::uint32_t>(h,0)!=0x80F45991U
        || policy::at<std::uint32_t>(h,4)!=0x80808507U || policy::at<std::uint32_t>(h,0x2C)!=r.platform.entity) return false;
    b.providerSelf=policy::at<std::uint32_t>(h,0x24);
    if(!read.copy(b.body,h) || policy::at<std::uint32_t>(h,0)!=0x80F4596FU
        || policy::at<std::uint32_t>(h,4)!=0x80808546U || policy::at<std::uint32_t>(h,0x2C)!=r.platform.entity) return false;
    b.bodySelf=policy::at<std::uint32_t>(h,0x24);
    return b.providerSelf!=UINT32_MAX && b.bodySelf!=UINT32_MAX;
}
bool boundaries() noexcept {
    struct Site {std::uintptr_t rva;std::array<std::uint8_t,16> prefix;};
    constexpr Site sites[]{
        {0xA205E0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}},
        {0xA1D700,{0x8B,0x41,0x10,0x4C,0x8B,0xDA,0x4C,0x8B,0xD1,0x3D,0xED,0xFE,0x0D,0xF0,0x74,0x74}},
        {0x5693E0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x41}},
        {0x569420,{0x4C,0x89,0x4C,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x55}}};
    gn::Read read{g_image};std::array<std::byte,16> bytes{};
    for(const auto& s:sites) if(!read.copy(g_image+s.rva,bytes) || std::memcmp(bytes.data(),s.prefix.data(),16)) return false;
    return true;
}
void after_update(void* character) noexcept {
    const auto request=garden::boss_request();if(!policy::wanted(request))return;
    AcquireSRWLockExclusive(&lock);
    if(observed!=request.enemy){reset();observed=request.enemy;}
    const auto now=GetTickCount64();
    const bool claim=!busy && now>=nextCheck;if(claim){busy=true;nextCheck=now+1000;}
    ReleaseSRWLockExclusive(&lock);if(!claim)return;
    struct Release {garden::EnemyReceipt enemy;~Release(){AcquireSRWLockExclusive(&lock);if(observed==enemy)busy=false;ReleaseSRWLockExclusive(&lock);}} release{request.enemy};
    Binding b{};if(!sample(character,request,b)) {report(request,"identity_guard");return;}
    if(b.parent==request.platform.entity && b.bone==policy::kBone) return;
    if(b.parent!=UINT32_MAX) {report(request,"other_parent",b.parent);return;}
    if(!boundaries()){report(request,"native_boundary_guard");return;}
    alignas(16) std::array<std::byte,80> socket{};const std::uint32_t flags{};
    const auto found=reinterpret_cast<Lookup>(g_image+0xA205E0)(b.plateRow,&policy::kSocket,1,socket.data(),&flags);
    gn::Read read{g_image};std::array<std::byte,64> marker{};
    if(found!=1 || !policy::pose(socket,b.providerSelf)
        || !read.copy(policy::at<std::uintptr_t>(socket,0),marker) || !policy::socket(marker)) {report(request,"socket_guard");return;}
    alignas(16) std::array<std::byte,40> parent{};
    reinterpret_cast<Interface>(g_image+0xA1D700)(socket.data()+0x30,parent.data());
    if(!policy::parent_interface(parent,b.bodySelf)){report(request,"interface_guard");return;}
    // Keep-world attach computes the offset from bone 1 itself. Applying the
    // marker's model-local offset a second time would move the boss off the plate.
    const auto current=garden::boss_request();Binding again{};
    if(current.owner!=request.owner || current.enemy!=request.enemy || current.platform!=request.platform
        || !sample(character,current,again) || again!=b) return;
    reinterpret_cast<Attach>(g_image+0x5693E0)(b.character.entity,parent.data(),policy::kBone);
    Binding after{};const auto latest=garden::boss_request();
    const bool attached=latest.owner==request.owner && latest.enemy==request.enemy && latest.platform==request.platform
        && sample(character,latest,after) && after.parent==request.platform.entity && after.bone==policy::kBone;
    report(request,attached?"attached":"attach_unconfirmed",after.parent);
}
} // namespace garden_carriage
