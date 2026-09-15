#include "../src/server/runtime/activity/native_capture_bridge.h"
#include "../src/server/runtime/activity/native_capture_runtime.h"
#include <array>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <vector>

namespace f=sunrise::server::runtime::activity::capture_feedback;
namespace activity_clock=sunrise::server::runtime::activity::activity_clock;
int checks{},failures{};
void check(bool value,const char* label) {++checks;if(!value){++failures;std::printf("FAIL %s\n",label);}}
template<class Array,class T> void put(Array& bytes,std::size_t offset,T value) {std::memcpy(bytes.data()+offset,&value,sizeof value);}
std::filesystem::path archivedCapture,archivedDefinition;
template<class Array> void archived(Array& into,const std::filesystem::path& path,std::size_t offset=0) {
    std::ifstream in(path,std::ios::binary);check(in.good(),"archived native fixture opens");
    in.seekg(static_cast<std::streamoff>(offset));in.read(reinterpret_cast<char*>(into.data()),into.size());
    check(in.good(),"complete archived native fixture read");
}
struct Fixture {
    // Native layout/identity constants are from qualified r9 capture20696.
    // Active clocks and progress below are explicitly fabricated UNIT INPUTS;
    // they are never sent to the game or presented as a native observation.
    std::array<std::byte,0x448> source{};
    std::array<std::byte,0x70> sourceDefinition{};
    std::array<std::byte,0xC0> entity{};
    std::array<std::byte,0x10> scene{};
    std::array<std::byte,0x1D0> before{},after{};
    std::array<std::byte,0x80> context{};
    std::uint64_t sequence{33};
    f::Ticket ticket{{{0x9EAA300100200001ULL,{1}},7,4,0x81550015,13},
        {1,2,3,0},{0x34D23982,0x815500A9,4,32},1,0x80C01781,0x815B8B3B,0x4C8,0x248,
        {false,1000.0F/30.0F},{true,{true,0,5385600,0,5385600,12000000,1.0F},false},9};
    Fixture() {
        put(source,0,0x815500A9U);put(source,4,0x80809928U);put(source,8,std::int64_t{0x4C8});
        put(source,0x20,0x26F9204CU);put(source,0x160,0x08F9004CU);put(source,0x164,0x80809927U);
        put(source,0x180,1U);source[0x188]=std::byte{1};put(source,0x2F0,1U);
        put(source,0x440,0x147B06F7U);put(source,0x444,0x7BFAA098U);
        put(sourceDefinition,0,0x815500A9U);put(sourceDefinition,4,0x80809927U);
        put(sourceDefinition,0x30,0x34D23982U);put(sourceDefinition,0x34,std::uint16_t{4});
        put(sourceDefinition,0x36,std::uint16_t{32});put(sourceDefinition,0x48,0x8080992FU);put(sourceDefinition,0x38,13U);
        put(entity,0,0x147B06F7U);put(entity,4,0x00270020U);put(entity,0xC,0x7BFAA098U);put(entity,0x4C,0x36F9E667U);
        put(scene,4,0x80C01781U);
        put(before,0,0x815B8B3BU);put(before,4,0x80804FCBU);put(before,8,std::int64_t{0x248});
        put(before,0x24,0x7DF9E672U);put(before,0x2C,0x7BFAA098U);
        if(!archivedCapture.empty()) {
            archived(source,archivedCapture/"native-component-76.bin");
            archived(sourceDefinition,archivedDefinition/"815500A9.bin",0x4C8);
            archived(entity,archivedCapture/"entity-7BFAA098.bin");
            archived(scene,archivedCapture/"scene-36F9E667.bin");
            archived(before,archivedCapture/"plate-component-815B8B3B-1150.bin");
            archived(context,archivedCapture/"native-context-original.bin");
            check(f::field<std::uint32_t>(source,0x20)==0x26F9204C && f::field<std::uint32_t>(source,0x24)==0,
                "archived source indexed handle is +20, not controller +24");
            check(f::field<std::uint32_t>(before,0x24)==0x7DF9E672,"archived controller independently uses +24");
            check(f::field<std::uint32_t>(sourceDefinition,0x30)==0x34D23982
                && f::field<std::uint16_t>(sourceDefinition,0x36)==32,"real package descriptor owns registry and slot");
        }
        // Only clock/progress below are fabricated UNIT states. Identity,
        // generation, associations and metadata above remain archived native bytes.
        before[0x30]=before[0x38]=std::byte{1};put(before,0x48,std::uint64_t{5385600});
        put(before,0x58,std::uint64_t{5385600});put(before,0x60,std::uint64_t{12000000});put(before,0x68,1.0F);
        after=before;put(after,0x1B8,0.25F);put(after,0x1BC,6.0F);
        context[0x10]=std::byte{1};put(context,0x18,ticket.clockConfiguration.timing);put(context,0x48,0x81550015U);
        put(context,0x50,std::uint64_t{12000000});put(context,0x68,std::uint64_t{312096210});
    }
    f::Capture capture() const {
        return {ticket,source,sourceDefinition,entity,scene,before,after,context,0x26F9204C,0x08F9004C,
            0x7BFAA098,0x36F9E667,0x7DF9E672,0x00FBC000,0x00FBC000,f::kTickRva,sequence};
    }
};
void match_fixture_to_ticket(Fixture& fixture,const f::Ticket& ticket,
    std::uint64_t sequence=1,bool complete=false) {
    fixture.ticket=ticket;
    put(fixture.source,0x180,ticket.generation);put(fixture.source,0x2F0,ticket.generation);
    put(fixture.sourceDefinition,0x30,ticket.source.registry);
    put(fixture.sourceDefinition,0x36,ticket.source.slot);
    put(fixture.sourceDefinition,0x38,static_cast<std::uint32_t>(ticket.domain.bubble));
    put(fixture.context,0x48,ticket.domain.scenario);
    put(fixture.context,0x18,std::bit_cast<std::uint32_t>(ticket.clockConfiguration.timing));
    const auto& state=ticket.requested.clock;
    for(auto* bytes:{&fixture.before,&fixture.after}) {
        (*bytes)[0x30]=std::byte{1};(*bytes)[0x38]=std::byte{1};
        put(*bytes,0x40,state.minimum);put(*bytes,0x48,state.maximum);
        put(*bytes,0x50,state.elapsed);put(*bytes,0x58,state.remaining);
        put(*bytes,0x60,state.anchor);put(*bytes,0x68,state.rate);
        (*bytes)[0x79]=std::byte{};
    }
    if(complete) {put(fixture.after,0x1B8,1.0F);fixture.after[0x79]=std::byte{1};}
    else {put(fixture.after,0x1B8,0.25F);put(fixture.after,0x1BC,6.0F);}
    fixture.context[0x10]=std::byte{1};fixture.context[0x14]=std::byte{static_cast<unsigned char>(ticket.clockConfiguration.field0)};
    fixture.sequence=sequence;
}
void mailbox_rebind_checks() {
    namespace bridge=sunrise::server::runtime::activity::capture_bridge;
    bridge::Mailbox mailbox;
    Fixture first,second,incomplete;
    second.ticket.source.slot=33;put(second.sourceDefinition,0x36,std::uint16_t{33});
    incomplete.ticket.source.slot=34;put(incomplete.sourceDefinition,0x36,std::uint16_t{34});
    check(mailbox.bind(first.ticket) && mailbox.bind(second.ticket) && mailbox.bind(incomplete.ticket),
        "two controllers and an incomplete controller bind independently");
    check(mailbox.submit(first.capture()),"first controller readiness is accepted");
    std::array<bridge::Event,4> events{};
    check(mailbox.drain(first.ticket.domain.owner,events)==1 && events[0].ready,
        "first controller readiness is consumed");
    auto old=first.ticket;put(first.after,0x1B8,1.0F);first.after[0x1BC]=std::byte{};first.after[0x79]=std::byte{1};
    first.sequence=34;check(mailbox.submit(first.capture()),"completed old controller row is accepted");
    check(mailbox.submit(second.capture()),"other controller event remains queued");
    auto fresh=old;fresh.generation++;fresh.token.incarnation++;fresh.requested.clock.anchor++;
    fresh.armEpoch=old.armEpoch+1;
    auto modeChanged=old;modeChanged.generation++;modeChanged.token.incarnation++;
    modeChanged.token.run=old.domain.owner.sessionId;
    modeChanged.runIdentity=f::RunIdentity::activitySession;
    modeChanged.requested.clock.anchor++;modeChanged.armEpoch=old.armEpoch+1;
    check(!mailbox.rebind(old,modeChanged),"same-domain rebind cannot change capture run identity");
    check(mailbox.rebind(old,fresh),"completed row rebinds with a newer generation and token");
    check(mailbox.drain(old.domain.owner,events)==1 && f::same(events[0].observation.ticket,second.ticket)
        && events[0].ready,"rebind purges only queued old events and preserves the other controller");
    auto stale=first.capture();stale.sequence=35;
    check(!mailbox.submit(stale),"old capture observation cannot complete the fresh row");
    check(f::same(mailbox.lookup(fresh.controllerDefinition,fresh.source.registry,fresh.source.slot),fresh),
        "lookup exposes the fresh ticket after rebind");
    match_fixture_to_ticket(first,fresh,1,false);
    check(mailbox.submit(first.capture()),"fresh controller readiness is accepted");
    check(mailbox.drain(fresh.domain.owner,events)==1 && events[0].ready,
        "fresh controller readiness is consumed");
    match_fixture_to_ticket(first,fresh,2,true);
    check(mailbox.submit(first.capture()),"fresh controller accepts its actual completion");
    check(mailbox.drain(fresh.domain.owner,events)==1 && events[0].completed,
        "fresh controller completion is an actual native receipt");

    auto candidate=old;candidate.generation++;candidate.token.incarnation++;
    check(!mailbox.rebind(old,candidate),"stale old ticket cannot be rebound after replacement");
    auto foreign=old;foreign.domain.owner.sessionId++;
    candidate=old;candidate.generation++;
    check(!mailbox.rebind(foreign,candidate),"wrong-owner old ticket cannot rebind");
    auto lower=fresh;lower.generation--;
    check(!mailbox.rebind(fresh,lower),"backward native source generation is rejected");
    auto sameToken=fresh;sameToken.generation++;
    check(!mailbox.rebind(fresh,sameToken),"identical CoO token is rejected");
    auto invalid=fresh;invalid.generation++;invalid.armEpoch=0;
    check(!mailbox.rebind(fresh,invalid),"invalid fresh ticket is rejected");

    check(mailbox.bind(incomplete.ticket),"incomplete row remains available for rejection coverage");
    auto incompleteFresh=incomplete.ticket;incompleteFresh.generation++;incompleteFresh.token.incarnation++;
    incompleteFresh.armEpoch++;
    check(!mailbox.rebind(incomplete.ticket,incompleteFresh),"incomplete prior native state cannot rebind");
}
void runtime_rearm_checks() {
    namespace activity=sunrise::server::runtime::activity;
    namespace coo=sunrise::state::activity::coo;
    namespace registry=coo::registry;
    namespace placement=activity::placement;
    std::array<registry::Slot,2> slots{{
        {32,4,0x80809927,0x8080992E,0x8080992F,0x815500A9},
        {33,4,0x80809927,0x8080992E,0x8080992F,0x815500A9}}};
    registry::Definition registryDefinition{"capture-test",0x81550015,0x34D23982,1,1,13,slots};
    std::array<placement::Capability,2> placements{{
        {&registryDefinition,32,1},{&registryDefinition,33,1}}};
    std::array<activity::native_capture::Binding,2> bindings{{
        {0,0x80C01781,0x815B8B3B,0x4C8,0x248,"duration"},
        {1,0x80C01781,0x815B8B3B,0x4C8,0x248,"duration"}}};
    struct Definition final {
        std::uint8_t bubble{};
        std::string_view clockFrequencyParameter{};
        std::span<const activity::native_capture::Binding> captures{};
        std::span<const placement::Capability> placements{};
    } definition{};
    definition.bubble=13;definition.clockFrequencyParameter="frequency";
    definition.placements=placements;definition.captures=bindings;
    struct Document final {
        coo::script::PolicyValue duration{"duration",8000,false};
        coo::script::Views view{};
        Document() {view.parameters=std::span(&duration,1);}
        [[nodiscard]] const coo::script::Views& views() const noexcept {return view;}
    } document;
    activity::native_capture::Runtime runtime;
    check(runtime.begin(definition,document),"runtime begins two native capture controllers");
    const activity_clock::Owner owner{0xAABBCCDD,{1}};
    const activity_clock::Domain domain{owner,7,1,0x81550015,13};
    activity_clock::Publication clock{domain,{false,1000.0F/30.0F},100};runtime.clock(clock);
    check(runtime.start(0,{1,1,0,0}) && runtime.start(1,{1,1,0,1}),
        "both controllers start under one activity owner");
    placement::wire::Batch beforeStart{};beforeStart.count=2;
    beforeStart.entries[0]={0x34D23982,32,13,{},91};beforeStart.entries[1]={0x34D23982,33,13,{},92};
    check(runtime.append(beforeStart) && beforeStart.entries[0].generation==1
        && beforeStart.entries[1].generation==1,"start projection carries actual current source generations");
    Fixture first,second;match_fixture_to_ticket(first,runtime.state(0).ticket);
    match_fixture_to_ticket(second,runtime.state(1).ticket);
    namespace bridge=sunrise::server::runtime::activity::capture_bridge;
    check(bridge::submit(first.capture()) && bridge::submit(second.capture()),
        "both controllers accept native readiness observations");
    check(runtime.poll([](coo::Event) noexcept {return true;}),"runtime consumes both readiness receipts");
    match_fixture_to_ticket(first,runtime.state(0).ticket,2,true);
    match_fixture_to_ticket(second,runtime.state(1).ticket,2,true);
    check(bridge::submit(first.capture()) && bridge::submit(second.capture()),
        "both controllers accept actual native completion observations");
    check(runtime.poll([](coo::Event) noexcept {return true;}) && runtime.state(0).completed
        && runtime.state(1).completed,"runtime records actual completion for both controllers");
    const auto oldFirst=runtime.state(0).ticket;const auto oldSecond=runtime.state(1).ticket;
    clock.elapsedTicks=200;runtime.clock(clock);
    check(!runtime.rearm(0,{2,2,0,0},2),"rearm rejects a token owned by another activity incarnation");
    check(!runtime.rearm(0,{1,2,0,0},oldFirst.generation),"rearm rejects a backward source generation");
    check(!runtime.rearm(0,oldFirst.token,oldFirst.generation+1),"rearm rejects an identical CoO token");
    check(runtime.rearm(0,{1,2,0,0},oldFirst.generation+1),
        "completed controller re-arms through the locked bridge wrapper");
    check(runtime.state(0).ticket.armEpoch>oldFirst.armEpoch
        && runtime.state(0).firstStartGeneration==oldFirst.generation
        && !runtime.state(0).ready && !runtime.state(0).completed
        && f::same(runtime.state(1).ticket,oldSecond) && runtime.state(1).completed,
        "rearm is row-local, clears evidence, and preserves the original first generation");
    placement::wire::Batch afterRearm{};afterRearm.count=2;
    afterRearm.entries[0]={0x34D23982,32,13,{},99};afterRearm.entries[1]={0x34D23982,33,13,{},99};
    check(runtime.append(afterRearm) && afterRearm.entries[0].generation==oldFirst.generation+1
        && afterRearm.entries[1].generation==oldSecond.generation
        && afterRearm.entries[0].capture.has_value(),
        "projection generation follows the rearmed ticket while the other stays unchanged");
    auto stale=first;stale.ticket=oldFirst;stale.sequence=3;
    check(!bridge::submit(stale.capture()),"old queued/callback identity cannot satisfy rearmed controller");
    match_fixture_to_ticket(first,runtime.state(0).ticket,1,false);
    check(bridge::submit(first.capture()) && runtime.poll([](coo::Event) noexcept {return true;})
        && runtime.state(0).ready && !runtime.state(0).completed,"fresh rearm begins at native readiness");
    match_fixture_to_ticket(first,runtime.state(0).ticket,2,true);
    check(bridge::submit(first.capture()) && runtime.poll([](coo::Event) noexcept {return true;})
        && runtime.state(0).completed,"fresh controller completes only from its current native receipt");
    check(runtime.state(1).completed,"second controller remains completed after one-controller rearm");
    bridge::release(owner);
    activity::native_capture::Runtime sessionRuntime;
    check(sessionRuntime.begin(definition,document,f::RunIdentity::activitySession),
        "runtime accepts explicit activity-session capture identity");
    sessionRuntime.clock(clock);
    check(!sessionRuntime.start(0,{owner.incarnation.value,3,0,3}),
        "explicit session identity rejects an incarnation-run token");
    check(sessionRuntime.start(0,{owner.sessionId,3,0,3}),
        "explicit session identity accepts a session-run token");
    check(sessionRuntime.state(0).ticket.runIdentity==f::RunIdentity::activitySession
        && sessionRuntime.state(0).ticket.token.run==owner.sessionId,
        "runtime ticket records the selected session namespace");
    bridge::release(owner);
}
void run_identity_checks() {
    Fixture fixture;
    auto session=fixture.ticket;session.runIdentity=f::RunIdentity::activitySession;
    session.token.run=session.domain.owner.sessionId;
    check(f::valid(session),"explicit session ticket accepts its session run");
    check(f::token_matches(session.domain,session.token,f::RunIdentity::activitySession),
        "shared validator accepts only the selected session namespace");
    auto incarnation=session;incarnation.token.run=session.domain.owner.incarnation.value;
    check(!f::valid(incarnation),"session ticket rejects the activity-incarnation run");
    auto foreign=session;foreign.token.run++;
    check(!f::valid(foreign),"capture ticket rejects a foreign run");
    auto unknown=session;unknown.runIdentity=static_cast<f::RunIdentity>(99);
    check(!f::valid(unknown),"capture ticket rejects an unknown run identity");
    auto wrongToken=session;wrongToken.token.incarnation=0;
    check(!f::valid(wrongToken),"capture ticket rejects a missing command incarnation");
}
int main(int argc,char** argv) {
    if(argc==3){archivedCapture=argv[1];archivedDefinition=argv[2];}
    Fixture v;f::Observation o{};
    check(f::qualify(v.ticket,v.capture(),o) && !o.completed && o.progress==0.25F,"real tick layout permits readiness only");
    put(v.after,0x1B8,1.0F);put(v.after,0x1BC,0.0F);v.after[0x79]=std::byte{1};
    check(f::qualify(v.ticket,v.capture(),o) && o.completed,"exact native latch edge qualifies completed");
    v.before[0x79]=std::byte{1};
    check(f::qualify(v.ticket,v.capture(),o) && !o.completed,"old completed latch is not new completion");
    v.before[0x79]=std::byte{};
    for(unsigned kind=0;kind<15;++kind) {
        auto bad=v.capture();
        if(kind==0)++bad.ticket.domain.owner.incarnation.value;
        if(kind==1)++bad.ticket.domain.boot;
        if(kind==2)++bad.ticket.domain.epoch;
        if(kind==3)++bad.ticket.token.incarnation;
        if(kind==4)++bad.ticket.source.registry;
        if(kind==5)++bad.ticket.source.slot;
        if(kind==6)++bad.ticket.generation;
        if(kind==7)++bad.ticket.armEpoch;
        if(kind==8)bad.producerRva=0;
        if(kind==9)bad.sequence=0;
        if(kind==10)bad.sourceHandle^=0x200000;
        if(kind==11)bad.controllerHandle^=0x200000;
        if(kind==12)bad.entityHandle^=0x200000;
        if(kind==13)bad.controllerClockContext^=0x200000;
        if(kind==14)bad.before=bad.before.first(0x1CF);
        check(!f::qualify(v.ticket,bad,o),"stale owner/token/source/native capture rejected");
    }
    for(unsigned kind=0;kind<17;++kind) {
        Fixture bad;
        if(kind==0)put(bad.source,0x180,2U);
        if(kind==1)put(bad.source,0x2F0,0U);
        if(kind==2)bad.source[0x188]=std::byte{};
        if(kind==3)put(bad.source,0x440,0x147B06F6U);
        if(kind==4)put(bad.entity,4,4U);
        if(kind==5)put(bad.scene,4,0x80C01780U);
        if(kind==6)put(bad.sourceDefinition,0x38,12U);
        if(kind==7)put(bad.sourceDefinition,0x36,std::uint16_t{33});
        if(kind==8)bad.after[0x30]=std::byte{};
        if(kind==9)put(bad.after,0x60,std::uint64_t{11999999});
        if(kind==10)put(bad.after,0x24,0x7CF9E672U);
        if(kind==11)bad.context[0x10]=std::byte{};
        if(kind==12)put(bad.context,0x18,0.0F);
        if(kind==13)put(bad.context,0x48,0x81550014U);
        if(kind==14)put(bad.context,0x68,UINT64_MAX);
        if(kind==15)put(bad.after,0x1B8,std::numeric_limits<float>::quiet_NaN());
        if(kind==16)put(bad.after,0x1BC,-1.0F);
        check(!f::qualify(bad.ticket,bad.capture(),o),"invalid native linkage/state does not produce receipt");
    }
    namespace bridge=sunrise::server::runtime::activity::capture_bridge;
    Fixture living;bridge::Mailbox mailbox;
    check(mailbox.bind(living.ticket),"bind exact authoritative capture ticket");
    check(mailbox.bind(living.ticket) && mailbox.size()==1,"idempotent binding does not duplicate rows");
    auto conflicting=living.ticket;++conflicting.domain.owner.incarnation.value;
    conflicting.token.run=conflicting.domain.owner.incarnation.value;
    check(!mailbox.bind(conflicting),"same native source cannot bind simultaneous owner");
    check(f::same(mailbox.lookup(0x815B8B3B,0x34D23982,32),living.ticket),"lookup qualified native key");
    check(!f::valid(mailbox.lookup(0x815B8B3B,0x34D23982,33)),"wrong native slot has no ticket");
    check(mailbox.submit(living.capture()),"native readiness queued once");
    check(!mailbox.submit(living.capture()),"replayed native sequence rejected");
    std::array<bridge::Event,4> receipts{};
    check(mailbox.drain({99,{1}},receipts)==0,"wrong owner cannot consume event");
    check(mailbox.drain(living.ticket.domain.owner,receipts)==1 && receipts[0].ready && !receipts[0].completed,
        "ordinary progress emits readiness without completing");
    auto progressed=living.capture();progressed.sequence=34;
    check(!mailbox.submit(progressed),"ordinary subsequent progress is not extra ready event");
    put(living.after,0x1B8,1.0F);put(living.after,0x1BC,0.0F);living.after[0x79]=std::byte{1};
    auto finished=living.capture();finished.sequence=35;
    check(mailbox.submit(finished),"native completion edge emitted");
    check(mailbox.drain(living.ticket.domain.owner,receipts)==1 && !receipts[0].ready && receipts[0].completed,
        "completion follows existing native readiness");
    finished.sequence=36;check(!mailbox.submit(finished),"completion is one-shot");
    mailbox.release(living.ticket.domain.owner);check(mailbox.size()==0,"retirement removes source association");
    ++living.ticket.armEpoch;check(mailbox.bind(living.ticket),"fresh arm can bind after retirement");
    finished.sequence=37;check(!mailbox.submit(finished),"late callback retains old arm and cannot satisfy new one");
    mailbox.release(living.ticket.domain.owner);
    check(mailbox.drain(living.ticket.domain.owner,receipts)==0,"retirement clears pending events");
    // Regression: launch1 completes; launch2/3 reuse the same bounded row.
    // New native identity and a restarted per-producer sequence must be accepted.
    bridge::Mailbox reruns;
    for(unsigned run=0;run<3;++run) {
        Fixture fresh;fresh.ticket.domain.owner.sessionId+=run;
        fresh.ticket.domain.epoch+=run;fresh.ticket.armEpoch+=run;
        check(reruns.bind(fresh.ticket),"new run binds after prior completion retirement");
        check(reruns.interested(fresh.ticket.controllerDefinition),"new run observes controller again");
        check(f::same(reruns.lookup(fresh.ticket.controllerDefinition,fresh.ticket.source.registry,32),fresh.ticket),
            "lookup returns new owner ticket rather than a retired completion");
        auto ready=fresh.capture();ready.sequence=1;
        check(reruns.submit(ready),"fresh low sequence emits new readiness");
        check(reruns.drain(fresh.ticket.domain.owner,receipts)==1 && receipts[0].ready && !receipts[0].completed,
            "each run receives its own readiness");
        put(fresh.after,0x1B8,1.0F);put(fresh.after,0x1BC,0.0F);fresh.after[0x79]=std::byte{1};
        auto complete=fresh.capture();complete.sequence=2;
        check(reruns.submit(complete),"each run accepts original completion edge");
        // Leave completion queued; release must clear its identity too.
        reruns.release(fresh.ticket.domain.owner);
        check(reruns.size()==0 && !reruns.interested(fresh.ticket.controllerDefinition),"retirement clears active identity");
        check(reruns.drain(fresh.ticket.domain.owner,receipts)==0,"retirement clears undrained completion");
        check(!reruns.submit(complete),"retired callback cannot refill queue");
    }
    // Remove a middle row, preserve moved tail state/events, then bind in the
    // vacated tail. Removing one owner must not reset another owner's readiness.
    bridge::Mailbox compact;
    std::array<Fixture,3> entries{};
    for(std::size_t i=0;i<entries.size();++i) {
        auto& item=entries[i];item.ticket.domain.owner.sessionId+=i;
        item.ticket.source.slot=static_cast<std::uint16_t>(32+i);
        put(item.sourceDefinition,0x36,item.ticket.source.slot);
        check(compact.bind(item.ticket),"distinct native source binds");
        check(compact.submit(item.capture()),"distinct native source ready");
    }
    compact.release(entries[1].ticket.domain.owner);
    check(compact.size()==2,"middle removal compacts exactly one row");
    check(compact.drain(entries[1].ticket.domain.owner,receipts)==0,"removed owner events gone");
    for(const std::size_t i:{0U,2U}) {
        const auto& item=entries[i];
        check(f::same(compact.lookup(item.ticket.controllerDefinition,item.ticket.source.registry,item.ticket.source.slot),item.ticket),
            "retained row identity survives middle compaction");
        check(compact.drain(item.ticket.domain.owner,receipts)==1 && receipts[0].ready,
            "retained readiness survives middle compaction");
    }
    Fixture replacement;replacement.ticket.domain.owner.sessionId+=9;replacement.ticket.source.slot=33;
    replacement.ticket.armEpoch+=9;put(replacement.sourceDefinition,0x36,std::uint16_t{33});
    check(compact.bind(replacement.ticket),"vacated tail admits replacement");
    auto replacementReady=replacement.capture();replacementReady.sequence=1;
    check(compact.submit(replacementReady),"vacated tail has no stale identity or sequence");
    check(compact.drain(replacement.ticket.domain.owner,receipts)==1 && receipts[0].ready,
        "replacement emits its own readiness");
    for(auto& item:entries)compact.release(item.ticket.domain.owner);
    check(compact.size()==1 && compact.interested(replacement.ticket.controllerDefinition),"unrelated owner stays active");
    compact.release(replacement.ticket.domain.owner);check(compact.size()==0,"final tail retirement is empty");
    mailbox_rebind_checks();
    runtime_rearm_checks();
    run_identity_checks();
    std::printf("native capture feedback: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
