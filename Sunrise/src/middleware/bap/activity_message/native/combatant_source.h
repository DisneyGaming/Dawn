#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

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
 * Counts are cumulative loose requests for each reflected native category, not members. */
struct Source final {
    std::uint32_t registry{};
    std::uint32_t generation{};
    std::uint16_t ruleSlot{};
    std::uint32_t looseRequested{};
    TacticalGroup tactical{};
    // Authored native category groups beyond the first use these extensions.
    std::uint32_t secondRequested{};
    bool hasSecondCategory{};
    /** False only for an authored source without a spawn-rule reference (for example a vendor). */
    bool hasSpawnRule{true};
    /** Explicit source-owned retirement. Normal publications keep the legacy wire value. */
    bool retireOwned{};
    std::array<std::uint32_t,6> additionalRequested{};
    // Zero preserves the legacy one/two-category construction surface.
    // Positive widths are bounded by the reflected eight-value sense mirror.
    std::uint8_t categoryCount{};
};
inline constexpr std::size_t kSourceBits = 641;
inline constexpr std::size_t kTwoCategorySourceBits = 673;
inline constexpr std::uint8_t kMaximumCategories=8;
[[nodiscard]] constexpr std::uint8_t categories(const Source& source) noexcept {
    return source.categoryCount?source.categoryCount:(source.hasSecondCategory?2U:1U);
}
[[nodiscard]] constexpr std::uint32_t requested(const Source& source,std::size_t category) noexcept {
    return category==0?source.looseRequested:category==1?source.secondRequested:
        category<kMaximumCategories?source.additionalRequested[category-2]:0U;
}
[[nodiscard]] constexpr std::size_t source_bits(const Source& source) noexcept {
    return kSourceBits+32U*(categories(source)-1U);
}

[[nodiscard]] constexpr bool valid(const Source& source) noexcept {
    if(source.registry==0 || source.registry==0x811C9DC5U
        || source.generation==0 || source.generation>0x7FFFFFFFU
        || source.ruleSlot>0x7FFFU || (!source.hasSpawnRule && source.ruleSlot!=0) || source.looseRequested>INT32_MAX
        || source.secondRequested>INT32_MAX) { return false; }
    const auto count=categories(source);
    if(!count || count>kMaximumCategories || source.hasSecondCategory!=(count>1)
        || (!source.categoryCount && count>2))return false;
    for(std::size_t i=0;i<kMaximumCategories;++i)
        if((i>=count && requested(source,i)) || requested(source,i)>INT32_MAX
            || (source.retireOwned && requested(source,i)))return false;
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
        && writer.write(1,1) && writer.write(categories(source),4);
    bool counts=ok;
    for(std::size_t i=0;i<categories(source) && counts;++i)
        counts=writer.write(0x80000000U+requested(source,i),32);
    const bool rest=counts
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
    return rest && writer.bit_count()-begin==source_bits(source);
}

} // namespace sunrise::middleware::bap::activity_message::native::combatant_source
