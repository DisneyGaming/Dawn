// Which authored combat objective each squad in the strike is costed against, and which of that
// objective's task groups it is allowed to take.
//
// A native squad has no behaviour of its own. Its actors are created by the spawner, but where
// they go, what they hold and how they move between firing areas is the authored objective's job,
// and the objective only drives a squad the host has assigned to it. The assignment is two
// separate things on the wire: a reference to the objective, and a task-group row selected from
// it. Publishing the reference with row -1 asks the native evaluator to cost every group for this
// squad without committing to one; it answers in the squad's own sense report, and the mission
// then republishes the cheapest group it is allowed to take. That is the whole loop, and it is
// what the accepted host build did on every squad-state report.
#pragma once
#include "catalog_all.h"
#include "../coo/task_costs.h"
namespace sunrise::state::activity::strike_pact {

/**
 * One squad's task costs, as raw seven-bit codes with a mask of the ones that have been reported.
 * Ordering by raw code is ordering by cost: the quantization is monotonic, so the host never needs
 * the dequantized route length, only which group is cheapest.
 *
 * The client reports this as a delta and nothing else ever restates it: a single report usually
 * carries one changed cost and omits the objective revision entirely. So this is retained state
 * that each report is merged into, never a message read on its own. Treating one report as the
 * whole picture selects from whichever group happened to change last and discards every report
 * that does not restate the revision, which is no selection at all.
 */
using TaskCosts = coo::TaskCosts;
/** The top code is the evaluator's own maximum, meaning the group cannot be reached from here. */
inline constexpr std::uint8_t kTaskUnreachable=127;

/** One section's objective. Revision is the assignment's own; the client reports it back beside
 * the costs and a report at any other revision belongs to an assignment we no longer own. */
struct TaskObjective final {
    std::uint32_t registry;
    std::uint16_t slot;
    std::uint8_t groups;
    std::uint32_t revision;
};
/** Every controller in the accepted build was constructed at revision one and never bumped it. */
inline constexpr std::uint32_t kTaskRevision=1;

inline constexpr TaskObjective kOpeningObjective{kOpening,kObjectiveSlot,9,kTaskRevision};
inline constexpr TaskObjective kForestObjective{kForest,kForestObjectiveSlot,kForestObjectiveGroups,kTaskRevision};
inline constexpr TaskObjective kChaseObjective{kChase,kChaseObjectiveSlot,kChaseTaskGroups,kTaskRevision};
inline constexpr TaskObjective kLedgeObjective{kLedge,kLedgeObjectiveSlot,kLedgeTaskGroups,kTaskRevision};
inline constexpr TaskObjective kBossObjective{kBoss,kBossObjectiveSlot,kBossObjectiveGroups,kTaskRevision};

/** The ledge and the boss share one registry and are told apart by their cohort. */
[[nodiscard]] constexpr const TaskObjective* task_objective(const Spawn& source) noexcept {
    if(source.registry==kOpening) { return &kOpeningObjective; }
    if(source.registry==kForest) { return &kForestObjective; }
    if(source.registry==kChase) { return &kChaseObjective; }
    if(source.registry==kLedge) {
        return source.cohort>=kBossCohortBase+1?&kBossObjective:&kLedgeObjective;
    }
    return nullptr;
}

/**
 * @return True when this source may be moved onto that task group.
 *
 * Each boss room's task groups resolve to firing areas inside that room alone, so a squad that
 * belongs to one room can never be sent to another room's group.
 */
[[nodiscard]] constexpr bool task_allowed(const Spawn& source,std::uint8_t group) noexcept {
    const auto* objective=task_objective(source);
    if(!objective || group>=objective->groups) { return false; }
    if(objective!=&kBossObjective) { return true; }
    const auto room=source.cohort==boss_cohort(kCohortRoom2)?2
        :source.cohort==boss_cohort(kCohortRoom3)?3:1;
    const auto* rows=boss_room(static_cast<std::uint8_t>(room));
    if(!rows) { return false; }
    for(const auto allowed:rows->groups) { if(allowed==group) { return true; } }
    return false;
}

/** Explicit initial assignments stay fixed. Valus Thuun changes rooms on his own health gates.
 * Other squads, including gateway defenders, need an assignment from the native evaluator:
 * a delivery creates actors but does not turn an evaluate-only row into a combat task. */
[[nodiscard]] constexpr bool task_fixed(const Spawn& source) noexcept {
    return source.tacticalRow>=0
        || (source.registry==kBoss && source.source==kBossSquad);
}

/** The row a source starts on, before any cost report. Minus one is the evaluate-only form. */
[[nodiscard]] constexpr std::int8_t task_initial(const Spawn& source) noexcept {
    if(source.tacticalRow>=0) { return source.tacticalRow; }
    // Thuun opens in the first room, and his fixed assignment is that room's arrival group.
    if(source.registry==kBoss && source.source==kBossSquad) {
        return static_cast<std::int8_t>(kBossRooms[0].arrivalGroup);
    }
    return -1;
}

static_assert([]{
    for(const auto& row:kAllSpawns) {
        const auto* objective=task_objective(row);
        if(!objective || objective->groups==0 || objective->groups>24) { return false; }
        const auto initial=task_initial(row);
        if(initial>=0 && !task_allowed(row,static_cast<std::uint8_t>(initial))) { return false; }
    }
    return true;
}(),"a strike source has no combat objective, or starts on a task group it may not take");

/** @return This source's row in the combined table, or its size when the pair names no source. */
[[nodiscard]] constexpr std::size_t all_spawn_index(std::uint32_t registry,std::uint16_t source) noexcept {
    for(std::size_t i=0;i<kAllSpawns.size();++i) {
        if(kAllSpawns[i].registry==registry && kAllSpawns[i].source==source) { return i; }
    }
    return kAllSpawns.size();
}

} // namespace sunrise::state::activity::strike_pact
