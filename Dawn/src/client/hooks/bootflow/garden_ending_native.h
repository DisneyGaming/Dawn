#pragma once
#include "omega_teardown_native.h"
#include "../../../state/activity/strike_bond/runtime.h"
namespace dawn::client::hooks::bootflow::garden_ending_native {
namespace garden=state::activity::strike_bond;
namespace native=omega_teardown_native;
inline constexpr std::array<std::uint32_t,4> arenaKeys{0x2CB86C0FU,0xA1F8CD1CU,0xB9395B1BU,0xC80A735BU};
struct Cleanup {native::Retirement owner{};state::activity::coo::Generation token{};std::uint32_t ordinal{};bool qualified{};};
inline Cleanup before(const native::Source& read,std::uintptr_t context,std::uintptr_t delta) noexcept {
    Cleanup out;const auto r=garden::request();
    if(!r.frame.campaign || !r.frame.endingFlow.retire || r.frame.endingFlow.retired
        || !native::retirement_owner(read,context,out.owner,0x80F47445U)) return {};
    out.token=r.owner;std::uint32_t count{},incoming{};
    if(!native::read(read,context+8+0x528,count) || !native::read(read,delta+0x528,incoming)
        || count>64 || incoming<count || incoming>64) return {};
    for(unsigned i=0;i<count;++i) {
        const auto off=0x52c+i*0x1f8;std::uint32_t bubble{},other{},n{};
        if(!native::read(read,context+8+off,bubble) || !native::read(read,delta+off,other)) return {};
        if(bubble!=17)continue;
        if(other!=17 || !native::read(read,context+8+off+4,n) || n!=4
            || !native::read(read,delta+off+4,n) || n!=4) return {};
        for(unsigned k=0;k<4;++k) {
            std::uint32_t a{},b{};std::uint8_t oldState{},newState{};
            if(!native::read(read,context+8+off+8+4*k,a) || a!=arenaKeys[k]
                || !native::read(read,delta+off+8+4*k,b) || b!=a
                || !native::read(read,context+8+off+0x198+k,oldState)
                || !native::read(read,delta+off+0x198+k,newState) || oldState!=newState) return {};
        }
        for(unsigned k=0;k<3;++k) if(!native::read(read,delta+off+0x188+4*k,n) || n) return {};
        out.ordinal=i;out.qualified=true;return out;
    }
    return {};
}
inline void after(const native::Source& read,const Cleanup& lease) noexcept {
    if(!lease.qualified || !native::same_retirement_owner(read,lease.owner,0x80F47445U))return;
    const auto r=garden::request();if(r.owner!=lease.token || !r.frame.endingFlow.retire)return;
    std::uint32_t value{};const auto off=0x52c+lease.ordinal*0x1f8;
    for(unsigned k=0;k<3;++k) if(!native::read(read,lease.owner.context+8+off+0x188+4*k,value)||value)return;
    std::int32_t count{};if(!native::read(read,lease.owner.owner+0x28,count) || count<0 || count>128)return;
    for(int i=0;i<count;++i) {
        if(!native::read(read,lease.owner.owner+0x2c+i*16,value))return;
        for(auto key:arenaKeys)if(key==value)return;
    }
    garden::observe_ending_retirement(lease.token);
}
// Cinematic sources use their native interface at +48, not the entity +24
// layout. The selector directory's component reference is (+20,+28), not +18.
inline void movie(const native::Source& read,std::uintptr_t component,std::uintptr_t directory) noexcept {
    const auto r=garden::request();if(!r.frame.campaign || !r.frame.endingFlow.arrived || !directory)return;
    std::uint32_t tag{},kind{},self{},recordHandle{},recordKey{},recordType{},baseHandle{},resourceSelf{},owner{},resourceOwner{},revision{};
    std::uint64_t offset{},relative{};std::uint8_t active{};
    if(!native::read(read,component,tag)||tag!=garden::kEndingMovieSource
        ||!native::read(read,component+4,kind)||kind!=0x80804F07U
        ||!native::read(read,component+8,offset)||offset!=0x2e8
        ||!native::read(read,component+0x48,self)||native::resolve(read,self).address!=component
        ||!native::read(read,component+0x170,recordHandle))return;
    const auto record=native::resolve(read,recordHandle).address;
    if(!record || !native::read(read,record,recordKey)||recordKey!=garden::kEndingMovieRegistry
        ||!native::read(read,record+4,recordType)||recordType!=6
        ||!native::read(read,directory+8,tag)||tag!=garden::kEndingMovieResource
        ||!native::read(read,directory+0x20,baseHandle)||!native::read(read,directory+0x28,relative)
        ||relative>0x1000000)return;
    const auto base=native::resolve(read,baseHandle).address;std::uintptr_t resource{};
    if(!base || !native::add(base,relative,resource)
        ||!native::read(read,resource,tag)||tag!=garden::kEndingMovieResource
        ||!native::read(read,resource+4,kind)||kind!=0x80806647U
        ||!native::read(read,resource+8,offset)||offset!=0x5758
        ||!native::read(read,resource+0x24,resourceSelf)||native::resolve(read,resourceSelf).address!=resource
        ||!native::read(read,directory+4,owner)||!native::read(read,resource+0x2c,resourceOwner)||owner!=resourceOwner
        ||!native::read(read,component+0x190,revision)||!native::read(read,component+0x260,active))return;
    garden::observe_ending_movie(r.owner,self,resourceSelf,revision,active!=0);
}
}
