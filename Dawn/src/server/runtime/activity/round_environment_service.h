#pragma once

#include "round_environment_definition.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace dawn::server::runtime::activity::round_environment {

namespace wire = middleware::bap::activity_message::native::status_effect;
using Owner = state::activity::ActivityInstanceKey;

enum class Result : std::uint8_t {
    accepted,
    unchanged,
    stale,
    notStarted,
    released,
    invalid,
    exhausted,
};

/**
 * Generic host lifecycle for a round's environment modifiers.
 *
 * The stored definition is a value-only view of immutable mission data. Native
 * admission and wire encoding remain in status_effect::Service and its shared
 * status-effect authority; this class only schedules owner-scoped requests.
 */
class Service final {
public:
    [[nodiscard]] bool begin(Owner owner, std::uint64_t boot,
        const Definition& definition) noexcept {
        if (started() || !owner || !boot || !valid(definition)) return false;

        status_effect::Service admitted;
        if (!admitted.begin(owner, boot, definition.effects)) return false;

        owner_ = owner;
        boot_ = boot;
        definition_ = definition;
        effects_ = admitted;
        requestCounter_ = 0;
        currentRound_ = 0;
        entryHudVariant_ = 0;
        released_ = false;
        return true;
    }

    [[nodiscard]] bool started() const noexcept { return static_cast<bool>(owner_); }
    [[nodiscard]] bool released() const noexcept { return released_; }
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    [[nodiscard]] std::uint64_t current_round() const noexcept { return currentRound_; }
    [[nodiscard]] std::int32_t entryHudVariant() const noexcept { return entryHudVariant_; }
    [[nodiscard]] std::int32_t entry_hud_variant() const noexcept { return entryHudVariant(); }
    [[nodiscard]] const Definition& definition() const noexcept { return definition_; }
    [[nodiscard]] std::span<const HashSwitch> forestSwitches() const noexcept {
        return started() ? definition_.forestSwitches : std::span<const HashSwitch>{};
    }
    [[nodiscard]] std::span<const HashSwitch> forest_switches() const noexcept {
        return forestSwitches();
    }

    /** Applies the last authored stage whose floor is at or below round. */
    [[nodiscard]] Result apply_round(std::uint64_t round) noexcept {
        if (!started()) return Result::notStarted;
        if (released_) return Result::released;
        if (!round) return Result::invalid;
        if (currentRound_ == round) return Result::unchanged;
        if (currentRound_ && round < currentRound_) return Result::stale;

        const auto* stage = stage_for(round);
        if (!stage) return Result::invalid;

        auto candidate = effects_;
        auto nextRequest = requestCounter_;
        for (std::size_t i = 0; i < definition_.effects.size(); ++i) {
            if (nextRequest == (std::numeric_limits<std::uint64_t>::max)())
                return Result::exhausted;
            const bool enabled = (stage->enabledMask & (std::uint32_t{1} << i)) != 0;
            const auto result = candidate.request(owner_, boot_, candidate.revision(),
                ++nextRequest, i, enabled);
            if (result != status_effect::Result::accepted
                && result != status_effect::Result::unchanged)
                return Result::exhausted;
        }

        effects_ = candidate;
        requestCounter_ = nextRequest;
        currentRound_ = round;
        entryHudVariant_ = stage->entryHudVariant;
        return Result::accepted;
    }

    /**
     * Turns every owned effect off but keeps the owner and inactive wire records
     * available until release_owner() is called by the final lifecycle owner.
     */
    [[nodiscard]] Result release() noexcept {
        if (!started()) return Result::notStarted;
        if (released_) return Result::unchanged;

        auto candidate = effects_;
        auto nextRequest = requestCounter_;
        for (std::size_t i = 0; i < definition_.effects.size(); ++i) {
            if (nextRequest == (std::numeric_limits<std::uint64_t>::max)())
                return Result::exhausted;
            const auto result = candidate.request(owner_, boot_, candidate.revision(),
                ++nextRequest, i, false);
            if (result != status_effect::Result::accepted
                && result != status_effect::Result::unchanged)
                return Result::exhausted;
        }
        effects_ = candidate;
        requestCounter_ = nextRequest;
        released_ = true;
        return Result::accepted;
    }

    /** Final owner release. Inactive records are intentionally retired here. */
    void release_owner() noexcept { *this = {}; }
    void reset() noexcept { release_owner(); }

    /** Projects the existing status-effect Batch for one admitted bubble. */
    [[nodiscard]] wire::Batch project(std::uint8_t bubble) const noexcept {
        return effects_.project(bubble);
    }

    /** Projects all owned status-effect records without introducing a new wire type. */
    [[nodiscard]] wire::Batch project() const noexcept {
        wire::Batch output{};
        for (unsigned bubble = 0; bubble <= 63 && output.count < output.entries.size(); ++bubble) {
            const auto batch = project(static_cast<std::uint8_t>(bubble));
            for (std::size_t i = 0; i < batch.count && output.count < output.entries.size(); ++i)
                output.entries[output.count++] = batch.entries[i];
        }
        return output;
    }

    [[nodiscard]] wire::Batch publication() const noexcept { return project(); }
    [[nodiscard]] wire::Batch publication(std::uint8_t bubble) const noexcept {
        return project(bubble);
    }
    [[nodiscard]] const status_effect::Service& effect_service() const noexcept { return effects_; }

private:
    [[nodiscard]] const ModifierStage* stage_for(std::uint64_t round) const noexcept {
        const ModifierStage* selected{};
        for (const auto& stage : definition_.stages) {
            if (stage.firstRound > round) break;
            selected = &stage;
        }
        return selected;
    }

    Definition definition_{};
    status_effect::Service effects_{};
    Owner owner_{};
    std::uint64_t boot_{};
    std::uint64_t requestCounter_{};
    std::uint64_t currentRound_{};
    std::int32_t entryHudVariant_{};
    bool released_{};
};

static_assert(std::is_trivially_copyable_v<Service>);

} // namespace dawn::server::runtime::activity::round_environment
