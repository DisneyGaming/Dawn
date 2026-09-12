#include "mission_population_observer.h"
#include "gateway_native_read.h"
#include "coo_enemy_readiness.h"
#include "coo_native_player_mount.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "../../../state/activity/strike_pact/runtime.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include "../../../state/activity/deadly_trial/runtime.h"

namespace sunrise::client::hooks::bootflow {
namespace {
namespace missions=state::activity;
template<class Receipt,class Observe>
void sample(std::uintptr_t image,const missions::coo::ReadinessRequest<Receipt>& request,
            Observe observe) noexcept {
    if(!image || !request.run || request.count>request.actors.size()) return;
    for(std::size_t i=0;i<request.count;++i) {
        gateway_native::Read read{image};const auto& receipt=request.actors[i];
        if(receipt.run==request.run) observe(receipt,coo_native::enemy(read,image,receipt));
    }
}
struct NativeMount final {
    std::uintptr_t image{};
    void controlled(std::uint32_t& out) const noexcept { reinterpret_cast<void(*)(std::uint32_t*)>(image+0x4B2260)(&out); }
    void parent(std::uintptr_t row,std::uint32_t& out) const noexcept { reinterpret_cast<void(*)(std::uintptr_t,std::uint32_t*)>(image+0x5582E0)(row,&out); }
    void position(std::uintptr_t row,std::array<float,4>& out) const noexcept { reinterpret_cast<void(*)(std::uintptr_t,std::array<float,4>*)>(image+0x558330)(row,&out); }
    bool valid(gateway_native::Read& read) const noexcept {
        constexpr std::array<std::uintptr_t,3> entries{0x4B2260,0x5582E0,0x558330};
        constexpr std::array<std::array<std::uint8_t,16>,3> prefixes{{
            {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48},
            {0x48,0x83,0xC1,0xA0,0xC7,0x02,0xFF,0xFF,0xFF,0xFF,0xB8,0x00,0x00,0x00,0x00,0xF6},
            {0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0x83,0xC1,0xA0,0xB8,0x00,0x00,0x00,0x00,0x48}}};
        for(std::size_t i=0;i<entries.size();++i) {
            std::array<std::uint8_t,16> bytes{};
            if(!read.value(image+entries[i],bytes) || bytes!=prefixes[i]) return false;
        }
        return true;
    }
};
void sample_mount(std::uintptr_t image,std::uint64_t now) noexcept {
    const auto owner=missions::deadly_trial::traversal_request(now);
    if(!owner.valid()) return;
    gateway_native::Read read{image};NativeMount native{image};coo_native::MountedPlayer mounted{};
    if(!native.valid(read) || !coo_native::mounted_pike(read,native,mounted)) return;
    missions::deadly_trial::observe_mount({owner,mounted.player,mounted.vehicle,mounted.seat},
        {mounted.position[0],mounted.position[1],mounted.position[2]});
}
}
void poll_mission_population_readiness(std::uintptr_t image,std::uint64_t now) noexcept {
    if(!image) return;
    const auto gateway=missions::gateway::readiness_request(now);
    sample(image,gateway,missions::gateway::observe_readiness);
    if(gateway.run) {
        gateway_native::Read read{image};
        missions::gateway::observe_capacity(gateway.run,coo_native::capacity(read,image+0x1F9D7F0));
    }
    const auto pact=missions::strike_pact::readiness_request(now);
    sample(image,pact,missions::strike_pact::observe_readiness);
    if(pact.run) {
        gateway_native::Read read{image};
        missions::strike_pact::observe_capacity(pact.run,coo_native::capacity(read,image+0x1F9D7F0));
    }
    sample(image,missions::strike_bond::readiness_request(now),missions::strike_bond::observe_readiness);
    sample(image,missions::deadly_trial::readiness_request(now),missions::deadly_trial::observe_readiness);
    sample(image,missions::deep_storage::readiness_request(now),missions::deep_storage::observe_readiness);
    sample(image,missions::hijacked::readiness_request(now),missions::hijacked::observe_readiness);
    sample_mount(image,now);
}
} // namespace sunrise::client::hooks::bootflow
