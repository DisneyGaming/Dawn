#pragma once
#include "ambient_population_monitor.h"
#include "population_service.h"
#include "ambient_population_named_points.h"

namespace sunrise::server::runtime::activity::ambient_population {
// Explicit definition policy: request an initial cumulative target on the first
// qualified native occupancy. initialRequests are source requests, NOT a proven
// retail actor count. This service has no replacement clock or renewal operation.
struct InitialPolicy final {
    const population::Capability* source{};
    std::uint16_t monitorSlot{};
    std::uint8_t initialRequests{};
    std::int32_t authorityToken{};
    bool development{};
    const named_points::Dependency* namedDependency{};
};
[[nodiscard]] inline bool valid(const InitialPolicy& policy) noexcept {
    if(!policy.source || !population::valid(*policy.source) || !policy.initialRequests || policy.initialRequests>63) return false;
    if(policy.namedDependency && (!named_points::valid(*policy.namedDependency)
        || policy.namedDependency->registry->bubble!=policy.source->registry->bubble))return false;
    for(const auto& slot:policy.source->registry->slots)
        if(slot.index==policy.monitorSlot) return slot.type==30 && slot.componentClass==0x8080952F
            && slot.senseSchema==0x80809531 && slot.authSchema==0x80809532;
    return false;
}
enum class InitialState : std::uint8_t { unavailable, awaitingMonitor, unoccupied, eligible, published, awaitingDependency };
struct InitialStatus final {
    InitialState state{InitialState::unavailable};
    std::uint32_t nativeRevision{};
    std::int32_t selectedPlayers{};
    bool monitorKnown{};
};
template<std::size_t Capacity=32>
class InitialActivation final {
    static_assert(Capacity>0 && Capacity<=32);
public:
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,std::span<const InitialPolicy> policies) noexcept {
        if(owner_ || !owner || !boot || policies.empty() || policies.size()>Capacity) return false;
        for(std::size_t i=0;i<policies.size();++i) {
            if(!valid(policies[i])) return false;
            for(std::size_t j=0;j<i;++j)
                if(policies[i].source->registry->key==policies[j].source->registry->key
                    && policies[i].source->slot==policies[j].source->slot) return false;
        }
        for(const auto& policy:policies)if(policy.namedDependency
            && !named_points::bind(named_points::ticket(owner,boot,*policy.namedDependency))) {
            named_points::release(owner);return false;
        }
        owner_=owner;boot_=boot;count_=policies.size();
        std::copy(policies.begin(),policies.end(),policies_.begin());return true;
    }
    // Transport/session/patch-epoch qualification is the owner's responsibility,
    // as with population::Service::observe. No untrusted positional data is used.
    [[nodiscard]] std::size_t observe(population::Owner owner,std::uint64_t boot,std::uint32_t bubble,
        const sense::SenseObject& object) noexcept {
        if(owner!=owner_ || boot!=boot_ || !owner_) return 0;
        std::size_t accepted{};
        for(std::size_t i=0;i<count_;++i) {
            const auto& policy=policies_[i];const auto& registry=*policy.source->registry;
            if(registry.bubble!=bubble || registry.key!=object.registryKey || policy.monitorSlot!=object.slotIndex) continue;
            const auto result=monitors_[i].observe(object,policy.authorityToken);
            accepted+=result==MonitorIntake::accepted?1U:0U;
        }
        return accepted;
    }
    void reset_observations() noexcept { for(auto& monitor:monitors_) monitor.reset(); }
    [[nodiscard]] InitialState state(std::size_t index) const noexcept {
        if(!owner_ || index>=count_) return InitialState::unavailable;
        if(published_[index]) return InitialState::published;
        if(!monitors_[index].known()) return InitialState::awaitingMonitor;
        if(!monitors_[index].occupied())return InitialState::unoccupied;
        const auto* dependency=policies_[index].namedDependency;
        if(dependency && !named_points::ready(named_points::ticket(owner_,boot_,*dependency)))return InitialState::awaitingDependency;
        return InitialState::eligible;
    }
    [[nodiscard]] InitialStatus diagnostics(std::size_t index) const noexcept {
        if(index>=count_) return {};
        return {state(index),monitors_[index].revision(),monitors_[index].selected(),monitors_[index].known()};
    }
    [[nodiscard]] std::size_t size() const noexcept {return count_;}
    [[nodiscard]] const InitialPolicy* policy(std::size_t index) const noexcept {
        return index<count_?&policies_[index]:nullptr;
    }
    // Publishes through the SAME native source service used by script/development
    // commands. Failed commands remain retryable; accepted ones latch forever for
    // this owner, including empty monitors, native death and zone revisits.
    [[nodiscard]] population::Result activate(std::size_t index,population::Service& service,std::uint32_t bubble) noexcept {
        if(service.owner()!=owner_ || service.boot()!=boot_ || !owner_) return population::Result::stale;
        if(index>=count_) return population::Result::unsupported;
        if(published_[index]) return population::Result::unchanged;
        if(state(index)!=InitialState::eligible) return population::Result::invalid;
        if(service.last_request()==UINT64_MAX) return population::Result::exhausted;
        const auto& policy=policies_[index];
        const auto result=service.request({owner_,service.revision(),service.last_request()+1,
            policy.source->registry->key,policy.source->slot,policy.initialRequests,boot_},bubble);
        if(result==population::Result::accepted || result==population::Result::unchanged) published_[index]=true;
        return result;
    }
private:
    population::Owner owner_{};
    std::uint64_t boot_{};
    // Copy policy values so a staged service can move into its activity owner
    // without retaining a span into staging storage. The profile/document still
    // owns the immutable capabilities referenced by each copied policy.
    std::array<InitialPolicy,Capacity> policies_{};
    std::size_t count_{};
    std::array<Monitor,Capacity> monitors_{};
    std::array<bool,Capacity> published_{};
};
} // namespace sunrise::server::runtime::activity::ambient_population
