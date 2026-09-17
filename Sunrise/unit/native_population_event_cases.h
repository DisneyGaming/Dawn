#pragma once
#include "state/activity/native_population_events.h"
#include "client/hooks/bootflow/native_population_pending.h"
#include "client/hooks/bootflow/native_population_retirement.h"
#include "client/hooks/bootflow/native_population_streaming.h"
void native_population_event_cases() {
    namespace events=sunrise::state::activity::native_population;
    events::Mailbox mailbox;
    const events::Lease lease{{42,{1}},{42,123,1,{0x74337EDD,0x80F5B68E,1,1},1},15};
    {
        events::Mailbox births;CHECK(births.bind(lease));
        auto ticket=births.begin_creation();events::Receipt receipt;
        events::Event provisional{lease,{lease.source,100,UINT32_MAX,ticket.nonce+1},200,events::Kind::admitted};
        CHECK(births.stage(ticket,provisional,receipt)==events::StageResult::rejected);
        ticket=births.begin_creation();provisional.actor.birthNonce=0;
        CHECK(births.stage(ticket,provisional,receipt)==events::StageResult::rejected);
        ticket=births.begin_creation();provisional.actor.birthNonce=ticket.nonce;
        CHECK(births.stage(ticket,provisional,receipt)==events::StageResult::staged);
        const auto secondTicket=births.begin_creation();auto reused=provisional;reused.actor.birthNonce=secondTicket.nonce;
        events::Receipt rejected;
        CHECK(births.stage(secondTicket,reused,rejected)==events::StageResult::rejected);
        CHECK(births.provisional(receipt,provisional));
        auto forged=provisional;++forged.actor.birthNonce;forged.actor.entity=300;
        CHECK(!births.provisional(receipt,forged));
        CHECK(births.admit(receipt,forged)==events::AdmitResult::rejected);
        forged.actor.birthNonce=0;CHECK(births.admit(receipt,forged)==events::AdmitResult::rejected);
        auto complete=provisional;complete.actor.entity=300;complete.memberCategory=1;
        CHECK(births.admit(receipt,complete)==events::AdmitResult::admitted);
        auto death=complete;death.kind=events::Kind::died;
        auto retired=complete;retired.kind=events::Kind::retired;
        CHECK(births.submit(death,receipt));CHECK(births.submit(retired,receipt));
        std::array<events::Event,3> output{};CHECK(births.drain(lease.activity,output)==3);
        CHECK(output[0].actor==output[1].actor && output[1].actor==output[2].actor);
        CHECK(output[0].actor.birthNonce==ticket.nonce && output[0].memberCategory==1);
    }
    {
        namespace stream=sunrise::client::hooks::bootflow::native_population_streaming;
        const stream::Counters saved{1,4,1,0},fresh{1,4,0,0};
        CHECK(stream::restore(saved,fresh,true,true));
        CHECK(stream::restore(saved,saved,true,true));
        CHECK(!stream::restore(saved,fresh,false,true));CHECK(!stream::restore(saved,fresh,true,false));
        CHECK(!stream::restore(saved,{1,4,2,0},true,true));
        CHECK(!stream::restore(saved,{1,4,0,1},true,true));
        CHECK(!stream::restore(saved,{1,5,0,0},true,true));
        CHECK(!stream::restore({1,4,5,0},fresh,true,true));
        CHECK(!stream::restore({2,4,1,0},fresh,true,true));
        CHECK(!stream::restore({1,4,-1,0},fresh,true,true));
        const stream::Counters savedTwo{2,4,1,0,3,2,0},freshTwo{2,4,0,0,3,0,0};
        CHECK(stream::restore(savedTwo,freshTwo,true,true));
        CHECK(stream::restore(savedTwo,savedTwo,true,true));
        CHECK(!stream::restore(savedTwo,{2,4,0,0,4,0,0},true,true));
        CHECK(!stream::restore(savedTwo,{2,4,0,0,3,1,0},true,true));
        CHECK(!stream::restore(savedTwo,{2,4,0,0,3,0,1},true,true));
        CHECK(stream::local_facet(0,-1,0,0));CHECK(!stream::local_facet(0,-2,0,0));
        CHECK(!stream::local_facet(1,-1,0,0));CHECK(!stream::local_facet(0,-1,4,0));
        CHECK(!stream::local_facet(0,-1,0,1));
        CHECK(stream::retained_facet(0,-1,0,0));CHECK(stream::retained_facet(0,-2,0,0));
        CHECK(!stream::retained_facet(0,0,0,0));CHECK(!stream::retained_facet(0,-3,0,0));
        CHECK(!stream::retained_facet(1,-2,0,0));CHECK(!stream::retained_facet(0,-2,4,0));
        CHECK(!stream::retained_facet(0,-2,0,1));
        events::Mailbox streaming;auto opted=lease;opted.discardStreamedReplicas=true;
        CHECK(streaming.bind(opted));const auto receipt=streaming.capture(opted);
        const events::Event recreated{opted,{opted.source},0x4567,events::Kind::sourceRecreated,0x3456};
        CHECK(streaming.submit(recreated,receipt));
        auto bad=recreated;bad.previousSourceHandle=bad.sourceHandle;CHECK(!streaming.submit(bad,receipt));
        bad=recreated;bad.previousSourceHandle=UINT32_MAX;CHECK(!streaming.submit(bad,receipt));
        bad=recreated;bad.actor.actor=0x1234;CHECK(!streaming.submit(bad,receipt));
        bad=recreated;bad.lease.discardStreamedReplicas=false;CHECK(!streaming.submit(bad,receipt));
        auto next=opted;++next.source.generation;
        CHECK(streaming.renew(opted,next)==events::RenewResult::busy);
        std::array<events::Event,2> output{};CHECK(streaming.drain(opted.activity,output)==1);
        CHECK(output[0].kind==events::Kind::sourceRecreated && output[0].previousSourceHandle==0x3456);
        auto noPolicy=next;noPolicy.discardStreamedReplicas=false;CHECK(!streaming.renew(opted,noPolicy));
        CHECK(streaming.renew(opted,next));CHECK(!streaming.submit(recreated,receipt));
        streaming.release(opted.activity);CHECK(streaming.bind(opted));CHECK(!streaming.submit(recreated,receipt));
        events::Mailbox ordinary;CHECK(ordinary.bind(lease));bad=recreated;bad.lease=lease;
        CHECK(!ordinary.submit(bad,ordinary.capture(lease)));
    }
    CHECK(!mailbox.pending(lease.activity));CHECK(!mailbox.pending({}));
    CHECK(mailbox.epoch()==0);CHECK(mailbox.bind(lease));
    const auto epoch=mailbox.epoch();CHECK(mailbox.bind(lease));CHECK(mailbox.epoch()==epoch);
    CHECK(mailbox.lookup(0x80F5B68E,0x74337EDD,1,1)==lease);
    CHECK(!mailbox.lookup(0x80F5B68E,0x74337EDD,1,2).activity);
    const events::Event admission{lease,{lease.source,0x1234,0x2345},0x3456,events::Kind::admitted};
    CHECK(!mailbox.submit(admission,epoch-1));CHECK(mailbox.submit(admission,epoch));
    auto death=admission;death.kind=events::Kind::died;CHECK(mailbox.submit(death,epoch));
    CHECK(mailbox.pending(lease.activity));CHECK(mailbox.pending(lease.activity));
    CHECK(!mailbox.pending({42,{2}}));CHECK(!mailbox.pending({43,{1}}));
    auto wrong=admission;wrong.actor.owner.generation=2;CHECK(!mailbox.submit(wrong,epoch));
    wrong=admission;wrong.sourceHandle=UINT32_MAX;CHECK(!mailbox.submit(wrong,epoch));
    std::array<events::Event,8> copied{};
    CHECK(mailbox.drain({99,{1}},copied)==0);CHECK(mailbox.drain(lease.activity,copied)==2);
    CHECK(copied[0].kind==events::Kind::admitted && copied[1].kind==events::Kind::died);
    CHECK(!mailbox.pending(lease.activity));CHECK(mailbox.epoch()==epoch);
    c::NativePopulationLedger<4> ledger;CHECK(ledger.begin(lease.source));
    CHECK(ledger.died(death.actor)==c::PopulationIntake::unknown);
    CHECK(ledger.admitted(copied[0].actor)==c::PopulationIntake::accepted);
    CHECK(ledger.died(copied[1].actor)==c::PopulationIntake::accepted);
    CHECK(ledger.counts().dead==1 && ledger.counts().resident==1); // Death does not imply retirement.
    CHECK(ledger.died(copied[1].actor)==c::PopulationIntake::duplicate);
    CHECK(mailbox.submit(admission,epoch));CHECK(mailbox.submit(death,epoch));
    CHECK(mailbox.drain(lease.activity,std::span{copied}.first(1))==1);
    CHECK(mailbox.pending(lease.activity));mailbox.release(lease.activity);
    CHECK(!mailbox.pending(lease.activity));
    CHECK(mailbox.drain(lease.activity,copied)==0 && mailbox.epoch()==0);
    CHECK(!mailbox.submit(admission,epoch));
    events::Mailbox renewable;CHECK(renewable.bind(lease));auto renewedLease=lease;++renewedLease.source.generation;
    CHECK(renewable.renew(lease,renewedLease));
    CHECK(!renewable.lookup(0x80F5B68E,0x74337EDD,1,1).activity);
    CHECK(renewable.lookup(0x80F5B68E,0x74337EDD,1,2)==renewedLease);
    auto invalidRenewal=renewedLease;++invalidRenewal.source.generation;invalidRenewal.source.source.type=2;
    CHECK(!renewable.renew(renewedLease,invalidRenewal));
    CHECK(renewable.lookup(0x80F5B68E,0x74337EDD,1,2)==renewedLease);
    events::Mailbox blockedRenewal;CHECK(blockedRenewal.bind(lease));
    CHECK(blockedRenewal.submit(admission,blockedRenewal.epoch()));
    const auto blockedEpoch=blockedRenewal.epoch();
    auto blockedNext=lease;++blockedNext.source.generation;
    CHECK(!blockedRenewal.renew(lease,blockedNext));CHECK(blockedRenewal.epoch()==blockedEpoch);
    CHECK(blockedRenewal.lookup(0x80F5B68E,0x74337EDD,1,1)==lease);
    auto next=lease;next.activity.incarnation.value=2;next.source.incarnation=2;
    CHECK(mailbox.bind(next));CHECK(!mailbox.submit(admission,mailbox.epoch()));
    auto ambiguous=next;ambiguous.activity.sessionId=43;ambiguous.source.activity=43;
    CHECK(mailbox.bind(ambiguous));CHECK(!mailbox.lookup(0x80F5B68E,0x74337EDD,1,1).activity);

    // Native creation precedes entity attachment. It must be retained without
    // relaxing the server mailbox's complete-identity requirement.
    namespace pending=sunrise::client::hooks::bootflow::native_population_pending;
    events::Mailbox staged;CHECK(staged.bind(lease));
    auto unbound=admission;unbound.actor.entity=UINT32_MAX;
    CHECK(!staged.submit(unbound,staged.epoch()));
    pending::Queue<2> births;
    CHECK(births.add({unbound,0x4456})==pending::Intake::accepted);
    CHECK(births.add({unbound,0x4456})==pending::Intake::duplicate);
    CHECK(births.add({unbound,0x4457})==pending::Intake::conflict);
    auto other=unbound;other.actor.actor=0x2234;
    CHECK(births.add({other,0x5456})==pending::Intake::accepted);
    other.actor.actor=0x3234;CHECK(births.add({other,0x6456})==pending::Intake::overflow);
    CHECK(births.size()==2 && births[0].event.actor.entity==UINT32_MAX);
    // A qualified attachment completes the previously captured birth; a death
    // arriving before a frame poll must be queued after that completed identity.
    auto complete=births[0].event;complete.actor.entity=admission.actor.entity;
    CHECK(staged.submit(complete,staged.epoch()));births.erase(0);
    CHECK(births.size()==1 && births[0].event.actor.actor==0x2234);
    CHECK(staged.submit(death,staged.epoch()));
    CHECK(staged.drain(lease.activity,copied)==2);
    CHECK(copied[0].kind==events::Kind::admitted && copied[1].kind==events::Kind::died);
    CHECK(copied[0].actor==copied[1].actor);
    auto bad=unbound;bad.kind=events::Kind::died;
    CHECK(births.add({bad,0x4456})==pending::Intake::invalid);
    CHECK(births.add({unbound,UINT32_MAX})==pending::Intake::invalid);
    // Observed package bodies differ; wrong class/alignment/offset stays rejected.
    CHECK(pending::definition(0x8080948F,0x728,0x8080948F));
    CHECK(pending::definition(0x8080948F,0x878,0x8080948F));
    CHECK(!pending::definition(0x8080948F,0x878,0x80809927));
    CHECK(!pending::definition(0x8080948F,-4,0x8080948F));
    CHECK(!pending::definition(0x8080948F,0x879,0x8080948F));
    CHECK(!pending::definition(0x8080948F,0x100004,0x8080948F));
    staged.observation_lost();CHECK(staged.overflow());

    namespace retirement=sunrise::client::hooks::bootflow::native_population_retirement;
    const retirement::Slot allocated{0x100000,0xA450,0xA440,UINT32_MAX,127};
    auto released=allocated;++released.generation;
    CHECK(retirement::released(allocated,released));
    CHECK(!retirement::released(allocated,allocated));
    auto wrongSlot=released;wrongSlot.base+=0xA450;CHECK(!retirement::released(allocated,wrongSlot));
    wrongSlot=released;wrongSlot.generation+=1;CHECK(!retirement::released(allocated,wrongSlot));
    wrongSlot=released;wrongSlot.generationOffset=wrongSlot.stride;CHECK(!retirement::valid(wrongSlot));
    auto wrapping=allocated;wrapping.generation=UINT32_MAX;released=wrapping;released.generation=0;
    CHECK(retirement::released(wrapping,released));
    unsigned originalCalls{},completed{};
    retirement::forward([&](std::uint32_t actor,std::uint8_t mode) noexcept {
        CHECK(actor==admission.actor.actor && mode==1);CHECK(completed==0);++originalCalls;
    },[&]() noexcept {CHECK(originalCalls==1);++completed;},admission.actor.actor,1);
    CHECK(originalCalls==1 && completed==1);
    events::Mailbox lifecycle;CHECK(lifecycle.bind(lease));
    auto retired=admission;retired.kind=events::Kind::retired;
    CHECK(lifecycle.submit(admission,lifecycle.epoch()));
    CHECK(lifecycle.submit(death,lifecycle.epoch()));CHECK(lifecycle.submit(retired,lifecycle.epoch()));
    CHECK(lifecycle.drain(lease.activity,copied)==3);
    c::NativePopulationLedger<2> completeLedger;CHECK(completeLedger.begin(lease.source));
    CHECK(completeLedger.admitted(copied[0].actor)==c::PopulationIntake::accepted);
    CHECK(completeLedger.died(copied[1].actor)==c::PopulationIntake::accepted);
    CHECK(completeLedger.counts().resident==1);
    CHECK(copied[2].kind==events::Kind::retired);
    CHECK(completeLedger.actor_retired(copied[2].actor)==c::PopulationIntake::accepted);
    CHECK(completeLedger.counts().resident==0 && completeLedger.counts().dead==1);
    CHECK(!completeLedger.counts().sourceRetired); // Actor release is not source quiescence.
    CHECK(retirement::identity(1,2,3,4,1,2,3,4));
    CHECK(retirement::identity(1,2,3,4,1,2,3,UINT32_MAX));
    CHECK(!retirement::identity(1,2,3,4,0x2001,2,3,UINT32_MAX)); // Reused slot, different salt.
    CHECK(!retirement::identity(1,2,3,4,1,5,3,UINT32_MAX));
    CHECK(!retirement::identity(1,2,3,4,1,2,5,UINT32_MAX));
    CHECK(!retirement::identity(1,2,3,4,1,2,3,5));
    CHECK(!retirement::identity(1,UINT32_MAX,3,4,1,UINT32_MAX,3,UINT32_MAX));
}
