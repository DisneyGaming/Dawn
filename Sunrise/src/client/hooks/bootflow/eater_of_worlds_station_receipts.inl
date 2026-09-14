// Exact Eater fire-station use receipts on the existing F36640 boundary.
// F32CD0 prompt publication is deliberately not treated as completed use.
namespace eater_station_native {
namespace eater=state::activity::eater_of_worlds;
namespace gn=gateway_native;

struct Slot final {eater::StationReceipt receipt{};};
struct Pending final {
    eater::StationReceipt receipt{};
    std::array<std::byte,8> requester{};
    std::uint32_t player{UINT32_MAX};
    std::size_t index{std::size(eater::kStationBindings)};
    bool valid() const noexcept {return receipt.generation.valid() && player!=UINT32_MAX
        && index<std::size(eater::kStationBindings);}
};

SRWLOCK lock=SRWLOCK_INIT;
std::array<Slot,std::size(eater::kStationBindings)> slots{};
std::uint64_t run{};
unsigned lines{};

bool same_identity(const eater::StationReceipt& a,const eater::StationReceipt& b) noexcept {
    return a.generation==b.generation && a.source==b.source && a.sourcePointer==b.sourcePointer
        && a.entity==b.entity && a.serial==b.serial && a.component==b.component;
}

void reset() noexcept {
    AcquireSRWLockExclusive(&lock);slots={};run=0;lines=0;ReleaseSRWLockExclusive(&lock);
}

bool select_run(std::uint64_t value) noexcept {
    if(!value || eater::native_run()!=value) return false;
    AcquireSRWLockExclusive(&lock);
    if(eater::native_run()!=value) {ReleaseSRWLockExclusive(&lock);return false;}
    if(run!=value) {slots={};run=value;lines=0;}
    ReleaseSRWLockExclusive(&lock);return true;
}

bool current(const eater::StationReceipt& receipt,std::size_t index,std::uintptr_t expected) noexcept {
    if(index>=std::size(eater::kStationBindings) || receipt.generation.run!=eater::native_run()) return false;
    const auto assetIndex=eater::asset_index(receipt.source);
    if(assetIndex==std::size(eater::kAssets)) return false;
    const auto request=eater::request();
    if(!request.owner.valid() || !request.frame.enabled) return false;
    const auto& desired=request.frame.native[assetIndex];
    if(!desired.managed || !desired.desired || !desired.prepared || !desired.active
        || !desired.acknowledged || desired.generation!=receipt.generation.value) return false;
    gn::Read read{g_image};std::array<std::byte,16> header{};std::uint32_t generation{},committed{};
    std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t component{};
    const auto& binding=eater::kStationBindings[index];
    if(binding.source!=receipt.source || !read.copy(receipt.sourcePointer,header)
        || !prefix(header.data(),binding.source.definition,0x80809928U,eater::kAssets[assetIndex].offset)
        || !read.value(receipt.sourcePointer+0x180,generation) || generation!=receipt.generation.value
        || !read.value(receipt.sourcePointer+0x2F0,committed) || committed!=generation
        || !read.value(receipt.sourcePointer+0x188,active) || active!=1
        || !read.value(receipt.sourcePointer+0x440,entity)
        || entity.handle!=receipt.entity || entity.serial!=receipt.serial || !read.weak(entity)
        || !read.resolve(receipt.component,component) || component!=expected) return false;
    std::array<std::byte,0x30> componentHeader{};
    if(!read.copy(component,componentHeader)
        || !prefix(componentHeader.data(),binding.config,eater::kStationInteractionKind,binding.offset)
        || at<std::uint32_t>(componentHeader.data()+0x24)!=receipt.component
        || at<std::uint32_t>(componentHeader.data()+0x2C)!=receipt.entity) return false;
    const auto final=eater::request();std::uint32_t finalGeneration{},finalCommitted{};
    return final.owner==request.owner && final.frame.enabled
        && final.frame.native[assetIndex].generation==desired.generation
        && final.frame.native[assetIndex].managed && final.frame.native[assetIndex].desired
        && final.frame.native[assetIndex].prepared && final.frame.native[assetIndex].active
        && final.frame.native[assetIndex].acknowledged
        && read.value(receipt.sourcePointer+0x180,finalGeneration) && finalGeneration==generation
        && read.value(receipt.sourcePointer+0x2F0,finalCommitted) && finalCommitted==committed
        && read.value(receipt.sourcePointer+0x440,again) && again==entity && read.weak(again);
}

} // namespace eater_station_native

void reset_eater_station_receipts() noexcept {eater_station_native::reset();}
void reset_eater_of_worlds_native_receipts() noexcept {
    reset_eater_cranium_receipts();reset_eater_station_receipts();reset_eater_platform_contacts();
    reset_eater_door_receipts();
}

void observe_eater_station_source(void* raw,std::size_t assetIndex) noexcept {
    namespace native=eater_station_native;namespace eater=native::eater;namespace gn=native::gn;
    const auto activeRun=eater::native_run();
    if(!activeRun || assetIndex>=std::size(eater::kAssets)) return;
    const auto index=eater::station_index(eater::kAssets[assetIndex].asset);
    if(index==std::size(eater::kStationBindings) || !native::select_run(activeRun)) return;
    const auto source=reinterpret_cast<std::uintptr_t>(raw);gn::Read read{g_image};
    const auto& binding=eater::kStationBindings[index];
    std::array<std::byte,16> header{};
    if(!read.copy(source,header)
        || binding.source!=eater::kAssets[assetIndex].asset
        || !prefix(header.data(),binding.source.definition,0x80809928U,
                   eater::kAssets[assetIndex].offset)) return;
    const auto request=eater::request();const auto& desired=request.frame.native[assetIndex];
    if(!request.owner.valid() || !request.frame.enabled || !desired.managed || !desired.desired
        || !desired.prepared || !desired.active || !desired.acknowledged) return;
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},again{};
    std::uintptr_t row{},component{},resolved{};
    if(!read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity)
        || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !coo_native::component<gn::Read,1024>(read,bundle,entity.handle,
                                                 eater::kStationInteractionKind,component)) return;
    std::array<std::byte,0x30> bytes{};
    if(!read.copy(component,bytes)
        || !prefix(bytes.data(),binding.config,eater::kStationInteractionKind,binding.offset)
        || at<std::uint32_t>(bytes.data()+0x2C)!=entity.handle) return;
    const auto self=at<std::uint32_t>(bytes.data()+0x24);
    if(self==UINT32_MAX || !read.resolve(self,resolved) || resolved!=component
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again)) return;
    eater::StationReceipt receipt{{activeRun,generation},binding.source,source,
        entity.handle,entity.serial,self,0,0,0};
    if(!native::current(receipt,index,component)) return;
    AcquireSRWLockExclusive(&native::lock);
    if(native::run==activeRun && !native::same_identity(native::slots[index].receipt,receipt))
        native::slots[index]={receipt};
    ReleaseSRWLockExclusive(&native::lock);
}

eater_station_native::Pending before_eater_station_use(void* component) noexcept {
    namespace native=eater_station_native;namespace eater=native::eater;
    const auto activeRun=eater::native_run();if(!native::select_run(activeRun)) return {};
    std::array<std::byte,0x2E8> bytes{};if(!copy(component,bytes)) return {};
    const auto self=at<std::uint32_t>(bytes.data()+0x24);
    const auto entity=at<std::uint32_t>(bytes.data()+0x2C);
    if(self==UINT32_MAX || entity==UINT32_MAX) return {};
    native::Pending result{};unsigned matches{};
    AcquireSRWLockShared(&native::lock);
    for(std::size_t i=0;i<std::size(native::slots);++i) if(native::slots[i].receipt.component==self
        && native::slots[i].receipt.entity==entity) {
        result.receipt=native::slots[i].receipt;result.index=i;++matches;
    }
    ReleaseSRWLockShared(&native::lock);
    if(matches!=1 || result.index>=std::size(eater::kStationBindings)
        || !prefix(bytes.data(),eater::kStationBindings[result.index].config,
                   eater::kStationInteractionKind,eater::kStationBindings[result.index].offset)
        || !native::current(result.receipt,result.index,reinterpret_cast<std::uintptr_t>(component))) return {};
    result.receipt.requested=at<std::int32_t>(bytes.data()+0x2DC);
    result.receipt.consumedBefore=at<std::int32_t>(bytes.data()+0x2D8);
    if(bytes[0x2C0]!=std::byte{} || at<std::uint8_t>(bytes.data()+0x2D0)>1
        || result.receipt.requested<=result.receipt.consumedBefore
        || result.receipt.consumedBefore<0) return {};
    const auto local=local_controlled_entity();if(local==UINT32_MAX
        || !public_event_deferred_placement_observer::live_entity(local)) return {};
    const auto requester=resolve_requester(static_cast<const std::byte*>(component)+0x2E0,local);
    if(requester.entity!=local || requester.localEntity!=local) return {};
    if(!current_eater_cranium_holder(result.receipt.source.registry,local,
                                     result.receipt.cranium)) return {};
    std::memcpy(result.requester.data(),bytes.data()+0x2E0,result.requester.size());
    result.player=local;return result;
}

void after_eater_station_use(void* component,eater_station_native::Pending pending) noexcept {
    namespace native=eater_station_native;namespace eater=native::eater;
    if(!pending.valid()) return;
    std::array<std::byte,0x2E8> bytes{};
    if(!copy(component,bytes)
        || !prefix(bytes.data(),eater::kStationBindings[pending.index].config,
                   eater::kStationInteractionKind,eater::kStationBindings[pending.index].offset)
        || at<std::uint32_t>(bytes.data()+0x24)!=pending.receipt.component
        || at<std::uint32_t>(bytes.data()+0x2C)!=pending.receipt.entity
        || bytes[0x2C0]!=std::byte{}
        || at<std::uint8_t>(bytes.data()+0x2D0)!=1
        || at<std::int32_t>(bytes.data()+0x2DC)!=pending.receipt.requested
        || std::memcmp(bytes.data()+0x2E0,pending.requester.data(),pending.requester.size())!=0
        || local_controlled_entity()!=pending.player
        || !public_event_deferred_placement_observer::live_entity(pending.player)) return;
    pending.receipt.consumedAfter=at<std::int32_t>(bytes.data()+0x2D8);
    if(pending.receipt.consumedAfter!=pending.receipt.requested
        || !native::current(pending.receipt,pending.index,reinterpret_cast<std::uintptr_t>(component))) return;
    const bool accepted=eater::observe_station(pending.receipt,pending.player);
    AcquireSRWLockExclusive(&native::lock);
    const bool reportLine=accepted && native::lines++<48;
    ReleaseSRWLockExclusive(&native::lock);
    if(reportLine) report("ev=eater_of_worlds stage=station_used run=%llu registry=%08X slot=%u source=%08X lane=%u generation=%u entity=%08X component=%08X player=%08X cranium=%08X cranium_component=%08X requested=%d consumed_before=%d consumed_after=%d mutation=observe_only",
        static_cast<unsigned long long>(pending.receipt.generation.run),pending.receipt.source.registry,
        static_cast<unsigned>(pending.receipt.source.slot),pending.receipt.source.definition,
        static_cast<unsigned>(eater::kStationBindings[pending.index].lane),pending.receipt.generation.value,
        pending.receipt.entity,pending.receipt.component,pending.player,pending.receipt.cranium.entity,
        pending.receipt.cranium.component,pending.receipt.requested,pending.receipt.consumedBefore,
        pending.receipt.consumedAfter);
}
