namespace eater=state::activity::eater_of_worlds;
namespace eater_retirement=eater_source_retirement;

struct EaterRetiredEntity {
    gateway_native::Weak actor{};std::uint32_t entity{UINT32_MAX},bundle{UINT32_MAX};
};
struct EaterNativeTables {
    std::uintptr_t actors{},entities{},manager{};std::uint32_t actorStride{},entityStride{},space{};
    friend bool operator==(const EaterNativeTables&,const EaterNativeTables&)=default;
};
struct EaterRetirement {
    state::activity::coo::Generation owner{};state::activity::coo::Asset asset{};
    std::uintptr_t address{};std::uint32_t handle{UINT32_MAX},generation{};
    EaterNativeTables tables{};std::array<EaterRetiredEntity,64> entities{};std::uint8_t entityCount{};
    eater_retirement::Gate gate{};std::uint64_t token{};bool active{};
};
SRWLOCK eaterRetirementLock=SRWLOCK_INIT;
std::array<EaterRetirement,std::size(eater::kSpawns)> eaterRetirements{};
std::atomic_uint eaterRetirementCount{};
std::atomic_uint64_t eaterRetirementNextPoll{};
std::atomic_uint64_t eaterRetirementToken{};

bool eater_native_tables(Read& read,EaterNativeTables& out) noexcept {
    return read.value(image+0x1F9D7F8,out.actors) && read.value(image+0x1F9D800,out.actorStride)
        && read.value(image+0x1F93428,out.entities) && read.value(image+0x1F93430,out.entityStride)
        && read.value(image+0x1F9344C,out.space) && read.value(image+0x2744A18,out.manager)
        && out.actors>=0x10000 && out.entities>=0x10000 && out.manager
        && out.actorStride>=0x70 && out.actorStride<=0x100000
        && out.entityStride>=0x9C && out.entityStride<=0x100000;
}
bool eater_row(std::uintptr_t table,std::uint32_t stride,std::uint32_t handle,
               std::uintptr_t& out) noexcept {
    if(handle==UINT32_MAX) return false;
    const auto offset=static_cast<std::uintptr_t>(handle&0x1FFFU)*stride;
    if(table<0x10000 || table>UINTPTR_MAX-offset-0xA0) return false;
    out=table+offset;return true;
}
enum class EaterWeakState : std::uint8_t {unreadable,replaced,live};
EaterWeakState eater_weak_state(Read& read,gateway_native::Weak weak) noexcept {
    if(weak.handle==UINT32_MAX) return EaterWeakState::replaced;
    std::uintptr_t directory{},registry{},metadata{},head{},elements{};std::int32_t directoryStride{};
    if(!read.value(image+0x2439C70,directory) || !read.value(directory,registry)
        || !read.value(directory+0x10,directoryStride) || directoryStride<=0 || directoryStride>0x1000)
        return EaterWeakState::unreadable;
    const auto index=((static_cast<std::int32_t>(weak.handle)>>31&0x3C00U)|0x3FFU)&(weak.handle>>13)&0xFFFFU;
    std::uint16_t count{};std::uint32_t offset{},stride{},serial{};
    if(!read.value(registry+static_cast<std::uintptr_t>(index)*directoryStride+0x10,metadata)
        || !read.value(metadata,head) || !read.value(head+0x1C,count)) return EaterWeakState::unreadable;
    if((weak.handle&0x1FFFU)>=count) return EaterWeakState::replaced;
    if(!read.value(metadata+8,elements) || !read.value(metadata+0x1C,offset)
        || !read.value(metadata+0x20,stride) || stride==0 || stride>0x100000
        || !read.value(elements+offset+static_cast<std::uintptr_t>(weak.handle&0x1FFFU)*stride,serial))
        return EaterWeakState::unreadable;
    return serial==weak.serial?EaterWeakState::live:EaterWeakState::replaced;
}
bool eater_source_identity(std::uintptr_t address,std::size_t& index,std::uint32_t& handle) noexcept {
    Read read{image};gateway_native::Ref definition{},self{};std::uintptr_t resolved{},base{};
    if(!read.value(address,definition) || definition.kind!=0x8080948FU || definition.offset<0 || definition.offset>0x100000
        || !read.value(address+0x30,self) || self.kind!=0x80809A3BU || self.offset
        || !read.resolve(self.handle,resolved) || resolved!=address) return false;
    for(std::size_t i=0;i<std::size(eater::kSpawns);++i) {
        const auto& spawn=eater::kSpawns[i];const auto asset=eater::asset_index(spawn.asset);
        if(spawn.asset.definition!=definition.handle || asset>=std::size(eater::kAssets)
            || eater::kAssets[asset].offset!=definition.offset) continue;
        std::array<std::byte,8> scoped{};
        if(!read.resolve(definition.handle,base) || base>UINTPTR_MAX-static_cast<std::uintptr_t>(definition.offset)-0x38
            || !read.copy(base+static_cast<std::uintptr_t>(definition.offset)+0x30,scoped)
            || at<std::uint32_t>(scoped.data())!=spawn.asset.registry
            || at<std::uint8_t>(scoped.data()+4)!=1
            || at<std::uint16_t>(scoped.data()+6)!=spawn.asset.slot) return false;
        index=i;handle=self.handle;return true;
    }
    return false;
}
bool eater_capture_entity(Read& read,const EaterNativeTables& tables,
                          std::uint32_t source,gateway_native::Weak weak,EaterRetiredEntity& out) noexcept {
    if(weak.handle==UINT32_MAX) {out={};return true;}
    if((weak.handle&0x1FFFU)>=256) return false;
    const auto weakState=eater_weak_state(read,weak);
    if(weakState==EaterWeakState::replaced) {out={};return true;}
    if(weakState==EaterWeakState::unreadable) {
        std::uintptr_t actor{};std::uint32_t self{};
        if(!eater_row(tables.actors,tables.actorStride,weak.handle,actor)
            || !read.value(actor+0x48,self) || self==weak.handle) return false;
        out={};return true;
    }
    std::uintptr_t actor{},entity{};std::array<std::byte,0x70> bytes{};
    if(!eater_row(tables.actors,tables.actorStride,weak.handle,actor) || !read.copy(actor,bytes)
        || at<std::uint32_t>(bytes.data()+0x48)!=weak.handle) return false;
    const auto ownerRef=at<gateway_native::Ref>(bytes.data()+0x38);
    const auto entityHandle=at<std::uint32_t>(bytes.data()+0x4C);
    if(ownerRef.handle!=source || ownerRef.kind!=0x80809A3BU || ownerRef.offset) return false;
    if(entityHandle==UINT32_MAX) {out={weak,UINT32_MAX,UINT32_MAX};return true;}
    if(!eater_row(tables.entities,tables.entityStride,entityHandle,entity)) return false;
    std::uint32_t self{},bundle{},flags{};
    if(!read.value(entity+0x0C,self) || self!=entityHandle || !read.value(entity+0x4C,bundle)
        || !read.value(entity+4,flags)) return false;
    out={weak,entityHandle,bundle};return true;
}
bool eater_reset_state(Read& read,std::uintptr_t address,eater_retirement::ResetState& out) noexcept {
    return read.value(address+0x1FC,out.incoming) && read.value(address+0x244,out.applied)
        && read.value(address+0x314,out.owned) && read.value(address+0x5E8,out.scheduling)
        && read.copy(address+0x638,std::as_writable_bytes(std::span(out.request)))
        && read.copy(address+0x670,std::as_writable_bytes(std::span(out.queues)));
}
bool eater_entities_settled(Read& read,const EaterRetirement& pending) noexcept {
    EaterNativeTables tables{};if(!eater_native_tables(read,tables) || tables!=pending.tables) return false;
    for(std::size_t i=0;i<pending.entityCount;++i) {
        const auto& lease=pending.entities[i];
        if(lease.entity==UINT32_MAX) {
            const auto weakState=eater_weak_state(read,lease.actor);
            if(weakState==EaterWeakState::replaced) continue;
            std::uintptr_t actor{};std::uint32_t self{};
            if(weakState!=EaterWeakState::unreadable
                || !eater_row(tables.actors,tables.actorStride,lease.actor.handle,actor)
                || !read.value(actor+0x48,self) || self==lease.actor.handle) return false;
            continue;
        }
        std::uintptr_t row{};std::uint32_t self{},flags{};
        if(!eater_row(tables.entities,tables.entityStride,lease.entity,row)
            || !read.value(row+0x0C,self) || !read.value(row+4,flags)) return false;
        const auto state=eater_retirement::entity(lease.entity,self,flags,true);
        if(state==eater_retirement::EntityState::live || state==eater_retirement::EntityState::unreadable) return false;
    }
    return true;
}
bool eater_current(const eater::SourceRequest& request,const EaterRetirement& pending) noexcept {
    return request.enabled && request.loaded && request.owner==pending.owner && request.asset==pending.asset
        && request.generation==pending.generation && request.managed && request.retiring
        && !request.active && !request.desired;
}
void clear_eater_retirement(std::size_t index,const EaterRetirement& expected) noexcept {
    AcquireSRWLockExclusive(&eaterRetirementLock);
    auto& live=eaterRetirements[index];
    if(live.active && live.token==expected.token && live.owner==expected.owner && live.address==expected.address
        && live.handle==expected.handle && live.generation==expected.generation) {
        live={};eaterRetirementCount.fetch_sub(1,std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&eaterRetirementLock);
}
void clear_eater_retirements() noexcept {
    AcquireSRWLockExclusive(&eaterRetirementLock);eaterRetirements={};
    eaterRetirementCount.store(0,std::memory_order_release);eaterRetirementNextPoll.store(0,std::memory_order_release);
    ReleaseSRWLockExclusive(&eaterRetirementLock);
}
void eater_retirement_report(const char* stage,const EaterRetirement& pending,
                             std::uint32_t owned=0) noexcept {
    std::array<char,320> line{};const auto n=std::snprintf(line.data(),line.size(),
        "ev=eater_of_worlds stage=source_retirement_%s run=%llu registry=%08X source=%u generation=%u handle=%08X owned=%u captured=%u",
        stage,static_cast<unsigned long long>(pending.owner.run),pending.asset.registry,pending.asset.slot,
        pending.generation,pending.handle,owned,pending.entityCount);
    if(n>0 && static_cast<std::size_t>(n)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
}
void capture_eater_source_retirement(std::uintptr_t address) noexcept {
    if(!eater::native_run()) return;
    std::size_t index{};std::uint32_t handle{};
    if(!eater_source_identity(address,index,handle)) return;
    const auto request=eater::source_request(eater::kSpawns[index].asset.registry,eater::kSpawns[index].asset.slot);
    EaterRetirement pending{};pending.owner=request.owner;pending.asset=request.asset;pending.address=address;
    pending.handle=handle;pending.generation=request.generation;
    if(!eater_current(request,pending)) return;
    Read read{image};std::uint32_t incoming{},applied{},count{};
    std::array<gateway_native::Weak,64> owned{};
    if(!read.value(address+0x1FC,incoming) || incoming!=pending.generation
        || !read.value(address+0x244,applied) || applied==pending.generation
        || !read.value(address+0x314,count) || count>owned.size()
        || (count && !read.copy(address+0x318,std::as_writable_bytes(std::span(owned).first(count))))
        || !eater_native_tables(read,pending.tables)) return;
    for(std::uint32_t i=0;i<count;++i) {
        EaterRetiredEntity entity{};
        if(!eater_capture_entity(read,pending.tables,handle,owned[i],entity)) return;
        if(entity.actor.handle!=UINT32_MAX) pending.entities[pending.entityCount++]=entity;
    }
    pending.token=eaterRetirementToken.fetch_add(1,std::memory_order_relaxed)+1;pending.active=true;
    AcquireSRWLockExclusive(&eaterRetirementLock);auto& live=eaterRetirements[index];
    if(!live.active) eaterRetirementCount.fetch_add(1,std::memory_order_release);
    live=pending;eaterRetirementNextPoll.store(0,std::memory_order_release);
    ReleaseSRWLockExclusive(&eaterRetirementLock);
    eater_retirement_report("captured",pending,count);
}
void poll_eater_source_retirements() noexcept {
    if(!eaterRetirementCount.load(std::memory_order_acquire)) return;
    const auto now=GetTickCount64();auto next=eaterRetirementNextPoll.load(std::memory_order_acquire);
    if(now<next || !eaterRetirementNextPoll.compare_exchange_strong(next,now+50,std::memory_order_acq_rel)) return;
    for(std::size_t index=0;index<eaterRetirements.size();++index) {
        EaterRetirement pending{};
        AcquireSRWLockShared(&eaterRetirementLock);pending=eaterRetirements[index];ReleaseSRWLockShared(&eaterRetirementLock);
        if(!pending.active) continue;
        const auto request=eater::source_request(pending.asset.registry,pending.asset.slot);
        if(!eater_current(request,pending)) {clear_eater_retirement(index,pending);continue;}
        std::size_t sourceIndex{};std::uint32_t sourceHandle{};Read read{image};eater_retirement::ResetState reset{};
        const bool nativeReady=eater_source_identity(pending.address,sourceIndex,sourceHandle)
            && sourceIndex==index && sourceHandle==pending.handle && eater_reset_state(read,pending.address,reset)
            && eater_retirement::reset(pending.generation,reset);
        const bool entitiesReady=nativeReady && eater_entities_settled(read,pending);
        bool receipt{};
        AcquireSRWLockExclusive(&eaterRetirementLock);auto& live=eaterRetirements[index];
        if(live.active && live.token==pending.token && live.owner==pending.owner && live.address==pending.address
            && live.handle==pending.handle && live.generation==pending.generation)
            receipt=live.gate.sample(pending.generation,nativeReady,entitiesReady);
        ReleaseSRWLockExclusive(&eaterRetirementLock);
        if(receipt && eater::observe_source_retired(pending.owner.run,pending.asset.registry,
            pending.asset.slot,pending.generation)) {
            eater_retirement_report("acknowledged",pending,reset.owned);clear_eater_retirement(index,pending);
        }
    }
}
