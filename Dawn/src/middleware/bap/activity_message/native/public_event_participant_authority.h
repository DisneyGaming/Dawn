#pragma once
#include "public_event_engagement_authority.h"
#include "adventure_cue_authority.h"
#include <bit>
#include <cmath>

namespace dawn::middleware::bap::activity_message::native::event_participant {
// Authored type71/schema80804F57. OriginalBF5AA0 compares entity with the
// native local player ID, producing +199; no direct UI bit is written here.
inline constexpr std::size_t kBits=183,kDecodedBytes=24;
struct Request final {
    std::uint32_t registry{};std::uint16_t slot{};std::uint32_t scope{UINT32_MAX};
    std::int32_t value{-1};
    std::uint64_t entity{};
    engagement::IdentifierEncoding identifierEncoding{engagement::IdentifierEncoding::unknown};
    cue::Reference region{};
    float leaveDelay{};
    friend bool operator==(const Request&,const Request&)=default;
};
[[nodiscard]] inline bool valid(const Request& r) noexcept {
    return r.registry && r.registry!=UINT32_MAX && r.registry!=cue::kAbsent && r.slot<=32767 && r.scope<=63
        && r.entity!=UINT64_MAX && (r.identifierEncoding==engagement::IdentifierEncoding::integer
            || r.identifierEncoding==engagement::IdentifierEncoding::byte_array)
        && cue::valid(r.region) && (r.region==cue::Reference{} || r.region.registry==r.registry)
        && std::isfinite(r.leaveDelay) && r.leaveDelay>=0;
}
[[nodiscard]] inline std::size_t body_bits(const Request& r) noexcept {return valid(r)?kBits:0;}
template<class Writer> [[nodiscard]] bool write(Writer& w,const Request& r) noexcept {
    if(!valid(r) || !w.write(0x80000000U+static_cast<std::uint32_t>(r.value),32))return false;
    if(r.identifierEncoding==engagement::IdentifierEncoding::integer) {
        if(!w.write(r.entity,64))return false;
    } else for(unsigned i=0;i<8;++i)if(!w.write((r.entity>>(8*i))&255U,8))return false;
    return w.write(r.region.registry,32) && w.write(static_cast<unsigned>(r.region.type)+1U,7)
        && w.write(static_cast<unsigned>(r.region.slot)+32768U,16)
        && w.write(std::bit_cast<std::uint32_t>(r.leaveDelay),32);
}
using Decoded=std::array<std::byte,kDecodedBytes>;
[[nodiscard]] inline Decoded decoded(const Request& r) noexcept {
    Decoded out{};if(!valid(r))return out;
    const auto put=[&](std::size_t at,auto v){std::memcpy(out.data()+at,&v,sizeof(v));};
    put(0,r.value);put(4,r.entity);put(12,r.region.registry);put(16,r.region.type);put(18,r.region.slot);put(20,r.leaveDelay);return out;
}
[[nodiscard]] inline bool matches_fields(std::span<const std::byte> bytes,const Request& r) noexcept {
    if(bytes.size()!=kDecodedBytes || !valid(r))return false;
    const auto expected=decoded(r);for(std::size_t i=0;i<kDecodedBytes;++i)if(i!=17 && bytes[i]!=expected[i])return false;return true;
}
struct Batch final {std::array<Request,4> entries{};std::size_t count{};};
[[nodiscard]] inline const Request* find(const Batch& b,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=71 || b.count>b.entries.size())return nullptr;
    for(std::size_t i=0;i<b.count;++i)if(b.entries[i].registry==key && b.entries[i].slot==slot)return &b.entries[i];return nullptr;
}
template<class Roster> [[nodiscard]] bool valid(const Batch& b,const Roster& roster,std::uint32_t region) noexcept {
    if(b.count>b.entries.size() || roster.groupCount>roster.groups.size() || roster.topLevelGroupCount>roster.groupCount)return false;
    for(std::size_t i=0;i<b.count;++i) {
        const auto& r=b.entries[i];if(!valid(r) || !authority_scope::valid(roster,r.registry,r.scope,region))return false;
        for(std::size_t j=0;j<i;++j)if(b.entries[j].registry==r.registry && b.entries[j].slot==r.slot)return false;
        unsigned matches{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& row=roster.groups[g];if(row.key!=r.registry)continue;
            if(g<roster.topLevelGroupCount || row.slotTypes.size()!=row.slotIndices.size() || row.slotFlags.size()!=row.slotIndices.size())return false;
            for(std::size_t s=0;s<row.slotIndices.size();++s)if(row.slotIndices[s]==r.slot) {
                if(row.slotTypes[s]!=71 || !(row.slotFlags[s]&2))return false;++matches;
            }
        }
        if(matches!=1)return false;
        if(r.region!=cue::Reference{}) {
            unsigned regions{};
            for(std::size_t g=0;g<roster.groupCount;++g) {
                const auto& row=roster.groups[g];if(row.key!=r.region.registry)continue;
                for(std::size_t s=0;s<row.slotIndices.size();++s)if(row.slotIndices[s]==r.region.slot) {
                    if(row.slotTypes[s]!=r.region.type || !(row.slotFlags[s]&1))return false;++regions;
                }
            }
            if(regions!=1)return false;
        }
    }
    return true;
}
} // namespace dawn::middleware::bap::activity_message::native::event_participant
