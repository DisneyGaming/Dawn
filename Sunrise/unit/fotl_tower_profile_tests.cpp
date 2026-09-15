#include "../src/server/runtime/activity/city_tower_social_d2_definition.h"
#include "../src/server/runtime/activity/native_activity_profiles.h"
#include "../src/server/runtime/activity/persistent_activity.h"
#include "../src/server/runtime/activity/equipment_interaction_gate.h"
#include "../src/state/account/festival_mask.h"
#include "../src/state/activity/coo/mission_script.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

namespace activity = sunrise::server::runtime::activity;
namespace tower = activity::city_tower_social_d2;
namespace coo = sunrise::state::activity::coo;

namespace {

bool check(bool value, const char* expression, int line) noexcept {
    if (!value) std::fprintf(stderr, "FAIL line %d: %s\n", line, expression);
    return value;
}

} // namespace

#define CHECK(value) do { if (!check(static_cast<bool>(value), #value, __LINE__)) return 1; } while (false)

int main(int argc, char** argv) {
    const std::filesystem::path script = argc > 1
        ? std::filesystem::path(argv[1])
        : std::filesystem::path("Sunrise/scripts/city_tower_social_d2.json");

    CHECK(tower::kRegistries.size() == 1);
    const auto& registry = tower::kRegistries[0];
    CHECK(registry.activity == "city_tower_social_d2");
    CHECK(registry.scenario == 0x80B4A0F4U);
    CHECK(registry.key == 0x7C6DE64FU);
    CHECK(registry.objectTag == 0x80B4AF5EU);
    CHECK(registry.bubble == 6);
    CHECK(registry.bubbleHash == 0xAB28899EU);
    CHECK(registry.slots.size() == 4);
    CHECK(tower::trusted::kComponentOffsets[0] == 0x878);
    CHECK(tower::trusted::kComponentOffsets[1] == 0x4C8);
    CHECK(tower::trusted::kComponentOffsets[2] == 0x358);
    CHECK(tower::trusted::kComponentOffsets[3] == 0x228);

    const auto slot_matches = [](const coo::registry::Slot& actual,
        std::uint16_t index, std::uint8_t type, std::uint32_t component,
        std::uint32_t sense, std::uint32_t auth, std::uint32_t descriptor) {
        return actual.index == index && actual.type == type
            && actual.componentClass == component && actual.senseSchema == sense
            && actual.authSchema == auth && actual.descriptorTag == descriptor;
    };
    CHECK(slot_matches(registry.slots[0], 0, 1, 0x80809A3B, 0x80807ECC, 0x80807EC9, 0x80B4AF52));
    CHECK(slot_matches(registry.slots[1], 1, 4, 0x80809927, 0x8080992E, 0x8080992F, 0x80B4AF55));
    CHECK(slot_matches(registry.slots[2], 2, 70, 0x808094EE, 0x808094F0, 0x808094F1, 0x80B4AF58));
    CHECK(slot_matches(registry.slots[3], 3, 42, 0x80809583, UINT32_MAX, 0x80809586, 0x80B4AF5B));
    for (const auto& slot : registry.slots) CHECK(slot.type != 66 && slot.index != 4);

    CHECK(activity::registry::valid(registry));
    CHECK(tower::kPopulations.size() == 1);
    CHECK(activity::population::valid(tower::kPopulations[0]));
    CHECK(!tower::kPopulations[0].hasRule);
    CHECK(tower::kPopulations[0].rule == 0);
    CHECK(tower::kPopulations[0].tactical.row == -1);
    CHECK(tower::kPlacements.size() == 1);
    CHECK(tower::kPlacements[0].slot == 1);
    CHECK(tower::kPlacements[0].generation == 1);
    CHECK(tower::kPlacements[0].interactionMode
        == activity::placement::interaction::Mode::unchanged);
    CHECK(tower::kEquipmentInteractionGates.size() == 1);
    CHECK(tower::kEquipmentInteractionGates[0].registry == registry.key);
    CHECK(tower::kEquipmentInteractionGates[0].slot == 1);
    CHECK(tower::kEquipmentInteractionGates[0].equipmentSlot
        == sunrise::state::account::inventory::EquipmentSlot::helmet);
    CHECK(tower::kEquipmentInteractionGates[0].allowedDefinitionHashes.size() == 3);
    CHECK(tower::kEvaAnimationCapabilities.size() == 1);
    CHECK(tower::kEvaAnimationCapabilities[0].slot == 3);
    CHECK(tower::kEvaAnimationActions[0].id == 1);
    CHECK(tower::kEvaAnimationActions[0].sequence == 0xCAEB4FC0U);
    CHECK(tower::kEvaAnimationActions[0].completion == 0x811C9DC5U);
    CHECK(tower::kPlacements[0].interactionMode == activity::placement::interaction::Mode::unchanged);
    CHECK(tower::kActions[2].command.asset.registry == registry.key);
    CHECK(tower::kActions[2].command.asset.slot == 1);
    CHECK(tower::kActions[2].command.asset.type == 4);

    CHECK(activity::native_activity_profile("city_tower_social_d2") == &tower::kActivity);
    CHECK(activity::native_activity_profile("city_tower_social_d2", 1) == &tower::kActivity);

    std::string error;
    auto parsed = coo::script::MissionDocument::read_native_policy(script, tower::kProfile, error);
    if (!parsed) std::fprintf(stderr, "MissionDocument rejected: %s\n", error.c_str());
    CHECK(parsed);
    CHECK(activity::PersistentActivity::valid(tower::kActivity, *parsed));
    std::shared_ptr<const coo::script::MissionDocument> document(std::move(parsed));

    const activity::population::Owner owner{91, {4}};
    activity::PersistentActivity persistent;
    CHECK(persistent.begin(owner, tower::kActivity, document, 73));

    const auto preArrival = persistent.update(7, false);
    CHECK(preArrival.populations.count == 0);
    CHECK(preArrival.animations.count == 0);
    CHECK(preArrival.placements.count == 0);

    const auto first = persistent.update(7, true);
    CHECK(first.populations.count == 1);
    CHECK(first.populations.entries[0].source.registry == registry.key);
    CHECK(first.populations.entries[0].slot == 0);
    CHECK(first.populations.entries[0].source.looseRequested == 1);
    CHECK(!first.populations.entries[0].source.hasSpawnRule);
    CHECK(first.populations.entries[0].source.ruleSlot == 0);
    CHECK(first.populations.entries[0].source.tactical.row == -1);

    CHECK(first.animations.count == 1);
    CHECK(first.animations.entries[0].registry == registry.key);
    CHECK(first.animations.entries[0].slot == 3);
    CHECK(first.animations.entries[0].control.sequence == 0xCAEB4FC0U);
    CHECK(first.animations.entries[0].control.completion == 0x811C9DC5U);
    CHECK(first.animations.entries[0].control.counter == 1);

    CHECK(first.placements.count == 1);
    CHECK(first.placements.entries[0].registry == registry.key);
    CHECK(first.placements.entries[0].slot == 1);
    CHECK(first.placements.entries[0].generation == 1);
    CHECK(first.placements.entries[0].interactionMode
        == activity::placement::interaction::Mode::unchanged);

    const auto populationRevision = persistent.population().revision();
    const auto animationRevision = persistent.animation().revision();
    const auto second = persistent.update(6, true);
    CHECK(second.populations.count == 1 && second.animations.count == 1 && second.placements.count == 1);
    CHECK(second.populations.entries[0].source.generation == first.populations.entries[0].source.generation);
    CHECK(second.animations.entries[0].control == first.animations.entries[0].control);
    CHECK(second.placements.entries[0].generation == first.placements.entries[0].generation);
    CHECK(persistent.population().revision() == populationRevision);
    CHECK(persistent.animation().revision() == animationRevision);

    const auto third = persistent.update(1, true);
    CHECK(third.populations.count == 1 && third.animations.count == 1 && third.placements.count == 1);
    CHECK(third.populations.entries[0].source.generation == first.populations.entries[0].source.generation);
    CHECK(third.animations.entries[0].control == first.animations.entries[0].control);
    CHECK(third.placements.entries[0].generation == first.placements.entries[0].generation);
    CHECK(persistent.population().revision() == populationRevision);
    CHECK(persistent.animation().revision() == animationRevision);

    auto authoredBubbleOnly = tower::kActivity;
    authoredBubbleOnly.activateAcrossRegions = false;
    activity::PersistentActivity gated;
    CHECK(gated.begin(owner, authoredBubbleOnly, document, 74));
    const auto gatedRegion = gated.update(7, true);
    CHECK(gatedRegion.populations.count == 0);
    CHECK(gatedRegion.animations.count == 0);
    CHECK(gatedRegion.placements.count == 0);
    const auto gatedAuthored = gated.update(tower::kActivity.bubble, true);
    CHECK(gatedAuthored.populations.count == 1);
    CHECK(gatedAuthored.animations.count == 1);
    CHECK(gatedAuthored.placements.count == 1);

    std::printf("PASS: Tower FoTL native profile, no-rule Eva source, stable idle, and one-shot launch projection\n");
    return 0;
}
