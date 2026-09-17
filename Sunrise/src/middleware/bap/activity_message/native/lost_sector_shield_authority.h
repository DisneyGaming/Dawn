#pragma once

#include "retained_authority_scope.h"
#include "../../../../state/activity/coo/native_device_authority.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::middleware::bap::activity_message::native::lost_sector_shield {
inline constexpr std::size_t kCapacity=8;
struct Request final {
    std::uint32_t registry{};std::uint16_t effectSlot{},filterSlot{},protectedSource{};
    std::uint8_t bubble{};bool enabled{};
    std::array<std::uint16_t,3> additionalProtected{};std::uint8_t protectedCount{1};
};
struct Batch final {std::array<Request,kCapacity> entries{};std::size_t count{};};

[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t registry,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if((type!=26 && type!=34) || batch.count>batch.entries.size())return nullptr;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(request.registry==registry && ((type==26 && request.effectSlot==slot)
            || (type==34 && request.filterSlot==slot)))return &request;
    }
    return nullptr;
}
template<class Roster> [[nodiscard]] bool valid(const Batch& batch,const Roster& roster,
    std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size())return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!request.registry || request.registry==UINT32_MAX || request.registry==0x811C9DC5U
            || request.effectSlot>32767 || request.filterSlot>32767 || request.protectedSource>32767
            || request.bubble>63 || !request.protectedCount || request.protectedCount>4
            || !authority_scope::valid(roster,request.registry,request.bubble,region))return false;
        for(std::size_t n=1;n<request.protectedCount;++n) {
            if(request.additionalProtected[n-1]>32767)return false;
            if(request.additionalProtected[n-1]==request.protectedSource)return false;
            for(std::size_t prior=1;prior<n;++prior)
                if(request.additionalProtected[n-1]==request.additionalProtected[prior-1])return false;
        }
        for(std::size_t j=0;j<i;++j)if(batch.entries[j].registry==request.registry
            && (batch.entries[j].effectSlot==request.effectSlot
                || batch.entries[j].filterSlot==request.filterSlot))return false;
        unsigned groups{},effects{},filters{},sources{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& group=roster.groups[g];if(group.key!=request.registry)continue;
            ++groups;
            if(group.slotTypes.size()!=group.slotIndices.size() || group.slotFlags.size()!=group.slotIndices.size())return false;
            for(std::size_t s=0;s<group.slotIndices.size();++s) {
                if(group.slotIndices[s]==request.effectSlot && group.slotTypes[s]==26)++effects;
                if(group.slotIndices[s]==request.filterSlot && group.slotTypes[s]==34)++filters;
                if(group.slotTypes[s]!=1)continue;
                if(group.slotIndices[s]==request.protectedSource)++sources;
                for(std::size_t n=1;n<request.protectedCount;++n)
                    if(group.slotIndices[s]==request.additionalProtected[n-1])++sources;
            }
        }
        if(groups!=1 || effects!=1 || filters!=1 || sources!=request.protectedCount)return false;
    }
    return true;
}
[[nodiscard]] inline std::size_t body_bits(const Batch& batch,std::uint32_t registry,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(!find(batch,registry,type,slot))return 0;
    return type==26?186U:4U+90U*find(batch,registry,type,slot)->protectedCount;
}
template<class Writer> [[nodiscard]] bool write_body(Writer& writer,const Batch& batch,
    std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    const auto* request=find(batch,registry,type,slot);if(!request)return false;
    if(type==26)return state::activity::coo::native_device::linked_effect(
        writer,registry,request->filterSlot,request->enabled);
    std::array<std::uint16_t,4> sources{};sources[0]=request->protectedSource;
    for(std::size_t i=1;i<request->protectedCount;++i)sources[i]=request->additionalProtected[i-1];
    return state::activity::coo::native_device::collection_sources(writer,registry,
        std::span(sources).first(request->protectedCount));
}
}
