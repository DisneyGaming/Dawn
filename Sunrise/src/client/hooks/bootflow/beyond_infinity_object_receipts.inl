// Included beside gateway_module_receipts.inl in the existing object-source hook.
void observe_beyond_object(void* raw) noexcept {
    namespace bi=state::activity::beyond_infinity;namespace gn=gateway_native;
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);
    std::array<std::byte,16> header{};if(!read.copy(source,header)) { return; }
    beyond_infinity_native::Definition definition{};std::memcpy(&definition,header.data(),sizeof definition);
    if(definition.kind!=0x80809928U) { return; }
    const bi::AssetBinding* binding{};
    for(const auto& a:bi::kAssets) { if(a.asset.type==4 && a.asset.definition==definition.tag && a.offset==definition.offset) { binding=&a;break; } }
    if(!binding) { return; }
    const auto request=bi::request();if(!request.owner.valid() || !request.frame.enabled) { return; }
    const auto& desired=request.frame.native[bi::asset_index(binding->asset)];if(!desired.managed) { return; }
    if(!desired.prepared) {
        std::array<std::byte,0x44> stateBlob{};state::activity::coo::Asset prepared{};
        if(read.copy(source+0x180,stateBlob) && beyond_infinity_native::preparation(header,stateBlob,request.frame,request.owner,prepared)) {
            bi::observe_prepared({request.owner.run,desired.generation},prepared);
        }
        return;
    }
    if((binding->asset!=bi::kLens && binding->asset!=bi::kPlate) || !desired.active || request.frame.lensDestroyed) { return; }
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{};
    if(!read.value(source+0x180,generation) || generation!=desired.generation || !read.value(source+0x2F0,committed)
        || committed!=generation || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity)) { return; }
    if(binding->asset==bi::kPlate) {
        std::uintptr_t row{},device{},timer{};std::uint32_t deviceHandle{},timerHandle{};
        gn::Read deviceRead{g_image},timerRead{g_image};
        if(!read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
            || !coo_native::component(deviceRead,bundle,entity.handle,0x80803910U,device)
            || !coo_native::component(timerRead,bundle,entity.handle,0x80804FCBU,timer)) { return; }
        std::array<std::byte,16> deviceHeader{},timerHeader{};gn::Weak after{};std::uint32_t again{};
        if(!read.copy(device,deviceHeader) || !prefix(deviceHeader.data(),0x80C7063BU,0x80803910U,0xA78)
            || !read.copy(timer,timerHeader) || !prefix(timerHeader.data(),0x815B8B3BU,0x80804FCBU,0x248)
            || !read.value(device+0x24,deviceHandle) || !read.value(timer+0x24,timerHandle)
            || !read.value(source+0x440,after) || after!=entity || !read.weak(after)
            || !read.value(source+0x180,again) || again!=generation
            || !read.value(source+0x2F0,again) || again!=generation || bi::request().owner!=request.owner) { return; }
        bi::observe_plate_binding({{request.owner.run,generation},source,entity.handle,entity.serial,deviceHandle,timerHandle});
        return;
    }
    std::uintptr_t row{};gateway_module_native_path::Probe probe{};
    if(!read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !gateway_module_native_path::find(read,bundle,entity.handle,probe)) { return; }
    gn::Weak after{};std::uint32_t again{};std::array<std::byte,16> afterHeader{};
    if(!read.value(source+0x440,after) || after!=entity || !read.weak(after)
        || !read.value(source+0x180,again) || again!=generation || !read.value(source+0x2F0,again) || again!=generation
        || !read.copy(source,afterHeader) || afterHeader!=header || bi::request().owner!=request.owner) { return; }
    const bi::LensReceipt lens{{request.owner.run,generation},source,entity.handle,entity.serial,probe.health};
    AcquireSRWLockExclusive(&g_lock);
    g_beyondLensCandidate={request.owner,lens};
    ReleaseSRWLockExclusive(&g_lock);
    bi::observe_lens(lens,probe.dead);
}
