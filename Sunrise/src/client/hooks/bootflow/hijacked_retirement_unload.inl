// 424D10 owns native area unload and calls A07BE0 before its general keep/remove
// and teardown sweeps. Retain exact exterior identities before native detachment;
// extend only A07BE0's boolean selection, leaving the engine to mark and sweep.
struct RetirementUnload {
    state::activity::coo::Generation run{};std::uint32_t destination{};
    std::array<mission::RetirementEnemy,256> targets{};
    std::array<bool,256> selected{};
    std::size_t count{};unsigned predicates{},matches{},rejected{};
    mission::Request request{};mission::LivingEnemies living{};BacktrackingBatch handoff{};bool handoffAttempted{};
};
thread_local RetirementUnload* retirementUnload{};
bool unload_source_enabled(const mission::Request& request,std::uint16_t source) noexcept {
    if(!request.owner.valid() || !request.frame.enabled || request.frame.finished || source<1 || source>7) {return false;}
    const auto asset=mission::find(0x3E9B74F3U,1,source)->asset;
    const auto& state=request.frame.native[mission::asset_index(asset)];
    // Native area exit also works if Lua has not yet published its retirement
    // command. Neither deaths nor cave objective progress authorize this path.
    return state.managed && (state.active || state.retired || state.suspended);
}
void unload_report(const char* stage,const RetirementUnload& frame) noexcept {
    std::array<char,320> line{};
    std::snprintf(line.data(),line.size(),
        "ev=hijacked_retirement stage=unload_%s run=%llu destination=%u retained=%zu predicates=%u selected=%u rejected=%u",
        stage,static_cast<unsigned long long>(frame.run.run),frame.destination,frame.count,frame.predicates,frame.matches,frame.rejected);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
__declspec(noinline) void __fastcall unload_region(std::uintptr_t manager,std::uint32_t destination,std::uintptr_t argument) noexcept {
    const hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(unloadRegion);
    // Vendors use the same native unload scope even outside Hijacked. Pair all
    // returns and nested unloads; no new physical hook or component predicate.
    struct VendorUnload {
        const hooking::CallGate::Scope& scope;
        VendorUnload(const hooking::CallGate::Scope& s) noexcept : scope(s) {begin_vendor_area_unload();}
        ~VendorUnload() {finish_vendor_area_unload(scope.accepts_side_effects() && retirement_allocator_ready());}
    } vendorUnload{scope};
    // Mask a parent selection during any nested unload, including another region.
    struct Restore {RetirementUnload* previous;~Restore() {retirementUnload=previous;}} restore{retirementUnload};
    retirementUnload=nullptr;
    const auto request=mission::request();
    if(!scope.accepts_side_effects() || destination!=mission::kMistsUnloadRegion
        || !request.owner.valid() || !request.frame.enabled || request.frame.finished) {fn(manager,destination,argument);return;}
    RetirementUnload frame{};frame.run=request.owner;frame.destination=destination;frame.request=request;frame.living=mission::living_enemies();
    retirement_capture(request);
    AcquireSRWLockShared(&retirementLock);
    for(const auto& target:retirementTargets) {
        if(target.run==request.owner && unload_source_enabled(request,target.source) && frame.count<frame.targets.size()) {
            frame.targets[frame.count++]=target;
        }
    }
    ReleaseSRWLockShared(&retirementLock);
    backtracking_sources(request,frame.living,frame.handoff);
    unload_report("begin",frame);retirementUnload=&frame;
    // No cache/placement lock is held while native component callbacks execute.
    const bool wasUnloading=backtrackingUnloading;backtrackingUnloading=true;
    fn(manager,destination,argument);
    retirementUnload=nullptr;
    backtracking_finish(frame.request,frame.handoff);
    backtrackingUnloading=wasUnloading;
    for(std::size_t i=0;i<frame.count;++i) {
        mission::RetirementWorld world{};std::array<std::byte,0x50> entity{};
        const auto state=retirement_entity_read(frame.targets[i],world,entity);
        retirement_cleanup_report(frame.selected[i]?"unload_selected_after":"unload_unselected_after",frame.targets[i],static_cast<unsigned>(state));
    }
    unload_report("end",frame);
}
__declspec(noinline) bool __fastcall find_component(std::uintptr_t address,std::uint32_t component,std::uint32_t bundle,void* output) noexcept {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(findComponent);
    const bool result=fn(address,component,bundle,output);
    auto* frame=retirementUnload;
    if(!scope.accepts_side_effects() || !frame || caller!=image+mission::kUnloadDiscardCaller
        || component!=mission::kUnloadDiscardComponent) {return result;}
    if(!frame->handoffAttempted) {
        frame->handoffAttempted=true;
        backtracking_suspend(frame->request,std::span(frame->targets).first(frame->count),frame->living,frame->handoff);
    }
    ++frame->predicates;
    Read read{image};std::uint32_t handle{UINT32_MAX};
    if(!read.value(address+0x0C,handle)) {return result;}
    for(std::size_t i=0;i<frame->count;++i) {
        const auto& target=frame->targets[i];
        if(target.entity!=handle) {continue;}
        const auto request=mission::request();
        mission::RetirementWorld world{};std::array<std::byte,0x50> entity{};
        const auto state=retirement_entity_read(target,world,entity);
        unsigned reason{};
        if(request.owner!=frame->run || !unload_source_enabled(request,target.source)) {reason=1;}
        else if(state!=mission::RetirementRead::live) {reason=2;}
        else if(!mission::retirement_unload_matches(target,request.owner,frame->destination,caller-image,component,address,bundle,world,entity)) {reason=3;}
        else if(!retirement_owner_unchanged(target)) {reason=4;}
        else if(!retirement_allocator_ready()) {reason=5;}
        if(reason) {++frame->rejected;retirement_cleanup_report("unload_rejected",target,reason);return result;}
        // Native state can change during the readback walk. Recheck identity and the
        // mission lease immediately before returning selection to native code.
        const auto live=mission::request();
        if(!scope.accepts_side_effects() || live.owner!=frame->run || !unload_source_enabled(live,target.source)
            || retirement_entity_read(target,world,entity)!=mission::RetirementRead::live
            || !mission::retirement_unload_matches(target,live.owner,frame->destination,caller-image,component,address,bundle,world,entity)) {
            ++frame->rejected;retirement_cleanup_report("unload_rejected",target,6);return result;
        }
        if(!frame->selected[i]) {frame->selected[i]=true;++frame->matches;}
        retirement_cleanup_report("unload_selected",target,result?1U:0U);
        // This specific caller reads only AL, then calls 56A8F0 itself. Preserve
        // the original output bytes; no fake component, TLS or authority writes.
        const auto& sourceState=request.frame.native[mission::asset_index(mission::find(0x3E9B74F3U,1,target.source)->asset)];
        return sourceState.retired || sourceState.suspended?true:result;
    }
    return result;
}
