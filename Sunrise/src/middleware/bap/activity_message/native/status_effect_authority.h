#pragma once

#include "retained_authority_scope.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::middleware::bap::activity_message::native::status_effect {

inline constexpr std::uint32_t kSchema=0x8080954BU;
inline constexpr std::uint32_t kComponentClass=0x8080953FU;
inline constexpr std::uint8_t kType=26;
inline constexpr std::uint32_t kPlayerSelector=0x80809155U;
inline constexpr std::uint32_t kAllPlayersSelector=0x8080915DU;
inline constexpr std::size_t kCapacity=16;
inline constexpr std::size_t kBiasedZeroChannelCount=4;
inline constexpr std::size_t kInactiveBits=186;
inline constexpr std::size_t kActiveBits=251;

/** Generic host-owned type-26 status-effect request. */
struct Request final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    bool enabled{};
    // Native +0 suppresses periodic reapplication. +10 is echoed by sense+8.
    bool once{};
    std::int32_t selectionRevision{};
};

struct Batch final {
    std::array<Request,kCapacity> entries{};
    std::size_t count{};
};

[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t registry,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=kType || batch.count>batch.entries.size())return nullptr;
    const Request* found{};
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(request.registry==registry && request.slot==slot) {
            if(found)return nullptr;
            found=&request;
        }
    }
    return found;
}

template<class Writer>
[[nodiscard]] bool write_authority(Writer& writer,const Request& request) noexcept {
    const auto enabled=request.enabled;
    const auto begin=writer.bit_count();
    if(request.selectionRevision<0 || !writer.write(request.once?1U:0U,1)
        || !writer.write(enabled?0U:1U,1))return false;
    for(std::size_t channel=0;channel<kBiasedZeroChannelCount;++channel)
        if(!writer.write(0x80000000U+(channel==3?
            static_cast<std::uint32_t>(request.selectionRevision):0U),32))return false;
    if(!writer.write(0x811C9DC5U,32) || !writer.write(0,7)
        || !writer.write(0x7FFFU,16) || !writer.write(enabled?1U:0U,1))return false;
    if(enabled && (!writer.write(kPlayerSelector,32) || !writer.write(1,1)
        || !writer.write(kAllPlayersSelector,32)))return false;
    return writer.bit_count()-begin==(enabled?kActiveBits:kInactiveBits);
}

template<class Writer>
[[nodiscard]] bool write_authority(Writer& writer,bool enabled) noexcept {
    Request request{};request.enabled=enabled;
    return write_authority(writer,request);
}

template<class Roster>
[[nodiscard]] bool valid(const Batch& batch,const Roster& roster,std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size())return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!request.registry || request.registry==UINT32_MAX || request.registry==0x811C9DC5U
            || request.selectionRevision<0
            || request.slot>32767 || request.bubble>63
            || !authority_scope::valid(roster,request.registry,request.bubble,region))return false;
        for(std::size_t j=0;j<i;++j)
            if(batch.entries[j].registry==request.registry && batch.entries[j].slot==request.slot)
                return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& group=roster.groups[g];
            if(group.key!=request.registry)continue;
            ++groups;
            if(group.slotTypes.size()!=group.slotIndices.size()
                || group.slotFlags.size()!=group.slotIndices.size())return false;
            for(std::size_t s=0;s<group.slotIndices.size();++s) {
                if(group.slotIndices[s]!=request.slot)continue;
                if(group.slotTypes[s]!=kType || group.slotFlags[s]!=3)return false;
                ++slots;
            }
        }
        if(groups!=1 || slots!=1)return false;
    }
    return true;
}

[[nodiscard]] inline constexpr std::size_t bits(bool enabled) noexcept {
    return enabled?kActiveBits:kInactiveBits;
}
[[nodiscard]] inline constexpr std::size_t bits(const Request& request) noexcept {
    return bits(request.enabled);
}

} // namespace sunrise::middleware::bap::activity_message::native::status_effect
