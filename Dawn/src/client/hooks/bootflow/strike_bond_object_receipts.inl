// Receipts attached to Dawn's existing object source callbacks. Route beam
// presentation is enabled only after salted native creation is verified;
// health, damage eligibility and guardian release remain mission-owned.
void observe_strike_bond_object(void* raw) noexcept {
    namespace garden=state::activity::strike_bond;namespace gn=gateway_native;
    const auto run=garden::native_run();if(!run) return;
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);
    std::array<std::byte,16> header{};if(!read.copy(source,header)) return;
    const garden::AssetBinding* binding{};
    for(const auto& a:garden::kAssets) if(a.asset.type==4 && prefix(header.data(),a.asset.definition,0x80809928U,a.offset)) {binding=&a;break;}
    if(!binding) return;
    const auto request=garden::request();if(!request.owner.valid() || !request.frame.enabled) return;
    const auto& desired=request.frame.native[garden::asset_index(binding->asset)];if(!desired.managed) return;
    if(!desired.prepared) {
        std::array<std::byte,0x44> bytes{};
        if(read.copy(source+0x180,bytes) && at<std::uint32_t>(bytes.data())==request.owner.value
            && state::activity::coo::native_device::inactive_state(bytes)) garden::observe_prepared(request.owner,binding->asset);
        return;
    }
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t row{};
    // Retired sources can retain a live render entity. Apply the explicit zero
    // before the active-only receipt path, while its salted identity still exists.
    if(!read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed)
        || !read.value(source+0x440,entity) || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)) return;
    if(committed!=generation) {
        const auto kind=garden::ending_object::kind(binding->asset);
        if(!request.frame.campaign || !kind) return;
        gn::Read componentRead{g_image};std::uintptr_t component{};std::array<std::byte,0x30> identity{};
        if(!coo_native::component(componentRead,bundle,entity.handle,kind,component) || !read.copy(component,identity)
            || !garden::ending_object::retained(request.owner,binding->asset,desired,generation,committed,entity.handle,
                {at<std::uint32_t>(identity.data()),at<std::uint32_t>(identity.data()+4),at<std::uint32_t>(identity.data()+0x24),
                 at<std::uint32_t>(identity.data()+0x2C),at<std::uint64_t>(identity.data()+8)})) return;
    }
    std::uint32_t stableGeneration{},stableCommitted{};
    if(!read.value(source+0x180,stableGeneration) || stableGeneration!=generation
        || !read.value(source+0x2F0,stableCommitted) || stableCommitted!=committed) return;
    if(!read.value(source+0x440,again) || again!=entity || !read.weak(again) || garden::request().owner!=request.owner) return;
    if(const auto* tether=garden::route_tether(binding->asset)) garden_tether::visible(*tether,request.owner,bundle,entity);
    if(!desired.active || !read.value(source+0x188,active) || active!=1) return;
    garden::observe_object({{run,generation},binding->asset,entity.handle,entity.serial});
    const auto index=garden::lens_index(binding->asset);if(index==std::size(garden::kLenses)) return;
    gn::Read componentRead{g_image},healthRead{g_image};std::uintptr_t health{};std::array<std::byte,0x340> bytes{};
    if(!coo_native::component<gn::Read,1024>(componentRead,bundle,entity.handle,0x80804B8AU,health) || !healthRead.copy(health,bytes)) return;
    const auto tag=at<std::uint32_t>(bytes.data()),kind=at<std::uint32_t>(bytes.data()+4);
    const auto handle=at<std::uint32_t>(bytes.data()+0x24);
    const bool boss=garden::kLenses[index].entity==0x80F45C9CU;
    if(tag!=(boss?0x815B5AA3U:0x80F48026U) || kind!=0x80804B8AU || handle==UINT32_MAX || at<std::uint32_t>(bytes.data()+0x2C)!=entity.handle) return;
    if(!read.value(source+0x440,again) || again!=entity || !read.weak(again) || garden::request().owner!=request.owner) return;
    garden::observe_lens({{run,generation},binding->asset,source,entity.handle,entity.serial,handle},(at<std::uint8_t>(bytes.data()+0x338)&1U)!=0);
}
