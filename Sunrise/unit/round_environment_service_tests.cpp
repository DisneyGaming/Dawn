#include "server/runtime/activity/haunted_forest_environment.h"
#include "server/runtime/activity/round_environment_service.h"
#include "middleware/bap/activity_message/native/status_effect_authority.h"
#include "middleware/encoding/bit_writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace environment = sunrise::server::runtime::activity::round_environment;
namespace haunted = sunrise::server::runtime::activity::haunted_forest::environment;
namespace registry = sunrise::state::activity::coo::registry;
namespace status = sunrise::server::runtime::activity::status_effect;
namespace wire = sunrise::middleware::bap::activity_message::native::status_effect;
namespace bits = sunrise::middleware::encoding::bits;
using Owner = sunrise::state::activity::ActivityInstanceKey;

constexpr Owner kOwner{91, {4}};
constexpr std::uint64_t kBoot = 27;
constexpr std::uint32_t kRegistryKey = 0x34D23982U;
constexpr std::uint8_t kBubble = 13;
unsigned checks{};

void expect(bool value, const char* expression) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "failed check %u: %s\n", checks, expression);
        std::exit(1);
    }
}

#define EXPECT(value) expect(static_cast<bool>(value), #value)

struct StartFixture final {
    std::array<registry::Slot, 5> slots{{
        {47, 26, 0x8080953FU, 0x8080954AU, 0x8080954BU, 0x815500D6U},
        {48, 26, 0x8080953FU, 0x8080954AU, 0x8080954BU, 0x815500D9U},
        {49, 26, 0x8080953FU, 0x8080954AU, 0x8080954BU, 0x815500DCU},
        {50, 26, 0x8080953FU, 0x8080954AU, 0x8080954BU, 0x815500DFU},
        {51, 26, 0x8080953FU, 0x8080954AU, 0x8080954BU, 0x815500E2U},
    }};
    registry::Definition definition{
        "infinite_abyss", 0x81550015U, kRegistryKey, 0x34D23982U,
        0x81550188U, kBubble, slots};
};

void wire_publication_preserves_active_and_inactive_widths(
    const wire::Batch& batch) {
    EXPECT(batch.count == 4);
    for (const auto& request : std::span<const wire::Request>(batch.entries).first(batch.count)) {
        std::array<std::byte, 40> bytes{};
        bits::Writer writer(bytes);
        EXPECT(wire::write_authority(writer, request));
        EXPECT(writer.bit_count() == (request.enabled ? wire::kActiveBits : wire::kInactiveBits));
    }
}

void haunted_data_uses_the_admitted_registry_and_excludes_lost() {
    StartFixture fixture;
    auto data = haunted::data_factory(&fixture.definition);
    const auto definition = data.definition();

    EXPECT(definition.effects.size() == 4);
    EXPECT(definition.stages.size() == 9);
    EXPECT(definition.forestSwitches.size() == 6);
    for (std::size_t i = 0; i < definition.effects.size(); ++i) {
        EXPECT(definition.effects[i].registry == &fixture.definition);
        EXPECT(definition.effects[i].slot == haunted::kEffectSlots[i]);
        EXPECT(definition.effects[i].slot != haunted::kLostSlot);
    }
    EXPECT(definition.effects[0].slot == 47 && definition.effects[1].slot == 48
        && definition.effects[2].slot == 50 && definition.effects[3].slot == 51);
    EXPECT(definition.forestSwitches[0].key == haunted::kForestVexKey);
    EXPECT(definition.forestSwitches[1].key == haunted::kForestCabalKey);
    EXPECT(definition.forestSwitches[2].key == haunted::kForestHiveKey);
    EXPECT(definition.forestSwitches[3].key == haunted::kForestFallenKey);
    EXPECT(definition.forestSwitches[0].value == haunted::kForestPopulationSelector);
    EXPECT(definition.forestSwitches[1].value == haunted::kForestPopulationSelector);
    EXPECT(definition.forestSwitches[2].value == haunted::kForestPopulationSelector);
    EXPECT(definition.forestSwitches[3].value == haunted::kForestPopulationSelector);
    EXPECT(definition.forestSwitches[4].key == 0x045DC993U);
    EXPECT(definition.forestSwitches[4].value == 0xFE1640D2U);
    EXPECT(definition.forestSwitches[5].key == 0x98CBCE18U);
    EXPECT(definition.forestSwitches[5].value == 0xB955F8A7U);
}

void all_nine_branches_select_the_floor_and_turn_previous_effects_off() {
    StartFixture fixture;
    auto data = haunted::data_factory(&fixture.definition);
    const auto definition = data.definition();
    environment::Service service;

    EXPECT(service.project().count == 0);
    EXPECT(service.begin(kOwner, kBoot, definition));
    EXPECT(service.project().count == 0);

    constexpr std::array<std::uint32_t, 9> masks{
        0, 1, 2, 4, 8, 9, 10, 12, 15};
    for (std::uint64_t round = 1; round <= masks.size(); ++round) {
        EXPECT(service.apply_round(round) == environment::Result::accepted);
        const auto batch = service.project(kBubble);
        EXPECT(batch.count == 4);
        for (std::size_t i = 0; i < batch.count; ++i)
            EXPECT(batch.entries[i].enabled == ((masks[round - 1] & (1U << i)) != 0));
        EXPECT(service.entryHudVariant() == static_cast<std::int32_t>(round - 1));
        wire_publication_preserves_active_and_inactive_widths(batch);
    }

    // The last authored floor owns every later round.
    EXPECT(service.apply_round(10) == environment::Result::accepted);
    EXPECT(service.entryHudVariant() == 8);
    const auto allOn = service.project(kBubble);
    EXPECT(allOn.count == 4);
    for (std::size_t i = 0; i < allOn.count; ++i) EXPECT(allOn.entries[i].enabled);

    const auto beforeSame = service.effect_service().snapshot();
    EXPECT(service.apply_round(10) == environment::Result::unchanged);
    const auto afterSame = service.effect_service().snapshot();
    EXPECT(beforeSame.revision == afterSame.revision
        && beforeSame.lastRequest == afterSame.lastRequest);

    const auto beforeStale = service.project(kBubble);
    const auto beforeStaleSnapshot = service.effect_service().snapshot();
    EXPECT(service.apply_round(9) == environment::Result::stale);
    const auto afterStale = service.project(kBubble);
    const auto afterStaleSnapshot = service.effect_service().snapshot();
    EXPECT(beforeStaleSnapshot.revision == afterStaleSnapshot.revision
        && beforeStaleSnapshot.lastRequest == afterStaleSnapshot.lastRequest);
    for (std::size_t i = 0; i < beforeStale.count; ++i)
        EXPECT(beforeStale.entries[i].enabled == afterStale.entries[i].enabled);
}

void release_turns_every_effect_off_and_retains_the_off_projection() {
    StartFixture fixture;
    auto data = haunted::data_factory(&fixture.definition);
    environment::Service service;
    EXPECT(service.begin(kOwner, kBoot, data.definition()));
    EXPECT(service.apply_round(9) == environment::Result::accepted);
    EXPECT(service.release() == environment::Result::accepted);
    EXPECT(service.released());
    const auto off = service.project(kBubble);
    EXPECT(off.count == 4);
    for (const auto& request : std::span<const wire::Request>(off.entries).first(off.count)) {
        EXPECT(!request.enabled);
        std::array<std::byte, 24> bytes{};
        bits::Writer writer(bytes);
        EXPECT(wire::write_authority(writer, request));
        EXPECT(writer.bit_count() == wire::kInactiveBits);
    }
    EXPECT(service.release() == environment::Result::unchanged);
    EXPECT(service.apply_round(10) == environment::Result::released);
    service.release_owner();
    EXPECT(!service.started() && service.project().count == 0);
}

void invalid_data_is_rejected_before_status_effect_admission() {
    StartFixture fixture;
    auto data = haunted::data_factory(&fixture.definition);
    const auto good = data.definition();

    auto check_invalid = [&](environment::Definition candidate) {
        environment::Service service;
        EXPECT(!environment::valid(candidate));
        EXPECT(!service.begin(kOwner, kBoot, candidate));
        EXPECT(service.project().count == 0);
    };

    auto badStages = std::array<environment::ModifierStage, 1>{{{0, 0, 0}}};
    check_invalid({good.effects, badStages, good.forestSwitches});

    auto badMask = std::array<environment::ModifierStage, 1>{{{1, 1U << 4, 0}}};
    check_invalid({good.effects, badMask, good.forestSwitches});

    auto duplicateStages = std::array<environment::ModifierStage, 2>{{
        {1, 0, 0}, {1, 1, 1}}};
    check_invalid({good.effects, duplicateStages, good.forestSwitches});

    auto duplicateSwitches = std::array<environment::HashSwitch, 6>{
        good.forestSwitches[0], good.forestSwitches[1],
        good.forestSwitches[2], good.forestSwitches[3],
        good.forestSwitches[4], good.forestSwitches[5]};
    duplicateSwitches[1].key = duplicateSwitches[0].key;
    check_invalid({good.effects, good.stages, duplicateSwitches});

    auto zeroSwitch = duplicateSwitches;
    zeroSwitch[1] = good.forestSwitches[1];
    zeroSwitch[0].key = 0;
    check_invalid({good.effects, good.stages, zeroSwitch});

    auto badSlots = fixture.slots;
    badSlots[0].type = 25;
    const registry::Definition badRegistry{
        "infinite_abyss", 0x81550015U, kRegistryKey, 0x34D23982U,
        0x81550188U, kBubble, badSlots};
    auto badData = haunted::data_factory(&badRegistry);
    check_invalid(badData.definition());
}

void fixed_bounds_are_enforced() {
    StartFixture fixture;
    auto data = haunted::data_factory(&fixture.definition);
    const auto good = data.definition();

    std::array<status::Capability, 17> effects{};
    std::array<registry::Slot, 17> slots{};
    for (std::size_t i = 0; i < slots.size(); ++i) {
        slots[i] = {static_cast<std::uint16_t>(100 + i), 26, 0x8080953FU,
            0x8080954AU, 0x8080954BU, static_cast<std::uint32_t>(0x90000000U + i)};
        effects[i] = {&fixture.definition, static_cast<std::uint16_t>(100 + i)};
    }
    const registry::Definition largeRegistry{
        "large", 1, 2, 3, 4, 1, slots};
    for (auto& effect : effects) effect.registry = &largeRegistry;
    {
        environment::Definition candidate{effects, good.stages, good.forestSwitches};
        EXPECT(!environment::valid(candidate));
    }

    std::array<environment::ModifierStage, 33> stages{};
    for (std::size_t i = 0; i < stages.size(); ++i) stages[i] = {i + 1, 0, 0};
    EXPECT(!environment::valid({good.effects, stages, good.forestSwitches}));

    std::array<environment::HashSwitch, 17> switches{};
    for (std::size_t i = 0; i < switches.size(); ++i)
        switches[i] = {static_cast<std::uint32_t>(0x1000U + i), 0x050C5D2EU};
    EXPECT(!environment::valid({good.effects, good.stages, switches}));
}

void no_effects_are_published_before_begin_or_apply() {
    StartFixture fixture;
    auto data = haunted::data_factory(&fixture.definition);
    environment::Service service;
    EXPECT(service.project().count == 0);
    EXPECT(service.begin(kOwner, kBoot, data.definition()));
    EXPECT(service.project().count == 0);
    EXPECT(service.apply_round(0) == environment::Result::invalid);
    EXPECT(service.project().count == 0);
}

int main() {
    haunted_data_uses_the_admitted_registry_and_excludes_lost();
    all_nine_branches_select_the_floor_and_turn_previous_effects_off();
    release_turns_every_effect_off_and_retains_the_off_projection();
    invalid_data_is_rejected_before_status_effect_admission();
    fixed_bounds_are_enforced();
    no_effects_are_published_before_begin_or_apply();
    std::printf("round environment service: %u checks, zero failures\n", checks);
}
