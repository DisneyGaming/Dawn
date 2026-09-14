#include "state/activity/nightfall/native_power.h"
#include "state/activity/coo/native_combatant_authority.h"
#include "state/activity/strike_bond/catalog.h"
#include "state/activity/strike_pact/catalog_all.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/datagen/family4/character/character_encoder.h"
#include "middleware/datagen/family4/character/layout.h"
#include "middleware/datagen/family4/progression/progression_bank_keys.h"
#include "state/unlocks/unlocks_runtime.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace nightfall = sunrise::state::activity::nightfall;
namespace contest = sunrise::state::activity::eater_of_worlds::contest;
namespace light = sunrise::state::equipment::light;
namespace combatant = sunrise::state::activity::coo::native_combatant;
namespace bits = sunrise::middleware::encoding::bits;
namespace character = sunrise::middleware::datagen::family4::character;

namespace sunrise::state::unlocks {

ScopedTable snapshot() noexcept {
    return {};
}

bool find_character(const ScopedTable&, std::uint64_t, CharacterTable& output) noexcept {
    output = {};
    return false;
}

} // namespace sunrise::state::unlocks

namespace sunrise::middleware::datagen::family4::progression {

bool key_bank(state::build_data::progressions::Scope,
              std::uint64_t,
              const state::unlocks::ScopedTable&,
              std::span<layout::Entry> bank) noexcept {
    for (layout::Entry& entry : bank) {
        entry = {};
        entry.definitionIndex = 0xFFFF;
    }
    return true;
}

} // namespace sunrise::middleware::datagen::family4::progression

namespace {

unsigned checks{};

void check(bool condition, int line) {
    ++checks;
    if (!condition) {
        std::printf("FAIL line %d\n", line);
        std::exit(1);
    }
}

#define CHECK(condition) check((condition), __LINE__)

light::Evaluation summary(std::int32_t score) {
    light::Evaluation value{};
    value.profile[0] = light::ItemScore{1, score};
    value.character[0] = light::ItemScore{2, score};
    value.divisor = 2;
    value.total = score * value.divisor;
    value.average = score;
    value.averageFloat = static_cast<float>(score);
    return value;
}

void exact_native_identity() {
    for (const std::int16_t activity : {std::int16_t{813}, std::int16_t{835}}) {
        const auto projection = nightfall::native_power_projection(activity, 30);
        CHECK(static_cast<bool>(projection));
        CHECK(projection.activity == activity);
        CHECK(projection.difficultySettings == nightfall::kGrandmasterDifficultySettings);
        CHECK(projection.authoredPower == 1100);
        CHECK(projection.effectiveCap == 1070);
        CHECK(projection.delta == 30);
    }

    CHECK(!nightfall::native_power_projection(811, 30));
    CHECK(!nightfall::native_power_projection(833, 30));
    CHECK(!nightfall::native_power_projection(813, 29));
    CHECK(!nightfall::native_power_projection(835, 51));
    CHECK(nightfall::native_power_projection(813, 50).effectiveCap == 1050);
}

void scalar_cap_never_boosts() {
    const auto projection = nightfall::native_power_projection(835, 30);
    CHECK(nightfall::cap_player_power(1120, projection) == 1070);
    CHECK(nightfall::cap_player_power(1070, projection) == 1070);
    CHECK(nightfall::cap_player_power(1000, projection) == 1000);
    CHECK(nightfall::cap_player_power(-1, projection) == -1);
    CHECK(nightfall::cap_player_power(1120, {}) == 1120);
    CHECK(nightfall::project_selected_item_level(110, true, projection) == 107);
    CHECK(nightfall::project_selected_item_level(100, true, projection) == 100);
    CHECK(nightfall::project_selected_item_level(110, false, projection) == 110);
    CHECK(nightfall::project_selected_item_level(110, true, {}) == 110);
    CHECK(nightfall::project_selected_item_level(
              110, true, nightfall::native_power_projection(835, 42))
          == 105);
}

void complete_equipment_summary_is_capped() {
    const auto projection = nightfall::native_power_projection(813, 30);
    auto value = summary(1100);
    CHECK(nightfall::cap_equipment_summary(value, projection)
          == nightfall::NativePowerApplyResult::applied);
    CHECK(value.profile[0]->score == 1100);
    CHECK(value.character[0]->score == 1070);
    CHECK(value.divisor == 2 && value.total == 2140 && value.average == 1070);
    CHECK(value.averageFloat == 1070.0F);

    auto harder = summary(1100);
    CHECK(nightfall::cap_equipment_summary(
              harder, nightfall::native_power_projection(835, 50))
          == nightfall::NativePowerApplyResult::applied);
    CHECK(harder.total == 2100 && harder.average == 1050);
}

void mixed_loadout_caps_selected_lane_and_effective_average_independently() {
    auto value = summary(1000);
    value.profile[0]->score = 1200;
    // The aggregate can also contain strict upgrades from other characters, so its total is not
    // derived from the two published arrays. Only its total/divisor pair defines effective light.
    value.total = 2200;
    value.average = 1100;
    value.averageFloat = 1100.0F;
    const auto beforeProfile = value.profile;
    CHECK(nightfall::cap_equipment_summary(
              value, nightfall::native_power_projection(813, 30))
          == nightfall::NativePowerApplyResult::applied);
    CHECK(value.profile == beforeProfile && value.character[0]->score == 1000);
    CHECK(value.total == 2140 && value.average == 1070 && value.averageFloat == 1070.0F);
    CHECK(nightfall::cap_player_power(1100, nightfall::native_power_projection(813, 30))
          == value.average);
}

void inactive_and_lower_summaries_stay_exact() {
    auto lower = summary(1040);
    const auto before = lower;
    CHECK(nightfall::cap_equipment_summary(
              lower, nightfall::native_power_projection(835, 30))
          == nightfall::NativePowerApplyResult::unchanged);
    CHECK(lower == before);

    auto inactive = summary(1100);
    const auto inactiveBefore = inactive;
    CHECK(nightfall::cap_equipment_summary(inactive, {})
          == nightfall::NativePowerApplyResult::inactive);
    CHECK(inactive == inactiveBefore);
}

void invalid_summary_is_transactional() {
    auto value = summary(1100);
    value.total = 2199;
    const auto before = value;
    CHECK(nightfall::cap_equipment_summary(
              value, nightfall::native_power_projection(813, 30))
          == nightfall::NativePowerApplyResult::invalid);
    CHECK(value == before);

    value = summary(1100);
    value.divisor = 0;
    const auto zeroBefore = value;
    CHECK(nightfall::cap_equipment_summary(
              value, nightfall::native_power_projection(813, 30))
          == nightfall::NativePowerApplyResult::invalid);
    CHECK(value == zeroBefore);
}

void live_rules_projection_is_coherent() {
    nightfall::leave();
    auto options = nightfall::defaults(sunrise::state::activity::strikes::Difficulty::grandmaster);
    options.powerDelta = 42;
    nightfall::arm(835, options);
    const auto projection = nightfall::current_native_power_projection();
    CHECK(projection.activity == 835 && projection.delta == 42);
    CHECK(projection.effectiveCap == 1058);
    nightfall::leave();
    CHECK(!nightfall::current_native_power_projection());
}

void production_character_encoder_caps_every_incremental_image() {
    sunrise::state::CharacterState selected{};
    selected.soid = 0x100;
    selected.previewAvailable = true;

    sunrise::middleware::datagen::family4::loadout::ResolvedLoadout loadout{};
    auto& item = loadout.items[0];
    item.inventoryRow = 0;
    item.equipmentSlot = 0;
    item.equipped = true;
    item.quantity = 1;
    item.mutationSerial = 0;
    item.instance.instanceSoid = 0x200;
    item.instance.bounds.itemDefinitionCount = 2;
    item.instance.baseDefinitionIndex = 1;
    loadout.itemCount = 1;
    loadout.nextInventorySerial = 1;

    light::Evaluation raw{};
    raw.character[0] = light::ItemScore{1, 1100};
    raw.divisor = 1;
    raw.total = 1100;
    raw.average = 1100;
    raw.averageFloat = 1100.0F;

    std::array<std::byte, character::layout::kObjectSize> bytes{};
    auto options = nightfall::defaults(sunrise::state::activity::strikes::Difficulty::grandmaster);
    nightfall::arm(813, options);
    // Full snapshots, item-state changes, and dismantles all call this same production encoder.
    CHECK(character::encode(selected, loadout, raw, bytes));
    const auto& gm = *reinterpret_cast<const character::layout::Object*>(bytes.data());
    CHECK(gm.equipmentSummary.character[0].definitionIndex == 1);
    CHECK(gm.equipmentSummary.character[0].score == 1070);
    CHECK(gm.equipmentSummary.total == 1070);
    CHECK(gm.equipmentSummary.light == 1070);
    CHECK(gm.equipmentSummary.lightScalar == 1070.0F);

    nightfall::leave();
    CHECK(character::encode(selected, loadout, raw, bytes));
    const auto& standard = *reinterpret_cast<const character::layout::Object*>(bytes.data());
    CHECK(standard.equipmentSummary.character[0].score == 1100);
    CHECK(standard.equipmentSummary.total == 1100);
    CHECK(standard.equipmentSummary.light == 1100);

    contest::arm(536, contest::Mode::contest);
    CHECK(character::encode(selected, loadout, raw, bytes));
    const auto& raid = *reinterpret_cast<const character::layout::Object*>(bytes.data());
    CHECK(raid.equipmentSummary.character[0].score == 730);
    CHECK(raid.equipmentSummary.total == 730 && raid.equipmentSummary.light == 730);
    CHECK(raid.equipmentSummary.lightScalar == 730.0F);
    const auto capturedContest = nightfall::current_native_power_projection();
    contest::leave();
    CHECK(character::encode(selected, loadout, raw, bytes, capturedContest));
    CHECK(reinterpret_cast<const character::layout::Object*>(bytes.data())->equipmentSummary.light == 730);
    CHECK(nightfall::project_selected_item_level(110, true, capturedContest) == 73);
    CHECK(character::encode(selected, loadout, raw, bytes));
    CHECK(reinterpret_cast<const character::layout::Object*>(bytes.data())->equipmentSummary.light == 1100);
}

void native_source_variant_wire_is_biased_and_bounded() {
    std::array<std::byte, 96> body{};
    combatant::Source source{0xC95ECB1AU, 7, 0, 1};
    source.hasRule = false;

    bits::Writer standardWriter(body);
    CHECK(combatant::write_source(standardWriter, source));
    CHECK(standardWriter.bit_count() == combatant::kSourceBits);
    bits::Reader standardReader(body);
    std::uint64_t value{};
    CHECK(standardReader.skip(159) && standardReader.read(3, value) && value == 1);

    source.variant = 5;
    bits::Writer grandmasterWriter(body);
    CHECK(combatant::write_source(grandmasterWriter, source));
    bits::Reader grandmasterReader(body);
    CHECK(grandmasterReader.skip(159) && grandmasterReader.read(3, value) && value == 6);

    source.variant = 6;
    bits::Writer invalidWriter(body);
    CHECK(!combatant::write_source(invalidWriter, source) && invalidWriter.bit_count() == 0);

    combatant::Source authored{0xC95ECB1AU, 7, 0, 1};
    authored.hasRule = false;
    authored.variant = 5;
    bits::Writer authoredWriter(body);
    CHECK(combatant::write_authored_source(authoredWriter, authored, {0, 0, 0, 0}));
    bits::Reader authoredReader(body);
    CHECK(authoredReader.skip(42) && authoredReader.read(3, value) && value == 6);

    authored.variant = 6;
    bits::Writer invalidAuthoredWriter(body);
    CHECK(!combatant::write_authored_source(
        invalidAuthoredWriter, authored, {0, 0, 0, 0}));
    CHECK(invalidAuthoredWriter.bit_count() == 0);
}

void grandmaster_variant_scope_is_package_allowlisted() {
    CHECK(sunrise::state::activity::strike_bond::grandmaster_substitution_source(
        0xC95ECB1AU, 16));
    CHECK(!sunrise::state::activity::strike_bond::grandmaster_substitution_source(
        0xC95ECB1AU, 15));
    CHECK(sunrise::state::activity::strike_pact::grandmaster_substitution_source(
        0x588E5FB9U, 2));
    CHECK(!sunrise::state::activity::strike_pact::grandmaster_substitution_source(
        0x588E5FB9U, 3));
}

void eater_contest_is_launch_scoped() {
    nightfall::leave(); contest::leave();
    const auto originalRevision = contest::power_revision();
    CHECK((originalRevision & 1U) == 0);
    contest::arm(536, contest::Mode::standard);
    CHECK(!nightfall::current_native_power_projection());
    CHECK(contest::power_revision() == originalRevision);
    contest::arm(536, contest::Mode::contest);
    const auto projection = nightfall::current_native_power_projection();
    CHECK(projection && projection.activity == 536 && projection.authoredPower == 750);
    CHECK(projection.delta == 20 && projection.effectiveCap == 730);
    CHECK(contest::power_revision() == originalRevision + 2);
    CHECK(nightfall::cap_player_power(1100, projection) == 730);
    CHECK(nightfall::cap_player_power(720, projection) == 720);
    CHECK(nightfall::project_selected_item_level(110, true, projection) == 73);
    CHECK(nightfall::project_selected_item_level(110, false, projection) == 110);
    auto effective = summary(1100);
    CHECK(nightfall::cap_equipment_summary(effective, projection) == nightfall::NativePowerApplyResult::applied);
    CHECK(effective.average == 730 && effective.total == 1460 && effective.character[0]->score == 730);
    CHECK(effective.profile[0]->score == 1100);
    CHECK(!nightfall::equipment_locked() && !nightfall::movement_blocked() && !nightfall::active());
    contest::enter(91, 536);
    CHECK(contest::enabled());
    contest::enter(91, 536); // Same session death/retry keeps difficulty.
    CHECK(contest::enabled());
    CHECK(contest::power_revision() == originalRevision + 2);
    contest::enter(92, 536); // Another launch cannot inherit the lease.
    CHECK(!contest::enabled());
    CHECK(!nightfall::current_native_power_projection());
    CHECK(contest::power_revision() == originalRevision + 4);
    for (const auto activity : {537, 538, 813, 835, -1}) {
        contest::arm(static_cast<std::int16_t>(activity), contest::Mode::contest);
        CHECK(!contest::enabled());
    }
    contest::arm(536, contest::Mode::contest);
    contest::enter(93, 299);
    CHECK(!contest::enabled());
    contest::arm(536, contest::Mode::contest);
    contest::leave();
    CHECK(nightfall::cap_player_power(1100, nightfall::current_native_power_projection()) == 1100);
}

void publication_retains_one_power_projection() {
    contest::arm(536, contest::Mode::contest);
    const auto before = contest::power_revision();
    {
        const nightfall::PowerPublication burst;
        CHECK(nightfall::current_native_power_projection().effectiveCap == 730);
        contest::leave(); // Model an orbit edge between roster and its item/banner companions.
        CHECK(!contest::enabled() && contest::power_revision() != before);
        CHECK(nightfall::current_native_power_projection().effectiveCap == 730);
        {
            const nightfall::PowerPublication nested;
            CHECK(nightfall::current_native_power_projection().effectiveCap == 730);
        }
        CHECK(nightfall::current_native_power_projection().effectiveCap == 730);
    }
    CHECK(!nightfall::current_native_power_projection());
    {
        const nightfall::PowerPublication standardBurst;
        contest::arm(536, contest::Mode::contest);
        CHECK(!nightfall::current_native_power_projection());
    }
    CHECK(nightfall::current_native_power_projection().effectiveCap == 730);
    contest::leave();
}

} // namespace

int main() {
    exact_native_identity();
    scalar_cap_never_boosts();
    complete_equipment_summary_is_capped();
    mixed_loadout_caps_selected_lane_and_effective_average_independently();
    inactive_and_lower_summaries_stay_exact();
    invalid_summary_is_transactional();
    live_rules_projection_is_coherent();
    production_character_encoder_caps_every_incremental_image();
    native_source_variant_wire_is_biased_and_bounded();
    grandmaster_variant_scope_is_package_allowlisted();
    eater_contest_is_launch_scoped();
    publication_retains_one_power_projection();
    std::printf("native nightfall power: %u checks passed\n", checks);
    return 0;
}
