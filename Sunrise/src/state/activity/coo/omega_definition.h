#pragma once
#include "mission_runtime.h"

namespace sunrise::state::activity::coo::omega {
// Logical plugin identities, not native registry/type/slot footprints.
inline constexpr std::array<ModuleBinding, 3> kModules{{
    {{0, 1, 0, 0}, 0}, {{0, 2, 0, 0}, 1}, {{0, 3, 0, 0}, 2}
}};
inline constexpr std::array<CommandSpec, 3> kControllers{{
    {Operation::mechanic, kModules[0].asset, kModules[0].id, Wait::requested},
    {Operation::mechanic, kModules[1].asset, kModules[1].id, Wait::requested},
    {Operation::mechanic, kModules[2].asset, kModules[2].id, Wait::requested}
}};
inline constexpr std::array<CommandSpec, 8> kMilestones{{
    {Operation::observation, {}, 0, Wait::observed},
    {Operation::observation, {}, 1, Wait::observed},
    {Operation::observation, {}, 2, Wait::observed},
    {Operation::observation, {}, 3, Wait::observed},
    {Operation::observation, {}, 4, Wait::observed},
    {Operation::observation, {}, 5, Wait::observed},
    {Operation::observation, {}, 6, Wait::observed},
    {Operation::observation, {}, 7, Wait::observed}
}};
inline constexpr std::array<Step, 9> kSteps{{
    {"mission services", 0, kControllers},
    {"opening and Ikora", 1U, {&kMilestones[0], 1}},
    {"Forest traversal", 2U, {&kMilestones[1], 1}},
    {"Lair arrival and encounters", 4U, {&kMilestones[2], 1}},
    {"Crown cycle 1", 8U, {&kMilestones[3], 1}},
    {"Crown cycle 2", 16U, {&kMilestones[4], 1}},
    {"Crown cycle 3", 32U, {&kMilestones[5], 1}},
    {"ending cinematic", 64U, {&kMilestones[6], 1}},
    {"handoff queued", 128U, {&kMilestones[7], 1}}
}};
inline constexpr std::array<ObservationBinding, 8> kObservations{{
    {0, 1, 0}, {1, 2, 0}, {2, 3, 0}, {3, 4, 0},
    {4, 5, 0}, {5, 6, 0}, {6, 7, 0}, {7, 8, 0}
}};
inline constexpr MissionDefinition kMission{{"Omega", Schema::omegaArchive, kSteps}, kModules, kObservations};
} // namespace sunrise::state::activity::coo::omega
