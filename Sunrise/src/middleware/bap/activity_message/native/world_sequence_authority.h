#pragma once
#include "retained_authority_scope.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::middleware::bap::activity_message::native::world_sequence {
// Native80804F04 on a type5 sequence source (not network message type5).
// This bounded contract supports authored sequences without parameter rows.
// NativeF9E980 starts only when the generation byte changes from its old value;
// FF withdraws it. Initial clocks and all unused native references are retained.
inline constexpr std::size_t kDecodedBytes=0x430,kPayloadBits=7359,kCapacity=4;
inline constexpr std::uint32_t kSchema=0x80804F04,kAbsent=0x811C9DC5;
struct Request final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t bubble{},generation{255};
    std::uint64_t startTicks{},endTicks{UINT64_MAX};
    friend bool operator==(const Request&,const Request&)=default;
};
struct Batch final {std::array<Request,kCapacity> entries{};std::size_t count{};};
[[nodiscard]] constexpr bool valid(const Request& r) noexcept {
    return r.registry && r.registry!=UINT32_MAX && r.registry!=kAbsent
        && r.slot<=32767 && r.bubble<64 && r.startTicks!=UINT64_MAX
        && (r.endTicks==UINT64_MAX || r.endTicks>=r.startTicks);
}
template<class Writer> [[nodiscard]] bool write(Writer& writer,const Request& r) noexcept {
    if(!valid(r) || !writer.write(r.startTicks,64) || !writer.write(r.endTicks,64)
        || !writer.write(r.generation,8))return false;
    for(unsigned i=0;i<4;++i)if(!writer.write(0,32))return false;
    // 128 fixed authored-parameter references plus the source's optional parent.
    for(unsigned i=0;i<129;++i)
        if(!writer.write(kAbsent,32) || !writer.write(0,7) || !writer.write(32767,16))return false;
    return true;
}
using Decoded=std::array<std::byte,kDecodedBytes>;
[[nodiscard]] inline Decoded decoded(const Request& r) noexcept {
    Decoded out{};if(!valid(r))return out;
    const auto put=[&](std::size_t at,auto value){std::memcpy(out.data()+at,&value,sizeof(value));};
    put(0,r.startTicks);put(8,r.endTicks);put(0x10,r.generation);
    for(std::size_t i=0;i<129;++i) {
        const auto at=0x24+i*8;put(at,kAbsent);out[at+4]=std::byte{255};put(at+6,std::uint16_t{65535});
    }
    return out;
}
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=5 || batch.count>batch.entries.size())return nullptr;
    for(std::size_t i=0;i<batch.count;++i)if(batch.entries[i].registry==key && batch.entries[i].slot==slot)return &batch.entries[i];
    return nullptr;
}
template<class Roster> [[nodiscard]] bool valid(const Batch& batch,const Roster& roster,std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size())return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& r=batch.entries[i];
        if(!valid(r) || !authority_scope::valid(roster,r.registry,r.bubble,region))return false;
        for(std::size_t j=0;j<i;++j)if(batch.entries[j].registry==r.registry && batch.entries[j].slot==r.slot)return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g)if(roster.groups[g].key==r.registry) {
            const auto& group=roster.groups[g];++groups;
            if(group.slotTypes.size()!=group.slotIndices.size() || group.slotFlags.size()!=group.slotIndices.size())return false;
            for(std::size_t s=0;s<group.slotIndices.size();++s)if(group.slotIndices[s]==r.slot) {
                if(group.slotTypes[s]!=5 || !(group.slotFlags[s]&2))return false;
                ++slots;
            }
        }
        if(groups!=1 || slots!=1)return false;
    }
    return true;
}
} // namespace sunrise::middleware::bap::activity_message::native::world_sequence
