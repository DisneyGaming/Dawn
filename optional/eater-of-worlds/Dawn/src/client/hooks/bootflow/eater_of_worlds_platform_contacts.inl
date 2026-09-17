// Read-only platform bindings and current-player volume receipts. Metadata is
// searched only on first bind or after a cached native identity stops matching.
#include "eater_of_worlds_arrival_motion.inl"
namespace eater_platform_contacts {
namespace eater=state::activity::eater_of_worlds;
namespace gn=gateway_native;
namespace contact=eater_platform_contact_native;
namespace cache=eater_platform_binding_cache;
inline constexpr cache::Definition deviceDefinition{eater::kReactorPlatformDevice.definition,
    eater::kReactorPlatformDevice.runtimeKind,eater::kReactorPlatformDevice.runtimeOffset};
struct Slot final {
    state::activity::coo::ObjectReceipt object{};
    std::uintptr_t source{};
    std::uint32_t bundle{UINT32_MAX};
    cache::Components components{};
    friend bool operator==(const Slot&,const Slot&)=default;
};
struct Snapshot {Slot slot{};std::uint64_t epoch{};};
SRWLOCK lock=SRWLOCK_INIT;
std::array<Slot,std::size(eater::kReactorPlatforms)> slots{};
std::array<std::uint8_t,std::size(eater::kReactorPlatforms)> occupancyStates{};
cache::SampleGate sampleGate{};
std::uint64_t run{},epoch{1};

void reset() noexcept {
    AcquireSRWLockExclusive(&lock);
    slots={};occupancyStates={};sampleGate={};run=0;if(++epoch==0) ++epoch;
    ReleaseSRWLockExclusive(&lock);
}
bool retained(std::size_t index,const Snapshot& saved) noexcept {
    bool valid{};AcquireSRWLockShared(&lock);
    valid=index<slots.size() && epoch==saved.epoch && run==saved.slot.object.owner.run
        && slots[index]==saved.slot;
    ReleaseSRWLockShared(&lock);return valid;
}
void invalidate(std::size_t index,const Snapshot& saved) noexcept {
    AcquireSRWLockExclusive(&lock);
    if(index<slots.size() && epoch==saved.epoch && slots[index]==saved.slot) {
        slots[index]={};occupancyStates[index]=0;
    }
    ReleaseSRWLockExclusive(&lock);
}

bool source_current(gn::Read& read,const Slot& slot,const eater::ObjectRequest& request,
                    std::size_t assetIndex) noexcept {
    if(assetIndex>=std::size(eater::kAssets) || !request.owner.valid() || !request.enabled
        || request.owner.run!=slot.object.owner.run
        || eater::kAssets[assetIndex].asset!=slot.object.source) return false;
    const auto& desired=request.desired;
    if(!desired.managed || !desired.desired || !desired.prepared || !desired.active
        || !desired.acknowledged || desired.generation!=slot.object.owner.value) return false;
    const auto& asset=eater::kAssets[assetIndex];
    std::array<std::byte,0x448> source{};std::array<std::byte,0x50> entityRow{};
    std::uintptr_t row{};gn::Weak again{};std::uint32_t generation{},committed{};
    if(!read.copy(slot.source,source)
        || !prefix(source.data(),asset.asset.definition,0x80809928U,asset.offset)
        || at<std::uint32_t>(source.data()+0x180)!=slot.object.owner.value
        || at<std::uint32_t>(source.data()+0x2F0)!=slot.object.owner.value
        || at<std::uint8_t>(source.data()+0x188)!=1) return false;
    const auto entity=at<gn::Weak>(source.data()+0x440);
    return entity.handle==slot.object.entity && entity.serial==slot.object.serial
        && read.entity_row(entity,row) && read.copy(row,entityRow)
        && at<std::uint32_t>(entityRow.data()+0xC)==entity.handle
        && !(at<std::uint32_t>(entityRow.data()+4)&4U)
        && at<std::uint32_t>(entityRow.data()+0x4C)==slot.bundle
        && read.value(slot.source+0x180,generation) && generation==slot.object.owner.value
        && read.value(slot.source+0x2F0,committed) && committed==generation
        && read.value(slot.source+0x440,again) && again==entity && read.weak(again);
}
} // namespace eater_platform_contacts

void reset_eater_platform_contacts() noexcept {eater_platform_contacts::reset();eater_arrival_motion::reset();}

bool observe_eater_platform_physics(void* raw,std::size_t assetIndex,std::uint32_t bundle,
    const state::activity::coo::ObjectReceipt& object,
    const state::activity::eater_of_worlds::ObjectRequest& request,
    eater_platform_contacts::Snapshot& out) noexcept {
    namespace native=eater_platform_contacts;namespace eater=native::eater;namespace gn=native::gn;
    const auto index=eater::platform_index(object.source);
    if(index>=native::slots.size() || !object.owner.valid()) return false;
    native::Snapshot saved{};bool admissible{};
    AcquireSRWLockShared(&native::lock);
    admissible=!native::run || native::run==object.owner.run;
    saved={native::slots[index],native::epoch};
    ReleaseSRWLockShared(&native::lock);
    if(!admissible) return false;
    native::Slot candidate{object,reinterpret_cast<std::uintptr_t>(raw),bundle,{}};
    const bool same=saved.slot.object==object && saved.slot.source==candidate.source
        && saved.slot.bundle==bundle;
    gn::Read sourceRead{g_image};
    if(!native::source_current(sourceRead,candidate,request,assetIndex)) {
        if(same) native::invalidate(index,saved);
        return false;
    }
    gn::Read cachedRead{g_image};
    const bool hit=same && native::cache::current(cachedRead,g_image,object.entity,
        native::deviceDefinition,saved.slot.components);
    if(hit) candidate.components=saved.slot.components;
    else {
        native::invalidate(index,saved);
        // An invalidated matching entry is now empty. A concurrent replacement
        // or reset will fail the publication comparison below.
        saved.slot={};
        gn::Read discovery{g_image};
        if(!native::cache::discover(discovery,g_image,bundle,object.entity,
            native::deviceDefinition,candidate.components)) return false;
    }
    gn::Read terminal{g_image};
    if(!native::source_current(terminal,candidate,request,assetIndex)
        || (!hit && !native::cache::current(terminal,g_image,object.entity,
            native::deviceDefinition,candidate.components))
        || eater::object_request(assetIndex)!=request
        || !eater::owner_current(request.owner)) return false;
    bool stored{};AcquireSRWLockExclusive(&native::lock);
    if(native::epoch==saved.epoch && (!native::run || native::run==object.owner.run)
        && native::slots[index]==saved.slot) {
        native::run=object.owner.run;native::slots[index]=candidate;out={candidate,saved.epoch};stored=true;
    }
    ReleaseSRWLockExclusive(&native::lock);
    if(stored && !hit) report("ev=eater_of_worlds stage=platform_physics_bound run=%llu path=%u index=%u registry=%08X slot=%u source=%08X generation=%u entity=%08X serial=%u component=%p volume_component=%p volume_body=%p policy=cached_authored_platform_volume mutation=observe_only",
        static_cast<unsigned long long>(object.owner.run),
        static_cast<unsigned>(eater::kReactorPlatforms[index].path+1),
        static_cast<unsigned>(eater::kReactorPlatforms[index].index),object.source.registry,
        static_cast<unsigned>(object.source.slot),object.source.definition,object.owner.value,
        object.entity,object.serial,reinterpret_cast<void*>(candidate.components.physics.address),
        reinterpret_cast<void*>(candidate.components.volume.address),reinterpret_cast<void*>(candidate.components.body));
    return stored;
}

void observe_eater_platform_contacts(void* playerPhysics,std::uint32_t player,
                                     std::uint64_t observedAt) noexcept {
    observe_eater_arrival_motion(playerPhysics,player,observedAt);
    namespace native=eater_platform_contacts;namespace eater=native::eater;namespace gn=native::gn;
    const auto playerAddress=reinterpret_cast<std::uintptr_t>(playerPhysics);
    if(!native::contact::can_add(playerAddress,0x208) || player==UINT32_MAX || !observedAt) return;
    const auto request=eater::contact_request();
    if(!request.ready || request.player!=player) return;
    const auto expected=request.platformIndex;
    native::Snapshot saved{};bool claimed{};
    AcquireSRWLockExclusive(&native::lock);
    if(native::run==request.object.owner.run && expected<native::slots.size()
        && native::sampleGate.claim({request.object.owner.run,player,request.attempt,expected},observedAt)) {
        saved={native::slots[expected],native::epoch};claimed=true;
    }
    ReleaseSRWLockExclusive(&native::lock);
    if(!claimed || saved.slot.object.source!=eater::kReactorPlatforms[expected].source) return;
    const auto& slot=saved.slot;gn::Read read{g_image};
    if(!native::source_current(read,slot,request.object,request.assetIndex)
        || !native::cache::current(read,g_image,slot.object.entity,native::deviceDefinition,slot.components)) {
        native::invalidate(expected,saved);return;
    }
    const auto volumeBody=slot.components.body;
    std::array<std::byte,0x30> playerHeader{};std::uint32_t playerSelf{};
    std::uintptr_t resolved{},playerRows{},playerBody{};std::int32_t playerCount{},playerIndex{};
    if(!read.copy(playerAddress,playerHeader)
        || !prefix(playerHeader.data(),native::contact::kPlayerPhysicsConfig,
                   native::contact::kPhysicsKind,native::contact::kPlayerPhysicsOffset)
        || (playerSelf=at<std::uint32_t>(playerHeader.data()+0x24))==UINT32_MAX
        || at<std::uint32_t>(playerHeader.data()+0x2C)!=player
        || !read.resolve(playerSelf,resolved) || resolved!=playerAddress
        || !read.value(playerAddress+0x190,playerRows) || !read.value(playerAddress+0x198,playerCount)
        || !read.value(playerAddress+0x204,playerIndex) || playerRows<0x10000 || playerCount<=0
        || playerCount>static_cast<std::int32_t>(native::contact::kMaximumBodies)
        || playerIndex<0 || playerIndex>=playerCount
        || !native::contact::can_add(playerRows,static_cast<std::size_t>(playerCount)*0x50)
        || !read.value(playerRows+static_cast<std::uintptr_t>(playerIndex)*0x50+0x20,playerBody)) return;
    // scan_once itself rechecks its collision rows, managers, worlds and contact
    // atoms. Two scans bracket the complete source/component/player proof.
    const auto occupancy=native::contact::scan_once(read,g_image,playerBody,volumeBody);
    if(occupancy==native::contact::Occupancy::invalid) return;
    gn::Read terminal{g_image};std::array<std::byte,0x30> finalHeader{};
    std::uintptr_t finalRows{},finalBody{},finalResolved{};std::int32_t finalCount{},finalIndex{};
    std::uint32_t controlled{};
    if(eater::contact_request()!=request
        || !native::source_current(terminal,slot,request.object,request.assetIndex)
        || !native::cache::current(terminal,g_image,slot.object.entity,native::deviceDefinition,slot.components)
        || !terminal.copy(playerAddress,finalHeader) || finalHeader!=playerHeader
        || !terminal.resolve(playerSelf,finalResolved) || finalResolved!=playerAddress
        || !terminal.value(playerAddress+0x190,finalRows) || finalRows!=playerRows
        || !terminal.value(playerAddress+0x198,finalCount) || finalCount!=playerCount
        || !terminal.value(playerAddress+0x204,finalIndex) || finalIndex!=playerIndex
        || !terminal.value(finalRows+static_cast<std::uintptr_t>(finalIndex)*0x50+0x20,finalBody)
        || finalBody!=playerBody
        || !hooks::teleport::read_local_player_entity(playerPhysics,controlled) || controlled!=player) return;
    gn::Read finalContact{g_image};
    const auto finalOccupancy=native::contact::scan_once(finalContact,g_image,playerBody,volumeBody);
    if(finalOccupancy!=occupancy || eater::contact_request()!=request
        || !native::retained(expected,saved)) return;
    const bool occupied=finalOccupancy==native::contact::Occupancy::present;
    bool changed{},current{};AcquireSRWLockExclusive(&native::lock);
    if(native::epoch==saved.epoch && native::run==request.object.owner.run
        && native::slots[expected]==slot) {
        current=true;const auto value=static_cast<std::uint8_t>(occupied?2:1);
        changed=native::occupancyStates[expected]!=value;native::occupancyStates[expected]=value;
    }
    ReleaseSRWLockExclusive(&native::lock);
    if(!current) return;
    if(changed) report("ev=eater_of_worlds stage=platform_occupancy run=%llu path=%u index=%u registry=%08X slot=%u source=%08X entity=%08X player=%08X occupied=%u policy=authored_platform_volume_solo_dwell mutation=observe_only",
        static_cast<unsigned long long>(slot.object.owner.run),static_cast<unsigned>(request.path),
        static_cast<unsigned>(request.next),slot.object.source.registry,
        static_cast<unsigned>(slot.object.source.slot),slot.object.source.definition,slot.object.entity,player,occupied?1U:0U);
    static_cast<void>(eater::observe_platform_contact({slot.object,observedAt,player,occupied}));
}
