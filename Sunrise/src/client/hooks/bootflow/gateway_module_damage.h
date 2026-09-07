#pragma once
#include "gateway_module_identity.h"
#include "../../../state/activity/gateway/ending_receipts.h"
#include <array>
namespace sunrise::client::hooks::bootflow::gateway_module_damage {
namespace gateway=state::activity::gateway;
struct Sample { std::uintptr_t address{};std::uint32_t health{},entity{};bool dead{}; };
// Health context ABI shared by B804E0, CDCB60 and B7E3C0. Read only the exact
// Gateway module; enemy and Omega damage keep the native result unchanged.
template<class Read> bool sample(Read& read,std::uintptr_t context,Sample& out) noexcept {
    std::array<std::byte,16> header{};
    if(context>UINTPTR_MAX-8 || !read.value(context+8,out.address) || !read.copy(out.address,header)
        || gateway_module_identity::at<std::uint32_t>(header,0)!=0x80F48026U
        || gateway_module_identity::at<std::uint32_t>(header,4)!=0x80804B8AU
        || gateway_module_identity::at<std::uint64_t>(header,8)!=0xB08U) { return false; }
    std::array<std::byte,0x340> health{};std::uintptr_t resolved{};
    if(!read.copy(out.address,health)) { return false; }
    out.health=gateway_module_identity::at<std::uint32_t>(health,0x24);
    out.entity=gateway_module_identity::at<std::uint32_t>(health,0x2C);
    if(!gateway_module_identity::health(health,out.health,out.entity)
        || !read.resolve(out.health,resolved) || resolved!=out.address) { return false; }
    out.dead=gateway_module_identity::dead(health,out.health,out.entity);return true;
}
template<class Read> bool current(Read& read,const gateway::EndingRequest& request,const Sample& sample) noexcept {
    const auto& owner=request.owner;
    if(!request.enabled || request.moduleDestroyed || !owner.valid() || owner.run!=request.run
        || owner.generation!=request.generation || owner.health!=sample.health || owner.entity!=sample.entity
        || owner.source>UINTPTR_MAX-0x448) { return false; }
    std::array<std::byte,16> header{};std::uint32_t generation{},committed{},serial{},entity{};std::uint8_t active{};
    return read.copy(owner.source,header)
        && gateway_module_identity::at<std::uint32_t>(header,0)==0x80F46F23U
        && gateway_module_identity::at<std::uint32_t>(header,4)==0x80809928U
        && gateway_module_identity::at<std::uint64_t>(header,8)==0x4C8U
        && read.value(owner.source+0x180,generation) && generation==request.generation
        && read.value(owner.source+0x2F0,committed) && committed==generation
        && read.value(owner.source+0x188,active) && active==1
        && read.value(owner.source+0x440,serial) && serial==owner.serial
        && read.value(owner.source+0x444,entity) && entity==owner.entity;
}
inline bool blocked(const gateway::EndingRequest& request) noexcept {
    return request.enabled && request.run!=0 && !request.moduleVulnerable && !request.moduleDestroyed;
}
// CDCB60 controls the lethal-health branch (B815FA), not every damage side effect.
// B804E0 is suppressed while protected. Once exposed, the native pipeline owns
// damage amounts, health loss, destruction and effects, with death permitted.
inline bool allowed(const gateway::EndingRequest& request,bool current,bool nativeResult) noexcept {
    if(!request.enabled || request.run==0 || request.moduleDestroyed) { return nativeResult; }
    if(!request.moduleVulnerable) { return false; } // Also protect the first frame before binding.
    return current?true:nativeResult;
}
}
