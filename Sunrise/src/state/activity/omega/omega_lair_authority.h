#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "omega_boss_authority.h"
#include "omega_lair_start.h"

namespace sunrise::state::activity::omega::lair_authority {
inline constexpr std::size_t kSourceBits = 641;
inline constexpr std::uint32_t kRegistry = lair_start::kRegistry;
struct Source { std::uint16_t slot, rule; std::uint32_t definition; std::uint8_t tacticalRow; };
// The first five spatial joins are unique. Source6 uses the documented broad
// rear-area policy, reconstructed because several authored firing areas overlap.
inline constexpr std::array<Source, 6> kSources{{
    {3,107,0x80F4791FU,6}, {4,34,0x80F47922U,6}, {5,36,0x80F47925U,7},
    {6,38,0x80F47928U,1}, {7,40,0x80F4792BU,9}, {8,42,0x80F4792EU,10}}};
[[nodiscard]] constexpr const Source* find(std::uint32_t registry, std::uint8_t type,
                                          std::uint16_t slot) noexcept {
    if (registry != kRegistry || type != 1) return nullptr;
    for (const auto& source : kSources) if (source.slot == slot) return &source;
    return nullptr;
}

/** Reflected 80807EC9. Every count is cumulative within the source generation.
 * Native providers retain AI scheduling, firing areas, and authored placement. */
template<class Writer>
[[nodiscard]] bool write_source(Writer& writer, const Source& source,
                                std::uint32_t generation, bool leftStarted) noexcept {
    const auto* expected = find(kRegistry,1,source.slot);
    if (!expected || expected->definition != source.definition || expected->rule != source.rule
        || expected->tacticalRow != source.tacticalRow
        || generation > 0x7FFFFFFFU || (leftStarted && generation == 0))
        return false;
    const auto requested = leftStarted ? 2U : 0U;
    bool ok = writer.write(1,1) && boss_authority::write_reference(writer,kRegistry,4,0x8000U)
        && writer.write(1,1) && boss_authority::write_reference(writer)
        && writer.write(1,1) && writer.write(0,3)
        && writer.write(1,1) && writer.write(1,4) && writer.write(0x80000000U+requested,32)
        && writer.write(1,1) && writer.write(0,4)
        && writer.write(1,1)
        && writer.write(1,3) && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,1) && writer.write(generation,31)
        && writer.write(1,1) && writer.write(0,32)
        && writer.write(1,1) && writer.write(boss_authority::kAbsentHash,32);
    for (unsigned reference=0;ok && reference<3;++reference)
        ok = writer.write(1,1) && boss_authority::write_reference(writer);
    return ok && writer.write(1,1)
        && boss_authority::write_reference(writer,kRegistry,67,0x8000U+source.rule)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(1,6)
        && writer.write(1,1) && writer.write(1U+source.tacticalRow,5)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,1) && writer.write(boss_authority::kAbsentHash,32);
}
} // namespace sunrise::state::activity::omega::lair_authority
