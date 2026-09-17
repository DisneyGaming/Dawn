#pragma once
#include "executor.h"
#include "lifecycle_service.h"
#include <cmath>
namespace sunrise::state::activity::coo {
enum class ObjectPhase : std::uint8_t { dormant,prepare,create,bind,apply,ready,retired };
struct ObjectBinding final { Asset source{},device{};float initialPosition{};bool freshGeneration{true}; };
struct ObjectReceipt final {
    Generation owner{};Asset source{};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},controller{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && entity!=UINT32_MAX && serial!=UINT32_MAX; }
    friend bool operator==(const ObjectReceipt&,const ObjectReceipt&)=default;
};
struct ObjectState final {
    ObjectPhase phase{};std::uint32_t generation{};float position{};std::int16_t revision{};
    bool create{},apply{};
};
template<std::size_t Objects>
class ObjectService final {
public:
    bool begin(Generation owner,std::span<const ObjectBinding> bindings) noexcept {
        if(!owner.valid() || bindings.size()!=Objects || owner.value>=32766) { return false; }
        for(std::size_t i=0;i<Objects;++i) {
            if(!std::isfinite(bindings[i].initialPosition) || !bindings[i].source.registry
                || bindings[i].source.type>=127 || bindings[i].source.slot>=32768
                || (bindings[i].device.registry && (bindings[i].device.type>=127 || bindings[i].device.slot>=32768))) { return false; }
            for(std::size_t j=0;j<i;++j) { if(bindings[i].source==bindings[j].source) { return false; } }
        }
        *this={};owner_=owner;
        for(std::size_t i=0;i<Objects;++i) {
            bindings_[i]=bindings[i];states_[i]={ObjectPhase::prepare,owner.value,bindings[i].initialPosition,1,false,false};
        }return true;
    }
    bool prepared(Generation owner,std::size_t i) noexcept {
        if(owner!=owner_ || !owner.valid() || i>=Objects || states_[i].phase!=ObjectPhase::prepare) { return false; }
        auto& s=states_[i];s.generation+=bindings_[i].freshGeneration?1U:0U;s.phase=ObjectPhase::create;s.create=true;return true;
    }
    bool observe(std::size_t i,const ObjectReceipt& receipt,bool applied,float position,std::int16_t revision) noexcept {
        if(i>=Objects || !receipt.valid() || receipt.owner.run!=owner_.run || receipt.source!=bindings_[i].source) { return false; }
        auto& s=states_[i];auto& prior=owners_[i];
        if(receipt.owner.value!=s.generation || s.phase<ObjectPhase::create || s.phase==ObjectPhase::retired) { return false; }
        if(prior.valid() && (prior.entity!=receipt.entity || prior.serial!=receipt.serial || (prior.controller!=UINT32_MAX && prior.controller!=receipt.controller))) { return false; }
        bool changed=!prior.valid();prior=receipt;
        if(s.phase==ObjectPhase::create) { s.phase=ObjectPhase::bind;changed=true; }
        if(receipt.controller==UINT32_MAX && bindings_[i].device.registry!=0) { return changed; }
        if(s.phase==ObjectPhase::bind) { s.phase=ObjectPhase::apply;s.apply=true;changed=true; }
        if(s.phase==ObjectPhase::apply && applied && revision==s.revision && position==s.position) { s.phase=ObjectPhase::ready;changed=true; }
        return changed;
    }
    bool set(std::size_t i,float position) noexcept {
        if(i>=Objects || !std::isfinite(position) || states_[i].phase==ObjectPhase::retired) { return false; }
        auto& s=states_[i];if(s.position==position) { return true; }
        if(s.revision==INT16_MAX) { return false; }s.position=position;++s.revision;
        if(s.phase==ObjectPhase::ready) { s.phase=ObjectPhase::apply; }return true;
    }
    bool rearm(Generation owner,std::size_t i,std::uint32_t generation) noexcept {
        if(owner!=owner_ || !owner.valid() || i>=Objects || generation<=states_[i].generation
            || generation>=32766) return false;
        states_[i]={ObjectPhase::prepare,generation,bindings_[i].initialPosition,1,false,false};
        owners_[i]={};return true;
    }
    void retire(std::size_t i) noexcept { if(i<Objects) { states_[i].phase=ObjectPhase::retired;states_[i].create=false; } }
    ObjectState state(std::size_t i) const noexcept { return i<Objects?states_[i]:ObjectState{}; }
    ObjectReceipt owner(std::size_t i) const noexcept { return i<Objects?owners_[i]:ObjectReceipt{}; }
private:
    Generation owner_{};std::array<ObjectBinding,Objects> bindings_{};
    std::array<ObjectState,Objects> states_{};std::array<ObjectReceipt,Objects> owners_{};
};
enum class DestructiblePhase : std::uint8_t { immune,vulnerable,destroyed };
struct LinkedDevice final { std::uint8_t object{};float immune{},vulnerable{},destroyed{};bool retireOnDestruction{}; };
template<class Receipt>
class DestructibleService final {
public:
    bool bind(const Receipt& owner) noexcept { if(!owner.valid() || owner_.valid()) { return false; }owner_=owner;return true; }
    void expose() noexcept { if(phase_!=DestructiblePhase::destroyed) { phase_=DestructiblePhase::vulnerable; } }
    bool destroyed(const Receipt& owner) noexcept {
        if(!owner.valid() || owner!=owner_ || phase_!=DestructiblePhase::vulnerable) { return false; }
        phase_=DestructiblePhase::destroyed;return true;
    }
    bool immune() const noexcept { return phase_==DestructiblePhase::immune; }
    bool dead() const noexcept { return phase_==DestructiblePhase::destroyed; }
    Receipt owner() const noexcept { return owner_; }
    template<std::size_t N> void project(ObjectService<N>& objects,std::span<const LinkedDevice> links) const noexcept {
        for(const auto& link:links) {
            static_cast<void>(objects.set(link.object,dead()?link.destroyed:immune()?link.immune:link.vulnerable));
            if(dead() && link.retireOnDestruction) { objects.retire(link.object); }
        }
    }
private:
    Receipt owner_{};DestructiblePhase phase_{};
};
}
