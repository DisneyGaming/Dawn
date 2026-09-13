#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "../../equipment/light/definition.h"
#include "../strike_variants.h"
#include "rules.h"

namespace sunrise::state::activity::nightfall {

/** Installed build-86657 native Grandmaster difficulty-settings definition. */
inline constexpr std::uint32_t kGrandmasterDifficultySettings = 0x81327CEFU;
/** Both direct Grandmaster activity records publish this native activity power. */
inline constexpr std::int32_t kGrandmasterActivityPower = 1100;

/** A validated, exact-activity contest projection. An empty value means no power override. */
struct NativePowerProjection final {
    std::int16_t activity{-1};
    std::uint32_t difficultySettings{};
    std::int32_t authoredPower{};
    std::int32_t effectiveCap{};
    std::uint8_t delta{};

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return (activity == 813 || activity == 835)
               && difficultySettings == kGrandmasterDifficultySettings
               && authoredPower == kGrandmasterActivityPower && delta >= 30
               && effectiveCap == authoredPower - static_cast<std::int32_t>(delta);
    }

    [[nodiscard]] bool operator==(const NativePowerProjection&) const noexcept = default;
};

/**
 * Resolves only the two installed direct Grandmaster identities. The native activity row remains
 * the enemy-side 1100 source; the projection limits only player-owned light publications.
 */
[[nodiscard]] constexpr NativePowerProjection
native_power_projection(std::int16_t activity, std::uint8_t requestedDelta) noexcept {
    const strikes::Variant* const variant = strikes::find(activity);
    if (variant == nullptr || variant->difficulty != strikes::Difficulty::grandmaster
        || (activity != 813 && activity != 835) || requestedDelta < 30
        || requestedDelta > 50) {
        return {};
    }
    return {activity,
            kGrandmasterDifficultySettings,
            kGrandmasterActivityPower,
            kGrandmasterActivityPower - static_cast<std::int32_t>(requestedDelta),
            requestedDelta};
}

/** Resolves one coherent lock-free rules snapshot into its exact native projection. */
[[nodiscard]] inline NativePowerProjection current_native_power_projection() noexcept {
    const Selection selected = selection();
    return native_power_projection(selected.activity, selected.options.powerDelta);
}

/** Caps one player-owned published light without raising a lower-power character. */
[[nodiscard]] constexpr std::int32_t
cap_player_power(std::int32_t power, const NativePowerProjection& projection) noexcept {
    return projection && power >= 0 ? (std::min)(power, projection.effectiveCap) : power;
}

/**
 * Projects one selected-character item level. Unequipped inventory retains its authored level.
 * Item levels represent ten power each, so a cap between level steps rounds down and remains at
 * least as difficult as the requested disadvantage.
 */
[[nodiscard]] constexpr std::int32_t
project_selected_item_level(std::int32_t level,
                            bool equipped,
                            const NativePowerProjection& projection) noexcept {
    return equipped && projection && level >= 0
               ? (std::min)(level,
                            projection.effectiveCap / equipment::light::kPowerPerLevel)
               : level;
}

enum class NativePowerApplyResult : std::uint8_t { inactive, unchanged, applied, invalid };

namespace detail {

[[nodiscard]] inline bool valid_summary(const equipment::light::Evaluation& value) noexcept {
    constexpr std::int32_t maximumDivisor =
        static_cast<std::int32_t>(build_data::items::details::kEquipmentSlotCount);
    if (value.divisor <= 0 || value.divisor > maximumDivisor || value.total < 0
        || value.average != value.total / value.divisor || !std::isfinite(value.averageFloat)
        || value.averageFloat
               != static_cast<float>(value.total) / static_cast<float>(value.divisor)) {
        return false;
    }
    for (const equipment::light::SlotScores* scores : {&value.profile, &value.character}) {
        for (const std::optional<equipment::light::ItemScore>& score : *scores) {
            if (score.has_value()
                && (score->definitionIndex == std::uint16_t{0xFFFF} || score->score < 0)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace detail

/**
 * Applies the player contest cap to the effective native Family-4 equipment aggregate and the
 * selected character's equipped score lanes. The native total can include strict upgrades from
 * other characters that are intentionally absent from both arrays, so it is capped independently
 * instead of being recomputed from the published arrays. Profile candidates keep their real score.
 * The input is committed only after its scalar invariants validate.
 */
[[nodiscard]] inline NativePowerApplyResult
cap_equipment_summary(equipment::light::Evaluation& value,
                      const NativePowerProjection& projection) noexcept {
    if (!projection) {
        return NativePowerApplyResult::inactive;
    }
    if (!detail::valid_summary(value)) {
        return NativePowerApplyResult::invalid;
    }

    equipment::light::Evaluation staged = value;
    for (std::optional<equipment::light::ItemScore>& score : staged.character) {
        if (score.has_value()) {
            score->score = cap_player_power(score->score, projection);
        }
    }
    const std::int64_t cappedTotal = static_cast<std::int64_t>(projection.effectiveCap)
                                     * static_cast<std::int64_t>(staged.divisor);
    if (static_cast<std::int64_t>(staged.total) <= cappedTotal) {
        if (staged.character == value.character) {
            return NativePowerApplyResult::unchanged;
        }
        value = staged;
        return NativePowerApplyResult::applied;
    }
    staged.total = static_cast<std::int32_t>(cappedTotal);
    staged.average = projection.effectiveCap;
    staged.averageFloat = static_cast<float>(projection.effectiveCap);
    if (!detail::valid_summary(staged)) {
        return NativePowerApplyResult::invalid;
    }
    value = staged;
    return NativePowerApplyResult::applied;
}

} // namespace sunrise::state::activity::nightfall
