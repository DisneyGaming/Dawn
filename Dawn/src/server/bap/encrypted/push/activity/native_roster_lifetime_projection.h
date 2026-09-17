#pragma once
#include "native_roster_lifetime.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"

namespace dawn::server::bap::encrypted::push::activity::roster_lifetime {
namespace wire = middleware::bap::activity_message::sensor_auth_update;

// Adopt only the exact warm-up publication; later requests preserve omitted
// ordinals. Both results are candidates until the enclosing BAP delivery commits.
[[nodiscard]] inline Result prepare(const State& prior, const Identity& identity,
    std::uint32_t bubble, std::uint8_t stateByte, bool warmup,
    const wire::Roster& roster, State& output) noexcept {
    if (roster.topLevelGroupCount > roster.groupCount || roster.groupCount > wire::kGroupCapacity
        || roster.bubbleSubBlocks.size() > kBlockCapacity) return Result::invalidView;
    std::array<std::uint32_t, wire::kGroupCapacity> top{};
    for (std::size_t i = 0; i < roster.topLevelGroupCount; ++i) top[i] = roster.groups[i].key;
    const auto topKeys = std::span(top).first(roster.topLevelGroupCount);
    if (warmup) {
        std::array<std::uint8_t, kTopCapacity> presence{}, states{};
        presence.fill(1); states.fill(stateByte);
        std::array<WireBlock, kBlockCapacity> blocks{};
        for (std::size_t i = 0; i < roster.bubbleSubBlocks.size(); ++i) {
            const auto& block = roster.bubbleSubBlocks[i];
            if (block.keys.size() > kBlockKeyCapacity) return Result::capacity;
            blocks[i] = {block.bubble, {block.keys,
                block.presence.empty() ? std::span<const std::uint8_t>(presence).first(block.keys.size()) : block.presence,
                block.states.empty() ? std::span<const std::uint8_t>(states).first(block.keys.size()) : block.states}};
        }
        return seed(identity, {topKeys, std::span(presence).first(topKeys.size()),
            std::span(states).first(topKeys.size())},
            std::span(blocks).first(roster.bubbleSubBlocks.size()), output);
    }
    std::array<DesiredBlock, kBlockCapacity> blocks{};
    for (std::size_t i = 0; i < roster.bubbleSubBlocks.size(); ++i) {
        const auto& block = roster.bubbleSubBlocks[i];
        blocks[i] = {block.bubble, {block.keys, block.presence}};
    }
    return plan(prior, {identity, bubble, stateByte, {topKeys, {}},
        std::span(blocks).first(roster.bubbleSubBlocks.size())}, output);
}

// Rebind after copying a detached candidate: no span may outlive its value owner.
[[nodiscard]] inline bool project(const State& state, wire::Roster& roster,
    std::span<wire::BubbleSubBlock> blocks) noexcept {
    if (state.top.count > kTopCapacity || state.blockCount > blocks.size()) return false;
    const auto top = view(state.top);
    roster.topLevelKeys = top.keys;
    roster.topLevelPresence = top.presence;
    roster.topLevelStates = top.states;
    for (std::size_t i = 0; i < state.blockCount; ++i) {
        if (state.blocks[i].entries.count > kBlockKeyCapacity) return false;
        const auto list = view(state.blocks[i].entries);
        blocks[i] = {state.blocks[i].bubble, list.keys, list.presence, list.states};
    }
    roster.bubbleSubBlocks = blocks.first(state.blockCount);
    return true;
}
}
