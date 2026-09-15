#pragma once
#include "ambient_population_monitor.h"
#include "population_service.h"
#include "../../../state/activity/coo/executor.h"

namespace sunrise::server::runtime::activity::occupancy_wait {
namespace coo=state::activity::coo;
struct Binding final {
    const registry::Definition* registry{};
    std::uint16_t slot{};
    std::int32_t authorityToken{};
};
[[nodiscard]] inline bool valid(const Binding& binding) noexcept {
    if(!binding.registry || !registry::valid(*binding.registry))return false;
    for(const auto& slot:binding.registry->slots)if(slot.index==binding.slot)
        return slot.type==30 && slot.componentClass==0x8080952F
            && slot.senseSchema==0x80809531 && slot.authSchema==0x80809532;
    return false;
}
// A wait is satisfied only by a newer, fully decoded occupied root delta
// received AFTER arming. Cached occupancy is deliberately not a receipt. The
// mirror continues while unarmed, so replaying its old revision cannot qualify.
// No player coordinates, monitor Auth, timer or capture-success state is made up.
class Service final {
public:
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,
        std::span<const Binding> bindings) noexcept {
        if(owner_ || !owner || !boot || bindings.size()>states_.size())return false;
        for(std::size_t i=0;i<bindings.size();++i) {
            if(!valid(bindings[i]))return false;
            for(std::size_t j=0;j<i;++j)
                if(bindings[i].registry->key==bindings[j].registry->key && bindings[i].slot==bindings[j].slot)return false;
        }
        owner_=owner;boot_=boot;bindings_=bindings;return true;
    }
    [[nodiscard]] bool arm(std::size_t index,coo::Token token) noexcept {
        if(!owner_ || index>=bindings_.size() || !token.run || !token.incarnation
            || token.run!=owner_.sessionId || states_[index].armed || states_[index].delivered)return false;
        states_[index].token=token;states_[index].armed=true;return true;
    }
    // A repeated platform uses a fresh CoO token while retaining the native
    // monitor mirror. The old revision is consequently still rejected by
    // Monitor::observe; only a delivered or explicitly cancelled wait may be
    // rearmed.
    [[nodiscard]] bool rearm(std::size_t index,coo::Token token) noexcept {
        if(!owner_ || index>=bindings_.size() || !token.run || !token.incarnation
            || token.run!=owner_.sessionId || states_[index].armed
            || (!states_[index].delivered && !states_[index].cancelled)
            || token==states_[index].token)return false;
        states_[index].token=token;states_[index].armed=true;
        states_[index].delivered=false;states_[index].cancelled=false;return true;
    }
    template<class Submit>
    [[nodiscard]] bool observe(population::Owner owner,std::uint64_t boot,std::uint32_t bubble,
        const ambient_population::sense::SenseObject& object,Submit submit) noexcept {
        if(owner!=owner_ || boot!=boot_ || !owner_)return false;
        for(std::size_t i=0;i<bindings_.size();++i) {
            const auto& binding=bindings_[i];auto& state=states_[i];
            if(binding.registry->bubble!=bubble || binding.registry->key!=object.registryKey || binding.slot!=object.slotIndex)continue;
            ambient_population::MonitorDelta delta{};
            if(!ambient_population::decode_monitor(object,delta))return false;
            const auto result=state.monitor.observe(object,binding.authorityToken);
            if((result!=ambient_population::MonitorIntake::accepted && result!=ambient_population::MonitorIntake::unchanged)
                || !delta.root || !state.monitor.occupied() || !state.armed || state.delivered)return false;
            // Owner calls this under its normal state mutex, outside update().
            // Executor still validates the exact active command token on intake.
            if(!submit(coo::Event{state.token,coo::Milestone::observed}))return false;
            state.delivered=true;state.armed=false;state.cancelled=false;return true;
        }
        return false;
    }
    void cancel(coo::Token token) noexcept {
        for(auto& state:states_)if(state.token==token){state.armed=false;state.cancelled=true;}
    }
    // Presentation may read the authenticated mirror without arming a gameplay wait.
    [[nodiscard]] bool occupied(std::size_t index) const noexcept {
        return index<bindings_.size() && states_[index].monitor.occupied();
    }
    [[nodiscard]] bool armed(std::size_t index) const noexcept {return index<bindings_.size() && states_[index].armed;}
    [[nodiscard]] bool delivered(std::size_t index) const noexcept {
        return index<bindings_.size() && states_[index].delivered;
    }
private:
    struct State {ambient_population::Monitor monitor{};coo::Token token{};bool armed{},delivered{},cancelled{};};
    population::Owner owner_{};std::uint64_t boot_{};
    std::span<const Binding> bindings_{};
    std::array<State,32> states_{};
};
}
