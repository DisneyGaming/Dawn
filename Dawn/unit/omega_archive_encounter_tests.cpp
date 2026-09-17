#include <array>
#include <cstdio>
#include <cstdlib>

#include "state/activity/omega_first_lair_encounter.h"
#include "state/activity/omega_ending_rules.h"
#include "state/activity/omega_arc_charge_authority.h"

namespace fight = dawn::state::activity::omega_first_lair;
namespace ending = dawn::state::activity::omega_ending;
namespace charge = dawn::state::activity::omega_arc_charge;

static unsigned checks{};
#define CHECK(value) do { ++checks; if (!(value)) { \
    std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); std::exit(1); } } while (false)

struct Run {
    fight::Encounter encounter;
    std::array<bool, fight::kAllGroups.size()> admitted{};
    std::uint32_t nextActor{100};

    void clear_new_groups() {
        for (std::size_t i = 0; i < admitted.size(); ++i) {
            if (admitted[i] || !encounter.group_enabled(i)) { continue; }
            admitted[i] = true;
            const auto& group = fight::kAllGroups[i];
            for (std::uint8_t n = 0; n < group.count; ++n) {
                const auto actor = nextActor++;
                const fight::ActorReceipt receipt{encounter.run(), actor, actor + 1000,
                    encounter.boss().generation, group.source, group.registry};
                CHECK(encounter.admitted(receipt));
                CHECK(!encounter.admitted(receipt));
                auto stale = receipt;
                ++stale.generation;
                CHECK(!encounter.died(stale));
                CHECK(encounter.died(receipt));
                CHECK(!encounter.died(receipt));
            }
        }
    }

    void summons() {
        for (unsigned guard = 0; guard < 16; ++guard) {
            const auto action = encounter.pending();
            if (action != fight::Action::summonLeft && action != fight::Action::summonRight
                && action != fight::Action::summonBoth) { return; }
            const auto owner = encounter.boss();
            CHECK(encounter.claim(owner, action));
            CHECK(!encounter.claim(owner, action));
            CHECK(encounter.summon_started(owner, action));
            clear_new_groups();
            CHECK(encounter.summon_finished(owner, action));
        }
        CHECK(false);
    }
};

void full_encounter() {
    Run run;
    auto& e = run.encounter;
    e.begin(42);
    const fight::Boss initial{42, 1, 2, 3, 7, 1, 0, 0};
    auto stale = initial;
    ++stale.run;
    CHECK(!e.initial_summon(stale));
    CHECK(e.initial_summon(initial));
    run.clear_new_groups();
    CHECK(e.initial_idle(initial));
    for (std::uint8_t i = 0; i < 4; ++i) { e.cannon_prepared(42, 7, i); }
    for (std::uint8_t i = 0; i < 7; ++i) { e.transit_prepared(42, 7, i); }
    for (std::uint8_t island = 0; island < 4; ++island) {
        CHECK(e.island() == island);
        run.summons();
        CHECK(e.pending() == fight::Action::depart);
        const auto owner = e.boss();
        CHECK(e.claim(owner, fight::Action::depart));
        CHECK(!e.departed(owner, true, false));
        if (island < 3) {
            CHECK(e.arrived(42, static_cast<std::uint8_t>(island + 1)));
            CHECK(e.island() == island);
        } else { CHECK(e.crown_arrived(42)); }
        CHECK(e.departed(owner, true, true));
    }
    CHECK(e.island() == 4);
    CHECK(e.crown_restricted());
    CHECK(e.cannon_active());

    for (std::uint8_t cycle = 1; cycle <= 3; ++cycle) {
        CHECK(e.cycle() == cycle);
        run.summons();
        CHECK(e.pending() == fight::Action::beginDeletion);
        CHECK(e.claim(e.boss(), fight::Action::beginDeletion));
        CHECK(e.animation(e.token(), fight::AnimationMilestone::deletionStarted));
        CHECK(e.animation(e.token(), fight::AnimationMilestone::deletionHold));
        CHECK(!e.charge_enabled());
        const std::uint16_t sceneSlot = cycle == 1 ? 9 : cycle == 2 ? 27 : 46;
        const auto scene = e.scene_request(sceneSlot);
        CHECK(scene.enabled);
        CHECK(e.scene(scene.token, sceneSlot, fight::SceneMilestone::rescueReady));
        CHECK(!e.scene(scene.token, sceneSlot, fight::SceneMilestone::rescueReady));
        if (cycle < 3) {
            CHECK(!e.charge_enabled());
            CHECK(e.gate_arrived(e.token(), fight::GateMilestone::chargePlatform, 8));
        }
        CHECK(e.charge_enabled());
        const auto& catalog = charge::kCycles[cycle - 1];
        fight::ChargeReceipt receipt{e.token(), 300, 7, 400, 8,
            UINT32_MAX, catalog.registry, catalog.carrySlot, catalog.sinkSlot};
        auto wrong = receipt;
        ++wrong.generation;
        CHECK(!e.charge(wrong, fight::ChargeMilestone::pickedUp));
        CHECK(e.charge(receipt, fight::ChargeMilestone::pickedUp));
        CHECK(e.transit_bridge());
        CHECK(e.charge(receipt, fight::ChargeMilestone::dropped));
        CHECK(e.charge_enabled());
        CHECK(e.charge(receipt, fight::ChargeMilestone::pickedUp));
        CHECK(!e.charge(receipt, fight::ChargeMilestone::dunked));
        receipt.sinkHandle = 500;
        CHECK(e.charge(receipt, fight::ChargeMilestone::dunked));
        CHECK(e.transit_target());
        CHECK(e.eye_status_active());
        CHECK(!e.health(receipt.token, fight::HealthMilestone::eyeThresholdReached));
        CHECK(e.claim(e.boss(), fight::Action::breakShield));
        CHECK(e.animation(e.token(), fight::AnimationMilestone::eyeExposing));
        CHECK(e.animation(e.token(), fight::AnimationMilestone::eyeVulnerable));
        CHECK(e.health(e.token(), fight::HealthMilestone::eyeThresholdReached));
        CHECK(e.claim(e.boss(), fight::Action::endEyePhase));
        const auto damageOwner = e.token();
        if (cycle < 3) {
            CHECK(e.animation(damageOwner, fight::AnimationMilestone::recoveryStarted));
            CHECK(e.animation(damageOwner, fight::AnimationMilestone::recovered));
            CHECK(e.return_created(42, 600U + cycle));
            CHECK(e.cycle() == cycle);
            CHECK(e.health(damageOwner, fight::HealthMilestone::checkpointReached));
            CHECK(!e.health(damageOwner, fight::HealthMilestone::checkpointReached));
            if (cycle == 2) {
                run.summons();
                CHECK(e.pending() == fight::Action::relocateFinal);
                CHECK(e.claim(e.boss(), fight::Action::relocateFinal));
                CHECK(!e.final_departed(e.token(), false, true));
                CHECK(e.final_departed(e.token(), true, true));
                CHECK(e.final_traversal());
                CHECK(e.gate_arrived(e.token(), fight::GateMilestone::finalCannon, 8));
                CHECK(e.gate_arrived(e.token(), fight::GateMilestone::finalPlatform, 8));
            }
        } else {
            CHECK(e.animation(damageOwner, fight::AnimationMilestone::deathStarted));
            CHECK(e.animation(damageOwner, fight::AnimationMilestone::deathFinished));
            CHECK(!e.ending_requested());
            CHECK(e.health(damageOwner, fight::HealthMilestone::bossDead));
            CHECK(e.ending_requested());
            CHECK(!e.crown_restricted());
            CHECK(e.claim(e.boss(), fight::Action::finishEncounter));
            CHECK(e.ending(e.token(), false));
            CHECK(e.ending(e.token(), true));
            CHECK(e.crown_stage() == fight::CrownStage::finished);
            CHECK(!e.failed());
        }
    }
    std::printf("encounter: islands=4 cycles=3 actors=%u ending=finished\n", run.nextActor - 100);
}

void ending_receipts() {
    ending::Ending movie;
    const ending::Token owner{42, 1, 1, 7};
    CHECK(movie.request(owner, 100));
    movie.advance(101, true);
    CHECK(movie.phase() == ending::Phase::retiring);
    CHECK(!movie.observe_arrival(owner, ending::kSlice));
    CHECK(movie.observe_retirement(owner, 102));
    CHECK(!movie.observe_arrival(owner, 120));
    CHECK(movie.observe_arrival(owner, ending::kSlice));
    movie.observe(owner, 0, false, true, 103);
    CHECK(movie.phase() == ending::Phase::offered);
    auto authority = movie.authority();
    movie.observe(owner, authority.revision, false, true, 104);
    CHECK(!movie.authority().complete);
    movie.observe(owner, authority.revision, true, true, 105);
    CHECK(movie.authority().started);
    CHECK(movie.skip());
    CHECK(!movie.authority().complete);
    authority = movie.authority();
    movie.observe(owner, authority.revision, false, true, 106);
    CHECK(movie.authority().complete);
    CHECK(movie.claim_handoff(owner));
    CHECK(!movie.claim_handoff(owner));
    CHECK(movie.note_handoff_result(owner, true));
    CHECK(movie.handoff() == ending::Handoff::queued);
    ending::Ending stalled;
    CHECK(stalled.request(owner, 1));
    stalled.advance(120001, false);
    CHECK(stalled.authority().failed);
    CHECK(!stalled.authority().complete);
}

int main() {
    full_encounter();
    ending_receipts();
    std::printf("PASS: %u checks\n", checks);
}
