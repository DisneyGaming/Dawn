#pragma once

#include <cstddef>
#include <cstdint>

namespace sunrise::middleware::bap::activity_message::native::combatant_source {

/** Optional native type-3 tactical group and zero-based authored row. */
struct TacticalGroup final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::int8_t row{-1};
    /** Assignment evaluation revision; zero remains the raw-codec compatibility default. */
    std::uint32_t revision{};
};

/** Reflected 80807EC9 source authority. The native spawner still owns template
 * selection, placement, request queuing, actor creation and AI initialization;
 * runtime AI behavior remains to be verified and is not proven by this codec.
 * Counts are cumulative loose requests for one native category, not members. */
struct Source final {
    std::uint32_t registry{};
    std::uint32_t generation{};
    std::uint16_t ruleSlot{};
    std::uint8_t looseRequested{};
    TacticalGroup tactical{};
    // Only sources with two authored native category groups set this extension.
    std::uint8_t secondRequested{};
    bool hasSecondCategory{};
    /** False only for an authored source without a spawn-rule reference (for example a vendor). */
    bool hasSpawnRule{true};
    /** Explicit source-owned retirement. Normal publications keep the legacy wire value. */
    bool retireOwned{};
};
inline constexpr std::size_t kSourceBits = 641;
inline constexpr std::size_t kTwoCategorySourceBits = 673;

[[nodiscard]] constexpr bool valid(const Source& source) noexcept {
    if(source.registry==0 || source.registry==0x811C9DC5U
        || source.generation==0 || source.generation>0x7FFFFFFFU
        || source.ruleSlot>0x7FFFU || (!source.hasSpawnRule && source.ruleSlot!=0) || source.looseRequested>63
        || source.secondRequested>63
        || (!source.hasSecondCategory && source.secondRequested!=0)
        || static_cast<unsigned>(source.looseRequested)+source.secondRequested>63
        || (source.retireOwned && (source.looseRequested || source.secondRequested))) { return false; }
    const auto& tactical=source.tactical;
    const bool assigned=tactical.row>=0;
    if(assigned ? (tactical.registry==0 || tactical.registry==0x811C9DC5U
                   || tactical.slot>0x7FFFU || tactical.row>=24
                   || tactical.revision>0x7FFFFFFFU)
                : (tactical.row!=-1 || tactical.registry!=0 || tactical.slot!=0
                   || tactical.revision!=0)) { return false; }
    return true;
}

template<class Writer>
[[nodiscard]] bool write_source(Writer& writer,const Source& source) noexcept {
    if(!valid(source)) { return false; }
    const auto& tactical=source.tactical;
    const bool assigned=tactical.row>=0;
    const auto begin=writer.bit_count();
    const auto absent=[&writer]() noexcept {
        return writer.write(1,1) && writer.write(0x811C9DC5U,32)
            && writer.write(0,7) && writer.write(32767U,16);
    };
    const bool ok=(assigned
        ? (writer.write(1,1) && writer.write(tactical.registry,32)
            && writer.write(4,7) && writer.write(32768U+tactical.slot,16))
        : absent()) && absent()
        && writer.write(1,1) && writer.write(0,3)
        && writer.write(1,1) && writer.write(source.hasSecondCategory?2U:1U,4)
        && writer.write(0x80000000U+source.looseRequested,32)
        && (!source.hasSecondCategory || writer.write(0x80000000U+source.secondRequested,32))
        && writer.write(1,1) && writer.write(0,4)
        // Variant 0 is required. -1 would index before six native template arrays.
        // Name tier 0 selects the authored display name; other overrides are absent.
        && writer.write(1,1) && writer.write(1,3)
        && writer.write(1,2) && writer.write(0,8)
        && writer.write(1,1) && writer.write(source.generation,31)
        && writer.write(1,1) && writer.write(0,32)
        && writer.write(1,1) && writer.write(0,32)
        && absent() && absent()
        && (source.hasSpawnRule
            ? (writer.write(1,1) && writer.write(source.registry,32)
                && writer.write(67,7) && writer.write(32768U+source.ruleSlot,16))
            : absent())
        && absent()
        // Root 13 is the objective assignment's evaluation revision. Keep the
        // absent form at zero so vendor/raw-codec callers retain their legacy body.
        && writer.write(1,1) && writer.write(assigned?tactical.revision:0U,31)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(0,6)
        && writer.write(1,1) && writer.write(assigned?static_cast<std::uint32_t>(tactical.row)+1U:0U,5)
        && writer.write(1,1) && writer.write(source.generation,31)
        // BC=2 is the established normal source request. BC=1 is reserved for
        // an explicit changed-generation retirement and must never be inferred
        // from a zero target or from an actor/death receipt.
        && writer.write(source.retireOwned?1U:2U,2) && writer.write(1,3)
        && writer.write(1,1) && writer.write(0x811C9DC5U,32);
    return ok && writer.bit_count()-begin==(source.hasSecondCategory?kTwoCategorySourceBits:kSourceBits);
}

} // namespace sunrise::middleware::bap::activity_message::native::combatant_source
