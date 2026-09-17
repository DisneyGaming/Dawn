#pragma once
#include "state/activity/coo/native_population_ledger.h"

void native_population_cases() {
    using Intake=c::PopulationIntake;
    const c::PopulationOwner owner{7,9,1,{0x100,0x200,1,0},1};
    const c::PopulationActor first{owner,0x10001,0x20001},second{owner,0x10002,0x20002};
    c::NativePopulationLedger<3> ledger;
    CHECK(!ledger.begin({}));CHECK(ledger.begin(owner));CHECK(!ledger.begin(owner));
    CHECK(ledger.source_retired(owner)==Intake::unrelated);
    CHECK(ledger.died(first)==Intake::unknown);CHECK(ledger.actor_retired(first)==Intake::unknown);
    CHECK(ledger.admitted(first)==Intake::accepted);CHECK(ledger.admitted(first)==Intake::duplicate);
    auto foreign=second;foreign.owner.activity=8;CHECK(ledger.admitted(foreign)==Intake::unrelated);
    foreign=second;foreign.owner.generation=2;CHECK(ledger.admitted(foreign)==Intake::unrelated);
    CHECK(ledger.died(first)==Intake::accepted);CHECK(ledger.died(first)==Intake::duplicate);
    CHECK(ledger.counts().alive==0);CHECK(ledger.counts().dead==1);CHECK(ledger.counts().resident==1);
    CHECK(ledger.retiring(owner));CHECK(!ledger.retiring(owner));
    CHECK(ledger.admitted(second)==Intake::accepted); // native birth already in flight
    CHECK(ledger.source_retired(owner)==Intake::accepted);CHECK(ledger.source_retired(owner)==Intake::duplicate);
    CHECK(ledger.phase()==c::PopulationPhase::retiring);CHECK(!ledger.begin(owner));
    CHECK(ledger.actor_retired(first)==Intake::accepted);CHECK(ledger.actor_retired(first)==Intake::duplicate);
    CHECK(ledger.phase()==c::PopulationPhase::retiring);
    CHECK(ledger.actor_retired(second)==Intake::accepted); // despawn need not be a kill
    CHECK(ledger.counts().dead==1);CHECK(ledger.counts().resident==0);
    CHECK(ledger.phase()==c::PopulationPhase::retired);CHECK(!ledger.begin(owner));
    auto sameRun=owner;++sameRun.generation;CHECK(ledger.begin(sameRun));
    CHECK(ledger.retiring(sameRun));CHECK(ledger.source_retired(sameRun)==Intake::accepted);
    auto next=sameRun;++next.generation;++next.incarnation;
    CHECK(ledger.begin(next));CHECK(ledger.died(first)==Intake::unrelated);CHECK(ledger.actor_retired(second)==Intake::unrelated);
    CHECK(ledger.counts().admitted==0);CHECK(ledger.retiring(next));CHECK(ledger.source_retired(next)==Intake::accepted);
    CHECK(ledger.phase()==c::PopulationPhase::retired);
    CHECK(!ledger.begin(owner)); // an older generation cannot be resurrected
    c::NativePopulationLedger<3> recurring;
    CHECK(recurring.begin(owner));CHECK(!recurring.renew(owner,sameRun)); // no admitted cohort
    CHECK(recurring.admitted(first)==Intake::accepted);
    CHECK(!recurring.renew(owner,sameRun)); // live and resident
    CHECK(recurring.died(first)==Intake::accepted);CHECK(!recurring.renew(owner,sameRun));
    CHECK(recurring.actor_retired(first)==Intake::accepted);
    auto skipped=sameRun;++skipped.generation;CHECK(!recurring.renew(owner,skipped));
    CHECK(recurring.renew(owner,sameRun));
    CHECK(recurring.owner()==sameRun && recurring.phase()==c::PopulationPhase::active);
    CHECK(recurring.counts().admitted==0 && !recurring.counts().sourceRetired);
    CHECK(recurring.admitted({sameRun,0x11001,0x21001})==Intake::accepted);
    // A single surviving teammate must not make finite actor storage a lifetime
    // refill cap. Completed births compact to bounded replay tombstones while
    // cumulative and per-category authority totals remain exact.
    c::NativePopulationLedger<64> sustained;
    CHECK(sustained.begin(owner));
    const c::PopulationActor survivor{owner,0x51001,0x61001,1};
    CHECK(sustained.admitted(survivor,0)==Intake::accepted);
    c::PopulationActor latest{};
    for(std::uint64_t i=0;i<300;++i) {
        latest={owner,0x51002,0x61002,i+2};
        const auto category=static_cast<std::uint8_t>(i&1U);
        CHECK(sustained.admitted(latest,category)==Intake::accepted);
        CHECK(sustained.died(latest)==Intake::accepted);
        CHECK(sustained.actor_retired(latest)==Intake::accepted);
        CHECK(sustained.counts().alive==1 && sustained.counts().resident==1);
    }
    CHECK(sustained.counts().admitted==301 && sustained.counts().dead==300);
    CHECK(sustained.counts(0).admitted==151 && sustained.counts(0).dead==150);
    CHECK(sustained.counts(0).alive==1 && sustained.counts(0).resident==1);
    CHECK(sustained.counts(1).admitted==150 && sustained.counts(1).dead==150);
    CHECK(sustained.counts(1).alive==0 && sustained.counts(1).resident==0);
    CHECK(!sustained.counts(2).failed && sustained.counts(2).admitted==0);
    CHECK(sustained.admitted(latest,1)==Intake::duplicate);
    CHECK(sustained.died(latest)==Intake::duplicate);
    CHECK(sustained.actor_retired(latest)==Intake::duplicate);
    auto thirdLane=sustained;const c::PopulationActor third{owner,0x51F02,0x61F02,400};
    CHECK(thirdLane.admitted(third,2)==Intake::accepted);
    CHECK(thirdLane.counts(2).admitted==1 && thirdLane.counts(2).alive==1);
    const c::PopulationActor eighth{owner,0x51F07,0x61F07,401};
    CHECK(thirdLane.admitted(eighth,7)==Intake::accepted && thirdLane.counts(7).admitted==1);
    CHECK(thirdLane.admitted({owner,0x51F08,0x61F08,402},8)==Intake::conflict);
    const c::PopulationActor evicted{owner,0x51002,0x61002,2};
    auto staleAdmission=sustained;
    CHECK(staleAdmission.admitted(evicted,0)==Intake::conflict);
    CHECK(staleAdmission.counts().failed);
    auto staleDeath=sustained;
    CHECK(staleDeath.died(evicted)==Intake::conflict);
    CHECK(staleDeath.counts().failed);
    auto staleRetirement=sustained;
    CHECK(staleRetirement.actor_retired(evicted)==Intake::conflict);
    CHECK(staleRetirement.counts().failed);
    auto reusedNonce=sustained;
    const c::PopulationActor forgedNonce{owner,0x51FFF,0x61FFF,latest.birthNonce};
    CHECK(reusedNonce.admitted(forgedNonce,1)==Intake::conflict);
    CHECK(reusedNonce.counts().failed);
    CHECK(!sustained.renew(owner,sameRun)); // the one survivor blocks history reset
    CHECK(sustained.died(survivor)==Intake::accepted);
    CHECK(sustained.actor_retired(survivor)==Intake::accepted);
    CHECK(sustained.counts().admitted==301 && sustained.counts().dead==301);
    CHECK(sustained.renew(owner,sameRun));
    CHECK(sustained.counts().admitted==0 && sustained.counts().dead==0);

    // Source recreation forgets only streamed survivors. Compacted casualties
    // and their cumulative category totals survive the streaming boundary.
    c::NativePopulationLedger<3> compactedStream;
    CHECK(compactedStream.begin(owner));
    const c::PopulationActor streamedSurvivor{owner,0x51501,0x61501,1};
    CHECK(compactedStream.admitted(streamedSurvivor,0)==Intake::accepted);
    for(std::uint64_t i=0;i<5;++i) {
        const c::PopulationActor casualty{owner,0x51502,0x61502,i+2};
        CHECK(compactedStream.admitted(casualty,1)==Intake::accepted);
        CHECK(compactedStream.died(casualty)==Intake::accepted);
        CHECK(compactedStream.actor_retired(casualty)==Intake::accepted);
    }
    CHECK(compactedStream.actor_retired(streamedSurvivor)==Intake::accepted);
    CHECK(compactedStream.source_recreated(owner));
    CHECK(compactedStream.counts().admitted==5 && compactedStream.counts().dead==5);
    CHECK(compactedStream.counts(0).admitted==0 && compactedStream.counts(0).dead==0);
    CHECK(compactedStream.counts(1).admitted==5 && compactedStream.counts(1).dead==5);

    // Capacity pressure must never evict a live actor, a dead actor without its
    // retirement receipt, or a retired survivor that was not observed dead.
    c::NativePopulationLedger<3> bounded;
    CHECK(bounded.begin(owner));
    const c::PopulationActor live{owner,0x52001,0x62001,1};
    const c::PopulationActor deadUnretired{owner,0x52002,0x62002,2};
    const c::PopulationActor retiredAlive{owner,0x52003,0x62003,3};
    CHECK(bounded.admitted(live,0)==Intake::accepted);
    CHECK(bounded.admitted(deadUnretired,0)==Intake::accepted);
    CHECK(bounded.died(deadUnretired)==Intake::accepted);
    CHECK(bounded.admitted(retiredAlive,1)==Intake::accepted);
    CHECK(bounded.actor_retired(retiredAlive)==Intake::accepted);
    CHECK(bounded.admitted({owner,0x52004,0x62004,4},1)==Intake::overflow);
    CHECK(bounded.counts().failed);

    // A zero creation receipt deliberately keeps the old conservative handle
    // identity even after both terminal observations arrive.
    c::NativePopulationLedger<2> legacyReuse;
    const c::PopulationActor legacy{owner,0x53001,0x63001,0};
    CHECK(legacyReuse.begin(owner));CHECK(legacyReuse.admitted(legacy,0)==Intake::accepted);
    CHECK(legacyReuse.died(legacy)==Intake::accepted);
    CHECK(legacyReuse.actor_retired(legacy)==Intake::accepted);
    CHECK(legacyReuse.admitted({owner,legacy.actor,legacy.entity,9},0)==Intake::conflict);
    CHECK(legacyReuse.counts().failed);

    // The stale check must run again after a capacity-triggered compaction,
    // because that compaction can advance the monotonic replay floor.
    c::NativePopulationLedger<2> postCompactReplay;
    CHECK(postCompactReplay.begin(owner));
    const c::PopulationActor floorSurvivor{owner,0x54001,0x64001,10};
    const c::PopulationActor completed20{owner,0x54002,0x64002,20};
    const c::PopulationActor completed30{owner,0x54003,0x64003,30};
    const c::PopulationActor completed40{owner,0x54004,0x64004,40};
    CHECK(postCompactReplay.admitted(floorSurvivor,0)==Intake::accepted);
    CHECK(postCompactReplay.admitted(completed20,0)==Intake::accepted);
    CHECK(postCompactReplay.died(completed20)==Intake::accepted);
    CHECK(postCompactReplay.actor_retired(completed20)==Intake::accepted);
    CHECK(postCompactReplay.admitted(completed30,0)==Intake::accepted);
    CHECK(postCompactReplay.died(completed30)==Intake::accepted);
    CHECK(postCompactReplay.actor_retired(completed30)==Intake::accepted);
    CHECK(postCompactReplay.admitted(completed40,0)==Intake::accepted);
    CHECK(postCompactReplay.died(completed40)==Intake::accepted);
    CHECK(postCompactReplay.actor_retired(completed40)==Intake::accepted);
    CHECK(postCompactReplay.admitted({owner,0x54FFF,0x64FFF,15},0)==Intake::conflict);
    CHECK(postCompactReplay.counts().failed);
    // A long session may renew native source generations only after native
    // source quiescence AND actor retirement. This is not a fixed respawn limit.
    for(unsigned i=0;i<500;++i) {
        ++next.generation;++next.incarnation;CHECK(ledger.begin(next));
        const c::PopulationActor actor{next,0x10000+i,0x20000+i};
        CHECK(ledger.admitted(actor)==Intake::accepted);CHECK(ledger.died(actor)==Intake::accepted);
        CHECK(ledger.retiring(next));CHECK(ledger.actor_retired(actor)==Intake::accepted);
        CHECK(ledger.phase()==c::PopulationPhase::retiring);CHECK(ledger.source_retired(next)==Intake::accepted);
        CHECK(ledger.phase()==c::PopulationPhase::retired);
    }
    c::NativePopulationLedger<1> overflow;
    CHECK(overflow.begin(owner));CHECK(overflow.admitted(first)==Intake::accepted);
    CHECK(overflow.admitted(second)==Intake::overflow);CHECK(overflow.counts().failed);
    CHECK(overflow.retiring(owner));CHECK(overflow.actor_retired(first)==Intake::accepted);
    CHECK(overflow.source_retired(owner)==Intake::accepted);CHECK(!overflow.begin(next));
    c::NativePopulationLedger<3> conflict;
    CHECK(conflict.begin(owner));CHECK(conflict.admitted(first)==Intake::accepted);
    auto reused=first;++reused.entity;CHECK(conflict.admitted(reused)==Intake::conflict);CHECK(conflict.counts().failed);
    c::NativePopulationLedger<3> late;
    CHECK(late.begin(owner));CHECK(late.retiring(owner));CHECK(late.source_retired(owner)==Intake::accepted);
    CHECK(late.admitted(first)==Intake::closed);CHECK(late.counts().failed);CHECK(!late.begin(next));
    // Stream out after one kill: only the surviving actor is replaced. Returning
    // repeatedly must neither fill the ledger nor turn streaming into kills.
    c::NativePopulationLedger<3> streamed;
    CHECK(streamed.begin(owner));CHECK(streamed.admitted(first,0)==Intake::accepted);
    CHECK(streamed.admitted(second,2)==Intake::accepted);
    CHECK(!streamed.source_recreated(owner));
    CHECK(streamed.died(first)==Intake::accepted);CHECK(streamed.actor_retired(first)==Intake::accepted);
    CHECK(!streamed.source_recreated(owner));CHECK(streamed.actor_retired(second)==Intake::accepted);
    std::array<std::uint8_t,8> survivors{};
    CHECK(!streamed.source_recreated(next,survivors));CHECK(streamed.source_recreated(owner,survivors));
    CHECK(survivors[0]==0 && survivors[2]==1);
    CHECK(streamed.counts().admitted==1 && streamed.counts().dead==1 && streamed.counts().resident==0);
    CHECK(streamed.died(second)==Intake::unknown);CHECK(streamed.died(first)==Intake::duplicate);
    for(unsigned i=0;i<500;++i) {
        const c::PopulationActor replacement{owner,0x80000+i,0x90000+i};
        CHECK(streamed.admitted(replacement,2)==Intake::accepted);
        CHECK(streamed.counts().alive==1 && streamed.counts().dead==1);
        CHECK(streamed.actor_retired(replacement)==Intake::accepted);
        CHECK(streamed.source_recreated(owner));
        CHECK(streamed.counts().admitted==1 && !streamed.counts().failed);
    }
    const c::PopulationActor replacement{owner,0xA0000,0xB0000};
    CHECK(streamed.admitted(replacement,2)==Intake::accepted);CHECK(streamed.died(replacement)==Intake::accepted);
    CHECK(streamed.actor_retired(replacement)==Intake::accepted);CHECK(streamed.source_recreated(owner));
    CHECK(streamed.counts().dead==2 && streamed.counts().admitted==2);
    CHECK(streamed.renew(owner,sameRun));CHECK(streamed.counts().admitted==0);
    // The legacy recreation callback has no exact member-category evidence.
    // It may forget a retired survivor, but cannot issue replacement quota.
    c::NativePopulationLedger<2> legacyStreamed;CHECK(legacyStreamed.begin(owner));
    const c::PopulationActor unknownCategory{owner,0xC0000,0xD0000,77};
    CHECK(legacyStreamed.admitted(unknownCategory)==Intake::accepted);
    CHECK(legacyStreamed.actor_retired(unknownCategory)==Intake::accepted);
    std::array<std::uint8_t,8> exactUnknown{};
    CHECK(!legacyStreamed.source_recreated(owner,exactUnknown));
    CHECK(legacyStreamed.source_recreated(owner));
    CHECK(legacyStreamed.counts().admitted==0 && !legacyStreamed.counts().failed);
    CHECK(!late.source_recreated(owner));CHECK(!conflict.source_recreated(owner));
}
