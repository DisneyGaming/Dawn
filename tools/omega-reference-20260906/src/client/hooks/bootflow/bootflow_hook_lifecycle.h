#pragma once

#include <array>
#include <cstdint>

namespace dawn::client::hooks::bootflow {

/**
 * Attaches the boot-step fixes that carry sign-in through to character select.
 * Each fix reports its own outcome, so a single miss never disables the others.
 * @return True when every fix attached.
 */
[[nodiscard]] bool install() noexcept;

/** Closes protected bootflow hook admission before their producers begin detaching. */
void quiesce() noexcept;

/** Detaches every boot-step fix, retaining live hooks when protected code is still active. */
[[nodiscard]] bool uninstall() noexcept;

/** @return True while at least one boot-step fix is attached. */
[[nodiscard]] bool is_installed() noexcept;

/** Publishes the client's own boot-flow step. Call it per frame, on a game thread. */
void poll_world_step() noexcept;

/**
 * Reports whether the player is in a loaded destination.
 * The step is published by a frame poll, so a tick that stops reads as out of world rather than
 * as the last step for ever.
 * @return True while the published step is `activity:in_world` and fresh.
 */
[[nodiscard]] bool in_world() noexcept;

/** Nearest live proximity anchor observed from the native mission-behaviour conditions. */
struct MissionTriggerSnapshot final {
    std::array<float, 3> playerPosition{};
    std::array<float, 3> anchorPosition{};
    std::array<float, 3> delta{};
    float distance{};
    float requiredMaximum{};
    std::uint32_t playerHandle{0xFFFFFFFFU};
    std::uint32_t anchorHandle{0xFFFFFFFFU};
    std::uint32_t anchorCount{};
    bool present{};
};

/**
 * Finds the closest anchor learned from the native proximity evaluator and compares it with the
 * local player's current position. This is navigation telemetry only; no object or condition is
 * changed.
 */
[[nodiscard]] MissionTriggerSnapshot mission_trigger_snapshot() noexcept;

} // namespace dawn::client::hooks::bootflow
