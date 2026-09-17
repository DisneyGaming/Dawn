// Included by the existing object-damage owner. No additional detours.
namespace garden_lens_damage {
namespace mission=state::activity::strike_bond;
bool query(const void* context,mission::LensRequest& request,bool& dead) noexcept {
    const auto address=reinterpret_cast<std::uintptr_t>(context);
    gateway_native::Read read{g_image};std::uintptr_t health{},resolved{};
    std::array<std::byte,0x340> bytes{};
    if(address<0x10000 || address>UINTPTR_MAX-8 || !read.value(address+8,health) || !read.copy(health,bytes)) return false;
    const auto tag=at<std::uint32_t>(bytes.data());
    if((tag!=0x80F48026U && tag!=0x815B5AA3U)
        || at<std::uint32_t>(bytes.data()+4)!=0x80804B8AU
        || at<std::uint64_t>(bytes.data()+8)!=0xB08U) return false;
    const auto self=at<std::uint32_t>(bytes.data()+0x24),entity=at<std::uint32_t>(bytes.data()+0x2C);
    if(self==UINT32_MAX || entity==UINT32_MAX || !read.resolve(self,resolved) || resolved!=health) return false;
    const native_box_identity::Sample sample{health,self,entity,(at<std::uint8_t>(bytes.data()+0x338)&1U)!=0};
    for(std::size_t i=0;i<std::size(mission::kLenses);++i) {
        const auto q=mission::lens_request(i);const auto& lens=q.lens;
        if(!q.enabled || !lens.valid() || lens.health!=self || lens.entity!=entity
            || tag!=(mission::kLenses[i].entity==0x80F45C9CU?0x815B5AA3U:0x80F48026U)) continue;
        const native_box_identity::Owner owner{lens.source,lens.owner.value,lens.serial,lens.entity,lens.health};
        if(!native_box_identity::current(read,owner,sample,lens.asset.definition)
            || !read.weak({lens.serial,lens.entity})) return false;
        const auto again=mission::lens_request(i);
        if(again.owner!=q.owner || again.lens!=lens || again.generation!=q.generation || !again.enabled) return false;
        request=again;dead=sample.dead;return true;
    }
    return false;
}
bool blocked(const void* context) noexcept {
    mission::LensRequest request{};bool dead{};
    return query(context,request,dead) && (!request.vulnerable || request.destroyed);
}
bool allowed(const void* context,bool original) noexcept {
    mission::LensRequest request{};bool dead{};
    return query(context,request,dead)?request.vulnerable && !request.destroyed:original;
}
void receipt(const void* context) noexcept {
    mission::LensRequest request{};bool dead{};
    if(query(context,request,dead) && dead && request.vulnerable && !request.destroyed) {
        mission::observe_lens(request.lens,true);
        garden_tether::retire_destroyed();
    }
}
}
