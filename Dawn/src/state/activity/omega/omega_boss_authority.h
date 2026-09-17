#pragma once

#include <cstddef>
#include <cstdint>

namespace dawn::state::activity::omega::boss_authority {

inline constexpr std::uint32_t kRegistry = 0x95FB2E01U;
inline constexpr std::uint32_t kParentSchema = 0x80807EC9U;
inline constexpr std::uint32_t kMemberSchema = 0x80807DA1U;
inline constexpr std::size_t kParentBits = 641;
inline constexpr std::size_t kMemberBits = 42;
// Independent .5 scalar control and .6 named animation-program revisions.
// Callers opt into explicit defaults; the active archive owner supplies both.
// Pinned descriptors:80807DAC at38F4550,80806F11 at38E3DA0,80806F09 rows.
// AB2FD0..AB2FFA broadcasts each float and calls576420 on the member actor.
struct ArmControl {
    std::uint32_t generation{}, revision{};
    bool right{}, high{};
};
struct IntroProgram {
    std::uint32_t generation{},revision{};
    bool play{};
    std::uint32_t sequence{0x65D2379FU};
    std::int8_t departure{-1}; // Authored path95FB2E01/48/55 milestone0..4; -1 is a named animation.
};
inline constexpr std::size_t kDefaultProgramBits=43,kIntroProgramBits=212;
[[nodiscard]] constexpr bool valid_intro(bool active,std::uint32_t generation,const IntroProgram& intro) noexcept {
    const bool named=intro.departure==-1 && (intro.sequence==0x65D2379FU || intro.sequence==0x65D2379CU
        || intro.sequence==0x65D2379EU || intro.sequence==0x65D2379DU || intro.sequence==0x65D2379BU);
    const bool movement=intro.departure>=0 && intro.departure<=4 && intro.sequence==0xCBFDCA32U;
    return !active || !intro.revision || (intro.generation==generation
        && intro.revision<=0x7FFFFFFFU && (!intro.play || named || movement));
}
inline constexpr std::size_t kDefaultControlBits=138,kArmControlBits=266;
inline constexpr std::uint32_t kLeftArmProperty=0xA2AE120FU,kRightArmProperty=0x8496ABD2U;
[[nodiscard]] constexpr bool valid_control(bool active,std::uint32_t generation,const ArmControl& arm) noexcept {
    return !active || !arm.revision || (arm.generation==generation && arm.revision<=0x7FFFFFFFU);
}
[[nodiscard]] constexpr std::size_t member_bits(bool active,std::uint32_t generation,const ArmControl& arm={},bool explicitControl=false,
    const IntroProgram& intro={},bool explicitProgram=false) noexcept {
    return (!active || (generation && generation<=0x7FFFFFFFU)) && valid_control(active,generation,arm) && valid_intro(active,generation,intro)?kMemberBits
        +(active && arm.revision?kArmControlBits:explicitControl?kDefaultControlBits:0U)
        +(active && intro.revision && intro.play?kIntroProgramBits:explicitProgram || (active && intro.revision)?kDefaultProgramBits:0U):0U;
}
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
[[nodiscard]] bool write_member(Writer& writer, bool active, std::uint32_t generation,const ArmControl& arm={},bool explicitControl=false,
    const IntroProgram& intro={},bool explicitProgram=false) noexcept {
    if (!valid_generation(active,generation) || !valid_control(active,generation,arm) || !valid_intro(active,generation,intro)) return false;
    // Pinned reflection37D5AC0. The Omega owner explicitly clears .5 when no
    // arm is issued so optional-field retention cannot replay a previous run.
    // The active archive owner also publishes explicit .6 defaults or a named
    // animation/movement program; legacy callers can retain the optional record.
    bool ok=writer.write(1,1) && writer.write(active ? generation : 0U,31)
        && writer.write(1,2) // +04 retirement mode0
        && writer.write(1,3) // +05 location mode0
        && writer.write(active ? 1U : 0U,1) // +06 enabled
        && writer.write(0,1) // +008 80807DA7
        && writer.write(explicitControl || (active && arm.revision)?1U:0U,1); // +04C 80807DAC
    if(explicitControl || (active && arm.revision)) {
        const bool owned=active && arm.revision;
        // Zero value/mask touches no actor flags. Empty hash list and absent
        // target retain control defaults; only the two authored float rows change.
        ok=ok && writer.write(owned?arm.revision:0U,31) && writer.write(0,6) && writer.write(0,6)
            && writer.write(0,3) && write_reference(writer) && writer.write(0,32)
            && writer.write(owned?2U:0U,5);
        if(owned) ok=ok && writer.write(kLeftArmProperty,32) && writer.write(arm.high && !arm.right?0x3F800000U:0U,32)
            && writer.write(kRightArmProperty,32) && writer.write(arm.high && arm.right?0x3F800000U:0U,32);
    }
    const bool program=explicitProgram || (active && intro.revision);
    ok=ok && writer.write(program?1U:0U,1); // +100 80807F6F
    if(program) {
        const bool play=active && intro.revision && intro.play;
        ok=ok && writer.write(active?intro.revision:0U,31) && writer.write(0,6) && writer.write(play?1U:0U,6);
        if(play) {
            // 80807F72 /80807F76: exact kind9 queue used by boss_intro_action.
            // Native default condition0. Movement targets the authored path milestone;
            // named animation programs retain their original absent target/mode0.
            ok=ok && writer.write(1,1) && writer.write(10,4) && writer.write(1,2)
                && writer.write(intro.departure<0?0xAFB11A12U:0x1F992208U,32) && writer.write(intro.sequence,32)
                && writer.write(kAbsentHash,32)
                && (intro.departure<0?write_reference(writer):write_reference(writer,kRegistry,49,0x8037U))
                && writer.write(1,3) && writer.write(static_cast<std::uint32_t>(128+(intro.departure<0?0:intro.departure)),8);
        }
    }
    return ok && writer.write(0,1); // +910 80807ED9
}

} // namespace dawn::state::activity::omega::boss_authority
