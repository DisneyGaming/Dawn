#include "../src/client/hooks/bootflow/eater_door_native.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace eater_door_fixture {
namespace native=dawn::client::hooks::bootflow::eater_door_native;
namespace gn=dawn::client::hooks::bootflow::gateway_native;

struct Segment final {std::uintptr_t address{};std::vector<std::byte> bytes{};};
struct Resolution final {std::uint32_t handle{};std::uintptr_t base{},allocation{};};
struct Read final {
    std::vector<Segment> segments{};std::vector<Resolution> resolutions{};
    std::vector<gn::Weak> weaks{};gn::Weak entity{};std::uintptr_t entityRow{};
    bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
        for(const auto& segment:segments) if(address>=segment.address
            && address-segment.address<=segment.bytes.size()
            && out.size()<=segment.bytes.size()-(address-segment.address)) {
            std::memcpy(out.data(),segment.bytes.data()+address-segment.address,out.size());return true;
        }
        return false;
    }
    template<class T> bool value(std::uintptr_t address,T& out) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}}));
    }
    template<class T> void put(std::uintptr_t address,const T& value) noexcept {
        for(auto& segment:segments) if(address>=segment.address
            && address-segment.address<=segment.bytes.size()
            && sizeof value<=segment.bytes.size()-(address-segment.address)) {
            std::memcpy(segment.bytes.data()+address-segment.address,&value,sizeof value);return;
        }
        std::abort();
    }
    bool resolve(std::uint32_t handle,std::uintptr_t& base,std::uintptr_t* allocation=nullptr) noexcept {
        for(const auto& item:resolutions) if(item.handle==handle) {
            base=item.base;if(allocation) *allocation=item.allocation;return true;
        }
        return false;
    }
    bool make_weak(std::uint32_t handle,gn::Weak& out) noexcept {
        for(const auto& item:weaks) if(item.handle==handle) {out=item;return true;}
        out={};return false;
    }
    bool weak(gn::Weak value) noexcept {
        return std::find(weaks.begin(),weaks.end(),value)!=weaks.end();
    }
    bool entity_row(gn::Weak value,std::uintptr_t& row) noexcept {
        if(value!=entity || !weak(value)) return false;row=entityRow;return true;
    }
};

struct Fixture final {
    static constexpr std::uintptr_t image=0x10000000,gate=0x20000000,source=0x21000000;
    static constexpr std::uintptr_t row=0x22000000,bundle=0x23000000,metadata=0x24000000;
    static constexpr std::uintptr_t firstDevice=bundle+0x100,secondDevice=bundle+0x200;
    static constexpr std::uintptr_t root=0x25000000;
    static constexpr std::uint32_t gateRef=0x101U,bundleRef=0x102U,metadataRef=0x103U;
    static constexpr gn::Weak firstEntity{0x11111111U,0x61FAA015U};
    static constexpr gn::Weak firstDeviceWeak{0x22222222U,0x51F9E015U};
    static constexpr gn::Weak secondEntity{0x33333333U,0x62FAA016U};
    static constexpr gn::Weak secondDeviceWeak{0x44444444U,0x52F9E016U};
    Read read{};unsigned exchanges{};bool invalidateAfterExchange{};

    Fixture() {
        const auto activityIndex=image+native::kActivityIndexRva;
        const auto tableAnchor=image+native::kActivityTableRva;
        read.segments={{activityIndex,std::vector<std::byte>(2)},
            {tableAnchor,std::vector<std::byte>(0x500)},
            {root,std::vector<std::byte>(0xE00)},
            {gate,std::vector<std::byte>(0x300)},
            {source,std::vector<std::byte>(0x500)},
            {row,std::vector<std::byte>(0x60)},
            {bundle,std::vector<std::byte>(0x1000)},
            {metadata,std::vector<std::byte>(0x300)}};
        read.put(activityIndex,std::uint16_t{2});read.put(tableAnchor,std::uint64_t{0x400});
        read.put(image+native::kActivityTableStrideRva,std::uint64_t{8});
        read.put(tableAnchor+0x400+16,root);read.put(root+8,std::uint32_t{1});
        read.put(root+native::kRegistryGroupsOffset,native::RuntimeGroup{44,0,0x93BF5E9DU});
        read.put(root+native::kRegistrySlotsOffset+2*native::kRegistrySlotStride,
            native::RuntimeSlotRef{gateRef,0x80804F45U,0});
        read.put(gate,native::Header{0x80B49F18U,native::kGateRuntimeKind,0x278});
        read.put(gate+0x1C0,1.F);read.put(gate+0x1C4,std::uint16_t{1});
        read.put(gate+0x1F0,gn::Weak{});
        read.resolutions={{gateRef,gate,gate+0x280},{bundleRef,bundle,bundle+0x800},
            {metadataRef,metadata,metadata+0x280},{firstDeviceWeak.handle,firstDevice,firstDevice+0x800}};
        read.weaks={firstEntity,firstDeviceWeak};read.entity=firstEntity;read.entityRow=row;
        object(1,firstEntity,firstDevice,firstDeviceWeak);
    }
    void object(std::uint32_t generation,gn::Weak entity,std::uintptr_t device,gn::Weak deviceWeak) {
        read.put(source,native::Header{0x80B49FD6U,native::kObjectRuntimeKind,0x4C8});
        read.put(source+0x180,generation);read.put(source+0x2F0,generation);
        read.put(source+0x188,std::uint8_t{1});read.put(source+0x440,entity);
        read.put(row+0xC,entity.handle);read.put(row+0x4C,bundleRef);
        read.put(bundle,std::uint32_t{});read.put(bundle+4,metadataRef);
        read.put(bundle+0x818,UINT32_MAX);read.put(metadata+0x68,std::uint64_t{1});
        read.put(metadata+0x70,std::int64_t{0x100});
        read.put(metadata+0x180+0x14,static_cast<std::int32_t>(device-bundle));
        read.put(device,native::Header{0x80F3D672U,0x80803910U,0xA78});
        read.put(device+0x24,deviceWeak.handle);read.put(device+0x2C,entity.handle);
        read.put(device+0x370,1.F);read.put(device+0x37C,1.F);
        read.put(device+0x960,static_cast<std::int32_t>(generation));
    }
    std::uint64_t compare_exchange(std::uintptr_t address,std::uint64_t expected,
                                   std::uint64_t desired) noexcept {
        ++exchanges;std::uint64_t current{};if(!read.value(address,current)) return desired;
        if(current==expected) read.put(address,desired);
        if(invalidateAfterExchange) read.put(source+0x180,std::uint32_t{2});
        return current;
    }
    gn::Weak bound() {gn::Weak value{};check(read.value(gate+0x1F0,value),"fixture gate binding readable");return value;}
    void relocate() {
        read.weaks={secondEntity,secondDeviceWeak};read.entity=secondEntity;
        read.resolutions.erase(std::remove_if(read.resolutions.begin(),read.resolutions.end(),
            [](const auto& item){return item.handle==firstDeviceWeak.handle;}),read.resolutions.end());
        read.resolutions.push_back({secondDeviceWeak.handle,secondDevice,secondDevice+0x800});
        object(2,secondEntity,secondDevice,secondDeviceWeak);
    }
    void use_ejection_gate() {
        read.put(root+native::kRegistrySlotsOffset,native::RuntimeSlotRef{gateRef,0x80804F45U,0});
        read.put(gate,native::Header{0x80B49EF7U,native::kGateRuntimeKind,0x278});
        read.put(gate+0x1F0,firstDeviceWeak);
        read.put(firstDevice,native::Header{0x80C7069BU,0x80803910U,0xA78});
    }
};
}

static void eater_door_native_binding_tests() {
    namespace f=eater_door_fixture;namespace n=f::native;
    {
        f::Fixture fixture;n::Candidate receipt{};
        check(n::bind(fixture.read,fixture,f::Fixture::image,f::Fixture::source,m::kAirlockExitObject,1,&receipt)
            ==n::BindResult::bound,"unbound exit gate receives exact spawned generic-device weak");
        check(fixture.bound()==f::Fixture::firstDeviceWeak && receipt.device==f::Fixture::firstDevice,
            "binding receipt preserves full salted device identity");
        n::GatePose applied{};
        check(n::gate_pose(fixture.read,f::Fixture::image,m::kAirlock.exitDoor,applied)
            && applied.device==f::Fixture::firstDevice
            && applied.deviceWeak==f::Fixture::firstDeviceWeak
            && applied.owner==f::Fixture::firstEntity && applied.revision==1 && applied.position==1.F,
            "exit receipt requires the bound device's exact applied open pose");
        check(n::binding_state(fixture.read,f::Fixture::image,m::kAirlock.exitDoor,
            f::Fixture::firstDeviceWeak,f::Fixture::firstEntity)==n::BindingState::present,
            "completed binding fast path verifies the current full weak target");
        fixture.read.put(f::Fixture::gate+0x1F0,f::gn::Weak{});
        check(n::binding_state(fixture.read,f::Fixture::image,m::kAirlock.exitDoor,
            f::Fixture::firstDeviceWeak,f::Fixture::firstEntity)==n::BindingState::missing,
            "cleared gate target rearms binding even when the object incarnation is unchanged");
        fixture.read.put(f::Fixture::gate+0x1F0,f::Fixture::firstDeviceWeak);
        fixture.read.put(f::Fixture::firstDevice+0x370,0.5F);
        check(!n::gate_pose(fixture.read,f::Fixture::image,m::kAirlock.exitDoor,applied),
            "an in-flight target is not acknowledged before current pose reaches it");
        fixture.read.put(f::Fixture::firstDevice+0x370,1.F);
        const auto exchanges=fixture.exchanges;
        check(n::bind(fixture.read,fixture,f::Fixture::image,f::Fixture::source,m::kAirlockExitObject,1)
            ==n::BindResult::alreadyBound && fixture.exchanges==exchanges,
            "current exit binding is idempotent without another memory write");
        fixture.relocate();
        check(n::bind(fixture.read,fixture,f::Fixture::image,f::Fixture::source,m::kAirlockExitObject,2)
            ==n::BindResult::bound && fixture.bound()==f::Fixture::secondDeviceWeak,
            "death replacement resolves the relocated device and replaces only its stale weak");
    }
    {
        f::Fixture fixture;constexpr f::gn::Weak nativeOwner{0xABCDEF01U,0x71F9E017U};
        fixture.read.weaks.push_back(nativeOwner);fixture.read.put(f::Fixture::gate+0x1F0,nativeOwner);
        check(n::bind(fixture.read,fixture,f::Fixture::image,f::Fixture::source,m::kAirlockExitObject,1)
            ==n::BindResult::occupied && fixture.bound()==nativeOwner && fixture.exchanges==0,
            "a different valid native gate owner is never overwritten");
        check(n::binding_state(fixture.read,f::Fixture::image,m::kAirlock.exitDoor,nativeOwner)
            ==n::BindingState::present,
            "foreign native ownership uses the non-mutating fast path while its weak remains live");
    }
    {
        f::Fixture fixture;fixture.read.put(f::Fixture::firstDevice+0x2C,f::Fixture::firstEntity.handle+1);
        check(n::bind(fixture.read,fixture,f::Fixture::image,f::Fixture::source,m::kAirlockExitObject,1)
            ==n::BindResult::invalid && fixture.exchanges==0,
            "generic device owned by another entity is rejected before mutation");
    }
    {
        f::Fixture fixture;fixture.invalidateAfterExchange=true;
        check(n::bind(fixture.read,fixture,f::Fixture::image,f::Fixture::source,m::kAirlockExitObject,1)
            ==n::BindResult::invalidated && fixture.bound()==f::gn::Weak{} && fixture.exchanges==2,
            "source incarnation change after exchange rolls the weak binding back");
    }
    {
        f::Fixture fixture;fixture.use_ejection_gate();n::GatePose applied{};
        check(n::gate_pose(fixture.read,f::Fixture::image,m::kEjectionTube,applied)
            && applied.device==f::Fixture::firstDevice && applied.position==1.F,
            "ejection receipt resolves its native binding and waits for the applied open pose");
    }
}

static void eater_door_authority_tests() {
    m::Frame frame{};const coo::Generation owner{71,9};frame.enabled=true;frame.spawnGeneration=9;
    const auto grateIndex=m::asset_index(m::kReactorExitGrate);
    auto& grate=frame.native[grateIndex];grate.managed=grate.desired=grate.prepared=grate.active=true;
    grate.generation=10;
    check(m::body_bits(frame,m::kReactorExitGrate.registry,4,m::kReactorExitGrate.slot)==252,
        "grate creation remains separate from its object-local pose authority");
    grate.acknowledged=true;
    check(m::body_bits(frame,m::kReactorExitGrate.registry,4,m::kReactorExitGrate.slot)==573,
        "acknowledged grate receives the exact generic-device pose record");
    Wire closed;
    check(m::write_body(closed,frame,m::kReactorExitGrate.registry,4,m::kReactorExitGrate.slot)
        && closed.bits==573,"closed grate pose serializes through the native type-4 authority");
    auto request=m::grate_request(frame,owner);
    check(request.object.enabled && request.revision==0 && !request.open && !request.poseAcknowledged,
        "grate request starts at native closed revision zero");
    frame.grate.revision=2;frame.grate.open=true;frame.grate.poseAcknowledged=false;
    Wire open;
    check(m::write_body(open,frame,m::kReactorExitGrate.registry,4,m::kReactorExitGrate.slot)
        && open.bits==573,"fresh retry revision republishes the retained grate open pose");

    const auto& pair=m::kGateObjectBindings[0];const auto gateIndex=m::asset_index(pair.gate);
    auto& gate=frame.native[gateIndex];
    gate.managed=gate.desired=gate.prepared=gate.active=true;gate.generation=7;
    gate.position=1.F;gate.acknowledged=false;
    const auto door=m::gate_request(frame,owner,pair.gate);
    check(door.enabled && door.gate==pair.gate
        && door.revision==7 && door.position==1.F && !door.acknowledged,
        "narrow exit request exposes only the expected gate revision and spawned object pair");
}
