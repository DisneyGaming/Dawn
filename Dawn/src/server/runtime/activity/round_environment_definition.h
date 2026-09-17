#pragma once

#include "status_effect_service.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace dawn::server::runtime::activity::round_environment {

inline constexpr std::size_t kEffectCapacity = 16;
inline constexpr std::size_t kStageCapacity = 32;
inline constexpr std::size_t kForestSwitchCapacity = 16;

/** One host-policy modifier stage. The first stage is effective from round one. */
struct ModifierStage final {
    std::uint64_t firstRound{};
    std::uint32_t enabledMask{};
    std::int32_t entryHudVariant{};
};

/** One trusted native forest selector key/value pair. */
struct HashSwitch final {
    std::uint32_t key{};
    std::uint32_t value{};
};

/** Immutable authored inputs for the generic round environment service. */
struct Definition final {
    std::span<const status_effect::Capability> effects{};
    std::span<const ModifierStage> stages{};
    std::span<const HashSwitch> forestSwitches{};
};

[[nodiscard]] inline constexpr bool valid_hash_switch(const HashSwitch& value) noexcept {
    return value.key != 0 && value.key != (std::numeric_limits<std::uint32_t>::max)()
        && value.value != 0 && value.value != (std::numeric_limits<std::uint32_t>::max)();
}

[[nodiscard]] inline constexpr bool valid_mask(std::uint32_t mask,
    std::size_t effectCount) noexcept {
    if (effectCount > kEffectCapacity) return false;
    if (effectCount == 0) return mask == 0;
    const auto allowed = (std::uint32_t{1} << effectCount) - 1U;
    return (mask & ~allowed) == 0;
}

/**
 * Validates only bounded trusted data. It does not admit a registry or publish a
 * status effect; the service performs the owner-scoped admission transaction.
 */
[[nodiscard]] inline bool valid(const Definition& definition) noexcept {
    if (definition.effects.empty() || definition.effects.size() > kEffectCapacity
        || definition.stages.empty() || definition.stages.size() > kStageCapacity
        || definition.forestSwitches.size() > kForestSwitchCapacity) return false;

    for (std::size_t i = 0; i < definition.effects.size(); ++i) {
        if (!status_effect::valid(definition.effects[i])) return false;
        for (std::size_t j = 0; j < i; ++j) {
            const auto& prior = definition.effects[j];
            const auto& current = definition.effects[i];
            if (prior.registry->key == current.registry->key && prior.slot == current.slot)
                return false;
        }
    }

    std::uint64_t priorRound{};
    for (std::size_t i = 0; i < definition.stages.size(); ++i) {
        const auto& stage = definition.stages[i];
        if (stage.firstRound == 0 || (i != 0 && stage.firstRound <= priorRound)
            || !valid_mask(stage.enabledMask, definition.effects.size())) return false;
        priorRound = stage.firstRound;
    }

    for (std::size_t i = 0; i < definition.forestSwitches.size(); ++i) {
        if (!valid_hash_switch(definition.forestSwitches[i])) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (definition.forestSwitches[j].key == definition.forestSwitches[i].key) return false;
    }
    return true;
}

} // namespace dawn::server::runtime::activity::round_environment
