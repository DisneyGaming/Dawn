// Share the existing E4A590 observation boundary. Original native interaction,
// player input, Ghost binding and animation run untouched before this observer.
namespace deep_scan {
namespace ds=state::activity::deep_storage;
bool current(const ds::ScanRequest& request) noexcept {
    const auto next=ds::scan_request(request.scan.index);
    return next.enabled && next.owner==request.owner && next.scan==request.scan && !next.complete;
}
bool capture(Read& read,std::uintptr_t sensor,const ds::ScanRequest& request,std::uintptr_t& device) noexcept {
    const auto& r=request.scan;const auto& binding=ds::kScans[r.index];
    Ref ref{},ownerRef{},sourceRef{};Weak weak{},entity{};std::uintptr_t definition{},row{};
    std::uint32_t self{},parent{},generation{},committed{};std::uint8_t active{};std::array<std::uint32_t,2> scope{};
    const auto* source=ds::find(binding.source.registry,4,binding.source.slot);
    return source && read.value(sensor,ref) && ref.handle==binding.link.definition && ref.kind==0x80804D32U && ref.offset==0x258
        && read.resolve(ref.handle,definition) && read.value(definition+0x288,scope)
        && scope[0]==binding.link.registry && scope[1]==65U
        && read.value(sensor+0x1D8,weak) && weak.handle==r.controller && read.weak(weak) && read.resolve(weak.handle,device)
        && read.value(device,ownerRef) && ownerRef.handle==binding.controllerDefinition && ownerRef.kind==0x80804D3AU && ownerRef.offset==0x358
        && read.value(device+0x24,self) && self==r.controller && read.value(device+0x2C,parent) && parent==r.entity
        && read.value(r.source,sourceRef) && sourceRef.handle==binding.source.definition && sourceRef.kind==0x80809928U && sourceRef.offset==source->offset
        && read.value(r.source+0x180,generation) && generation==r.owner.value
        && read.value(r.source+0x2F0,committed) && committed==generation && read.value(r.source+0x188,active) && active==1
        && read.value(r.source+0x440,entity) && entity.handle==r.entity && entity.serial==r.serial && read.entity_row(entity,row)
        && current(request);
}
void update(std::uintptr_t sensor) noexcept {
    for(std::uint8_t i=0;i<2;++i) {
        const auto request=ds::scan_request(i);if(!request.enabled || !request.scan.valid() || request.complete) {continue;}
        Read read{image};std::uintptr_t device{};if(!capture(read,sensor,request,device)) {continue;}
        ds::ScanPlayback playback{};
        if(!read.value(device+0x294,playback.revision) || !read.value(device+0x298,playback.mode)
            || !read.value(device+0x299,playback.active) || !read.value(device+0x29C,playback.elapsed)
            || !read.value(device+0x290,playback.duration)) {return;}
        bool participant{};std::array<Weak,6> members{};
        if(read.value(device+0x80,members)) {for(const auto& member:members) {
            std::uintptr_t actor{};Ref ref{};std::uint32_t self{};
            if(!read.weak(member) || !read.resolve(member.handle,actor) || !read.value(actor,ref)
                || ref.kind!=0x80803F45U || !read.value(actor+0x24,self) || self!=member.handle) {continue;}
            Weak target{};std::int64_t offset{};
            if(read.value(actor+0x1C8,target) && target.handle==request.scan.controller && read.weak(target)
                && read.value(actor+0x1D0,offset) && offset==0x30) {participant=true;break;}
        }}
        std::uintptr_t again{};if(!capture(read,sensor,request,again) || again!=device) {return;}
        ds::observe_scan_playback(request.scan,playback,participant);
        return;
    }
}
}
