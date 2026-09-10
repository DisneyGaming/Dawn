#pragma once
#include "retained_authority_scope.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::middleware::bap::activity_message::native::engagement {
// Bounded native type70/schema808094F1 participant authority.
// Original9F1820 adopts the208-byte image;9F1290 checks its participant list.
// Flags3 inhibit both automatic collection branches in9F2A00. Flags2 permit
// native active-player collection while disabling source-reference collection.
// Neither route claims authored engagement-volume policy or event timing.
inline constexpr std::size_t kDecodedBytes=0xD0,kMaximumParticipants=16;
// Native9FBEF0 selects80807BF1 (uint64) or80809ACF (8 bytes) via50BF00.
// The host must qualify that native gate before selecting the wire format.
enum class IdentifierEncoding : std::uint8_t { unknown, integer, byte_array };
enum class Collection : std::uint8_t { explicitParticipants, activePlayers, none };
struct Request final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint32_t scope{UINT32_MAX};
    std::int16_t generation{};
    std::array<std::uint64_t,kMaximumParticipants> participants{};
    std::size_t participantCount{};
    IdentifierEncoding identifierEncoding{IdentifierEncoding::unknown};
    Collection collection{Collection::explicitParticipants};
    friend bool operator==(const Request&,const Request&)=default;
};
[[nodiscard]] constexpr bool valid(const Request& request) noexcept {
    if(!request.registry || request.registry==UINT32_MAX || request.registry==0x811C9DC5U
        || request.slot>32767 || request.scope>63 || request.generation<0
        || request.participantCount>kMaximumParticipants)return false;
    if(request.collection==Collection::activePlayers)
        return request.participantCount==0 && request.identifierEncoding==IdentifierEncoding::unknown;
    if(request.collection==Collection::none)
        return request.participantCount==0 && request.identifierEncoding==IdentifierEncoding::unknown;
    if(request.collection!=Collection::explicitParticipants || !request.participantCount
        || (request.identifierEncoding!=IdentifierEncoding::integer
            && request.identifierEncoding!=IdentifierEncoding::byte_array))return false;
    for(std::size_t i=0;i<request.participantCount;++i) {
        if(!request.participants[i] || request.participants[i]==UINT64_MAX)return false;
        for(std::size_t j=0;j<i;++j)if(request.participants[j]==request.participants[i])return false;
    }
    return true;
}
[[nodiscard]] constexpr std::size_t body_bits(const Request& request) noexcept {
    return valid(request)?32+64*request.participantCount:0;
}
template<class Writer> [[nodiscard]] bool write(Writer& writer,const Request& request) noexcept {
    // Optional omission preserves prior list storage. Explicit count0 clears
    // the source-ref count even when the native delta target is reused.
    if(!valid(request) || !writer.write(request.collection==Collection::activePlayers?2:3,5)
        || !writer.write(1,1) || !writer.write(0,4)
        || !writer.write(static_cast<unsigned>(request.generation)+32768U,16)
        || !writer.write(1,1) || !writer.write(request.participantCount,5))return false;
    for(std::size_t i=0;i<request.participantCount;++i) {
        if(request.identifierEncoding==IdentifierEncoding::integer) {
            if(!writer.write(request.participants[i],64))return false;
        } else for(unsigned byte=0;byte<8;++byte)
            if(!writer.write((request.participants[i]>>(8*byte))&255U,8))return false;
    }
    return true;
}
using Decoded=std::array<std::byte,kDecodedBytes>;
[[nodiscard]] inline Decoded decoded(const Request& request) noexcept {
    Decoded result{};if(!valid(request))return result;
    const auto put=[&](std::size_t at,auto value) {std::memcpy(result.data()+at,&value,sizeof(value));};
    result[0]=request.collection==Collection::activePlayers?std::byte{2}:std::byte{3};put(0x48,request.generation);
    put(0x4C,static_cast<std::uint32_t>(request.participantCount));
    for(std::size_t i=0;i<request.participantCount;++i)put(0x50+8*i,request.participants[i]);
    return result;
}
[[nodiscard]] inline bool matches_fields(std::span<const std::byte> bytes,const Request& request) noexcept {
    if(bytes.size()!=kDecodedBytes || !valid(request))return false;
    const auto expected=decoded(request);
    // Original decoding preserves padding, unused IDs and source-ref storage
    // behind the explicitly cleared count. Only decoded semantic fields match.
    if(bytes[0]!=expected[0])return false;
    for(std::size_t i=4;i<8;++i)if(bytes[i]!=expected[i])return false;
    for(std::size_t i=0x48;i<0x4A;++i)if(bytes[i]!=expected[i])return false;
    for(std::size_t i=0x4C;i<0x50+8*request.participantCount;++i)if(bytes[i]!=expected[i])return false;
    return true;
}
struct Batch final { std::array<Request,4> entries{};std::size_t count{}; };
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t key,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=70 || batch.count>batch.entries.size()) return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].registry==key && batch.entries[i].slot==slot) return &batch.entries[i];
    return nullptr;
}
template<class Roster>
[[nodiscard]] bool valid(const Batch& batch,const Roster& roster,std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size()
        || roster.topLevelGroupCount>roster.groupCount) return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!request.registry || request.registry==UINT32_MAX || request.slot>32767 || request.scope>63
            || !body_bits(request)
            || !authority_scope::valid(roster,request.registry,request.scope,region)) return false;
        for(std::size_t j=0;j<i;++j)
            if(batch.entries[j].registry==request.registry && batch.entries[j].slot==request.slot) return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& row=roster.groups[g];if(row.key!=request.registry) continue;
            ++groups;
            if(g<roster.topLevelGroupCount)return false;
            if(row.slotTypes.size()!=row.slotIndices.size() || row.slotFlags.size()!=row.slotIndices.size()) return false;
            for(std::size_t s=0;s<row.slotIndices.size();++s) if(row.slotIndices[s]==request.slot) {
                if(row.slotTypes[s]!=70 || (row.slotFlags[s]&2)==0) return false;
                ++slots;
            }
        }
        if(groups!=1 || slots!=1) return false;
    }
    return true;
}
}
