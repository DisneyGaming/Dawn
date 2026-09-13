#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include "state/activity/omega_first_lair_encounter.h"
#include "state/activity/coo/omega_reveal.h"
#include "state/activity/omega_arc_charge_authority.h"
#include "fixtures/coo_combat_legacy.h"
#include "fixtures/omega_archive_arm_control.h"
namespace fight=sunrise::state::activity::omega_first_lair;
namespace frozen=sunrise::state::activity::frozen_combat;
namespace coo=sunrise::state::activity::coo;
namespace present=sunrise::state::activity::omega_presentation;
namespace charge=sunrise::state::activity::omega_arc_charge;
static unsigned checks{}, comparisons{};
static const char* operation="init";
static std::array<unsigned,3> recoveryOrder{0,1,2};
static bool deadFirst{},lateInitial{},earlyFinal{},keepCabalAlive{},batchRecovery{},killAfterSummon{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"line %d after %s: %s\n",__LINE__,operation,#value);std::exit(1); } } while(false)
class Pair final {
public:
    fight::Encounter modern;
    sunrise::state::activity::omega_archive_arm::Ledger arm;
    sunrise::state::activity::omega_archive_intro::Ledger programs;
    frozen::Encounter reference;
    bool deferred{};
    void sync() { if(!deferred) { modern.update_executor();compare(); } }
    void flush() { deferred=false;sync(); }
    void compare() {
        ++comparisons;
        if(modern.failed()) { const auto d=modern.executor_diagnostics();std::fprintf(stderr,"failed section=%u cycle=%u wave=%u stage=%u phase=%u active=%X complete=%X failure=%u\n",modern.executor_section(),modern.cycle(),modern.wave(),static_cast<unsigned>(modern.crown_stage()),static_cast<unsigned>(modern.phase()),d.active,d.complete,static_cast<unsigned>(d.failure)); }
        CHECK(!modern.failed());
        CHECK(modern.boss()==std::bit_cast<fight::Boss>(reference.boss()));
        CHECK(static_cast<unsigned>(modern.phase())==static_cast<unsigned>(reference.phase()));
        CHECK(static_cast<unsigned>(modern.pending())==static_cast<unsigned>(reference.pending()));
        CHECK(static_cast<unsigned>(modern.island())==static_cast<unsigned>(reference.island()));
        CHECK(static_cast<unsigned>(modern.cycle())==static_cast<unsigned>(reference.cycle()));
        CHECK(static_cast<unsigned>(modern.wave())==static_cast<unsigned>(reference.wave()));
        CHECK(static_cast<unsigned>(modern.crown_stage())==static_cast<unsigned>(reference.crown_stage()));
        CHECK(modern.failed()==reference.failed());
        CHECK(modern.cannon_active()==reference.cannon_active());
        CHECK(modern.final_cannon_active()==reference.final_cannon_active());
        CHECK(modern.crown_restricted()==reference.crown_restricted());
        CHECK(modern.cannon_prepared_mask()==reference.cannon_prepared_mask());
        CHECK(modern.route_enabled()==reference.route_enabled());
        CHECK(modern.transit_bridge()==reference.transit_bridge());
        CHECK(modern.transit_target()==reference.transit_target());
        CHECK(modern.charge_dunked()==reference.charge_dunked());
        CHECK(modern.eye_status_active()==reference.eye_status_active());
        CHECK(modern.return_launch()==reference.return_launch());
        CHECK(modern.charge_enabled()==reference.charge_enabled());
        CHECK(modern.final_traversal()==reference.final_traversal());
        CHECK(modern.ending_requested()==reference.ending_requested());
        CHECK(modern.transit_prepared_mask()==reference.transit_prepared_mask());
        for(std::size_t i=0;i<fight::kAllGroups.size();++i) { CHECK(modern.group_enabled(i)==reference.group_enabled(i)); }
        for(const auto& scene:sunrise::state::activity::omega_rescue_npc::kScenes) {
            const auto a=modern.scene_request(scene.slot);const auto b=reference.scene_request(scene.slot);
            CHECK(a.token==std::bit_cast<fight::CrownToken>(b.token));CHECK(a.enabled==b.enabled);
            CHECK(a.command.generation==b.command.generation);CHECK(a.command.stop==b.command.stop);
            CHECK(a.command.eventCount==b.command.eventCount);CHECK(a.command.events==b.command.events);
        }
    }
    void begin(std::uint64_t run) { operation="begin";arm={};programs={};reference.begin(run);modern.begin(run,true);sync(); }
    void initial_program(fight::Boss boss) {
        if(programs.status().program.revision || boss.run!=modern.run() || boss.island || boss.actionEpoch) { return; }
        auto owner=omega_archive_arm_fixture::arm_owner(boss);owner.revision=0;
        CHECK(programs.request(owner));owner.revision=1;CHECK(programs.acknowledge(owner));
    }
    bool initial_summon(fight::Boss boss) { initial_program(boss);operation="initial_summon";const bool a=modern.initial_summon(boss);const bool b=reference.initial_summon(std::bit_cast<frozen::Boss>(boss));CHECK(a==b);sync();return a; }
    bool initial_idle(fight::Boss boss) { initial_program(boss);operation="initial_idle";const bool a=modern.initial_idle(boss);const bool b=reference.initial_idle(std::bit_cast<frozen::Boss>(boss));CHECK(a==b);sync();return a; }
    bool claim(fight::Boss boss, fight::Action action) {
        operation="claim";const bool a=modern.claim(boss,action);
        const bool b=reference.claim(std::bit_cast<frozen::Boss>(boss),std::bit_cast<frozen::Action>(action));CHECK(a==b);
        if(a && (action==fight::Action::summonLeft || action==fight::Action::summonRight)) {
            const auto owner=omega_archive_arm_fixture::arm_owner(boss);
            const auto before=arm.status().control.revision;
            CHECK(arm.prepare(owner,action==fight::Action::summonRight));CHECK(arm.status().control.revision==before+1);
            CHECK(!arm.prepare(owner,action==fight::Action::summonRight));
            const omega_archive_arm_fixture::ControlFixture native(boss,arm.status().control);
            const std::array<float,4> zero{},one{1,1,1,1};
            CHECK(arm.acknowledge(owner,native.receipt(),action==fight::Action::summonLeft?one:zero,
                action==fight::Action::summonRight?one:zero));
        }
        if(a && action==fight::Action::summonBoth) {
            const auto owner=omega_archive_arm_fixture::arm_owner(boss);
            constexpr std::array<std::uint32_t,4> sequences{0x65D2379CU,0x65D2379EU,0x65D2379DU,0x65D2379BU};
            CHECK(programs.next(owner,sequences[modern.cycle()]));
            auto native=owner;native.revision=programs.status().program.revision;
            CHECK(programs.acknowledge(native));CHECK(programs.status().incarnation==boss.revision);
            CHECK(programs.status().owner.revision==boss.revision); // Scene and arm tokens retain their actor incarnation.
            CHECK(arm.status().control.revision!=0 && !arm.status().control.high && !arm.status().pending);
        }
        if(a && (action==fight::Action::depart || action==fight::Action::relocateFinal)) {
            const auto owner=omega_archive_arm_fixture::arm_owner(boss);
            CHECK(programs.depart(owner));CHECK(!programs.depart(owner));
            auto native=owner;native.revision=programs.status().program.revision;
            CHECK(!programs.acknowledge(owner));CHECK(programs.acknowledge(native));
            CHECK(programs.status().incarnation==boss.revision && programs.status().program.departure==boss.island);
        }
        sync();return a;
    }
    bool summon_started(fight::Boss boss, fight::Action action) { operation="summon_started";const bool a=modern.summon_started(boss, action);const bool b=reference.summon_started(std::bit_cast<frozen::Boss>(boss), std::bit_cast<frozen::Action>(action));CHECK(a==b);sync();return a; }
    bool summon_finished(fight::Boss boss, fight::Action action) {
        operation="summon_finished";
        const bool playing=(action==fight::Action::summonLeft && modern.phase()==fight::Phase::leftPlaying)
            || (action==fight::Action::summonRight && modern.phase()==fight::Phase::rightPlaying);
        if(playing && boss==modern.boss()) {
            const auto owner=omega_archive_arm_fixture::arm_owner(boss);
            CHECK(arm.release(owner,true));CHECK(!arm.release(owner,true));
            const omega_archive_arm_fixture::ControlFixture native(boss,arm.status().control);
            const std::array<float,4> zero{};CHECK(arm.acknowledge(owner,native.receipt(),zero,zero));
        }
        const bool a=modern.summon_finished(boss,action);
        const bool b=reference.summon_finished(std::bit_cast<frozen::Boss>(boss),std::bit_cast<frozen::Action>(action));CHECK(a==b);
        sync();return a;
    }
    bool admitted(fight::ActorReceipt receipt) { operation="admitted";const bool a=modern.admitted(receipt);const bool b=reference.admitted(std::bit_cast<frozen::ActorReceipt>(receipt));CHECK(a==b);sync();return a; }
    bool died(fight::ActorReceipt receipt) { operation="died";const bool a=modern.died(receipt);const bool b=reference.died(std::bit_cast<frozen::ActorReceipt>(receipt));CHECK(a==b);sync();return a; }
    bool departed(fight::Boss boss, bool folded, bool milestone) { operation="departed";const bool a=modern.departed(boss, folded, milestone);const bool b=reference.departed(std::bit_cast<frozen::Boss>(boss), folded, milestone);CHECK(a==b);sync();return a; }
    bool arrived(std::uint64_t run, std::uint8_t island) { operation="arrived";const bool a=modern.arrived(run, island);const bool b=reference.arrived(run, island);CHECK(a==b);sync();return a; }
    bool crown_arrived(std::uint64_t run) { operation="crown_arrived";const bool a=modern.crown_arrived(run);const bool b=reference.crown_arrived(run);CHECK(a==b);sync();return a; }
    bool animation(fight::CrownToken owner, fight::AnimationMilestone event) { operation="animation";const bool a=modern.animation(owner, event);const bool b=reference.animation(std::bit_cast<frozen::CrownToken>(owner), std::bit_cast<frozen::AnimationMilestone>(event));CHECK(a==b);sync();return a; }
    bool scene(fight::CrownToken owner, std::uint16_t slot, fight::SceneMilestone event) { operation="scene";const bool a=modern.scene(owner, slot, event);const bool b=reference.scene(std::bit_cast<frozen::CrownToken>(owner), slot, std::bit_cast<frozen::SceneMilestone>(event));CHECK(a==b);sync();return a; }
    bool charge(fight::ChargeReceipt receipt, fight::ChargeMilestone event) { operation="charge";const bool a=modern.charge(receipt, event);const bool b=reference.charge(std::bit_cast<frozen::ChargeReceipt>(receipt), std::bit_cast<frozen::ChargeMilestone>(event));CHECK(a==b);sync();return a; }
    bool health(fight::CrownToken owner, fight::HealthMilestone event) { operation="health";const bool a=modern.health(owner, event);const bool b=reference.health(std::bit_cast<frozen::CrownToken>(owner), std::bit_cast<frozen::HealthMilestone>(event));CHECK(a==b);sync();return a; }
    bool ending(fight::CrownToken owner, bool finished) { operation="ending";const bool a=modern.ending(owner, finished);const bool b=reference.ending(std::bit_cast<frozen::CrownToken>(owner), finished);CHECK(a==b);sync();return a; }
    bool gate_arrived(fight::CrownToken owner, fight::GateMilestone gate, std::uint32_t player) { operation="gate_arrived";const bool a=modern.gate_arrived(owner, gate, player);const bool b=reference.gate_arrived(std::bit_cast<frozen::CrownToken>(owner), std::bit_cast<frozen::GateMilestone>(gate), player);CHECK(a==b);sync();return a; }
    bool final_departed(fight::CrownToken owner, bool folded, bool milestone) { operation="final_departed";const bool a=modern.final_departed(owner, folded, milestone);const bool b=reference.final_departed(std::bit_cast<frozen::CrownToken>(owner), folded, milestone);CHECK(a==b);sync();return a; }
    bool return_created(std::uint64_t run, std::uint32_t entity) { operation="return_created";const bool a=modern.return_created(run, entity);const bool b=reference.return_created(run, entity);CHECK(a==b);sync();return a; }
    void cannon_prepared(std::uint64_t run,std::uint32_t gen,std::uint8_t index) { operation="cannon_prepared";modern.cannon_prepared(run,gen,index);reference.cannon_prepared(run,gen,index);sync(); }
    void transit_prepared(std::uint64_t run,std::uint32_t gen,std::uint8_t index) { operation="transit_prepared";modern.transit_prepared(run,gen,index);reference.transit_prepared(run,gen,index);sync(); }
    decltype(auto) run() const { return modern.run(); }
    decltype(auto) boss() const { return modern.boss(); }
    decltype(auto) pending() const { return modern.pending(); }
    decltype(auto) token() const { return modern.token(); }
    decltype(auto) island() const { return modern.island(); }
    decltype(auto) cycle() const { return modern.cycle(); }
    decltype(auto) wave() const { return modern.wave(); }
    decltype(auto) crown_stage() const { return modern.crown_stage(); }
    decltype(auto) crown_restricted() const { return modern.crown_restricted(); }
    decltype(auto) cannon_active() const { return modern.cannon_active(); }
    decltype(auto) charge_enabled() const { return modern.charge_enabled(); }
    decltype(auto) transit_bridge() const { return modern.transit_bridge(); }
    decltype(auto) transit_target() const { return modern.transit_target(); }
    decltype(auto) eye_status_active() const { return modern.eye_status_active(); }
    decltype(auto) final_traversal() const { return modern.final_traversal(); }
    decltype(auto) ending_requested() const { return modern.ending_requested(); }
    decltype(auto) failed() const { return modern.failed(); }
    bool group_enabled(std::size_t i) const { return modern.group_enabled(i); }
    auto scene_request(std::uint16_t slot) const { return modern.scene_request(slot); }
};
struct Run {
    Pair encounter;
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
                if(group.required || !keepCabalAlive) {
                    CHECK(encounter.died(receipt));
                    CHECK(!encounter.died(receipt));
                }
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
            if(!killAfterSummon) { clear_new_groups(); }
            CHECK(encounter.summon_finished(owner, action));
            if(killAfterSummon) { clear_new_groups(); }
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
    if(!lateInitial) { CHECK(e.initial_summon(initial));run.clear_new_groups(); }
    CHECK(e.initial_idle(initial));
    if(lateInitial) { run.clear_new_groups(); }
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
            e.deferred=batchRecovery;
            for(unsigned i=0;i<3;++i) {
                switch(recoveryOrder[i]) {
                case 0:CHECK(e.animation(damageOwner,fight::AnimationMilestone::recovered));break;
                case 1:CHECK(e.return_created(42,600U+cycle));break;
                case 2:CHECK(e.health(damageOwner,fight::HealthMilestone::checkpointReached));break;
                }
                if(i<2) { CHECK(e.cycle()==cycle); }
            }
            e.flush();
            CHECK(!e.health(damageOwner, fight::HealthMilestone::checkpointReached));
            if (cycle == 2) {
                run.summons();
                CHECK(e.pending() == fight::Action::relocateFinal);
                CHECK(e.claim(e.boss(), fight::Action::relocateFinal));
                CHECK(!e.final_departed(e.token(), false, true));
                if(earlyFinal) { CHECK(e.gate_arrived(e.token(),fight::GateMilestone::finalPlatform,8)); }
                CHECK(e.final_departed(e.token(), true, true));
                CHECK(e.final_traversal());
                if(!earlyFinal) {
                    CHECK(e.gate_arrived(e.token(), fight::GateMilestone::finalCannon, 8));
                    CHECK(e.gate_arrived(e.token(), fight::GateMilestone::finalPlatform, 8));
                }
            }
        } else {
            CHECK(e.animation(damageOwner, fight::AnimationMilestone::deathStarted));
            if(deadFirst) { CHECK(e.health(damageOwner,fight::HealthMilestone::bossDead)); }
            else { CHECK(e.animation(damageOwner,fight::AnimationMilestone::deathFinished)); }
            CHECK(!e.ending_requested());
            if(deadFirst) { CHECK(e.animation(damageOwner,fight::AnimationMilestone::deathFinished)); }
            else { CHECK(e.health(damageOwner,fight::HealthMilestone::bossDead)); }
            CHECK(e.ending_requested());
            CHECK(!e.crown_restricted());
            CHECK(e.claim(e.boss(), fight::Action::finishEncounter));
            CHECK(e.ending(e.token(), false));
            CHECK(e.ending(e.token(), true));
            CHECK(e.crown_stage() == fight::CrownStage::finished);
            CHECK(!e.failed());
        }
    }
    CHECK(run.nextActor-100==143);CHECK(e.modern.executor_diagnostics().phase==coo::Phase::complete);
}


void reveal_parity() {
    for(unsigned path=0;path<32;++path) {
        present::Intro legacy;coo::reveal::Intro modern;
        legacy.reset(42);modern.reset(42,true);
        const auto compare=[&] { CHECK(legacy.phase()==modern.phase());CHECK(legacy.command().revision==modern.command().revision);CHECK(legacy.command().play==modern.command().play);CHECK(legacy.busy()==modern.busy()); };
        const auto observe=[&](std::uint32_t revision,bool active,bool ready,bool audio,bool flight,std::uint64_t now) {
            legacy.observe(revision,active,ready,audio,flight,now);modern.observe(revision,active,ready,audio,flight,now);compare();
        };
        if(path&1U) { legacy.passed();modern.passed(); }
        legacy.request(100);modern.request(100);compare();
        for(unsigned attempt=0;attempt<4;++attempt) {
            const auto now=100U+attempt*4000U;
            observe(0,false,false,true,true,now);
            observe(0,false,true,false,true,now+1);
            observe(0,false,true,true,false,now+2);
            observe(0,false,true,true,false,now+3);
            observe(0,false,true,true,true,now+4);
            const auto revision=legacy.command().revision;
            observe(revision+1,true,true,true,true,now+5);
            observe(revision,false,true,true,true,now+6);
            if(path&2U) { observe(revision,false,true,true,true,now+1004);continue; }
            if(path&4U) { legacy.passed();modern.passed();compare(); }
            observe(revision,true,true,true,true,now+7);
            if(path&8U) { legacy.advance(now+30007);modern.advance(now+30007);compare();break; }
            if(path&16U) { legacy.passed();modern.passed();compare(); }
            observe(revision,false,true,true,true,now+8);
        }
        legacy.advance(100000);modern.advance(100000);compare();
    }
}
int main() {
    operation="reveal";reveal_parity();
    for(const auto* definition:coo::combat::kSections) { CHECK(coo::Executor::valid(*definition)); }
    unsigned runs{};
    do {
        for(unsigned variant=0;variant<8;++variant) {
            deadFirst=(variant&1U)!=0;lateInitial=(variant&2U)!=0;earlyFinal=(variant&4U)!=0;
            keepCabalAlive=variant!=0;batchRecovery=(variant&2U)!=0;killAfterSummon=(variant&1U)!=0;
            full_encounter();++runs;
        }
    } while(std::next_permutation(recoveryOrder.begin(),recoveryOrder.end()));
    // Receipts from cancelled runs cannot drive a fresh actor binding.
    fight::Encounter reset;reset.begin(42,true);reset.update_executor();const auto incarnation=reset.executor_diagnostics().incarnation;
    const fight::Boss stale{42,1,2,3,7,1,0,0};
    CHECK(reset.initial_summon(stale));reset.begin(43,true);reset.update_executor();
    CHECK(reset.executor_diagnostics().incarnation>incarnation);CHECK(!reset.initial_idle(stale));
    reset.invalidate(43);reset.update_executor();CHECK(reset.pending()==fight::Action::none);
    std::printf("Full-fight variants=%u; all recovery/death joins, delayed deaths, retained Cabal, early arrivals and batched native facts\n",runs);
    std::printf("PASS: %u checks; %u frozen combat state comparisons\n",checks,comparisons);
}
