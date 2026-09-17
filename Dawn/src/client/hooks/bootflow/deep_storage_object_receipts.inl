// Reuse 9EFFC0/9F0750. These callbacks observe ownership; they do not run animation setters.
void observe_deep_storage_object(void* raw) noexcept {
    namespace ds=state::activity::deep_storage;namespace gn=gateway_native;
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);std::array<std::byte,16> header{};
    if(!read.copy(source,header) || at<std::uint32_t>(header.data()+4)!=0x80809928U) {return;}
    const ds::AssetBinding* binding{};
    for(const auto& a:ds::kAssets) {if(a.asset.type==4 && prefix(header.data(),a.asset.definition,0x80809928U,a.offset)) {binding=&a;break;}}
    if(!binding) {return;}const auto request=ds::request();if(!request.owner.valid() || !request.frame.enabled) {return;}
    const auto& desired=request.frame.native[ds::asset_index(binding->asset)];if(!desired.managed) {return;}
    if(!desired.prepared) {
        std::array<std::byte,0x44> bytes{};
        if(read.copy(source+0x180,bytes) && at<std::uint32_t>(bytes.data())==request.owner.value
            && state::activity::coo::native_device::inactive_state(bytes)) {ds::observe_prepared(request.owner,binding->asset);}return;
    }
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t row{};
    if(!desired.active || !read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)) {return;}
    if(committed!=generation) {
        if(binding->asset!=ds::hologram_owner::kSource) {return;}
        gn::Read componentRead{g_image};std::uintptr_t generic{};std::array<std::byte,0x30> h{};
        if(!coo_native::component(componentRead,bundle,entity.handle,0x80803910U,generic) || !read.copy(generic,h)
            || !ds::hologram_owner::retained(request.owner,binding->asset,desired,generation,committed,entity.handle,
                {at<std::uint32_t>(h.data()),at<std::uint32_t>(h.data()+4),at<std::uint32_t>(h.data()+0x24),
                 at<std::uint32_t>(h.data()+0x2C),at<std::uint64_t>(h.data()+8)})) {return;}
    }
    std::uint32_t stableGeneration{},stableCommitted{};std::uint8_t stableActive{};
    if(!read.value(source+0x440,again) || again!=entity || !read.weak(again)
        || !read.value(source+0x180,stableGeneration) || stableGeneration!=generation
        || !read.value(source+0x2F0,stableCommitted) || stableCommitted!=committed
        || !read.value(source+0x188,stableActive) || stableActive!=1 || ds::request().owner!=request.owner) {return;}
    ds::observe_object({{request.owner.run,generation},binding->asset,entity.handle,entity.serial});
    if(binding->asset==ds::kLens) {
        gn::Read componentRead{g_image},identityRead{g_image};std::uintptr_t health{};
        std::array<std::byte,0x340> bytes{};
        if(!coo_native::component<gn::Read,1024>(componentRead,bundle,entity.handle,0x80804B8AU,health)
            || !identityRead.copy(health,bytes)) {return;}
        const auto handle=at<std::uint32_t>(bytes.data()+0x24);
        if(!native_box_identity::health(bytes,handle,entity.handle)) {return;}
        const ds::LensReceipt lens{{request.owner.run,generation},source,entity.handle,entity.serial,handle};
        const native_box_identity::Sample sample{health,handle,entity.handle,native_box_identity::dead(bytes,handle,entity.handle)};
        const deep_storage_lens_damage::Candidate candidate{request.owner,lens};
        if(ds::request().owner!=request.owner || !deep_storage_lens_damage::current(identityRead,ds::lens_request(),candidate,sample)) {return;}
        AcquireSRWLockExclusive(&g_lock);g_deepLensCandidate=candidate;ReleaseSRWLockExclusive(&g_lock);
        ds::observe_lens(lens,sample.dead);return;
    }
    for(std::uint8_t i=0;i<3;++i) {if(binding->asset!=ds::kPlates[i].source) {continue;}
        std::uintptr_t device{},timer{};std::uint32_t deviceHandle{},timerHandle{};std::array<std::byte,16> d{},t{};
        gn::Read deviceRead{g_image},timerRead{g_image};
        if(!coo_native::component(deviceRead,bundle,entity.handle,0x80803910U,device)
            || !coo_native::component(timerRead,bundle,entity.handle,0x80804FCBU,timer)
            || !read.copy(device,d) || !prefix(d.data(),0x80C7063BU,0x80803910U,0xA78)
            || !read.copy(timer,t) || !prefix(t.data(),0x815B8B3BU,0x80804FCBU,0x248)
            || !read.value(device+0x24,deviceHandle) || !read.value(timer+0x24,timerHandle)
            || !read.value(source+0x440,again) || again!=entity || !read.weak(again)) {return;}
        ds::observe_plate_binding({{request.owner.run,generation},source,i,entity.handle,entity.serial,deviceHandle,timerHandle});return;
    }
    for(std::uint8_t i=0;i<2;++i) {if(binding->asset!=ds::kScans[i].source) {continue;}
        std::uintptr_t controller{};std::uint32_t handle{};std::array<std::byte,16> c{};gn::Read componentRead{g_image};
        // E4A590's linked Ghost device uses the 80804D3A definition reference
        // in its live header (as captured for Deadly Trial), not 80804D3B.
        if(!coo_native::component(componentRead,bundle,entity.handle,0x80804D3AU,controller)
            || !read.copy(controller,c) || !prefix(c.data(),ds::kScans[i].controllerDefinition,0x80804D3AU,0x358)
            || !read.value(controller+0x24,handle) || !read.value(source+0x440,again) || again!=entity || !read.weak(again)) {return;}
        ds::observe_scan_binding({{request.owner.run,generation},source,i,entity.handle,entity.serial,handle});return;
    }
}
