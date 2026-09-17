#include "../src/client/hooks/bootflow/eater_platform_contact_native.h"
#include "../src/client/hooks/bootflow/eater_platform_binding_cache.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace native=dawn::client::hooks::bootflow::eater_platform_contact_native;
namespace cache=dawn::client::hooks::bootflow::eater_platform_binding_cache;

namespace {
struct Segment final {std::uintptr_t address{};std::vector<std::byte> bytes{};};
struct Read {
    std::vector<Segment> segments{};
    std::uintptr_t mutateAddress{};unsigned mutateOn{},mutateCopies{};std::size_t mutateOffset{};
    std::uint32_t resolveHandle{0x12345678U};std::uintptr_t resolveAddress{};
    std::vector<std::pair<std::uint32_t,std::uintptr_t>> resolutions{};
    std::size_t copyCalls{},resolveCalls{},metadataReads{};
    std::uintptr_t metadataBegin{},metadataEnd{};
    bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
        ++copyCalls;
        if(address>=metadataBegin && address<metadataEnd) ++metadataReads;
        for(const auto& segment:segments) if(address>=segment.address
            && address-segment.address<=segment.bytes.size()
            && out.size()<=segment.bytes.size()-(address-segment.address)) {
            std::memcpy(out.data(),segment.bytes.data()+address-segment.address,out.size());
            if(address==mutateAddress && ++mutateCopies==mutateOn
                && mutateOffset<out.size()) out[mutateOffset]^=std::byte{1};
            return true;
        }
        return false;
    }
    template<class T> bool value(std::uintptr_t address,T& out) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}}));
    }
    bool resolve(std::uint32_t handle,std::uintptr_t& out) noexcept {
        ++resolveCalls;
        for(const auto& [key,address]:resolutions) if(handle==key) {out=address;return true;}
        if(handle!=resolveHandle || !resolveAddress) return false;
        out=resolveAddress;return true;
    }
    bool resolve(std::uint32_t handle,std::uintptr_t& out,std::uintptr_t* allocation) noexcept {
        if(!resolve(handle,out)) return false;
        if(allocation) *allocation=out+0x800;
        return true;
    }
    template<class T> void put(std::uintptr_t address,const T& value) {
        for(auto& segment:segments) if(address>=segment.address
            && address-segment.address<=segment.bytes.size()
            && sizeof value<=segment.bytes.size()-(address-segment.address)) {
            std::memcpy(segment.bytes.data()+address-segment.address,&value,sizeof value);return;
        }
        std::abort();
    }
};

struct Fixture final {
    static constexpr std::uintptr_t image=0x20000000;
    static constexpr std::uintptr_t player=0x110000;
    static constexpr std::uintptr_t platform=0x120000;
    static constexpr std::uintptr_t collisions=0x130000;
    static constexpr std::uintptr_t link=0x140000;
    static constexpr std::uintptr_t manager=0x150000;
    static constexpr std::uintptr_t atom=0x160000;
    static constexpr std::uintptr_t world=0x170000;
    static constexpr std::uintptr_t collisionInput=0x180000;
    static constexpr std::uintptr_t dispatcher=0x190000;
    static constexpr std::uintptr_t component=0x1A0000;
    static constexpr std::uintptr_t shape=0x1B0000;
    static constexpr std::uint32_t entity=0x61234015U;
    Read read{{{player,std::vector<std::byte>(0x200)},
               {platform,std::vector<std::byte>(0x40)},
               {collisions,std::vector<std::byte>(0x40)},
               {link,std::vector<std::byte>(0x20)},
               {manager,std::vector<std::byte>(0xC0)},
               {atom,std::vector<std::byte>(0x100)},
               {world,std::vector<std::byte>(0xD0)},
               {collisionInput,std::vector<std::byte>(0x20)},
               {component,std::vector<std::byte>(0x100)},
               {shape,std::vector<std::byte>(0x20)}}};
    Fixture() {
        const auto bodyVtable=image+native::kRigidBodyVtableRva;
        const auto managerVtable=image+native::kContactManagerVtableRva;
        read.resolveAddress=component;
        read.put(component,native::kPlatformVolumeConfig);
        read.put(component+4,native::kPlatformVolumeKind);
        read.put<std::int64_t>(component+8,native::kPlatformVolumeOffset);
        read.put(component+0x24,read.resolveHandle);read.put(component+0x2C,entity);
        read.put(component+native::kPlatformVolumeBodyOffset,platform);
        read.put(player,bodyVtable);read.put(platform,bodyVtable);read.put(platform+0x20,shape);
        read.put(shape,image+native::kCylinderShapeVtableRva);
        read.put(player+0x10,world);read.put(platform+0x10,world);
        read.put(player+0x90,collisions);read.put<std::int32_t>(player+0x98,1);
        read.put<std::uint32_t>(player+0x9C,1);
        read.put(collisions,native::CollisionRow{link,platform+0x20});
        read.put(link+8,manager);read.put(manager,managerVtable);
        read.put(manager+0xA0,player);read.put(manager+0xA8,platform);read.put(manager+0x18,world);
        read.put(manager+0x68,atom);read.put(world,image+native::kWorldVtableRva);
        read.put(world+0xB8,collisionInput);read.put(world+0xC8,dispatcher);read.put(collisionInput,dispatcher);
        read.put<float>(collisionInput+0x10,.05F);
        read.put<std::uint16_t>(atom+4,1);
        read.put(atom+0x30,native::ContactPoint{{1.F,2.F,3.F,1.F},{0.F,0.F,1.F,-.01F}});
    }
    bool binding() {
        std::uintptr_t body{};
        return native::platform_volume_body(read,image,component,entity,body) && body==platform;
    }
    native::Occupancy occupancy() {
        return native::stable_occupancy(read,image,player,platform);
    }
};

struct CacheFixture final {
    static constexpr std::uintptr_t image=0x30000000;
    static constexpr std::uintptr_t physics=0x210000,volume=0x220000,device=0x230000;
    static constexpr std::uintptr_t body=0x240000,shape=0x250000;
    static constexpr std::uint32_t entity=0x61234015U;
    static constexpr cache::Definition deviceDefinition{0x81234567U,0x80809999U,0x330};
    Read read{{{physics,std::vector<std::byte>(0x100)},
               {volume,std::vector<std::byte>(0x100)},
               {device,std::vector<std::byte>(0x100)},
               {body,std::vector<std::byte>(0x40)},
               {shape,std::vector<std::byte>(0x20)}}};
    cache::Components components{};
    CacheFixture() {
        component(physics,0x101U,cache::kPhysics);
        component(volume,0x102U,cache::kVolume);
        component(device,0x103U,deviceDefinition);
        read.put(volume+native::kPlatformVolumeBodyOffset,body);
        read.put(body,image+native::kRigidBodyVtableRva);read.put(body+0x20,shape);
        read.put(shape,image+native::kCylinderShapeVtableRva);
        components={ {physics,0x101U},{volume,0x102U},{device,0x103U},body,shape };
        read.metadataBegin=0x280000;read.metadataEnd=0x290000;
    }
    void component(std::uintptr_t address,std::uint32_t self,cache::Definition definition) {
        read.put(address,definition.config);read.put(address+4,definition.kind);
        read.put(address+8,definition.offset);read.put(address+0x24,self);read.put(address+0x2C,entity);
        read.resolutions.push_back({self,address});
    }
    bool current() {return cache::current(read,image,entity,deviceDefinition,components);}
};

struct DiscoveryFixture final {
    static constexpr std::uintptr_t image=CacheFixture::image,base=0x260000,metadata=0x2A0000;
    static constexpr std::uintptr_t rows=metadata+0x180,physics=base+0x1000,volume=base+0x1100;
    static constexpr std::uintptr_t device=base+0x1200,relocatedPhysics=base+0x1400;
    static constexpr std::uintptr_t secondVolume=base+0x1500,body=base+0x1800,shape=base+0x1900;
    static constexpr std::uint32_t bundle=0x201U,type=0x202U,entity=CacheFixture::entity;
    static constexpr cache::Definition deviceDefinition=CacheFixture::deviceDefinition;
    Read read{{{base,std::vector<std::byte>(0x3000)},{metadata,std::vector<std::byte>(0x1000)}}};
    DiscoveryFixture() {
        read.resolutions={{bundle,base},{type,metadata}};
        read.put(base,std::uint32_t{});read.put(base+4,type);read.put(base+0x818,UINT32_MAX);
        read.put<std::uint64_t>(metadata+0x68,3);read.put<std::int64_t>(metadata+0x70,0x100);
        row(0,static_cast<std::int32_t>(physics-base));
        row(1,static_cast<std::int32_t>(volume-base));
        row(2,static_cast<std::int32_t>(device-base));
        component(physics,0x301U,cache::kPhysics);component(volume,0x302U,cache::kVolume);
        component(device,0x303U,deviceDefinition);
        read.put(volume+native::kPlatformVolumeBodyOffset,body);
        read.put(body,image+native::kRigidBodyVtableRva);read.put(body+0x20,shape);
        read.put(shape,image+native::kCylinderShapeVtableRva);
    }
    void row(std::size_t index,std::int32_t offset) {read.put(rows+index*24+0x14,offset);}
    void component(std::uintptr_t address,std::uint32_t self,cache::Definition definition) {
        read.put(address,definition.config);read.put(address+4,definition.kind);
        read.put(address+8,definition.offset);read.put(address+0x24,self);read.put(address+0x2C,entity);
        read.resolutions.push_back({self,address});
    }
    void relocate_physics() {
        component(relocatedPhysics,0x301U,cache::kPhysics);
        row(0,static_cast<std::int32_t>(relocatedPhysics-base));
        for(auto& [handle,address]:read.resolutions)
            if(handle==0x301U) {address=relocatedPhysics;break;}
    }
};

unsigned checks{};
bool expect(bool value,const char* name) {
    ++checks;if(value)return true;std::fprintf(stderr,"FAIL %s\n",name);return false;
}
}

int eater_platform_contact_native_tests() {
    bool ok=true;
    {Fixture f;ok&=expect(f.binding(),"exact platform volume binding");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{
        {9.244956970214844F,210.5023651123047F,-775.75F,30.404083251953125F},
        {0.F,-0.F,-1.F,-1.25F}});
        ok&=expect(f.occupancy()==native::Occupancy::present,"captured centre downward-normal occupancy");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{
        {9.971567153930664F,213.46388244628906F,-775.1500244140625F,60.45756530761719F},
        {.15757310390472412F,.9875072836875916F,0.F,-.03499937057495117F}});
        ok&=expect(f.occupancy()==native::Occupancy::present,"captured edge horizontal-normal occupancy");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{
        {9.305288314819336F,213.4951629638672F,-774.8156127929688F,52.71345138549805F},
        {-.06437182426452637F,.9979259967803955F,0.F,-.051270484924316406F}});
        ok&=expect(f.occupancy()==native::Occupancy::present,"captured return horizontal-normal occupancy");}
    {Fixture f;
        f.read.put(f.manager+0xA0,f.platform);f.read.put(f.manager+0xA8,f.player);
        f.read.put(f.atom+0x30,native::ContactPoint{{},{0.F,0.F,-1.F,0.F}});
        ok&=expect(f.occupancy()==native::Occupancy::present,"manager body order does not change membership");
    }
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{{},{.4F,0.F,.9165151F,.02F}});
        ok&=expect(f.occupancy()==native::Occupancy::present,"native skin distance retained");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{{},{0.F,0.F,1.F,.05F}});
        ok&=expect(f.occupancy()==native::Occupancy::present,"native tolerance boundary");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{{},{0.F,0.F,1.F,.05001F}});
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"separation beyond native tolerance");}
    {Fixture f;f.read.put(f.manager+0x18,std::uintptr_t{0x190000});
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"manager world mismatch");}
    {Fixture f;f.read.put(f.world,f.image+native::kWorldVtableRva+8);
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"wrong world class");}
    {Fixture f;f.read.put(f.collisionInput,f.world+0xC0);
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"collision dispatcher mismatch");}
    {Fixture f;f.read.put(f.collisionInput,f.world+0xC8);
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"dispatcher field address is not its pointer value");}
    {Fixture f;f.read.mutateAddress=f.world+0xC8;f.read.mutateOn=2;f.read.mutateOffset=0;
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"world dispatcher mutation rejected");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{{},{1.F,0.F,1.F,0.F}});
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"normal length rejected");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{{},{0.F,0.F,0.F,0.F}});
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"zero normal rejected");}
    {Fixture f;f.read.put(f.atom+0x30,native::ContactPoint{{},{1e20F,0.F,1e20F,0.F}});
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"large finite normal rejected");}
    {Fixture f;f.read.put<std::uint16_t>(f.atom+4,0);
        ok&=expect(f.occupancy()==native::Occupancy::absent,"empty atom");}
    {Fixture f;
        constexpr std::uintptr_t unrelated=0x1C0000,otherLink=0x1D0000;
        constexpr std::uintptr_t otherManager=0x1E0000,otherAtom=0x1F0000;
        f.read.segments.push_back({unrelated,std::vector<std::byte>(0x40)});
        f.read.segments.push_back({otherLink,std::vector<std::byte>(0x20)});
        f.read.segments.push_back({otherManager,std::vector<std::byte>(0xC0)});
        f.read.segments.push_back({otherAtom,std::vector<std::byte>(0x100)});
        f.read.put(unrelated,f.image+native::kRigidBodyVtableRva);f.read.put(unrelated+0x10,f.world);
        f.read.put<std::int32_t>(f.player+0x98,2);f.read.put<std::uint32_t>(f.player+0x9C,2);
        f.read.put(f.collisions,native::CollisionRow{otherLink,unrelated+0x20});
        f.read.put(f.collisions+sizeof(native::CollisionRow),
                   native::CollisionRow{f.link,f.platform+0x20});
        f.read.put(otherLink+8,otherManager);
        f.read.put(otherManager,f.image+native::kContactManagerVtableRva);
        f.read.put(otherManager+0x18,f.world);f.read.put(otherManager+0x68,otherAtom);
        f.read.put(otherManager+0xA0,f.player);f.read.put(otherManager+0xA8,unrelated);
        f.read.put<std::uint16_t>(otherAtom+4,1);
        f.read.put(otherAtom+0x30,native::ContactPoint{{},{0.F,0.F,1.F,-1.F}});
        f.read.put<std::uint16_t>(f.atom+4,0);
        ok&=expect(f.occupancy()==native::Occupancy::absent,
                   "unrelated child manifold cannot replace empty exact volume");}
    {Fixture f;ok&=expect(f.occupancy()==native::Occupancy::present,"jump sequence initial membership");
        f.read.put<std::uint16_t>(f.atom+4,0);
        ok&=expect(f.occupancy()==native::Occupancy::absent,"jump sequence outside volume");
        f.read.put<std::uint16_t>(f.atom+4,1);
        ok&=expect(f.occupancy()==native::Occupancy::present,"jump sequence returned membership");}
    {Fixture f;f.read.put<std::int32_t>(f.player+0x98,0);
        ok&=expect(f.occupancy()==native::Occupancy::absent,"no platform collision");}
    {Fixture f;f.read.put(f.manager+0xA8,std::uintptr_t{0x170000});
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"wrong manager pair");}
    {Fixture f;f.read.put(f.manager,f.image+native::kContactManagerVtableRva+8);
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"wrong manager class");}
    {Fixture f;auto point=native::ContactPoint{{},{0.F,0.F,1.F,0.F}};
        point.normalDistance[3]=std::numeric_limits<float>::quiet_NaN();f.read.put(f.atom+0x30,point);
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"nonfinite contact rejected");}
    {Fixture f;f.read.put<std::int32_t>(f.player+0x98,65);
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"collision bound");}
    {Fixture f;ok&=expect(native::stable_occupancy(f.read,f.image,f.player,UINTPTR_MAX-0x10)
                          ==native::Occupancy::invalid,"platform pointer overflow");}
    {Fixture f;f.read.mutateAddress=f.atom+0x30;f.read.mutateOn=2;f.read.mutateOffset=0x18;
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"contact mutation rejected");}
    {Fixture f;f.read.mutateAddress=f.collisions;f.read.mutateOn=2;f.read.mutateOffset=8;
        ok&=expect(f.occupancy()==native::Occupancy::invalid,"collision row mutation rejected");}
    {Fixture f;f.read.put(f.component,std::uint32_t{0x80C70467U});
        ok&=expect(!f.binding(),"generic child configuration rejected");}
    {Fixture f;f.read.put(f.component+4,std::uint32_t{0x80FEB6EDU});
        ok&=expect(!f.binding(),"wrong child runtime kind rejected");}
    {Fixture f;f.read.put<std::int64_t>(f.component+8,0x2C0);
        ok&=expect(!f.binding(),"wrong child runtime offset rejected");}
    {Fixture f;f.read.put(f.component+0x2C,std::uint32_t{f.entity+1});
        ok&=expect(!f.binding(),"wrong platform entity rejected");}
    {Fixture f;f.read.resolveAddress=f.component+0x10;
        ok&=expect(!f.binding(),"recycled component self rejected");}
    {Fixture f;f.read.put(f.shape,f.image+native::kCylinderShapeVtableRva+8);
        ok&=expect(!f.binding(),"wrong volume shape rejected");}
    {Fixture f;f.read.put(f.platform,f.image+native::kRigidBodyVtableRva+8);
        ok&=expect(!f.binding(),"wrong volume body rejected");}
    {Fixture f;f.read.mutateAddress=f.component;f.read.mutateOn=2;f.read.mutateOffset=0x2C;
        ok&=expect(!f.binding(),"recycled volume identity rejected");}

    {CacheFixture f;ok&=expect(f.current(),"cached components remain current without metadata traversal");
        ok&=expect(f.read.metadataReads==0,"cached validation does not traverse resource metadata");
        const auto baselineCopies=f.read.copyCalls,baselineResolves=f.read.resolveCalls;
        ok&=expect(baselineCopies<=32 && baselineResolves<=8,"one cached validation has a fixed bounded read budget");
        for(unsigned retained=1;retained<56;++retained) {
            const auto beforeCopies=f.read.copyCalls,beforeResolves=f.read.resolveCalls;
            ok&=expect(f.current(),"repeated cached component validation remains current");
            ok&=expect(f.read.copyCalls-beforeCopies==baselineCopies
                && f.read.resolveCalls-beforeResolves==baselineResolves,
                "cached read budget is independent of retained platform count");
        }
        ok&=expect(f.read.metadataReads==0,"repeated cached validation never falls back to metadata discovery");}
    {CacheFixture f;f.read.put(f.physics+0x24,std::uint32_t{0x104U});
        ok&=expect(!f.current(),"cached physics self change rejected");}
    {CacheFixture f;f.read.put(f.physics,std::uint32_t{cache::kPhysics.config+1});
        ok&=expect(!f.current(),"cached physics configuration change rejected");}
    {CacheFixture f;f.read.put(f.physics+4,std::uint32_t{cache::kPhysics.kind+1});
        ok&=expect(!f.current(),"cached physics kind change rejected");}
    {CacheFixture f;f.read.put(f.physics+8,std::int64_t{cache::kPhysics.offset+8});
        ok&=expect(!f.current(),"cached physics offset change rejected");}
    {CacheFixture f;f.read.put(f.physics+0x2C,std::uint32_t{CacheFixture::entity+1});
        ok&=expect(!f.current(),"cached physics owner change rejected");}
    {CacheFixture f;for(auto& [handle,address]:f.read.resolutions) if(handle==0x101U) address=f.physics+0x40;
        ok&=expect(!f.current(),"cached component relocation rejected even when source selection is unchanged");}
    {CacheFixture f;f.components.body+=0x10;
        ok&=expect(!f.current(),"cached component set rejects changed volume body");}
    {CacheFixture f;f.components.shape+=0x10;
        ok&=expect(!f.current(),"cached component set rejects changed volume shape");}
    {CacheFixture f;f.read.put(f.volume+native::kPlatformVolumeBodyOffset,f.body+0x10);
        ok&=expect(!f.current(),"live volume body replacement rejects cached component set");}
    {CacheFixture f;f.read.put(f.body+0x20,f.shape+0x10);
        ok&=expect(!f.current(),"live volume shape replacement rejects cached component set");}
    {
        constexpr std::uintptr_t base=0x260000,metadata=0x280000,rows=metadata+0x180;
        constexpr std::uint32_t bundle=0x201U,type=0x202U,entity=0x61234015U;
        constexpr std::int32_t firstOffset=0x1000,secondOffset=0x1100;
        Read read{{{base,std::vector<std::byte>(0x2000)},{metadata,std::vector<std::byte>(0x1000)}}};
        read.resolutions={{bundle,base},{type,metadata},{0x301U,base+firstOffset}};
        read.put(base,std::uint32_t{});read.put(base+4,type);read.put(base+0x818,UINT32_MAX);
        read.put<std::uint64_t>(metadata+0x68,2);read.put<std::int64_t>(metadata+0x70,0x100);
        read.put(rows+0x14,firstOffset);read.put(rows+24+0x14,firstOffset);
        read.put(base+firstOffset,cache::kVolume.config);read.put(base+firstOffset+4,cache::kVolume.kind);
        read.put(base+firstOffset+8,cache::kVolume.offset);read.put(base+firstOffset+0x24,std::uint32_t{0x301U});
        read.put(base+firstOffset+0x2C,entity);
        std::uintptr_t found{};
        ok&=expect(dawn::client::hooks::bootflow::coo_native::component<Read,1024>(
            read,bundle,entity,cache::kVolume.kind,found) && found==base+firstOffset,
            "metadata aliases of one component discover one unique cache identity");
        read.put(rows+24+0x14,secondOffset);
        read.resolutions.push_back({0x302U,base+secondOffset});
        read.put(base+secondOffset,cache::kVolume.config);read.put(base+secondOffset+4,cache::kVolume.kind);
        read.put(base+secondOffset+8,cache::kVolume.offset);read.put(base+secondOffset+0x24,std::uint32_t{0x302U});
        read.put(base+secondOffset+0x2C,entity);found=0;
        ok&=expect(!dawn::client::hooks::bootflow::coo_native::component<Read,1024>(
            read,bundle,entity,cache::kVolume.kind,found),
            "distinct matching components make metadata discovery ambiguous");
    }
    {DiscoveryFixture f;cache::Components first{};
        ok&=expect(cache::discover(f.read,f.image,f.bundle,f.entity,f.deviceDefinition,first),
            "composite discovery binds physics, volume, and device components");
        ok&=expect(first.physics.address==f.physics && first.volume.address==f.volume
            && first.device.address==f.device && first.body==f.body && first.shape==f.shape,
            "composite discovery returns the exact component and volume identities");
        ok&=expect(cache::current(f.read,f.image,f.entity,f.deviceDefinition,first),
            "fresh composite discovery output validates as current");
        f.relocate_physics();
        ok&=expect(!cache::current(f.read,f.image,f.entity,f.deviceDefinition,first),
            "old composite cache rejects same-handle physics relocation");
        cache::Components rebound{};
        ok&=expect(cache::discover(f.read,f.image,f.bundle,f.entity,f.deviceDefinition,rebound)
            && rebound.physics.address==f.relocatedPhysics,
            "fresh composite discovery rebinds relocated physics metadata");
        ok&=expect(cache::current(f.read,f.image,f.entity,f.deviceDefinition,rebound),
            "relocated composite cache validates after rebind");}
    {DiscoveryFixture f;cache::Components out{{f.physics,1},{f.volume,2},{f.device,3},f.body,f.shape};
        f.read.put(f.device+4,std::uint32_t{f.deviceDefinition.kind+1});
        ok&=expect(!cache::discover(f.read,f.image,f.bundle,f.entity,f.deviceDefinition,out)
            && out==cache::Components{},"missing composite component fails and clears output");}
    {DiscoveryFixture f;cache::Components out{{f.physics,1},{f.volume,2},{f.device,3},f.body,f.shape};
        f.read.put<std::uint64_t>(f.metadata+0x68,4);
        f.row(3,static_cast<std::int32_t>(f.secondVolume-f.base));
        f.component(f.secondVolume,0x304U,cache::kVolume);
        ok&=expect(!cache::discover(f.read,f.image,f.bundle,f.entity,f.deviceDefinition,out)
            && out==cache::Components{},"ambiguous composite component fails and clears output");}

    {cache::SampleGate gate;cache::SampleKey key{1,100,7,0};
        ok&=expect(gate.claim(key,1000),"sample gate accepts first valid sample");
        ok&=expect(!gate.claim(key,1049),"sample gate rejects 49 ms repeat");
        ok&=expect(gate.claim(key,1050),"sample gate accepts exact 50 ms edge");
        ok&=expect(!gate.claim(key,1050) && !gate.claim(key,1049),"sample gate rejects duplicate and older timestamps");
        auto changed=key;++changed.run;ok&=expect(gate.claim(changed,1050),"new run claims immediately");
        changed.player=101;ok&=expect(gate.claim(changed,1050),"new player claims immediately");
        ++changed.attempt;ok&=expect(gate.claim(changed,1050),"new attempt claims immediately");
        changed.platform=1;ok&=expect(gate.claim(changed,1050),"new platform claims immediately");
        const auto retained=gate;
        auto invalid=changed;invalid.run=0;ok&=expect(!gate.claim(invalid,1100),"zero run rejected");
        invalid=changed;invalid.player=UINT32_MAX;ok&=expect(!gate.claim(invalid,1100),"invalid player rejected");
        invalid=changed;invalid.platform=56;ok&=expect(!gate.claim(invalid,1100),"out-of-range platform rejected");
        ok&=expect(!gate.claim(changed,0),"zero timestamp rejected");
        ok&=expect(gate.key==retained.key && gate.last==retained.last,"invalid sample keys do not disturb the gate");}
    std::printf("eater platform native contact: %u checks\n",checks);
    return ok?0:1;
}

#ifndef EATER_CONTACT_EMBEDDED
int main() { return eater_platform_contact_native_tests(); }
#endif
