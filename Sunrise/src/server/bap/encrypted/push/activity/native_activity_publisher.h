#pragma once

#include "../../../region_lineage.h"
#include "native_roster_lifetime.h"
#include "../../../../runtime/activity/native_activity_definition.h"

namespace sunrise::server::bap::encrypted::push::activity::native_publisher {
namespace wire = middleware::bap::activity_message::sensor_auth_update;
using Definition = server::runtime::activity::NativeActivityDefinition;
enum class Role : std::uint8_t { invalid, creator, derived };

// A derived activity borrows region reports, not the creator's native objects.
// The caller also validates that this lineage still belongs to the live binding.
[[nodiscard]] constexpr Role role(state::activity::ActivityInstanceKey instance,
    const RegionLineage& lineage) noexcept {
    if (!instance || !lineage || lineage.bound != instance) return Role::invalid;
    if (lineage.owns(instance)) return Role::creator;
    return lineage.kind == RegionLineageKind::groupDerivedBorrow && lineage.source != instance
        ? Role::derived : Role::invalid;
}

[[nodiscard]] inline bool owns_key(const Definition& definition, std::uint32_t key) noexcept {
    for (const auto& registry : definition.registries) if (registry.key == key) return true;
    // Disabled options must not leave a second descriptor behind for later activation.
    for (const auto& binding : definition.optionalRegistries)
        if (binding.registry && binding.registry->key == key) return true;
    for (const auto& opening : definition.adventureOpenings) {
        if (opening.overlay && (opening.overlay->root.key == key || opening.overlay->local.key == key))
            return true;
        if (!opening.gateway) continue;
        const auto* region = opening.gateway->region;
        if (region && (region->root.key == key || region->local.key == key)) return true;
        for (const auto& endpoint : opening.gateway->endpoints)
            if (endpoint.registry && endpoint.registry->key == key) return true;
    }
    return false;
}

// Called on the base roster BEFORE descriptor admission and lifetime seeding.
// Filtering both phases is essential: an empty service batch would still emit
// generic auth bodies into a second native source with the same registry/slot.
// Preserve unrelated child/player groups and their order. Existing policy
// ordinals cannot be compacted safely; upgrading such a binding needs restart.
template<class Storage,class OwnsKey>
[[nodiscard]] Role prepare(state::activity::ActivityInstanceKey instance, const RegionLineage& lineage,
    const roster_lifetime::State& prior, Storage& storage, wire::Roster& roster, OwnsKey owns) noexcept {
    const auto selected = role(instance, lineage);
    if (selected != Role::derived) return selected;
    if (roster.groupCount > roster.groups.size() || roster.topLevelGroupCount > roster.groupCount
        || roster.bubbleSubBlocks.size() > storage.rosterSubBlocks.size()
        || roster.bubbleSubBlocks.size() > storage.rosterSubBlockKeys.size()
        || !roster.topLevelKeys.empty() || !roster.topLevelPresence.empty()
        || !roster.topLevelStates.empty() || owns(roster.playerKeyGroup)) return Role::invalid;
    if (prior.identity.owner) {
        if (roster_lifetime::validate(prior) != roster_lifetime::Result::ready
            || prior.identity.owner != instance.sessionId
            || prior.identity.incarnation != instance.incarnation.value) return Role::invalid;
        for (const auto key : roster_lifetime::view(prior.top).keys)
            if (owns(key)) return Role::invalid;
        for (std::size_t b = 0; b < prior.blockCount; ++b)
            for (const auto key : roster_lifetime::view(prior.blocks[b].entries).keys)
                if (owns(key)) return Role::invalid;
    }
    // All validation precedes mutation of the caller's scratch storage.
    for (const auto& block : roster.bubbleSubBlocks) {
        if (block.bubble > wire::kMaximumSubBlockBubble
            || block.keys.size() > wire::kBubbleKeyCapacity
            || block.keys.size() > storage.rosterSubBlockKeys[0].size()
            || !block.presence.empty() || !block.states.empty()) return Role::invalid;
    }
    wire::Roster candidate = roster;
    candidate.groupCount = candidate.topLevelGroupCount = 0;
    for (std::size_t g = 0; g < roster.groupCount; ++g) {
        if (owns(roster.groups[g].key)) continue;
        candidate.groups[candidate.groupCount++] = roster.groups[g];
        if (g < roster.topLevelGroupCount) ++candidate.topLevelGroupCount;
    }
    std::size_t count{};
    for (const auto& block : roster.bubbleSubBlocks) {
        auto& keys = storage.rosterSubBlockKeys[count];
        std::size_t kept{};
        for (const auto key : block.keys) if (!owns(key)) keys[kept++] = key;
        if (kept) storage.rosterSubBlocks[count++] = {block.bubble, std::span(keys).first(kept)};
    }
    candidate.bubbleSubBlocks = std::span(storage.rosterSubBlocks).first(count);
    roster = candidate;
    return selected;
}
template<class Storage>
[[nodiscard]] Role prepare(const Definition& definition,
    state::activity::ActivityInstanceKey instance, const RegionLineage& lineage,
    const roster_lifetime::State& prior, Storage& storage, wire::Roster& roster) noexcept {
    return prepare(instance,lineage,prior,storage,roster,
        [&definition](std::uint32_t key) noexcept {return owns_key(definition,key);});
}
}
