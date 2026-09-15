#include <Windows.h>
#include "runtime.h"
#include "ghost_native.h"
#include "lighting_native.h"
#include "entrance_native.h"
#include "cache_native.h"
#include "../../coo/native_device_authority.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_native_player_mount.h"
#include "../../../../client/hooks/bootflow/internal.h"
#include "../../../../core/logging/log.h"
#include <mutex>
#include <cstdio>

namespace sunrise::state::activity::newlight::launchpad {
namespace {
namespace gn=client::hooks::bootflow::gateway_native;
namespace cn=client::hooks::bootflow::coo_native;
std::mutex mutex;
coo::Generation observedOwner{};
std::array<std::uintptr_t,kObjects.size()> sources{};
template<class T> T at(const std::byte* bytes) noexcept {T v{};std::memcpy(&v,bytes,sizeof v);return v;}
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
}
void observe_native_object(void* raw) noexcept {
    const auto req=request();if(!req.frame.enabled) {return;}
    const auto source=reinterpret_cast<std::uintptr_t>(raw);gn::Read read{image()};std::array<std::byte,16> header{};
    if(!read.copy(source,header) || at<std::uint32_t>(header.data()+4)!=0x80809928U) {return;}
    const AssetBinding* binding{};
    for(const auto& a:kAssets) if(a.asset.type==4 && at<std::uint32_t>(header.data())==a.asset.definition
        && at<std::uint64_t>(header.data()+8)==a.offset) {binding=&a;break;}
    if(!binding) {return;}const auto& desired=req.frame.native[asset_index(binding->asset)];
    {
        const std::lock_guard lock(mutex);
        if(observedOwner!=req.owner) {observedOwner=req.owner;sources={};}
        sources[object_index(binding->asset)]=source;
    }
    // The native source may be constructed before its mission command. Keep its
    // validated address so a later request can observe preparation without a second callback.
    if(!desired.managed) {return;}
    if(!desired.prepared) {
        std::array<std::byte,0x44> bytes{};
        if(read.copy(source+0x180,bytes) && at<std::uint32_t>(bytes.data())==desired.generation && coo::native_device::inactive_state(bytes)
            && request().owner==req.owner) {observe_prepared(req.owner,binding->asset);}return;
    }
    std::uint32_t generation{},committed{};std::uint8_t active{};gn::Weak entity{},again{};
    if(!desired.active || !read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || !read.weak(entity)
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again) || request().owner!=req.owner) {return;}
    observe_object({{req.owner.run,generation},binding->asset,entity.handle,entity.serial});
}
void poll_native_objects() noexcept {
    const auto req=request();if(!req.frame.enabled) {return;}
    // Only the loose first Vandal needs a direct native start. Named members
    // already own their entry programs; observe those rather than queuing twice.
    static coo::Generation entranceOwner{};static EnemyReceipt queuedEntrance{};
    static std::uint32_t entranceEntity{UINT32_MAX};static std::uint64_t entranceStart{},entranceNext{};
    if(entranceOwner!=req.owner) {entranceOwner=req.owner;queuedEntrance={};entranceEntity=UINT32_MAX;}
    for(std::size_t index=0;index<=std::size(kAmbushCues);++index) {
        if(!entrance::wanted(req,index)) {continue;}
        entrance::Binding before{},checked{},after{};gn::Read first{image()};
        constexpr std::array<std::uint8_t,16> prefix{0x48,0x83,0xEC,0x38,0x45,0x0F,0xB6,0xD8,0x4C,0x8B,0xC2,0x48,0x85,0xD2,0x0F,0x84};
        std::array<std::uint8_t,16> actual{};
        if(first.value(image()+0xC66590,actual) && actual==prefix && entrance::sample(first,image(),req,before,index)) {
            const auto current=request();gn::Read second{image()};
            const auto actor=entrance::actor(req,index);
            if(current.owner==req.owner && entrance::actor(current,index)==actor
                && entrance::sample(second,image(),current,checked,index) && checked==before) {
                using Start=bool(__fastcall*)(void*,const void*,std::uint8_t) noexcept;
                if(!index && !before.started && queuedEntrance!=actor) {
                    if(reinterpret_cast<Start>(image()+0xC66590)(reinterpret_cast<void*>(before.channel),reinterpret_cast<void*>(before.entry),1)) {
                        queuedEntrance=actor;entranceEntity=before.entity;entranceStart=GetTickCount64();entranceNext=entranceStart;
                    }
                }
                gn::Read final{image()};
                if(request().owner==req.owner && entrance::sample(final,image(),current,after,index) && after.playing
                    && after.character==before.character && after.channel==before.channel && after.entity==before.entity) {observe_entrance(req.owner,actor);}
            }
        }
    }
    // One bounded trace of the first reveal distinguishes a misplaced spawn
    // from an entrance played off-screen. Never change its transform to log it.
    const auto entranceNow=GetTickCount64();
    if(entranceEntity!=UINT32_MAX && entranceNow>=entranceNext && entranceNow-entranceStart<=4000
        && req.frame.firstVandal==queuedEntrance && req.frame.section==2) {
        entranceNext=entranceNow+500;gn::Read read{image()};cn::NativeMount native{image()};std::array<float,4> position{};
        if(native.valid(read) && cn::enemy(read,image(),queuedEntrance).created
            && entrance::position(read,native,queuedEntrance.actor,entranceEntity,position) && request().owner==req.owner) {
            std::array<char,192> line{};std::snprintf(line.data(),line.size(),
                "ev=launchpad stage=ambush_position actor=%08X elapsed=%llu xyz=%.3f,%.3f,%.3f",
                queuedEntrance.actor,static_cast<unsigned long long>(entranceNow-entranceStart),position[0],position[1],position[2]);
            core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        }
    }
    if(req.frame.ghost.phase!=ghost::Phase::dormant && req.frame.ghost.phase!=ghost::Phase::retired) {
        static coo::Generation owner{};static std::uint64_t next{};static unsigned previous{UINT_MAX};
        const auto now=GetTickCount64();
        if(owner!=req.owner || now>=next) {
            owner=req.owner;next=now+100;unsigned reason{};gn::Read read{image()};
            const auto sample=ghost::sample(read,image(),req.frame.ghostActor,reason);
            if(request().owner==req.owner) {observe_ghost(req.owner,req.frame.ghostActor,sample);}
            if(reason!=previous) {
                previous=reason;std::array<char,128> line{};
                std::snprintf(line.data(),line.size(),"ev=launchpad stage=ghost_reader result=%u actor=%08X",reason,req.frame.ghostActor.actor);
                core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
            }
        }
    }
    if(!req.frame.finished && req.frame.cinematic.phase==cinematics::Phase::gameplay) {
        gn::Read read{image()};cn::NativeMount native{image()};cn::MountedPlayer sample{};
        const auto state=!native.valid(read)?1:!cn::controlled_player(read,native,sample)?2:3;
        if(state==3 && request().owner==req.owner) {
            observe_position(sample.position[0],sample.position[1],sample.position[2]);
        }
        static coo::Generation owner{};static int previous{};static std::uint64_t next{};
        const auto now=GetTickCount64();
        if(owner!=req.owner || (state!=previous && now>=next)) {
            owner=req.owner;previous=state;next=now+2000;
            core::log::write(core::log::Channel::client,core::log::Level::info,state==1
                ?"ev=launchpad stage=position_reader result=signature_mismatch":state==2
                ?"ev=launchpad stage=position_reader result=controlled_entity_unavailable"
                :"ev=launchpad stage=position_reader result=observed");
        }
    }
    std::array<std::uintptr_t,kObjects.size()> pending{};
    {const std::lock_guard lock(mutex);if(observedOwner!=req.owner) {return;}pending=sources;}
    for(const auto source:pending) {if(source) {observe_native_object(reinterpret_cast<void*>(source));}}
    for(std::size_t pickup=1;pickup<std::size(kPickups);++pickup) {
        if(!req.frame.pickups[pickup].armed || req.frame.pickups[pickup].used) {continue;}
        const auto source=pending[object_index(kPickups[pickup])];cache::Binding before{},after{};gn::Read first{image()};
        if(!cache::sample(first,source,req,pickup,before)) {continue;}
        const auto current=request();gn::Read second{image()};
        if(current.owner==req.owner && cache::sample(second,source,current,pickup,after) && before==after) {
            observe_cache_looted(req.owner,before.object);
        }
    }
    if(req.frame.lightRequested && !req.frame.light) {
        const auto source=pending[object_index(lighting::kSource)];lighting::Binding before{},checked{};
        gn::Read first{image()};
        const auto revision=req.frame.native[asset_index(lighting::kSource)].generation;
        const unsigned status=!source?1:!lighting::sample(first,image(),source,req,before)?3
            :before.target!=1.F || before.revision!=static_cast<std::int32_t>(revision)?4:0;
        static coo::Generation lightOwner{};static unsigned previous{UINT_MAX};
        if(lightOwner!=req.owner || previous!=status) {
            lightOwner=req.owner;previous=status;const auto& wanted=req.frame.native[asset_index(lighting::kSource)];
            std::array<char,192> line{};std::snprintf(line.data(),line.size(),
                "ev=launchpad stage=light_reader result=%u generation=%u prepared=%u active=%u bound=%u native_revision=%d",
                status,wanted.generation,wanted.prepared?1U:0U,wanted.active?1U:0U,wanted.acknowledged?1U:0U,status==0 || status==4?before.revision:-2);
            core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        }
        if(status) {return;}
        const auto current=request();gn::Read second{image()};
        if(current.owner!=req.owner || !lighting::sample(second,image(),source,current,checked) || checked!=before) {return;}
        observe_lights(req.owner,checked.object);
    }
}
}
