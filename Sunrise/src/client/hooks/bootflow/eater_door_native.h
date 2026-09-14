#pragma once

#include "coo_native_components.h"
#include "gateway_native_read.h"
#include "../../../state/activity/eater_of_worlds/doors.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sunrise::client::hooks::bootflow::eater_door_native {
namespace eater=state::activity::eater_of_worlds;
namespace coo=state::activity::coo;
namespace gn=gateway_native;

// Pinned build-86657 activity runtime map. This is the same native map used by
// the roster sensor consumer: selected activity -> registry group -> slot ref.
inline constexpr std::uintptr_t kActivityIndexRva=0x1F91FE8U;
inline constexpr std::uintptr_t kActivityTableRva=0x2109B80U;
inline constexpr std::uintptr_t kActivityTableStrideRva=0x2109B90U;
inline constexpr std::size_t kRegistryGroupsOffset=0xCU;
inline constexpr std::size_t kRegistryGroupStride=0x18U;
inline constexpr std::size_t kRegistrySlotsOffset=0xC20U;
inline constexpr std::size_t kRegistrySlotStride=0x28U;
inline constexpr std::uint32_t kGateRuntimeKind=0x80804F46U;
inline constexpr std::uint32_t kObjectRuntimeKind=0x80809928U;

struct RuntimeGroup final {
    std::uint32_t slots{},first{},registry{},unknown0{},unknown1{},unknown2{};
};
struct RuntimeSlotRef final {std::uint32_t handle{UINT32_MAX},kind{},offset{};};
struct Header final {std::uint32_t definition{},kind{};std::uint64_t offset{};};

struct ObjectDevice final {
    std::uintptr_t source{},device{};std::uint32_t generation{},bundle{UINT32_MAX};
    gn::Weak entity{},deviceWeak{};
    friend bool operator==(const ObjectDevice&,const ObjectDevice&)=default;
};

struct Candidate final {
    const eater::GateObjectBinding* binding{};
    std::uintptr_t gateSource{},objectSource{},device{};
    std::uint32_t generation{},bundle{UINT32_MAX};
    gn::Weak entity{},deviceWeak{},previous{};
};

struct GatePose final {
    std::uintptr_t source{},device{};
    gn::Weak owner{},deviceWeak{};
    std::int32_t revision{-1};float position{};
    friend bool operator==(const GatePose&,const GatePose&)=default;
};

enum class BindResult : std::uint8_t {
    invalid,
    occupied,
    raced,
    alreadyBound,
    bound,
    invalidated,
};
enum class BindingState : std::uint8_t {unknown,present,missing};

[[nodiscard]] inline std::uint64_t packed(gn::Weak weak) noexcept {
    std::uint64_t value{};std::memcpy(&value,&weak,sizeof value);return value;
}

[[nodiscard]] inline bool add(std::uintptr_t base,std::uint64_t amount,
                              std::uintptr_t& result) noexcept {
    if(amount>UINTPTR_MAX || base>UINTPTR_MAX-static_cast<std::uintptr_t>(amount)) return false;
    result=base+static_cast<std::uintptr_t>(amount);return true;
}

template<class Read>
[[nodiscard]] bool object_device(Read& read,std::uintptr_t objectSource,coo::Asset object,
                                 std::uint32_t generation,eater::ObjectDeviceBinding expected,
                                 ObjectDevice& out) noexcept {
    out={};const auto* definition=eater::find(object.registry,object.type,object.slot);
    if(!definition || definition->asset!=object || object.type!=4
        || definition->component!=0x80809927U || definition->offset!=0x4C8U
        || !generation || generation>=0x7FFFFFFFU) return false;
    Header objectHeader{};std::uint32_t applied{},committed{},bundle{},rowEntity{};
    std::uint8_t active{};gn::Weak entity{};std::uintptr_t row{};
    if(!read.value(objectSource,objectHeader)
        || objectHeader.definition!=object.definition || objectHeader.kind!=kObjectRuntimeKind
        || objectHeader.offset!=definition->offset
        || !read.value(objectSource+0x180,applied) || applied!=generation
        || !read.value(objectSource+0x2F0,committed) || committed!=generation
        || !read.value(objectSource+0x188,active) || active!=1
        || !read.value(objectSource+0x440,entity) || !read.entity_row(entity,row)
        || !read.value(row+0xC,rowEntity) || rowEntity!=entity.handle
        || !read.value(row+0x4C,bundle)) return false;
    std::uintptr_t device{};
    if(!coo_native::component<Read,1024>(read,bundle,entity.handle,expected.kind,device)) return false;
    Header deviceHeader{};std::uint32_t self{},owner{};gn::Weak deviceWeak{};
    if(!read.value(device,deviceHeader) || deviceHeader.definition!=expected.configuration
        || deviceHeader.kind!=expected.kind || deviceHeader.offset!=expected.offset
        || !read.value(device+0x24,self) || !read.value(device+0x2C,owner)
        || owner!=entity.handle || !read.make_weak(self,deviceWeak) || !read.weak(deviceWeak)) return false;
    Header stableHeader{};std::uint32_t stableGeneration{},stableCommitted{},stableBundle{},stableRowEntity{};
    std::uint8_t stableActive{};gn::Weak stableEntity{};
    if(!read.value(objectSource,stableHeader) || std::memcmp(&stableHeader,&objectHeader,sizeof objectHeader)!=0
        || !read.value(objectSource+0x180,stableGeneration) || stableGeneration!=generation
        || !read.value(objectSource+0x2F0,stableCommitted) || stableCommitted!=generation
        || !read.value(objectSource+0x188,stableActive) || stableActive!=1
        || !read.value(objectSource+0x440,stableEntity) || stableEntity!=entity || !read.weak(stableEntity)
        || !read.value(row+0xC,stableRowEntity) || stableRowEntity!=entity.handle
        || !read.value(row+0x4C,stableBundle) || stableBundle!=bundle) return false;
    out={objectSource,device,generation,bundle,entity,deviceWeak};return true;
}

template<class Read>
[[nodiscard]] bool device_identity(Read& read,gn::Weak owner,gn::Weak deviceWeak,
                                   eater::ObjectDeviceBinding expected) noexcept {
    if(!read.weak(owner) || !read.weak(deviceWeak)) return false;
    std::uintptr_t row{},device{};std::uint32_t rowOwner{},self{},deviceOwner{};
    Header header{};gn::Weak current{};
    if(!read.entity_row(owner,row) || !read.value(row+0xC,rowOwner) || rowOwner!=owner.handle
        || !read.resolve(deviceWeak.handle,device) || !read.value(device,header)
        || header.definition!=expected.configuration || header.kind!=expected.kind
        || header.offset!=expected.offset || !read.value(device+0x24,self)
        || !read.value(device+0x2C,deviceOwner) || self!=deviceWeak.handle
        || deviceOwner!=owner.handle || !read.make_weak(self,current) || current!=deviceWeak) return false;
    std::uintptr_t stableDevice{};std::uint32_t stableSelf{},stableOwner{};Header stableHeader{};
    return read.weak(owner) && read.weak(deviceWeak) && read.resolve(deviceWeak.handle,stableDevice)
        && stableDevice==device && read.value(stableDevice,stableHeader)
        && std::memcmp(&stableHeader,&header,sizeof header)==0
        && read.value(stableDevice+0x24,stableSelf) && stableSelf==self
        && read.value(stableDevice+0x2C,stableOwner) && stableOwner==deviceOwner;
}

template<class Read>
[[nodiscard]] bool runtime_source(Read& read,std::uintptr_t image,const coo::Asset& asset,
                                  std::uintptr_t& source) noexcept {
    source=0;
    const auto* definition=eater::find(asset.registry,asset.type,asset.slot);
    if(!definition || definition->asset!=asset || asset.type!=23
        || definition->component!=0x80804F45U || definition->offset!=0x278U) return false;
    std::uintptr_t indexAddress{},tableAnchor{},strideAddress{};
    if(!add(image,kActivityIndexRva,indexAddress) || !add(image,kActivityTableRva,tableAnchor)
        || !add(image,kActivityTableStrideRva,strideAddress)) return false;
    std::uint16_t activity{};std::uint64_t tableRelative{},tableStride{};
    if(!read.value(indexAddress,activity) || !read.value(tableAnchor,tableRelative)
        || !read.value(strideAddress,tableStride) || tableStride<sizeof(std::uintptr_t)
        || tableStride>0x10000U) return false;
    std::uintptr_t table{},rootPointer{};
    if(!add(tableAnchor,tableRelative,table)
        || static_cast<std::uint64_t>(activity)>UINT64_MAX/tableStride
        || !add(table,static_cast<std::uint64_t>(activity)*tableStride,rootPointer)) return false;
    std::uintptr_t root{};std::uint32_t count{};
    if(!read.value(rootPointer,root) || root<0x10000
        || !read.value(root+8,count) || !count || count>128) return false;
    RuntimeGroup selected{};bool found{};
    for(std::uint32_t i=0;i<count;++i) {
        RuntimeGroup group{};std::uintptr_t row{};
        if(!add(root,kRegistryGroupsOffset+static_cast<std::uint64_t>(i)*kRegistryGroupStride,row)
            || !read.value(row,group)) return false;
        if(group.registry!=asset.registry) continue;
        if(found || !group.slots || group.slots>4096 || group.first>65535) return false;
        selected=group;found=true;
    }
    if(!found || asset.slot>=selected.slots
        || static_cast<std::uint64_t>(selected.first)+asset.slot>65535) return false;
    const auto ordinal=static_cast<std::uint64_t>(selected.first)+asset.slot;
    std::uintptr_t slotAddress{};
    if(!add(root,kRegistrySlotsOffset+ordinal*kRegistrySlotStride,slotAddress)) return false;
    RuntimeSlotRef reference{},again{};
    if(!read.value(slotAddress,reference) || reference.handle==UINT32_MAX
        || reference.kind!=definition->component || reference.offset>0x400000U) return false;
    std::uintptr_t base{},address{};
    if(!read.resolve(reference.handle,base) || !add(base,reference.offset,address)) return false;
    Header header{},stable{};
    if(!read.value(address,header) || header.definition!=asset.definition
        || header.kind!=kGateRuntimeKind || header.offset!=definition->offset
        || !read.value(slotAddress,again) || std::memcmp(&again,&reference,sizeof reference)!=0
        || !read.value(address,stable) || std::memcmp(&stable,&header,sizeof header)!=0) return false;
    source=address;return true;
}

template<class Read>
[[nodiscard]] BindingState binding_state(Read& read,std::uintptr_t image,coo::Asset gate,
                                         gn::Weak expected) noexcept {
    if(expected.handle==UINT32_MAX) return BindingState::missing;
    std::uintptr_t source{};gn::Weak current{};
    if(!runtime_source(read,image,gate,source) || !read.value(source+0x1F0,current))
        return BindingState::unknown;
    return current==expected && read.weak(expected)?BindingState::present:BindingState::missing;
}
template<class Read>
[[nodiscard]] BindingState binding_state(Read& read,std::uintptr_t image,coo::Asset gate,
                                         gn::Weak expected,gn::Weak owner) noexcept {
    const auto present=binding_state(read,image,gate,expected);
    if(present!=BindingState::present) return present;
    const auto* binding=eater::gate_device_binding(gate);
    return binding && device_identity(read,owner,expected,binding->device)
        ?BindingState::present:BindingState::missing;
}

// Resolve an authored type-23 gate and its bound generic device on every
// sample. A receipt is available only when the device's current and target
// positions agree at the gate's committed revision.
template<class Read>
[[nodiscard]] bool gate_pose(Read& read,std::uintptr_t image,coo::Asset gate,
                             GatePose& out) noexcept {
    out={};const auto* expected=eater::gate_device_binding(gate);
    if(!expected) return false;
    std::uintptr_t source{};
    if(!runtime_source(read,image,gate,source)) return false;
    float committed{};std::uint16_t committedRevision{};gn::Weak bound{};
    if(!read.value(source+0x1C0,committed) || !std::isfinite(committed)
        || !read.value(source+0x1C4,committedRevision)
        || !read.value(source+0x1F0,bound) || !read.weak(bound)) return false;
    std::uintptr_t device{};
    if(!read.resolve(bound.handle,device)) return false;
    Header header{};std::uint32_t self{},ownerHandle{},rowOwner{};
    gn::Weak currentDevice{},owner{};std::uintptr_t row{};
    std::array<std::byte,16> pose{};
    std::int32_t revision{};
    if(!read.value(device,header) || header.definition!=expected->device.configuration
        || header.kind!=expected->device.kind || header.offset!=expected->device.offset
        || !read.value(device+0x24,self) || self!=bound.handle
        || !read.make_weak(self,currentDevice) || currentDevice!=bound
        || !read.value(device+0x2C,ownerHandle) || ownerHandle==UINT32_MAX
        || !read.make_weak(ownerHandle,owner) || !read.entity_row(owner,row)
        || !read.value(row+0xC,rowOwner) || rowOwner!=owner.handle
        || !read.copy(device+0x370,pose) || !read.value(device+0x960,revision)) return false;
    const auto current=gn::at<float>(pose.data()),target=gn::at<float>(pose.data()+12);
    if(revision<0 || revision!=committedRevision || !std::isfinite(current)
        || !std::isfinite(target) || current!=target || target!=committed) return false;
    std::uintptr_t stableSource{},stableDevice{},stableRow{};Header stableHeader{};
    float stableCommitted{};std::uint16_t stableCommittedRevision{};
    std::uint32_t stableSelf{},stableOwnerHandle{},stableRowOwner{};
    gn::Weak stableBound{},stableDeviceWeak{},stableOwner{};
    std::array<std::byte,16> stablePose{};std::int32_t stableRevision{};
    if(!runtime_source(read,image,gate,stableSource) || stableSource!=source
        || !read.value(stableSource+0x1C0,stableCommitted) || stableCommitted!=committed
        || !read.value(stableSource+0x1C4,stableCommittedRevision)
        || stableCommittedRevision!=committedRevision
        || !read.value(stableSource+0x1F0,stableBound) || stableBound!=bound
        || !read.weak(stableBound) || !read.resolve(stableBound.handle,stableDevice)
        || stableDevice!=device || !read.value(stableDevice,stableHeader)
        || std::memcmp(&stableHeader,&header,sizeof header)!=0
        || !read.value(stableDevice+0x24,stableSelf) || stableSelf!=self
        || !read.make_weak(stableSelf,stableDeviceWeak) || stableDeviceWeak!=bound
        || !read.value(stableDevice+0x2C,stableOwnerHandle) || stableOwnerHandle!=ownerHandle
        || !read.make_weak(stableOwnerHandle,stableOwner) || stableOwner!=owner
        || !read.entity_row(stableOwner,stableRow) || stableRow!=row
        || !read.value(stableRow+0xC,stableRowOwner) || stableRowOwner!=ownerHandle
        || !read.copy(stableDevice+0x370,stablePose) || stablePose!=pose
        || !read.value(stableDevice+0x960,stableRevision) || stableRevision!=revision) return false;
    out={source,device,owner,bound,revision,target};return true;
}

template<class Read>
[[nodiscard]] bool discover(Read& read,std::uintptr_t image,std::uintptr_t objectSource,
                            coo::Asset object,std::uint32_t generation,Candidate& out) noexcept {
    out={};
    const auto* pair=eater::gate_object_binding(object);
    if(!pair || pair->object!=object) return false;
    ObjectDevice owned{};
    if(!object_device(read,objectSource,object,generation,pair->device,owned)) return false;
    std::uintptr_t gate{};
    if(!runtime_source(read,image,pair->gate,gate)) return false;
    gn::Weak previous{};
    if(!read.value(gate+0x1F0,previous)) return false;
    out={pair,gate,objectSource,owned.device,generation,owned.bundle,owned.entity,owned.deviceWeak,previous};return true;
}

[[nodiscard]] inline bool same_identity(const Candidate& left,const Candidate& right) noexcept {
    return left.binding==right.binding && left.gateSource==right.gateSource
        && left.objectSource==right.objectSource && left.device==right.device
        && left.generation==right.generation && left.bundle==right.bundle
        && left.entity==right.entity && left.deviceWeak==right.deviceWeak;
}

// Exchange supplies compare_exchange(address, expected, desired) and returns
// the value that occupied address at the exchange point.
template<class Read,class Exchange>
[[nodiscard]] BindResult bind(Read& read,Exchange& exchange,std::uintptr_t image,
                              std::uintptr_t objectSource,coo::Asset object,
                              std::uint32_t generation,Candidate* receipt=nullptr) noexcept {
    Candidate candidate{};
    if(!discover(read,image,objectSource,object,generation,candidate)) return BindResult::invalid;
    if(candidate.previous==candidate.deviceWeak) {
        if(receipt) *receipt=candidate;return BindResult::alreadyBound;
    }
    if(candidate.previous.handle!=UINT32_MAX && read.weak(candidate.previous)) {
        if(receipt) *receipt=candidate;return BindResult::occupied;
    }
    Candidate current{};
    if(!discover(read,image,objectSource,object,generation,current)
        || !same_identity(candidate,current) || current.previous!=candidate.previous) return BindResult::invalidated;
    const auto before=packed(candidate.previous),desired=packed(candidate.deviceWeak);
    const auto observed=exchange.compare_exchange(candidate.gateSource+0x1F0,before,desired);
    if(observed!=before) return observed==desired?BindResult::alreadyBound:BindResult::raced;
    Candidate after{};
    if(!discover(read,image,objectSource,object,generation,after)
        || !same_identity(candidate,after) || after.previous!=candidate.deviceWeak) {
        static_cast<void>(exchange.compare_exchange(candidate.gateSource+0x1F0,desired,before));
        return BindResult::invalidated;
    }
    if(receipt) *receipt=after;return BindResult::bound;
}

} // namespace sunrise::client::hooks::bootflow::eater_door_native
