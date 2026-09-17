#include "state/activity/native_population_events.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace events=dawn::state::activity::native_population;
namespace coo=dawn::state::activity::coo;
namespace {
unsigned checks{};
void check(bool value,int line) {++checks;if(!value){std::printf("FAIL line %d\n",line);std::exit(1);}}
#define CHECK(value) check((value),__LINE__)

events::Lease lease(std::uint32_t registry,std::uint32_t generation=1) {
    const dawn::state::activity::ActivityInstanceKey owner{42,{7}};
    return {owner,{owner.sessionId,91,owner.incarnation.value,
        {registry,static_cast<std::uint32_t>(registry^0x0F0F0F0FU),1,0},generation},15};
}
events::Event event(const events::Lease& owner,std::uint32_t actor,events::Kind kind,
    std::uint32_t entity=UINT32_MAX) {
    return {owner,{owner.source,actor,entity},static_cast<std::uint32_t>(actor+0x10000U),kind};
}

void provisional_blocks_renewal() {
    events::Mailbox mailbox;const auto a=lease(0x74337EDDU);CHECK(mailbox.bind(a));
    const auto creation=mailbox.begin_creation();CHECK(static_cast<bool>(creation));
    auto provisional=event(a,0x1001,events::Kind::admitted);
    provisional.actor.birthNonce=creation.nonce;
    events::Receipt receipt;
    CHECK(mailbox.stage(creation,provisional,receipt)==events::StageResult::staged);
    CHECK(receipt && mailbox.provisional(receipt,provisional));
    CHECK(mailbox.pending_lease(a) && mailbox.pending(a.activity));
    auto next=a;++next.source.generation;
    CHECK(mailbox.renew(a,next)==events::RenewResult::busy);
    CHECK(mailbox.capture(a)==receipt && !mailbox.capture(next));
}

void unrelated_receipt_survives_renewal() {
    events::Mailbox mailbox;const auto a=lease(0x74337EDDU),b=lease(0x564C6ECEU);
    CHECK(mailbox.bind(a) && mailbox.bind(b));const auto bReceipt=mailbox.capture(b);CHECK(static_cast<bool>(bReceipt));
    const std::array both{a,b};CHECK(mailbox.quiescent(both));
    auto next=a;++next.source.generation;
    CHECK(mailbox.renew(a,next)==events::RenewResult::renewed);
    CHECK(mailbox.capture(b)==bReceipt);
    const auto death=event(b,0x2001,events::Kind::died,0x3001);
    CHECK(mailbox.submit(death,bReceipt));
    CHECK(!mailbox.quiescent(both));
    std::array<events::Event,4> drained{};CHECK(mailbox.drain(a.activity,drained)==1);
    CHECK(drained[0].lease==b && drained[0].kind==events::Kind::died);
}

void complete_lifecycle() {
    events::Mailbox mailbox;const auto a=lease(0xEB1E8934U);CHECK(mailbox.bind(a));
    const auto creation=mailbox.begin_creation();CHECK(static_cast<bool>(creation));
    auto provisional=event(a,0x3101,events::Kind::admitted);provisional.actor.birthNonce=creation.nonce;
    events::Receipt receipt;CHECK(mailbox.stage(creation,provisional,receipt)==events::StageResult::staged);
    auto admission=provisional;admission.actor.entity=0x4101;
    CHECK(mailbox.admit(receipt,admission)==events::AdmitResult::admitted);
    CHECK(!mailbox.provisional(receipt,provisional));
    auto death=admission;death.kind=events::Kind::died;CHECK(mailbox.submit(death,receipt));
    auto retirement=admission;retirement.kind=events::Kind::retired;CHECK(mailbox.submit(retirement,receipt));
    std::array<events::Event,4> drained{};CHECK(mailbox.drain(a.activity,drained)==3);
    CHECK(drained[0].kind==events::Kind::admitted && drained[1].kind==events::Kind::died
        && drained[2].kind==events::Kind::retired);
    coo::NativePopulationLedger<4> ledger;CHECK(ledger.begin(a.source));
    CHECK(ledger.admitted(drained[0].actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.died(drained[1].actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.actor_retired(drained[2].actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.counts().dead==1 && ledger.counts().alive==0 && ledger.counts().resident==0);
    CHECK(!mailbox.pending_lease(a));
}

void aba_is_rejected() {
    events::Mailbox mailbox;const auto a=lease(0x2571C34DU);CHECK(mailbox.bind(a));
    const auto oldReceipt=mailbox.capture(a);const auto creation=mailbox.begin_creation();
    CHECK(static_cast<bool>(oldReceipt) && static_cast<bool>(creation));
    mailbox.release(a.activity);CHECK(mailbox.bind(a));const auto fresh=mailbox.capture(a);
    CHECK(fresh && fresh!=oldReceipt);
    const auto complete=event(a,0x5101,events::Kind::admitted,0x6101);
    CHECK(!mailbox.submit(complete,oldReceipt));
    events::Receipt staged;
    auto provisional=event(a,0x5102,events::Kind::admitted);provisional.actor.birthNonce=creation.nonce;
    CHECK(mailbox.stage(creation,provisional,staged)==events::StageResult::ended);
    CHECK(!staged && !mailbox.overflow() && !mailbox.pending_lease(a));
    CHECK(mailbox.submit(complete,fresh));
}

void inflight_and_burst_are_retryable() {
    events::Mailbox mailbox;const auto a=lease(0x4A3E4900U);CHECK(mailbox.bind(a));
    const auto creation=mailbox.begin_creation();CHECK(static_cast<bool>(creation));
    auto next=a;++next.source.generation;
    CHECK(mailbox.renew(a,next)==events::RenewResult::busy);
    mailbox.cancel(creation);CHECK(mailbox.renew(a,next)==events::RenewResult::renewed);
    const auto receipt=mailbox.capture(next);CHECK(static_cast<bool>(receipt));
    for(std::uint32_t i=0;i<65;++i)
        CHECK(mailbox.submit(event(next,0x7000U+i,events::Kind::died,0x8000U+i),receipt));
    std::array<events::Event,64> first{};CHECK(mailbox.drain(next.activity,first)==64);
    CHECK(mailbox.pending_lease(next) && !mailbox.overflow());
    std::array<events::Event,2> last{};CHECK(mailbox.drain(next.activity,last)==1);
    CHECK(!mailbox.pending_lease(next) && last[0].actor.actor==0x7040U);
}
}

int main() {
    provisional_blocks_renewal();unrelated_receipt_survives_renewal();complete_lifecycle();
    aba_is_rejected();inflight_and_burst_are_retryable();
    std::printf("native_population_bridge_tests: %u checks passed\n",checks);
}
