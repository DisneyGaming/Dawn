// Streaming hands a genuine source-owned survivor back to that source. The
// generic retained-facet template deliberately lacks the source creation extra.
// Native 170FC90(manager, saltedFacet), void __fastcall, ends that old network
// lifetime (children, pending masks, payload, sync and net binding) without
// manufacturing an owner or a death. Native area unload removes the old entity.
using mission::BacktrackingPart;
struct BacktrackingFacet {
    std::uintptr_t root{},manager{};std::uint16_t row{};
    std::uint32_t facet{UINT32_MAX},net{UINT32_MAX},entity{UINT32_MAX};
    std::array<BacktrackingPart,16> parts{};std::size_t count{};
    friend bool operator==(const BacktrackingFacet&,const BacktrackingFacet&)=default;
};
struct BacktrackingLease {
    mission::EnemyReceipt receipt{};mission::RetirementEnemy target{};BacktrackingFacet facet{};
};
struct BacktrackingReturn {
    state::activity::coo::Generation run{};std::uint32_t oldSource{UINT32_MAX};bool pending{};
};
struct BacktrackingBatch {
    std::array<BacktrackingLease,mission::kExteriorPopulation> leases{};std::size_t count{};
    std::array<BacktrackingReturn,7> sources{};
    std::array<std::uint32_t,7> sourceHandles{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
};
SRWLOCK backtrackingLock=SRWLOCK_INIT;
std::array<BacktrackingReturn,7> backtrackingReturns{};
bool backtrackingNativeReady{};
std::atomic_bool backtrackingHasReturns{};
thread_local bool backtrackingUnloading{};
bool exterior_slot(std::uint16_t slot) noexcept {return mission::exterior_source(slot);}
bool facet_entity(const std::array<std::byte,0x70>& row,std::uint32_t& entity) noexcept {
    Read read{image};std::uintptr_t pool{},localAddress{},componentAddress{},allocation{};
    std::uint32_t stride{},local{},component{},bound{};
    const auto net=at<std::uint32_t>(row.data()+4),facet=at<std::uint32_t>(row.data()+8);
    if(net==UINT32_MAX || facet==UINT32_MAX || !read.value(image+0x2037D48,pool)
        || !read.value(image+0x2037D50,stride) || stride!=12
        || !read.value(pool+static_cast<std::uintptr_t>(net&0x1FFFU)*stride,bound) || bound!=facet
        || !read.value(pool+static_cast<std::uintptr_t>(net&0x1FFFU)*stride+4,local)
        || !read.resolve(local,localAddress) || !read.value(localAddress+0xC8,component)
        || !read.resolve(component,componentAddress,&allocation)
        || !read.value(allocation+0x10,entity)) {return false;}
    return entity!=UINT32_MAX;
}
bool facet_row(std::uintptr_t manager,std::uint16_t index,std::array<std::byte,0x70>& row) noexcept {
    Read read{image};std::uint8_t allocated{},owned{};std::int16_t mapped{-1};
    if(index>=1024 || !read.value(image+0x30B0340+index/8,allocated) || !(allocated&(1U<<(index%8)))
        || !read.value(manager+0xC920+index/8,owned) || !(owned&(1U<<(index%8)))
        || !read.value(image+0x30B0440+static_cast<std::uintptr_t>(index)*0x70,row)) {return false;}
    const auto facet=at<std::uint32_t>(row.data()+8);
    return facet!=UINT32_MAX && read.value(manager+0x114+static_cast<std::uintptr_t>(facet&0x1FFFU)*6,mapped)
        && mapped==index && at<std::uint8_t>(row.data())==0
        && (at<std::int8_t>(row.data()+1)==-1 || at<std::int8_t>(row.data()+1)==-2);
}
bool backtracking_retained(const BacktrackingFacet&) noexcept;
bool backtracking_facet(const mission::RetirementEnemy& target,BacktrackingFacet& out) noexcept {
    if(!backtrackingNativeReady || !retirement_allocator_ready()) {return false;}
    const auto root=reinterpret_cast<Context>(image+0x16FC600)();
    if(!root) {return false;}
    unsigned matches{};
    for(unsigned group=0;group<3;++group) {
        const auto manager=root+0x206C8+static_cast<std::uintptr_t>(group)*0x11E08+0x270;
        Read read{image};std::uint32_t peers{};std::array<std::uintptr_t,31> slots{};std::array<std::uint8_t,128> owned{};
        if(!read.value(manager+0x110,peers) || peers || !read.value(manager+0x18,slots)
            || !read.value(manager+0xC920,owned)) {continue;}
        bool occupied{};for(const auto slot:slots) {occupied|=slot!=0;}if(occupied) {continue;}
        for(std::uint16_t index=0;index<1024;++index) {
            if(!(owned[index/8]&(1U<<(index%8)))) {continue;}
            std::array<std::byte,0x70> row{};std::uint32_t entity{};
            if(!facet_row(manager,index,row) || at<std::uint32_t>(row.data()+0xC)!=UINT32_MAX
                || !facet_entity(row,entity) || entity!=target.entity) {continue;}
            const auto facet=at<std::uint32_t>(row.data()+8);
            BacktrackingFacet candidate{root,manager,index,facet,at<std::uint32_t>(row.data()+4),entity};
            candidate.parts[candidate.count++]={index,facet,candidate.net,UINT32_MAX};
            // The native salted parent chain owns attachments such as weapons.
            // Children can have distinct entities, but must share this exact local manager.
            for(std::uint16_t child=0;child<1024;++child) {
                if(!(owned[child/8]&(1U<<(child%8))) || child==index) {continue;}
                std::array<std::byte,0x70> descendant{};Read childRead{image};
                if(!childRead.value(image+0x30B0440+static_cast<std::uintptr_t>(child)*0x70,descendant)) {return false;}
                const auto ancestry=mission::native_ancestry(facet,at<std::uint32_t>(descendant.data()+0xC),[&](std::uint32_t parent,std::uint32_t& next) {
                    Read chain{image};std::int16_t parentIndex{-1};std::array<std::byte,0x70> ancestor{};
                    if(!chain.value(manager+0x114+static_cast<std::uintptr_t>(parent&0x1FFFU)*6,parentIndex)
                        || parentIndex<0 || parentIndex>=1024 || !facet_row(manager,static_cast<std::uint16_t>(parentIndex),ancestor)
                        || at<std::uint32_t>(ancestor.data()+8)!=parent) {return false;}
                    next=at<std::uint32_t>(ancestor.data()+0xC);return true;
                });
                if(ancestry==mission::NativeAncestry::invalid) {return false;}
                if(ancestry==mission::NativeAncestry::descendant) {
                    std::uint32_t childEntity{};
                    if(!facet_row(manager,child,descendant) || !facet_entity(descendant,childEntity)
                        || candidate.count==candidate.parts.size()) {return false;}
                    candidate.parts[candidate.count++]={child,at<std::uint32_t>(descendant.data()+8),
                        at<std::uint32_t>(descendant.data()+4),at<std::uint32_t>(descendant.data()+0xC)};
                }
            }
            if(!backtracking_retained(candidate)) {return false;}
            out=candidate;++matches;
        }
    }
    return matches==1;
}
void backtracking_report(const char* stage,std::uint64_t run,std::uint16_t source,std::size_t count,unsigned reason=0) noexcept {
    std::array<char,256> line{};
    std::snprintf(line.data(),line.size(),"ev=hijacked_backtracking stage=%s run=%llu source=%u survivors=%zu reason=%u",
        stage,static_cast<unsigned long long>(run),source,count,reason);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
// Capture source identity before native unload may change the selected registry
// or destroy the source component. The actual living receipts provide its salt.
void backtracking_sources(const mission::Request& request,const mission::LivingEnemies& living,BacktrackingBatch& batch) noexcept {
    if(!backtrackingNativeReady || living.owner!=request.owner) {return;}
    for(const auto slot:mission::kExteriorSources) {
        const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;const auto& state=request.frame.native[mission::asset_index(asset)];
        if(!state.active || !state.prepared || state.suspended || state.retired) {continue;}
        std::uint32_t handle=UINT32_MAX;bool consistent=true;std::size_t count{};
        for(std::size_t i=0;i<living.count;++i) {
            const auto& r=living.actors[i];if(r.registry!=asset.registry || r.source!=slot) {continue;}
            if(handle!=UINT32_MAX && handle!=r.owner) {consistent=false;break;}handle=r.owner;++count;
        }
        if(!consistent) {continue;}
        std::uintptr_t address{};Read read{image};
        if(count) {
            if(!read.resolve(handle,address) || !retirement_source_matches(request,slot,address,handle)) {continue;}
        } else {
            SourceIdentity source{};
            if(!native_source(mission::spawn_index(asset),state.generation,source,true)
                || !retirement_source_matches(request,slot,source.address,source.handle)) {continue;}
            handle=source.handle;
        }
        batch.sourceHandles[slot-1]=handle;
        // No live native identity exists to capture for an entirely killed cohort.
        // Only the controller's complete, genuine death ledger can authorize zero.
        if(!count && mission::suspend_exterior(request.owner,slot,{})) {
            batch.sources[slot-1]={request.owner,handle,true};
            backtracking_report("source_suspended",request.owner.run,slot,0);
        }
    }
}
// Called once from A07BE0's first predicate while native424D10 has entered its
// world teardown scope. Preflight the entire source before changing any lifetime.
void backtracking_suspend(const mission::Request& request,std::span<const mission::RetirementEnemy> targets,
    const mission::LivingEnemies& living,BacktrackingBatch& batch) noexcept {
    if(!backtrackingNativeReady || living.owner!=request.owner || !retirement_allocator_ready()) {return;}
    for(const auto slot:mission::kExteriorSources) {
        const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
        const auto& state=request.frame.native[mission::asset_index(asset)];
        if(!state.active || !state.prepared || state.retired || state.suspended) {continue;}
        std::array<BacktrackingLease,16> leases{};std::array<mission::EnemyReceipt,16> receipts{};std::size_t count{};bool valid=true;unsigned reason{};
        const auto sourceHandle=batch.sourceHandles[slot-1];
        if(batch.sources[slot-1].pending) {continue;}
        if(sourceHandle==UINT32_MAX) {backtracking_report("preflight_rejected",request.owner.run,slot,0,1);continue;}
        for(std::size_t i=0;i<living.count;++i) {
            const auto& receipt=living.actors[i];if(receipt.registry!=asset.registry || receipt.source!=slot) {continue;}
            if(count==leases.size() || receipt.owner!=sourceHandle) {valid=false;reason=2;break;}
            auto& lease=leases[count];lease.receipt=receipt;bool found{};
            for(const auto& target:targets) {
                if(target.run==request.owner && target.source==slot && target.actor==receipt.actor && target.owner==receipt.owner) {
                    lease.target=target;found=true;break;
                }
            }
            RetirementSnapshot snapshot{};
            if(!found) {valid=false;reason=3;break;}
            if(!retirement_snapshot(receipt.actor,snapshot)
                || !mission::retirement_entity_matches(lease.target,snapshot.world,snapshot.actor,snapshot.entity)) {valid=false;reason=4;break;}
            if(!backtracking_facet(lease.target,lease.facet)) {valid=false;reason=5;break;}
            receipts[count++]=receipt;
        }
        if(!valid) {backtracking_report("preflight_rejected",request.owner.run,slot,count,reason);continue;}
        // The controller independently requires all expected admissions, an exact
        // current living set, and a fresh generation before permitting suspension.
        if(count>batch.leases.size()-batch.count) {backtracking_report("preflight_rejected",request.owner.run,slot,count,6);continue;}
        if(!mission::suspend_exterior(request.owner,slot,std::span(receipts).first(count))) {backtracking_report("preflight_rejected",request.owner.run,slot,count,7);continue;}
        for(std::size_t i=0;i<count;++i) {batch.leases[batch.count++]=leases[i];}
        batch.sources[slot-1]={request.owner,sourceHandle,true};
        backtracking_report("source_suspended",request.owner.run,slot,count);
    }
}
bool backtracking_retained(const BacktrackingFacet& expected) noexcept {
    if(reinterpret_cast<Context>(image+0x16FC600)()!=expected.root) {return false;}
    Read read{image};std::uint32_t mask{};std::array<std::uintptr_t,31> peers{};
    if(!read.value(expected.manager+0x110,mask) || mask || !read.value(expected.manager+0x18,peers)) {return false;}
    for(const auto peer:peers) {if(peer) {return false;}}
    for(std::size_t i=0;i<expected.count;++i) {
        const auto& part=expected.parts[i];std::array<std::byte,0x70> row{};
        if(!facet_row(expected.manager,part.row,row) || !mission::native_part_matches(part,
            at<std::uint8_t>(row.data()),at<std::int8_t>(row.data()+1),at<std::uint32_t>(row.data()+8),
            at<std::uint32_t>(row.data()+4),at<std::uint32_t>(row.data()+0xC))) {return false;}
    }
    // 170FC90 follows +48 firstChild and each child's +44 nextSibling.
    // Check the actual recursive list as well as the independent parent links.
    std::size_t children{};
    for(std::size_t i=0;i<expected.count;++i) {
        Read listRead{image};std::uint32_t child{};
        if(!listRead.value(image+0x30B0440+static_cast<std::uintptr_t>(expected.parts[i].row)*0x70+0x48,child)) {return false;}
        std::size_t visited{};
        while(child!=UINT32_MAX) {
            if(++visited>=expected.count || ++children>=expected.count) {return false;}
            const BacktrackingPart* part{};
            for(std::size_t j=0;j<expected.count;++j) {if(expected.parts[j].facet==child) {part=&expected.parts[j];break;}}
            if(!part || part->parent!=expected.parts[i].facet
                || !listRead.value(image+0x30B0440+static_cast<std::uintptr_t>(part->row)*0x70+0x44,child)) {return false;}
        }
    }
    if(children+1!=expected.count) {return false;}
    // No newly attached child may be swept by the native recursive operation.
    std::array<std::uint8_t,128> owned{};Read ownerRead{image};
    if(!ownerRead.value(expected.manager+0xC920,owned)) {return false;}
    for(std::uint16_t index=0;index<1024;++index) {
        if(!(owned[index/8]&(1U<<(index%8)))) {continue;}
        Read childRead{image};std::uint32_t parent{};
        if(!childRead.value(image+0x30B0440+static_cast<std::uintptr_t>(index)*0x70+0xC,parent)) {return false;}
        bool captured{};for(std::size_t i=0;i<expected.count;++i) {captured|=expected.parts[i].row==index;}
        if(captured) {continue;}
        for(std::size_t i=0;i<expected.count;++i) {if(parent==expected.parts[i].facet) {return false;}}
    }
    return true;
}
void backtracking_finish(const mission::Request& request,const BacktrackingBatch& batch) noexcept {
    if(!backtrackingNativeReady || !retirement_allocator_ready()) {return;}
    for(const auto slot:mission::kExteriorSources) {
        if(!batch.sources[slot-1].pending) {continue;}
        bool valid=true;std::size_t count{};
        // Original424D10 has completed. FE removal can enqueue an old-net-ID
        // destroy command now; no original component iterator is still using it.
        for(std::size_t i=0;i<batch.count;++i) {
            const auto& lease=batch.leases[i];if(lease.receipt.source!=slot) {continue;}++count;
            mission::RetirementWorld world{};std::array<std::byte,0x50> entity{};
            const auto state=retirement_entity_read(lease.target,world,entity);
            if(!mission::retirement_expired(state) || !backtracking_retained(lease.facet)) {valid=false;break;}
        }
        if(!valid) {backtracking_report("handoff_rejected",request.owner.run,slot,count);continue;}
        for(std::size_t i=0;i<batch.count;++i) {
            const auto& lease=batch.leases[i];if(lease.receipt.source!=slot) {continue;}
            if(!gate.accepting() || mission::request().owner!=request.owner || !backtracking_retained(lease.facet)) {valid=false;break;}
            // Never inspect a facet/child pointer after this native invalidation.
            reinterpret_cast<void(__fastcall*)(std::uintptr_t,std::uint32_t) noexcept>(image+0x170FC90)(lease.facet.manager,lease.facet.facet);
        }
        if(!valid) {backtracking_report("handoff_incomplete",request.owner.run,slot,count);continue;}
        AcquireSRWLockExclusive(&backtrackingLock);backtrackingReturns[slot-1]=batch.sources[slot-1];
        backtrackingHasReturns.store(true,std::memory_order_release);ReleaseSRWLockExclusive(&backtrackingLock);
        backtracking_report("native_lifetime_ended",request.owner.run,slot,count);
    }
}
void backtracking_resume(const mission::Request& request) noexcept {
    if(backtrackingUnloading || !backtrackingNativeReady || !backtrackingHasReturns.load(std::memory_order_acquire)) {return;}
    std::array<BacktrackingReturn,7> returns{};bool any{};
    AcquireSRWLockExclusive(&backtrackingLock);
    for(auto& entry:backtrackingReturns) {if(entry.run!=request.owner) {entry={};}any|=entry.pending;}
    returns=backtrackingReturns;backtrackingHasReturns.store(any,std::memory_order_release);ReleaseSRWLockExclusive(&backtrackingLock);
    if(!any || !in_context(37)) {return;}
    for(std::uint16_t slot=1;slot<=7;++slot) {
        const auto pending=returns[slot-1];
        if(pending.run!=request.owner) {continue;}
        if(!pending.pending || !exterior_slot(slot)) {continue;}
        const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;const auto& state=request.frame.native[mission::asset_index(asset)];
        SourceIdentity source{};
        // A genuinely reloaded source must reflect the new zero-count generation
        // before placement readiness can permit a survivor request.
        if(!state.suspended || !native_source(mission::spawn_index(asset),state.generation,source)
            || source.handle==pending.oldSource) {continue;}
        if(mission::resume_exterior(request.owner,slot)) {
            AcquireSRWLockExclusive(&backtrackingLock);
            if(backtrackingReturns[slot-1].run==request.owner && backtrackingReturns[slot-1].oldSource==pending.oldSource) {backtrackingReturns[slot-1].pending=false;}
            ReleaseSRWLockExclusive(&backtrackingLock);
            std::array<char,320> line{};
            std::snprintf(line.data(),line.size(),"ev=hijacked_backtracking stage=source_resumed run=%llu source=%u generation=%u old_source=%08X new_source=%08X survivors=%u",
                static_cast<unsigned long long>(request.owner.run),slot,state.generation,pending.oldSource,source.handle,state.survivingRequested);
            core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        }
    }
}
