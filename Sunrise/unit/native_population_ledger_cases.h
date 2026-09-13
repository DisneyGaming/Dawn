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
}
