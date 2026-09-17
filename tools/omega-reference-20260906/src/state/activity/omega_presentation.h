#pragma once

#include "omega_presentation_rules.h"

namespace dawn::state::activity::omega_presentation {

/** Called only for the committed local Omega roster after its in-world seed latch. */
[[nodiscard]] Presentation snapshot(std::uint64_t run, std::uint64_t now,
                                    int arrivalRegion, bool entrance) noexcept;
/** Game-thread position samples; no native pointers are retained. Other activities are ignored. */
void observe_position(Point position) noexcept;
/** Native type-53 dispatch receipt. Audio itself remains owned by Destiny. */
void observe_submission(std::uint32_t bank, std::uint8_t row,
                        std::uint32_t generation) noexcept;
/** Native scene apply receipt, matched against the exact authored component definition. */
void observe_scene(std::uint32_t definition, bool active) noexcept;
/** Type-6 authority receipt and native cinematic registration readiness. */
void observe_intro(std::uint32_t revision, bool active, bool resourceReady) noexcept;
/** Fully validated native member/character receipt. Returns true only for the
 * current requested native generation, independent of diagnostic factory hooks. */
bool observe_boss(std::uint64_t run, std::uint32_t definition, std::uint32_t object,
                  std::uint32_t nativeGeneration) noexcept;
/** Claim once, after validating a live native member, while the entry reveal is priming. */
[[nodiscard]] bool claim_boss_intro_action(std::uint64_t run) noexcept;
/** Fully validated owned graph0/node1 flight receipt; releases the primed camera
 * on its next native readiness observation. Independent of the later summon event. */
[[nodiscard]] bool observe_boss_flight(std::uint64_t run, std::uint32_t nativeGeneration) noexcept;
struct Navigation final {
    std::uint64_t run{};
    Landmark landmark{};
    bool enabled{}, forestComplete{};
    NavigationGoal goal{};
};
/** The completed route remains retired across streaming and backtracking until the next run. */
[[nodiscard]] Navigation navigation() noexcept;
/**
 * Encounter-controller boundary for the remaining boss mechanics. The producer supplies the
 * current mission generation and an observed phase/cycle; duplicate phase reports are harmless.
 * This API does not create the encounter or infer a kill/charge/eye state from player position.
 */
void note_encounter(std::uint64_t run, Encounter event, std::uint8_t cycle) noexcept;
/** True only after the final Osiris line's accepted native submission window. */
[[nodiscard]] bool ending_dialogue_finished(std::uint64_t run, std::uint64_t now) noexcept;
/** Fast periodic publication while a line or objective has changed; no roster reconstruction. */
[[nodiscard]] bool publication_due(std::uint64_t now) noexcept;
/** Clears admission at orbit/teardown. */
void reset() noexcept;

} // namespace dawn::state::activity::omega_presentation
