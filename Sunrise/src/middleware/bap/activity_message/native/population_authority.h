#pragma once
#include "retained_authority_scope.h"
#include "combatant_source.h"
#include <array>
#include <span>

namespace sunrise::middleware::bap::activity_message::native::population {
inline constexpr std::size_t kPopulationCapacity = 128;
inline constexpr std::uint16_t kNoNamedMember = UINT16_MAX;
// Server-owned values only. No destination names, file IO, actor pointers or
// policy selection in this envelope. The descriptor admission layer verifies
// schema provenance before a service can construct these requests.
struct Request final {
    combatant_source::Source source{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    /** Optional authored local type-2 member owned by this type-1 source. */
    std::uint16_t namedMember{kNoNamedMember};
};
using BatchRequest = Request;
struct Batch final {
    std::array<Request,kPopulationCapacity> entries{};
    std::size_t count{};
};
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t key,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=1 || batch.count>batch.entries.size()) return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].source.registry==key && batch.entries[i].slot==slot) return &batch.entries[i];
    return nullptr;
}
/** Find the source request which names an exact local member descriptor. */
[[nodiscard]] inline const Request* find_member(const Batch& batch,std::uint32_t key,
    std::uint8_t type=2,std::uint16_t slot=kNoNamedMember) noexcept {
    if(type!=2 || slot==kNoNamedMember || batch.count>batch.entries.size()) return nullptr;
    const Request* found{};
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(request.source.registry==key && request.namedMember==slot) {
            if(found) return nullptr;
            found=&request;
        }
    }
    return found;
}
[[nodiscard]] inline std::size_t bits(const Request& request) noexcept {
    return request.source.hasSecondCategory?combatant_source::kTwoCategorySourceBits:combatant_source::kSourceBits;
}
inline constexpr std::size_t kMemberBits = 42;
[[nodiscard]] inline constexpr std::size_t member_bits(bool /*retiring*/=false) noexcept { return kMemberBits; }

/** Generic copy of the proven 80807DA1 named-member bind/retire codec. */
template<class Writer>
[[nodiscard]] bool write_member(Writer& writer,std::uint32_t generation,bool retiring=false) noexcept {
    if(generation==0 || generation>0x7FFFFFFFU) return false;
    const auto begin=writer.bit_count();
    const bool ok=writer.write(1,1) && writer.write(generation,31)
        && (retiring
            ? (writer.write(1,2) && writer.write(2,3) && writer.write(0,1)
                && writer.write(0,1) && writer.write(0,1) && writer.write(0,1) && writer.write(0,1))
            : (writer.write(0,2) && writer.write(2,3) && writer.write(1,1)
                && writer.write(0,1) && writer.write(0,1) && writer.write(0,1) && writer.write(0,1)));
    return ok && writer.bit_count()-begin==kMemberBits;
}
template<class Writer>
[[nodiscard]] bool write_bind_member(Writer& writer,std::uint32_t generation) noexcept {
    return write_member(writer,generation,false);
}
template<class Writer>
[[nodiscard]] bool write_retire_member(Writer& writer,std::uint32_t generation) noexcept {
    return write_member(writer,generation,true);
}
template<class Roster>
[[nodiscard]] bool valid(const Batch& batch,const Roster& roster,std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size()) return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!combatant_source::valid(request.source) || request.slot>32767 || request.bubble>63
            || !authority_scope::valid(roster,request.source.registry,request.bubble,region)) return false;
        for(std::size_t j=0;j<i;++j)
            if(batch.entries[j].source.registry==request.source.registry && batch.entries[j].slot==request.slot) return false;
        if(request.namedMember!=kNoNamedMember)
            for(std::size_t j=0;j<i;++j)
                if(batch.entries[j].source.registry==request.source.registry
                    && batch.entries[j].namedMember==request.namedMember) return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& row=roster.groups[g];
            if(row.key!=request.source.registry) continue;
            ++groups;
            if(row.slotTypes.size()!=row.slotIndices.size() || row.slotFlags.size()!=row.slotIndices.size()) return false;
            for(std::size_t s=0;s<row.slotIndices.size();++s)
                if(row.slotIndices[s]==request.slot) {
                    if(row.slotTypes[s]!=1 || (row.slotFlags[s]&2)==0) return false;
                    ++slots;
                }
        }
        if(groups!=1 || slots!=1) return false;
        if(request.namedMember!=kNoNamedMember) {
            unsigned members{},occurrences{};
            for(std::size_t g=0;g<roster.groupCount;++g) {
                const auto& row=roster.groups[g];
                if(row.key!=request.source.registry) continue;
                if(row.slotTypes.size()!=row.slotIndices.size() || row.slotFlags.size()!=row.slotIndices.size()) return false;
                for(std::size_t s=0;s<row.slotIndices.size();++s) {
                    if(row.slotIndices[s]!=request.namedMember) continue;
                    ++occurrences;
                    if(row.slotTypes[s]==2 && row.slotFlags[s]==3) ++members;
                }
            }
            if(members!=1 || occurrences!=1) return false;
        }
    }
    return true;
}
} // namespace sunrise::middleware::bap::activity_message::native::population
