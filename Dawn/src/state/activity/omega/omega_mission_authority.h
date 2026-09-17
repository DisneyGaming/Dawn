#pragma once
#include "omega_mission_state.h"
#include "omega_boss_authority.h"

namespace dawn::state::activity::omega::mission_authority {
inline const mission::Source* find(std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=1) return nullptr;
    const auto index=mission::source_index(key,slot);
    return index<mission::kSources.size()?&mission::kSources[index]:nullptr;
}
inline const mission::Source* member(std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(type==2) for(const auto& source:mission::kSources)
        if(source.registry==key && source.member && source.member==slot) return &source;
    return nullptr;
}
inline bool registry(std::uint32_t key) noexcept {
    for(const auto& source:mission::kSources) if(source.registry==key) return true;
    return false;
}
inline std::size_t bits(const mission::Source& source) noexcept { return 609U+32U*source.categories; }
template<class Writer>
[[nodiscard]] bool write_source(Writer& writer, const mission::Source& source,
                                std::uint32_t generation, std::array<std::uint8_t,2> counts) noexcept {
    const auto index=mission::source_index(source.registry,source.slot);
    if(index==mission::kSources.size() || &source!=&mission::kSources[index]
        || !generation || generation>0x7FFFFFFFU
        || counts[0]>source.requested[0] || counts[1]>source.requested[1]) return false;
    // A proxy belongs to its native member; its parent never requests a loose duplicate.
    if(source.member) counts={};
    bool ok = writer.write(1,1) && boss_authority::write_reference(writer,source.registry,4,0x8000U+source.tactical)
        && writer.write(1,1) && boss_authority::write_reference(writer)
        && writer.write(1,1) && writer.write(0,3)
        && writer.write(1,1) && writer.write(source.categories,4);
    for(unsigned category=0;ok && category<source.categories;++category)
        ok=writer.write(0x80000000U+counts[category],32);
    ok=ok && writer.write(1,1) && writer.write(0,4)
        && writer.write(1,1)
        && writer.write(1,3) && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,1) && writer.write(generation,31)
        && writer.write(1,1) && writer.write(0,32)
        && writer.write(1,1) && writer.write(boss_authority::kAbsentHash,32);
    for (unsigned reference=0;ok && reference<3;++reference)
        ok = writer.write(1,1) && boss_authority::write_reference(writer);
    return ok && writer.write(1,1)
        && boss_authority::write_reference(writer,source.registry,67,0x8000U+source.rule)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(1,6)
        && writer.write(1,1) && writer.write(1U+source.row,5)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,1) && writer.write(boss_authority::kAbsentHash,32);
}
} // namespace dawn::state::activity::omega::mission_authority
