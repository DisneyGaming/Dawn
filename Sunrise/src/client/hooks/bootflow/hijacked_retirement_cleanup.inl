// Camera/constructor polling only captures authenticated identities. Native
// removal runs at the source-retirement boundary with a verified allocator TLS
// service, without holding the placement or retirement-cache locks.
struct RetirementSnapshot {
    mission::RetirementWorld world{};std::uintptr_t actors{};std::uint32_t actorStride{};
    std::array<std::byte,0x68> actor{};std::array<std::byte,0x50> entity{};
};
bool retirement_snapshot(std::uint32_t handle,RetirementSnapshot& out) noexcept {
    Read read{image};std::uintptr_t table{},parent{};std::uint32_t stride{};
    if(handle==UINT32_MAX || (handle&0x1FFFU)>=256
        || !read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0x68 || stride>0x100000
        || !read.copy(table+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride,out.actor)
        || at<std::uint32_t>(out.actor.data()+0x48)!=handle
        || !read.value(image+0x1F93428,out.world.entities) || !read.value(image+0x1F93430,out.world.stride)
        || !read.value(image+0x1F9344C,out.world.space) || !read.value(image+0x2744A18,out.world.manager) || out.world.stride<0x50 || out.world.stride>0x100000) {return false;}
    out.actors=table;out.actorStride=stride;
    const auto entity=at<std::uint32_t>(out.actor.data()+0x4C);
    const auto ai=at<std::uint32_t>(out.actor.data()+0x50);
    std::uint32_t kind{},self{},aiEntity{},aiActor{};
    // A live AI parent must independently point back to this actor and entity.
    return entity!=UINT32_MAX && ai!=UINT32_MAX && read.resolve(ai,parent)
        && read.value(parent+4,kind) && kind==0x808082ECU
        && read.value(parent+0x24,self) && self==ai
        && read.value(parent+0x2C,aiEntity) && aiEntity==entity
        && read.value(parent+0x1470,aiActor) && aiActor==handle
        && read.copy(out.world.entities+static_cast<std::uintptr_t>(entity&0x1FFFU)*out.world.stride,out.entity);
}
bool retirement_origin_snapshot(std::uint32_t handle,const mission::Request& request,std::uint16_t slot,
    std::uint32_t ownerHandle,mission::RetirementOrigin& origin) noexcept {
    Read read{image};std::uintptr_t table{},parent{};std::uint32_t stride{},kind{},self{},aiActor{};
    std::array<std::byte,0x68> actor{};
    if(handle==UINT32_MAX || (handle&0x1FFFU)>=256
        || !read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0x68 || stride>0x100000 || !read.copy(table+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride,actor)
        || at<std::uint32_t>(actor.data()+0x48)!=handle
        || !mission::capture_retirement_origin(request.owner,slot,ownerHandle,table,stride,actor,origin)
        || !read.resolve(origin.parent,parent)) {return false;}
    return read.value(parent+4,kind) && kind==0x808082ECU
        && read.value(parent+0x24,self) && self==origin.parent
        && read.value(parent+0x1470,aiActor) && aiActor==handle;
}
void retirement_cleanup_report(const char* stage,const mission::RetirementEnemy& target,unsigned result) noexcept {
    std::array<char,384> line{};
    std::snprintf(line.data(),line.size(),
        "ev=hijacked_retirement stage=%s run=%llu registry=3E9B74F3 source=%u actor=%08X expected_owner=%08X parent=%08X entity=%08X bundle=%08X result=%u",
        stage,static_cast<unsigned long long>(target.run.run),target.source,target.actor,target.owner,target.parent,target.entity,target.bundle,result);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
// A separate short-held lock allows the existing admission callback to retain
// an identity during native placement calls without re-entering the placement lock.
SRWLOCK retirementLock=SRWLOCK_INIT;
state::activity::coo::Generation retirementRun{};
std::array<mission::RetirementEnemy,8192> retirementTargets{};
std::array<bool,8192> retirementAttempted{};
std::array<mission::RetirementOrigin,256> retirementOrigins{};
std::array<mission::RetirementRead,8192> retirementReadState{};
void reset_retirement_run(const mission::Request& request) noexcept {
    // Caller holds retirementLock; never reset from an obsolete run snapshot.
    if(retirementRun!=request.owner) {
        retirementRun=request.owner;retirementTargets.fill({});retirementAttempted.fill(false);
        retirementOrigins.fill({});retirementReadState.fill(mission::RetirementRead::live);
    }
}
void retain_retirement_target(const mission::Request& request,const mission::RetirementEnemy& target) noexcept {
    AcquireSRWLockExclusive(&retirementLock);
    const auto live=mission::request();
    if(live.owner!=request.owner || !live.frame.enabled || live.frame.finished) {ReleaseSRWLockExclusive(&retirementLock);return;}
    reset_retirement_run(request);
    const auto index=target.entity&0x1FFFU;const auto& old=retirementTargets[index];
    const bool changed=old.entity!=target.entity || old.bundle!=target.bundle || old.world!=target.world;
    if(changed) {retirementAttempted[index]=false;retirementReadState[index]=mission::RetirementRead::live;}
    retirementTargets[index]=target;
    ReleaseSRWLockExclusive(&retirementLock);
    if(changed) {retirement_cleanup_report("entity_retained",target,1);}
}
bool capture_retirement_target(const mission::Request& request,std::uint16_t slot,
    const SourceIdentity& source,std::uint32_t actorHandle) noexcept {
    mission::RetirementOrigin origin{};
    if(!retirement_origin_snapshot(actorHandle,request,slot,source.handle,origin)
        || !retirement_source_matches(request,slot,source.address,source.handle)) {return false;}
    AcquireSRWLockExclusive(&retirementLock);
    const auto live=mission::request();
    if(live.owner!=request.owner || !live.frame.enabled || live.frame.finished) {ReleaseSRWLockExclusive(&retirementLock);return false;}
    reset_retirement_run(request);
    retirementOrigins[actorHandle&0x1FFFU]=origin;
    ReleaseSRWLockExclusive(&retirementLock);
    // A0D510 precedes entity readiness. The authenticated actor/AI-parent origin
    // survives until the later ready poll, including source detachment in between.
    RetirementSnapshot snapshot{};mission::RetirementEnemy target{};
    if(retirement_snapshot(actorHandle,snapshot)
        && mission::bind_retirement_entity(origin,snapshot.actors,snapshot.actorStride,snapshot.actor,snapshot.world,snapshot.entity,target)) {
        retain_retirement_target(request,target);
    }
    return true;
}
void resolve_retirement_origins(const mission::Request& request) noexcept {
    std::array<mission::RetirementOrigin,256> origins{};
    AcquireSRWLockShared(&retirementLock);origins=retirementOrigins;ReleaseSRWLockShared(&retirementLock);
    for(const auto& origin:origins) {
        if(origin.run!=request.owner) {continue;}
        Read read{image};std::uintptr_t table{};std::uint32_t stride{};std::array<std::byte,0x68> actor{};
        if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
            || table!=origin.actors || stride!=origin.stride
            || !read.copy(table+static_cast<std::uintptr_t>(origin.actor&0x1FFFU)*stride,actor)) {continue;}
        if(!mission::retirement_origin_matches(origin,table,stride,actor)) {
            AcquireSRWLockExclusive(&retirementLock);
            auto& current=retirementOrigins[origin.actor&0x1FFFU];
            if(current.run==origin.run && current.actor==origin.actor && current.parent==origin.parent) {current={};}
            ReleaseSRWLockExclusive(&retirementLock);continue;
        }
        RetirementSnapshot snapshot{};mission::RetirementEnemy target{};
        if(retirement_snapshot(origin.actor,snapshot)
            && mission::bind_retirement_entity(origin,snapshot.actors,snapshot.actorStride,snapshot.actor,snapshot.world,snapshot.entity,target)) {
            retain_retirement_target(request,target);
        }
    }
}
mission::RetirementRead retirement_entity_read(const mission::RetirementEnemy& target,mission::RetirementWorld& world,
    std::array<std::byte,0x50>& entity) noexcept {
    Read read{image};
    const bool worldReadable=read.value(image+0x1F93428,world.entities) && read.value(image+0x1F93430,world.stride)
        && read.value(image+0x1F9344C,world.space) && read.value(image+0x2744A18,world.manager);
    const bool entityReadable=worldReadable && world==target.world
        && read.copy(world.entities+static_cast<std::uintptr_t>(target.entity&0x1FFFU)*world.stride,entity);
    return mission::retirement_entity_state(target,world,worldReadable,entityReadable,entity);
}
void retirement_read_report(const mission::RetirementEnemy& target,mission::RetirementWorld actual,mission::RetirementRead state) noexcept {
    std::array<char,640> line{};
    std::snprintf(line.data(),line.size(),
        "ev=hijacked_retirement stage=entity_read run=%llu source=%u entity=%08X read_state=%u expected_table=%llX actual_table=%llX expected_stride=%u actual_stride=%u expected_space=%u actual_space=%u expected_world_token=%llX actual_world_token=%llX",
        static_cast<unsigned long long>(target.run.run),target.source,target.entity,static_cast<unsigned>(state),
        static_cast<unsigned long long>(target.world.entities),static_cast<unsigned long long>(actual.entities),target.world.stride,actual.stride,
        target.world.space,actual.space,static_cast<unsigned long long>(target.world.manager),static_cast<unsigned long long>(actual.manager));
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
bool retirement_owner_unchanged(const mission::RetirementEnemy& target) noexcept {
    Read read{image};std::uintptr_t table{};std::uint32_t stride{};
    if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0x68 || stride>0x100000) {return false;}
    // The original actor may have been recycled. Reject a different current
    // owner on any actor that still refers to this exact retained entity.
    for(std::uint32_t i=0;i<256;++i) {
        std::array<std::byte,0x68> actor{};
        if(!read.copy(table+static_cast<std::uintptr_t>(i)*stride,actor)) {return false;}
        const auto self=at<std::uint32_t>(actor.data()+0x48);
        if(self!=UINT32_MAX && (self&0x1FFFU)==i && !mission::retirement_actor_allows(target,actor)) {return false;}
    }
    return true;
}
__declspec(noinline) void retirement_capture(const mission::Request& request) noexcept {
    // Capture each game-thread poll, with no timer window that can skip an
    // actor created immediately before native source detachment.
    for(std::uint16_t slot=1;slot<=7;++slot) {
        const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
        const auto& state=request.frame.native[mission::asset_index(asset)];
        if(!state.managed || (!state.active && !state.retired)) {continue;}
        SourceIdentity source{};Read read{image};std::uint32_t count{};
        if(!native_source(mission::spawn_index(asset),state.generation,source,true)
            || !read.value(source.address+0x314,count) || count>64) {continue;}
        for(std::uint32_t i=0;i<count;++i) {
            gateway_native::Weak weak{};
            if(read.value(source.address+0x318+static_cast<std::uintptr_t>(i)*8,weak) && read.weak(weak)) {
                static_cast<void>(capture_retirement_target(request,slot,source,weak.handle));
            }
        }
    }
    resolve_retirement_origins(request);
}
__declspec(noinline) void retirement_dispatch(const mission::Request& request) noexcept {
    if(!retirement_allocator_ready()) {return;}
    unsigned retired{};
    for(std::uint16_t slot=1;slot<=7;++slot) {
        const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
        if(request.frame.native[mission::asset_index(asset)].retired) {retired|=1U<<slot;}
    }
    if(!retired) {return;}
    std::array<mission::RetirementEnemy,256> pending{};std::size_t pendingCount{};
    AcquireSRWLockShared(&retirementLock);
    for(std::size_t i=0;i<retirementTargets.size() && pendingCount<pending.size();++i) {
        const auto& target=retirementTargets[i];
        if(target.run==request.owner && !retirementAttempted[i] && (retired&(1U<<target.source))) {
            pending[pendingCount++]=target;
        }
    }
    ReleaseSRWLockShared(&retirementLock);
    for(std::size_t n=0;n<pendingCount;++n) {
        const auto target=pending[n];const auto i=target.entity&0x1FFFU;
        const auto live=mission::request();
        if(live.owner!=request.owner || !live.frame.enabled || live.frame.finished) {return;}
        const auto asset=mission::find(0x3E9B74F3U,1,target.source)->asset;
        if(!live.frame.native[mission::asset_index(asset)].retired) {continue;}
        mission::RetirementWorld world{};std::array<std::byte,0x50> entity{};
        const auto readState=retirement_entity_read(target,world,entity);
        AcquireSRWLockExclusive(&retirementLock);
        const bool changed=retirementReadState[i]!=readState;retirementReadState[i]=readState;
        ReleaseSRWLockExclusive(&retirementLock);
        if(changed) {retirement_read_report(target,world,readState);}
        if(mission::retirement_expired(readState)) {
            AcquireSRWLockExclusive(&retirementLock);
            if(retirementTargets[i].run==target.run && retirementTargets[i].entity==target.entity
                && retirementTargets[i].bundle==target.bundle && retirementTargets[i].world==target.world) {retirementTargets[i]={};}
            ReleaseSRWLockExclusive(&retirementLock);
            retirement_cleanup_report("entity_identity_expired",target,static_cast<unsigned>(readState));continue;
        }
        // World/context changes and transient read failures defer, never erase,
        // an authenticated target. Only a same-world identity read can expire it.
        if(readState!=mission::RetirementRead::live) {continue;}
        if(!retirement_owner_unchanged(target)) {continue;}
        if(!retirement_allocator_ready()) {return;}
        // Recheck after the actor-table walk. No actor salt or AI parent has to
        // survive streaming; the captured native entity itself must still match.
        if(retirement_entity_read(target,world,entity)!=mission::RetirementRead::live) {continue;}
        AcquireSRWLockExclusive(&retirementLock);
        const auto& current=retirementTargets[i];
        const bool same=current.run==target.run && current.entity==target.entity && current.bundle==target.bundle
            && current.world==target.world && current.owner==target.owner && !retirementAttempted[i];
        if(same) {retirementAttempted[i]=true;}
        ReleaseSRWLockExclusive(&retirementLock);
        if(!same) {continue;}
        // Runtime progression can reset on another thread during native reads.
        // Recheck the current lease at the dispatch boundary, not only the cache.
        const auto dispatch=mission::request();
        if(dispatch.owner!=target.run || !dispatch.frame.enabled || dispatch.frame.finished
            || !dispatch.frame.native[mission::asset_index(asset)].retired) {return;}
        // Use the mark/remove operation from native generation retirement. The
        // engine schedules full teardown itself; never invoke 56B550 from camera.
        retirement_cleanup_report("entity_mark_requested",target,1);
        reinterpret_cast<void(__fastcall*)(std::uint32_t) noexcept>(image+0x56A8F0)(target.entity);
        unsigned result{}; // 0 unreadable, 1 retired/replaced, 2 still present.
        const auto after=retirement_entity_read(target,world,entity);
        if(mission::retirement_expired(after)) {result=1;}
        else if(after==mission::RetirementRead::live) {result=2;}
        retirement_cleanup_report("entity_mark_returned",target,result);
        if(result==2) {
            AcquireSRWLockExclusive(&retirementLock);
            const auto& again=retirementTargets[i];
            if(again.run==target.run && again.entity==target.entity && again.world==target.world
                && again.bundle==target.bundle) {retirementAttempted[i]=false;}
            ReleaseSRWLockExclusive(&retirementLock);
        }
    }
}
