#pragma once
#include "retained_authority_scope.h"
#include "combatant_source.h"
#include <array>
#include <span>

namespace sunrise::middleware::bap::activity_message::native::population {
// The largest package-pinned open-world definition is Nessus: 234 ordinary
// sources plus 131 Lost Sector sources. The 9-bit wire count permits up to 511;
// 384 keeps bounded headroom while matching server, binding and client storage.
inline constexpr std::size_t kSourceCapacity = 384;
// Server-owned values only. No destination names, file IO, actor pointers or
// policy selection in this envelope. The descriptor admission layer verifies
// schema provenance before a service can construct these requests.
struct Request final {
    combatant_source::Source source{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
};
struct Batch final {
    std::array<Request,kSourceCapacity> entries{};
    std::size_t count{};
};
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t key,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=1 || batch.count>batch.entries.size()) return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].source.registry==key && batch.entries[i].slot==slot) return &batch.entries[i];
    return nullptr;
}
[[nodiscard]] inline std::size_t bits(const Request& request) noexcept {
    return request.source.hasSecondCategory?combatant_source::kTwoCategorySourceBits:combatant_source::kSourceBits;
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
    }
    return true;
}
} // namespace sunrise::middleware::bap::activity_message::native::population
