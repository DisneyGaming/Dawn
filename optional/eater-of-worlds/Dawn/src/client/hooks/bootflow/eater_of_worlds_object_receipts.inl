// Eater object-source receipts at the existing guarded native source boundary.
#include "eater_of_worlds_platform_contacts.inl"
#include "eater_of_worlds_platform_receipts.inl"
namespace eater_door_receipts {
namespace native=eater_door_native;
struct Attempt final {
    std::uint64_t run{};std::uint32_t generation{};gateway_native::Weak entity{};
    gateway_native::Weak target{};unsigned attempts{},cooldown{};bool complete{},foreign{};
};
Attempt g_attempt{};

bool claim(std::uint64_t run,std::uint32_t generation,gateway_native::Weak entity,
           gateway_native::Read& read) noexcept {
    gateway_native::Weak target{};bool complete{},foreign{};
    AcquireSRWLockExclusive(&g_lock);
    if(g_attempt.run!=run || g_attempt.generation!=generation || g_attempt.entity!=entity)
        g_attempt={run,generation,entity};
    target=g_attempt.target;complete=g_attempt.complete;foreign=g_attempt.foreign;
    ReleaseSRWLockExclusive(&g_lock);
    if(complete) {
        const auto state=foreign?native::binding_state(read,g_image,
            state::activity::eater_of_worlds::kAirlock.exitDoor,target)
            :native::binding_state(read,g_image,
                state::activity::eater_of_worlds::kAirlock.exitDoor,target,entity);
        if(state==native::BindingState::present || state==native::BindingState::unknown) return false;
        AcquireSRWLockExclusive(&g_lock);
        if(g_attempt.run==run && g_attempt.generation==generation && g_attempt.entity==entity
            && g_attempt.complete && g_attempt.foreign==foreign && g_attempt.target==target) {
            g_attempt.complete=g_attempt.foreign=false;g_attempt.target={};
            g_attempt.attempts=g_attempt.cooldown=0;
        }
        ReleaseSRWLockExclusive(&g_lock);
    }
    AcquireSRWLockExclusive(&g_lock);
    bool accepted{};
    if(g_attempt.run==run && g_attempt.generation==generation && g_attempt.entity==entity
        && !g_attempt.complete) {
        if(g_attempt.attempts<8) accepted=true;
        else if(++g_attempt.cooldown>=32) {
            g_attempt.attempts=g_attempt.cooldown=0;accepted=true;
        }
        if(accepted) ++g_attempt.attempts;
    }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
void finish(std::uint64_t run,std::uint32_t generation,gateway_native::Weak entity,
            native::BindResult result,const native::Candidate& candidate) noexcept {
    if(result!=native::BindResult::bound && result!=native::BindResult::alreadyBound
        && result!=native::BindResult::occupied) return;
    AcquireSRWLockExclusive(&g_lock);
    if(g_attempt.run==run && g_attempt.generation==generation && g_attempt.entity==entity) {
        g_attempt.target=result==native::BindResult::occupied?candidate.previous:candidate.deviceWeak;
        g_attempt.complete=true;g_attempt.foreign=result==native::BindResult::occupied;g_attempt.cooldown=0;
    }
    ReleaseSRWLockExclusive(&g_lock);
}
struct Exchange final {
    std::uint64_t compare_exchange(std::uintptr_t address,std::uint64_t expected,
                                   std::uint64_t desired) noexcept {
        return static_cast<std::uint64_t>(InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(address),static_cast<LONG64>(desired),
            static_cast<LONG64>(expected)));
    }
};
}
void reset_eater_door_receipts() noexcept {
    AcquireSRWLockExclusive(&g_lock);eater_door_receipts::g_attempt={};ReleaseSRWLockExclusive(&g_lock);
}
bool sample_eater_object_pose(std::uintptr_t source,state::activity::coo::Asset object,
    std::uint32_t generation,
    const state::activity::eater_of_worlds::ObjectDeviceBinding& definition,
    eater_door_native::ObjectDevice& owned,std::int32_t& revision,float& position) noexcept {
    namespace native=eater_door_native;namespace gn=gateway_native;
    gn::Read read{g_image};native::ObjectDevice before{},after{};std::array<std::byte,16> pose{},stable{};
    std::int32_t firstRevision{},lastRevision{};
    if(!native::object_device(read,source,object,generation,definition,before)
        || !read.copy(before.device+0x370,pose) || !read.value(before.device+0x960,firstRevision)) return false;
    const auto current=at<float>(pose.data()),target=at<float>(pose.data()+12);
    if(firstRevision<0 || !std::isfinite(current) || !std::isfinite(target) || current!=target) return false;
    if(!native::object_device(read,source,object,generation,definition,after) || after!=before
        || !read.copy(after.device+0x370,stable) || at<float>(stable.data())!=current
        || at<float>(stable.data()+12)!=target || !read.value(after.device+0x960,lastRevision)
        || lastRevision!=firstRevision) return false;
    owned=after;revision=lastRevision;position=target;return true;
}
void observe_eater_gate_pose(state::activity::coo::Asset gate) noexcept {
    namespace eater=state::activity::eater_of_worlds;
    const auto request=eater::gate_request(gate);
    if(!request.enabled || request.acknowledged || request.revision<0) return;
    gateway_native::Read read{g_image};eater_door_native::GatePose applied{};
    if(!eater_door_native::gate_pose(read,g_image,request.gate,applied)
        || applied.revision!=request.revision || applied.position!=request.position
        || eater::gate_request(gate)!=request) return;
    eater::observe_device(request.owner,request.gate,request.revision,request.position);
}
void observe_eater_grate_pose(std::uintptr_t source,std::uint32_t generation,
                              gateway_native::Weak entity) noexcept {
    namespace eater=state::activity::eater_of_worlds;
    const auto request=eater::grate_request();
    if(!request.object.enabled || !request.object.desired.active || !request.object.desired.acknowledged) return;
    if(request.poseAcknowledged && request.acknowledgedObject.owner.run==request.object.owner.run
        && request.acknowledgedObject.owner.value==generation
        && request.acknowledgedObject.entity==entity.handle
        && request.acknowledgedObject.serial==entity.serial) {
        gateway_native::Read identityRead{g_image};
        const gateway_native::Weak acknowledged{request.acknowledgedDeviceSerial,
            request.acknowledgedDevice};
        if(eater_door_native::device_identity(identityRead,entity,acknowledged,eater::kDoorObjectDevice))
            return;
    }
    eater_door_native::ObjectDevice owned{};std::int32_t revision{};float position{};
    if(!sample_eater_object_pose(source,eater::kReactorExitGrate,generation,eater::kDoorObjectDevice,
        owned,revision,position) || revision<0 || static_cast<std::uint32_t>(revision)!=request.revision
        || position!=(request.open?1.F:0.F) || eater::grate_request()!=request) return;
    const state::activity::coo::ObjectReceipt object{{request.object.owner.run,generation},eater::kReactorExitGrate,
        owned.entity.handle,owned.entity.serial,owned.deviceWeak.handle};
    static_cast<void>(eater::observe_grate_pose({object,owned.deviceWeak.handle,
        owned.deviceWeak.serial,revision,position}));
}
void observe_eater_of_worlds_object(void* raw) noexcept {
    namespace eater=state::activity::eater_of_worlds;
    namespace gn=gateway_native;
    const auto run=eater::native_run();if(!run) return;
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);
    std::array<std::byte,16> header{};if(!read.copy(source,header)) return;
    const eater::AssetBinding* binding{};
    for(const auto& asset:eater::kAssets) if(asset.asset.type==4
        && prefix(header.data(),asset.asset.definition,0x80809928U,asset.offset)) {binding=&asset;break;}
    if(!binding) return;
    const auto index=static_cast<std::size_t>(binding-std::data(eater::kAssets));
    auto request=eater::object_request(index);
    if(!request.owner.valid() || !request.enabled) return;
    const auto desired=request.desired;if(!desired.managed) return;
    if(!desired.prepared) {
        std::array<std::byte,0x44> bytes{};
        if(read.copy(source+0x180,bytes) && at<std::uint32_t>(bytes.data())==request.owner.value
            && state::activity::coo::native_device::inactive_state(bytes))
            eater::observe_prepared(request.owner,binding->asset);
        return;
    }
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t row{};
    if(!read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity)
        || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)) return;
    std::uint32_t stableGeneration{},stableCommitted{};
    if(!read.value(source+0x180,stableGeneration) || stableGeneration!=generation
        || !read.value(source+0x2F0,stableCommitted) || stableCommitted!=committed
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again)
        || !eater::owner_current(request.owner)) return;
    if(eater::gate_object_binding(binding->asset)
        && eater_door_receipts::claim(run,generation,entity,read)) {
        gn::Read doorRead{g_image};eater_door_receipts::Exchange exchange{};
        eater_door_native::Candidate candidate{};
        const auto result=eater_door_native::bind(doorRead,exchange,g_image,source,binding->asset,
            generation,&candidate);
        eater_door_receipts::finish(run,generation,entity,result,candidate);
        if(result==eater_door_native::BindResult::bound
            || result==eater_door_native::BindResult::occupied
            || result==eater_door_native::BindResult::raced
            || result==eater_door_native::BindResult::invalidated) {
            report("ev=eater_of_worlds stage=door_binding run=%llu generation=%u object=%08X "
                "entity=%08X/%08X gate=%08X device=%08X/%08X result=%u mutation=native_weak_binding",
                static_cast<unsigned long long>(run),generation,binding->asset.definition,
                entity.serial,entity.handle,candidate.binding?candidate.binding->gate.definition:UINT32_MAX,
                candidate.deviceWeak.serial,candidate.deviceWeak.handle,static_cast<unsigned>(result));
        }
    }
    const state::activity::coo::ObjectReceipt object{{run,generation},binding->asset,entity.handle,entity.serial};
    eater::observe_object(object);
    if(binding->asset==eater::kAirlockExitObject) {
        // This one retained object supplies a bounded tick boundary for the
        // four exact traversal gates. Only gates awaiting an applied-pose
        // receipt perform native reads; acknowledged gates return immediately.
        observe_eater_gate_pose(eater::kAirlock.entranceDoor);
        observe_eater_gate_pose(eater::kAirlock.exitDoor);
        observe_eater_gate_pose(eater::kEjectionTube);
        observe_eater_gate_pose(eater::kPiston);
    }
    if(binding->asset==eater::kReactorExitGrate)
        observe_eater_grate_pose(source,generation,entity);
    if(eater::reactor_platform(binding->asset)) {
        // Refresh the first acknowledgement without copying the full frame.
        if(!request.desired.acknowledged) request=eater::object_request(index);
        eater_platform_contacts::Snapshot platform{};
        if(observe_eater_platform_physics(raw,index,bundle,object,request,platform))
            observe_eater_platform_pose(index,platform,request);
        return;
    }
    observe_eater_cranium_source(raw,index);
    observe_eater_station_source(raw,index);
    const auto healthIndex=eater::health_index(binding->asset);
    if(healthIndex==std::size(eater::kHealthBindings)) return;
    const auto& healthBinding=eater::kHealthBindings[healthIndex];
    gn::Read componentRead{g_image},healthRead{g_image};std::uintptr_t health{},resolved{};
    std::array<std::byte,0x340> healthBytes{};
    if(!coo_native::component<gn::Read,1024>(componentRead,bundle,entity.handle,eater::kHealthRuntimeKind,health)
        || !healthRead.copy(health,healthBytes)) return;
    const auto config=at<std::uint32_t>(healthBytes.data());
    const auto kind=at<std::uint32_t>(healthBytes.data()+4);
    const auto offset=at<std::uint64_t>(healthBytes.data()+8);
    const auto self=at<std::uint32_t>(healthBytes.data()+0x24);
    if(config!=healthBinding.config || kind!=eater::kHealthRuntimeKind || offset!=healthBinding.offset
        || self==UINT32_MAX || at<std::uint32_t>(healthBytes.data()+0x2C)!=entity.handle
        || !read.resolve(self,resolved) || resolved!=health) return;
    std::uint32_t finalGeneration{},finalCommitted{};gn::Weak finalEntity{};
    if(!read.value(source+0x180,finalGeneration) || finalGeneration!=generation
        || !read.value(source+0x2F0,finalCommitted) || finalCommitted!=generation
        || !read.value(source+0x440,finalEntity) || finalEntity!=entity || !read.weak(finalEntity)
        || !eater::owner_current(request.owner)) return;
    eater::observe_health({{run,generation},binding->asset,source,entity.handle,entity.serial,self},
        (at<std::uint8_t>(healthBytes.data()+0x338)&1U)!=0);
}
