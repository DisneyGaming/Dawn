#pragma once

#include <cstddef>
#include <cstdint>

namespace sunrise::state::activity::omega::boss_authority {

inline constexpr std::uint32_t kRegistry = 0x95FB2E01U;
inline constexpr std::uint32_t kParentSchema = 0x80807EC9U;
inline constexpr std::uint32_t kMemberSchema = 0x80807DA1U;
inline constexpr std::size_t kParentBits = 641;
inline constexpr std::size_t kMemberBits = 42;
inline constexpr std::uint32_t kAbsentHash = 0x811C9DC5U;

[[nodiscard]] constexpr bool parent_slot(std::uint32_t registry, std::uint8_t type,
                                         std::uint16_t index) noexcept {
    return registry == kRegistry && type == 1 && index == 0;
}

[[nodiscard]] constexpr bool member_slot(std::uint32_t registry, std::uint8_t type,
                                         std::uint16_t index) noexcept {
    return registry == kRegistry && type == 2 && index == 1;
}

// The native source and member store a nonnegative, 31-bit generation. Reject an
// invalid activation before touching the writer; a dormant body always uses zero.
[[nodiscard]] constexpr bool valid_generation(bool active, std::uint32_t generation) noexcept {
    return !active || (generation != 0 && generation <= 0x7FFFFFFFU);
}

template<class Writer>
[[nodiscard]] bool write_reference(Writer& writer, std::uint32_t registry = kAbsentHash,
                                    std::uint32_t type = 0, std::uint32_t index = 0x7FFFU) noexcept {
    // Schema80809C42: raw hash, signed byte biased by1, signed word biased by32768.
    // Canonical absent is {811C9DC5,-1,-1}; it selects packaged fallback placement.
    return writer.write(registry,32) && writer.write(type,7) && writer.write(index,16);
}

template<class Writer>
[[nodiscard]] bool write_parent(Writer& writer, bool active, std::uint32_t generation) noexcept {
    if (!valid_generation(active,generation)) return false;
    // Pinned reflection38F8378:21 fields,19 optional. Keep every parent field
    // explicit so no earlier authority value can survive a generation reset.
    bool ok = writer.write(1,1) && write_reference(writer) // +00 tactical reference
        && writer.write(1,1) && write_reference(writer) // +08 command reference
        && writer.write(1,1) && writer.write(0,3) // +10 empty hash array
        && writer.write(1,1) && writer.write(1,4) // +2C one authored actor category
        && writer.write(0x80000000U,32) // +30 zero requested loose actors
        && writer.write(1,1) && writer.write(0,4) // +50 empty secondary count array
        && writer.write(1,1) // +74 five actor overrides80807ED2
        && writer.write(1,3) && writer.write(1,2) // variant0; name/difficulty tier0
        && writer.write(0,3) && writer.write(0,2) && writer.write(0,3) // remaining overrides=-1
        && writer.write(1,1) && writer.write(active ? generation : 0U,31) // +7C generation
        && writer.write(1,1) && writer.write(0,32) // +80 native zero
        && writer.write(1,1) && writer.write(kAbsentHash,32); // +84 native hash sentinel
    // +88,+90,+98 retain canonical absent references. +A0 is the authored
    // secondary spawn rule selected first by4E6480 for member location mode0.
    for (unsigned reference=0; ok && reference<3; ++reference)
        ok = writer.write(1,1) && write_reference(writer);
    return ok && writer.write(1,1)
        && write_reference(writer,kRegistry,67,0x8039U) // sr_boss_location_1 /66/57
        && writer.write(1,1) && writer.write(0,31) // +A8
        && writer.write(1,1) && writer.write(0,31) // +AC
        && writer.write(1,1) && writer.write(1,6) // +B0 native zero, bias1
        && writer.write(1,1) && writer.write(1,5) // +B4 native zero, bias1
        && writer.write(1,1) && writer.write(0,31) // +B8 independent Scene revision
        && writer.write(1,2) && writer.write(1,3) // +BC retirement0; +BD request mode0
        && writer.write(1,1) && writer.write(kAbsentHash,32); // +C0 native hash sentinel
}

template<class Writer>
[[nodiscard]] bool write_member(Writer& writer, bool active, std::uint32_t generation) noexcept {
    if (!valid_generation(active,generation)) return false;
    // Pinned reflection37D5AC0. The four absent nested records leave all native
    // registration defaults intact, including command revision/head/count=0.
    return writer.write(1,1) && writer.write(active ? generation : 0U,31)
        && writer.write(1,2) // +04 retirement mode0
        && writer.write(1,3) // +05 location mode0
        && writer.write(active ? 1U : 0U,1) // +06 enabled
        && writer.write(0,1) // +008 80807DA7
        && writer.write(0,1) // +04C 80807DAC
        && writer.write(0,1) // +100 80807F6F
        && writer.write(0,1); // +910 80807ED9
}

} // namespace sunrise::state::activity::omega::boss_authority
