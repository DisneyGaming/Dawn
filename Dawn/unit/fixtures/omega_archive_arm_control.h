#pragma once
#include "state/activity/omega_first_lair_runtime.h"
namespace omega_archive_arm_fixture {
namespace fight=dawn::state::activity::omega_first_lair;
namespace arm=dawn::state::activity::omega_archive_arm;
template<class T,std::size_t N> void put(std::array<std::byte,N>& bytes,std::size_t offset,T value) {
    std::memcpy(bytes.data()+offset,&value,sizeof value);
}
struct ControlFixture {
    std::array<std::byte,0xAB4> member{};
    std::array<std::byte,0x108> auth{};
    explicit ControlFixture(const fight::Boss& boss,const arm::Control& control) {
        put(member,0,std::uint32_t{0x80F4756D});put(member,4,std::uint32_t{0x80807D9D});put(member,8,std::int64_t{0xB58});
        put(member,0x21C,boss.actor);put(member,0x180,boss.generation);put(member,0x190,boss.revision);
        put(auth,0,boss.generation);put(auth,6,std::uint8_t{1});put(auth,0x100,boss.revision);
        put(member,0xAB0,control.revision);put(auth,0x4C,control.revision);
        for(unsigned i=0;i<6;++i) { put(auth,0x58+i*4,std::uint32_t{0x811C9DC5});put(member,0xA94+i*4,std::uint32_t{0x811C9DC5}); }
        put(auth,0x70,std::uint32_t{0x811C9DC5});put(auth,0x74,std::int8_t{-1});put(auth,0x76,std::int16_t{-1});put(auth,0x78,std::int32_t{-1});
        if(control.revision) {
            put(auth,0x7C,std::uint32_t{2});put(auth,0x80,std::uint32_t{0xA2AE120F});put(auth,0x88,std::uint32_t{0x8496ABD2});
            put(auth,0x84,control.high && !control.right?1.0F:0.0F);put(auth,0x8C,control.high && control.right?1.0F:0.0F);
        }
    }
    arm::NativeControl receipt() const { return arm::inspect(member,auth); }
};
arm::Owner arm_owner(const fight::Boss& boss) {
    return {boss.run,boss.actor,boss.character,boss.entity,boss.generation,boss.revision,77,boss.island,boss.actionEpoch};
}
}
