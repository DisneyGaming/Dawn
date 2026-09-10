#pragma once
#include "retained_authority_scope.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sunrise::middleware::bap::activity_message::native::world_device {
// Original80804F48. Native10699C0 stages24bytes;106AD60 applies only
// channels with a positive revision newer than the device's committed value.
inline constexpr std::uint32_t kSchema=0x80804F48;
inline constexpr std::uint32_t kRuntimeClass=0x80804F45,kSenseSchema=0x80804F47;
inline constexpr std::size_t kPayloadBits=147;
inline constexpr std::size_t kCapacity=16;
struct Channel final {float value{};std::int16_t revision{};bool snap{};
    friend constexpr bool operator==(const Channel&,const Channel&)=default;};
struct State final {Channel position{};Channel power{1.0F,0,false};Channel lock{};
    friend constexpr bool operator==(const State&,const State&)=default;};

[[nodiscard]] inline bool valid(Channel value) noexcept {
    return std::isfinite(value.value) && value.value>=0.0F && value.value<=1.0F;
}
[[nodiscard]] inline bool valid(const State& value) noexcept {
    return valid(value.position) && valid(value.power) && valid(value.lock);
}
template<class Writer> [[nodiscard]] bool write_channel(Writer& writer,Channel value) noexcept {
    return writer.write(std::bit_cast<std::uint32_t>(value.value),32)
        && writer.write(static_cast<std::uint16_t>(value.revision)^0x8000U,16)
        && writer.write(value.snap?1U:0U,1);
}
template<class Writer> [[nodiscard]] bool write_payload(Writer& writer,const State& value) noexcept {
    if(!valid(value))return false;
    return write_channel(writer,value.position) && write_channel(writer,value.power)
        && write_channel(writer,value.lock);
}
struct Request final {std::uint32_t registry{};std::uint16_t slot{};std::uint8_t bubble{};State state{};};
struct Batch final {std::array<Request,kCapacity> entries{};std::size_t count{};};
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t registry,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=23 || batch.count>batch.entries.size())return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].registry==registry && batch.entries[i].slot==slot)return &batch.entries[i];
    return nullptr;
}
template<class Roster> [[nodiscard]] bool valid(const Batch& batch,const Roster& roster,
    std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size())return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!request.registry || request.registry==UINT32_MAX || request.registry==0x811C9DC5
            || request.slot>32767 || request.bubble>63 || !authority_scope::valid(roster,request.registry,request.bubble,region) || !valid(request.state))return false;
        for(std::size_t j=0;j<i;++j)
            if(batch.entries[j].registry==request.registry && batch.entries[j].slot==request.slot)return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& group=roster.groups[g];if(group.key!=request.registry)continue;
            ++groups;
            if(group.slotTypes.size()!=group.slotIndices.size() || group.slotFlags.size()!=group.slotIndices.size())return false;
            for(std::size_t s=0;s<group.slotIndices.size();++s)if(group.slotIndices[s]==request.slot) {
                if(group.slotTypes[s]!=23 || group.slotFlags[s]!=3)return false;
                ++slots;
            }
        }
        if(groups!=1 || slots!=1)return false;
    }
    return true;
}
} // namespace sunrise::middleware::bap::activity_message::native::world_device
