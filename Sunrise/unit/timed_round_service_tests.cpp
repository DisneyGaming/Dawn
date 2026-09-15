#include "server/runtime/activity/timed_round_service.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace timed=sunrise::server::runtime::activity::timed_round;
using Owner=sunrise::state::activity::ActivityInstanceKey;
constexpr Owner kOwner{41,{7}};
constexpr std::uint64_t kBoot=19;
unsigned checks{};

void expect(bool value) {
    ++checks;
    if(!value) { std::fprintf(stderr,"failed check %u\n",checks); std::exit(1); }
}
timed::Token token(const timed::Service& service) { return service.snapshot().token; }
timed::Service make_service(std::uint64_t ticks=100,std::uint32_t target=10,
    std::uint8_t activePhaseMask=timed::kTraversalPhaseBit) {
    timed::Service result;expect(result.begin(kOwner,kBoot,{ticks,target,activePhaseMask}));return result;
}
void nine_round_sequence_pauses_budget_and_finishes_at_rewards() {
    auto run=make_service();std::uint64_t clock=0;
    expect(run.capture_complete(token(run),10)==timed::Result::accepted);
    expect(run.add_progress(token(run),12,1,5)==timed::Result::accepted);
    expect(run.tick(token(run),20)==timed::Result::accepted);
    expect(run.add_progress(token(run),25,2,5)==timed::Result::accepted);
    expect(run.encounter_arrived(token(run),30)==timed::Result::accepted);
    expect(run.encounter_defeated(token(run),40,1001)==timed::Result::accepted);
    expect(run.snapshot().remainingTicks==85 && run.snapshot().completedRounds==1);
    expect(run.returned(token(run),45)==timed::Result::accepted);
    clock=45;
    for(std::uint64_t roundNumber=2;roundNumber<9;++roundNumber) {
        expect(run.capture_complete(token(run),++clock)==timed::Result::accepted);
        expect(run.add_progress(token(run),clock,roundNumber,10)==timed::Result::accepted);
        expect(run.encounter_arrived(token(run),++clock)==timed::Result::accepted);
        expect(run.encounter_defeated(token(run),++clock,1000+roundNumber)==timed::Result::accepted);
        expect(run.snapshot().completedRounds==roundNumber);
        expect(run.returned(token(run),++clock)==timed::Result::accepted);
    }
    expect(run.snapshot().token.round==9);
    expect(run.capture_complete(token(run),++clock)==timed::Result::accepted);
    expect(run.tick(token(run),clock+1000)==timed::Result::accepted);
    auto expired=run.snapshot();expect(expired.expired && expired.remainingTicks==0);
    expect(run.add_progress(token(run),clock+1000,9,10)==timed::Result::accepted);
    expect(run.encounter_arrived(token(run),clock+1001)==timed::Result::accepted);
    expect(run.encounter_defeated(token(run),clock+1001,1009)==timed::Result::accepted);
    expect(run.snapshot().phase==timed::Phase::rewards && run.snapshot().completedRounds==9);
    expect(run.rewards_finished(token(run),clock+1002)==timed::Result::accepted);
    expect(run.snapshot().phase==timed::Phase::complete && run.snapshot().completedRounds==9);
    expect(run.returned(token(run),clock+1003)==timed::Result::unsupported);
}

void early_wipe_is_nonterminal_and_expired_wipe_excludes_current_round() {
    auto run=make_service(10);expect(run.capture_complete(token(run),0)==timed::Result::accepted);
    const auto before=run.snapshot();
    expect(run.all_players_defeated(token(run),1,2)==timed::Result::unsupported);
    expect(run.snapshot().phase==timed::Phase::traversal && run.snapshot().revision==before.revision);
    expect(run.tick(token(run),10)==timed::Result::accepted);
    expect(run.all_players_defeated(token(run),11,2)==timed::Result::accepted);
    expect(run.snapshot().phase==timed::Phase::rewards && run.snapshot().completedRounds==0);
}

void duplicate_stale_wrong_owner_and_phase_barriers_do_not_mutate() {
    auto run=make_service();expect(run.capture_complete(token(run),5)==timed::Result::accepted);
    const auto current=token(run);
    expect(run.add_progress(current,6,55,4)==timed::Result::accepted);
    const auto beforeDuplicate=run.snapshot();
    expect(run.add_progress(current,9,55,4)==timed::Result::duplicate);
    expect(run.snapshot().remainingTicks==beforeDuplicate.remainingTicks
        && run.snapshot().revision==beforeDuplicate.revision);
    auto wrong=current;wrong.owner=kOwner;wrong.owner.sessionId++;
    expect(run.add_progress(wrong,10,56,4)==timed::Result::stale);
    auto wrongBoot=current;wrongBoot.boot++;
    expect(run.add_progress(wrongBoot,10,56,4)==timed::Result::stale);
    expect(run.snapshot().revision==beforeDuplicate.revision);
    expect(run.encounter_arrived(current,10)==timed::Result::unsupported);
    expect(run.capture_complete(current,10)==timed::Result::unsupported);
    expect(run.tick(current,4)==timed::Result::invalidClock);
    expect(run.snapshot().remainingTicks==beforeDuplicate.remainingTicks);
    expect(run.add_progress(current,10,56,6)==timed::Result::accepted);
    const auto old=current;
    expect(run.encounter_arrived(token(run),11)==timed::Result::accepted);
    expect(run.encounter_defeated(old,12,99)==timed::Result::stale);
}

void last_enemy_at_exact_deadline_and_completion_requires_rewards_receipt() {
    auto run=make_service(5,1);expect(run.capture_complete(token(run),0)==timed::Result::accepted);
    expect(run.add_progress(token(run),5,77,1)==timed::Result::accepted);
    expect(run.snapshot().expired && run.snapshot().phase==timed::Phase::toEncounter);
    expect(run.encounter_arrived(token(run),5)==timed::Result::accepted);
    expect(run.encounter_defeated(token(run),5,88)==timed::Result::accepted);
    expect(run.last_defeated_identity()==88 && run.snapshot().phase==timed::Phase::rewards);
    expect(run.rewards_finished(token(run),5)==timed::Result::accepted);
    expect(run.snapshot().phase==timed::Phase::complete);
}

void fault_transitions_are_owner_and_generation_scoped() {
    auto run=make_service();auto current=token(run);auto foreign=current;foreign.owner.sessionId++;
    expect(run.cancel(foreign)==timed::Result::stale);
    expect(run.cancel(current)==timed::Result::accepted);
    expect(run.snapshot().phase==timed::Phase::faulted);
    const auto faulted=run.snapshot();
    expect(run.observation_lost(current)==timed::Result::stale);
    expect(run.observation_lost(token(run))==timed::Result::unsupported);
    expect(run.snapshot().revision==faulted.revision);
}

void capacity_and_input_overflow_fail_closed() {
    auto run=make_service(1000,timed::Service::kMaximumProgressTarget);
    expect(run.capture_complete(token(run),0)==timed::Result::accepted);
    for(std::uint64_t identity=1;identity<=timed::Service::kDeathCapacity;++identity)
        expect(run.add_progress(token(run),0,identity,1)==timed::Result::accepted);
    const auto full=run.snapshot();
    expect(run.add_progress(token(run),1,1025,1)==timed::Result::exhausted);
    expect(run.snapshot().progress==full.progress && run.snapshot().remainingTicks==full.remainingTicks);
    expect(run.add_progress(token(run),0,1025,0)==timed::Result::unsupported);
    expect(run.add_progress(token(run),0,1025,std::numeric_limits<std::uint32_t>::max())
        ==timed::Result::unsupported);

    auto invalidClock=make_service();expect(invalidClock.capture_complete(token(invalidClock),0)
        ==timed::Result::accepted);
    const auto beforeInvalidClock=invalidClock.snapshot();
    expect(invalidClock.tick(token(invalidClock),std::numeric_limits<std::uint64_t>::max())
        ==timed::Result::invalidClock);
    expect(invalidClock.snapshot().remainingTicks==beforeInvalidClock.remainingTicks
        && invalidClock.snapshot().revision==beforeInvalidClock.revision);

    auto saturated=make_service(100,100);expect(saturated.capture_complete(token(saturated),0)==timed::Result::accepted);
    expect(saturated.add_progress(token(saturated),0,1,60)==timed::Result::accepted);
    expect(saturated.add_progress(token(saturated),0,2,60)==timed::Result::accepted);
    expect(saturated.snapshot().progress==100 && saturated.snapshot().phase==timed::Phase::toEncounter);
}

void repeated_capture_does_not_reset_budget() {
    auto run=make_service(20);expect(run.capture_complete(token(run),0)==timed::Result::accepted);
    expect(run.tick(token(run),7)==timed::Result::accepted);
    const auto before=run.snapshot();
    expect(run.capture_complete(token(run),8)==timed::Result::unsupported);
    expect(run.snapshot().remainingTicks==before.remainingTicks && run.snapshot().revision==before.revision);
}

void active_combat_budget_accounts_travel_and_boss_but_pauses_entry() {
    auto run=make_service(15,1,timed::ActiveCombatPhases);
    expect(run.capture_complete(token(run),100)==timed::Result::accepted);
    expect(run.add_progress(token(run),100,1,1)==timed::Result::accepted);
    expect(run.encounter_arrived(token(run),105)==timed::Result::accepted);
    expect(run.snapshot().remainingTicks==10);
    expect(run.encounter_defeated(token(run),115,1001)==timed::Result::accepted);
    const auto expired=run.snapshot();
    expect(expired.expired && expired.remainingTicks==0 && expired.phase==timed::Phase::rewards);

    auto repeated=make_service(20,1,timed::ActiveCombatPhases);
    expect(repeated.capture_complete(token(repeated),0)==timed::Result::accepted);
    expect(repeated.add_progress(token(repeated),0,2,1)==timed::Result::accepted);
    expect(repeated.tick(token(repeated),4)==timed::Result::accepted);
    const auto beforeRepeat=repeated.snapshot();
    expect(repeated.tick(token(repeated),4)==timed::Result::duplicate);
    expect(repeated.snapshot().remainingTicks==beforeRepeat.remainingTicks
        && repeated.snapshot().revision==beforeRepeat.revision);

    auto defaultService=make_service(20,1);
    expect(defaultService.capture_complete(token(defaultService),0)==timed::Result::accepted);
    expect(defaultService.add_progress(token(defaultService),0,3,1)==timed::Result::accepted);
    const auto beforeUnsupported=defaultService.snapshot();
    expect(defaultService.tick(token(defaultService),4)==timed::Result::unsupported);
    expect(defaultService.snapshot().remainingTicks==beforeUnsupported.remainingTicks
        && defaultService.snapshot().revision==beforeUnsupported.revision);
}

int main() {
    nine_round_sequence_pauses_budget_and_finishes_at_rewards();
    early_wipe_is_nonterminal_and_expired_wipe_excludes_current_round();
    duplicate_stale_wrong_owner_and_phase_barriers_do_not_mutate();
    last_enemy_at_exact_deadline_and_completion_requires_rewards_receipt();
    capacity_and_input_overflow_fail_closed();
    repeated_capture_does_not_reset_budget();
    fault_transitions_are_owner_and_generation_scoped();
    active_combat_budget_accounts_travel_and_boss_but_pauses_entry();
    std::printf("timed round service: %u checks, zero failures\n",checks);
}
