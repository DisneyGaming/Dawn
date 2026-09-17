// Native named-sequence delivery on the already-owned character update.
namespace garden_ending_animation {
struct Binding {
    garden::EndingActor actor{};std::uintptr_t controller{},selector{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
bool sample(gateway_native::Read& read,std::uintptr_t character,Binding& result) noexcept {
    std::array<std::byte,0x30> header{};std::uintptr_t resolved{},rows{};std::uint32_t stride{},bundle{},flags{},full{},serial{},value{};
    if(!read.copy(character,header) || strike_bond_fire_trace::field<std::uint32_t>(header,0)!=0x80F4516EU
        || strike_bond_fire_trace::field<std::uint32_t>(header,4)!=0x80803A00U) return false;
    const auto self=strike_bond_fire_trace::field<std::uint32_t>(header,0x24),entity=strike_bond_fire_trace::field<std::uint32_t>(header,0x2c);
    if(self==UINT32_MAX || entity==UINT32_MAX || !read.resolve(self,resolved) || resolved!=character
        || !read.value(g_image+0x1F93428,rows)||!read.value(g_image+0x1F93430,stride)||stride<0xE0||stride>0x1000)return false;
    const auto row=rows+static_cast<std::uintptr_t>(entity&8191)*stride;
    if(!read.value(row+12,full)||full!=entity||!read.value(row,serial)
        ||!read.value(row+4,flags)||(flags&5U)||!read.value(row+0x4c,bundle))return false;
    Binding b;b.actor.entity=entity;b.actor.serial=serial;
    if(!coo_native::component<gateway_native::Read,1024>(read,bundle,entity,0x80806832U,b.controller)
        ||!coo_native::component<gateway_native::Read,1024>(read,bundle,entity,0x8080686CU,b.selector))return false;
    if(!strike_bond_intro_release::component_header(read,b.controller,0x80F6690BU,0x80806832U,0x738,entity,b.actor.controller)
        ||!read.value(b.selector,value)||value!=0x80F45174U
        ||!read.value(b.selector+0x24,b.actor.selector)||!read.resolve(b.actor.selector,resolved)||resolved!=b.selector
        ||!read.value(b.selector+0x30,value)||value!=b.actor.controller)return false;
    std::uint64_t count{};
    if(!read.value(b.selector+0x40,count)||count!=8||!read.value(b.selector+0xB4+7*64,value)||value>1)return false;
    // Full native actor backlink, independent of the entity table's low index.
    std::uint32_t actor{},actorStride{};std::uintptr_t actors{};
    if(!read.value(b.controller+0xC0,actor)||actor==UINT32_MAX
        ||!read.value(g_image+0x1F9D7F8,actors)||!read.value(g_image+0x1F9D800,actorStride)
        ||actorStride<0x70||actorStride>0x100000)return false;
    const auto actorRow=actors+static_cast<std::uintptr_t>(actor&8191)*actorStride;
    if(!read.value(actorRow+0x48,value)||value!=actor||!read.value(actorRow+0x4c,value)||value!=entity)return false;
    result=b;return b.actor.valid();
}
void after_update(void* character) noexcept {
    const auto r=garden::request();const auto& ending=r.frame.endingFlow;
    if(!r.frame.campaign || !r.frame.enabled || r.frame.region!=136 || !ending.wipe || ending.retire)return;
    gateway_native::Read read{g_image};Binding b,again;
    const auto address=reinterpret_cast<std::uintptr_t>(character);
    if(!sample(read,address,b))return;
    std::uint32_t active{};if(!read.value(b.selector+0xB4+7*64,active))return;
    if(ending.claimed) {
        if(ending.actor==b.actor) garden::observe_ending_animation(r.owner,b.actor,active!=0,GetTickCount64());
        return;
    }
    constexpr std::array<std::uint8_t,16> prefix{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C,0x24,0x10,0x55,0x48,0x8B,0xEC,0x48,0x83};
    std::array<std::uint8_t,16> bytes{};
    gateway_native::Read checked{g_image};
    if(!read.value(g_image+0xC620F0,bytes)||bytes!=prefix||!sample(checked,address,again)||again!=b
        ||!garden::claim_ending_animation(r.owner,b.actor))return;
    const auto action=strike_bond_boss_cycle::action(garden::kEndingWipe);
    using Start=void(__fastcall*)(void*,const void*,void*) noexcept;
    reinterpret_cast<Start>(g_image+0xC620F0)(reinterpret_cast<void*>(b.controller),action.data(),nullptr);
    // Keep the scene's authored holding position; do not stop or retire its actor.
    if(read.value(b.selector+0xB4+7*64,active))garden::observe_ending_animation(r.owner,b.actor,active!=0,GetTickCount64());
}
}
