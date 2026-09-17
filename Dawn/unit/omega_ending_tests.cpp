#define main previous_state_main
#include "omega_mission_state_tests.cpp"
#undef main
#include "state/activity/omega/omega_ending_runtime.h"
#include "state/activity/omega/omega_ending_authority.h"
#include "state/activity/omega/omega_transit_authority.h"
#include "state/activity/membership/transactions/internal.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include "core/logging/log.h"
namespace dawn::core::log { void write(Channel,Level,std::string_view) noexcept {} }
namespace a=dawn::state::activity;
namespace e=a::omega::ending;
namespace p=a::omega::mission_presentation;
namespace t=a::omega::transit;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace bits=dawn::middleware::encoding::bits;
std::uint64_t take(bits::Reader& reader,std::uint8_t n) {std::uint64_t value{};check(reader.read(n,value),"packet field fits");return value;}
void handshake() {
    for(unsigned previous=0;previous<256;++previous) {
        e::Scope scope{{boss,40},{42,{7}},55};e::Transit tx;
        a::membership::TeleportState local{0,static_cast<std::uint8_t>(previous),-1,0};
        check(!tx.project(scope,true,false,local,112).present,"default idle has no provenance");
        auto busy=local;busy.state=2;
        check(!tx.project(scope,true,true,busy,112).present,"unrelated native transition not overwritten");
        check(!tx.project(scope,false,true,local,112).present,"other activity cannot issue");
        auto result=tx.project(scope,true,true,local,112);
        check(result.present && result.host.state==1 && !result.arrived,"native idle issues host1");
        check(result.host.token && result.host.token!=previous,"fresh command token, including wrap");
        local=result.host;local.state=3;
        for(unsigned field=0;field<8;++field) {
            auto wrong=scope;auto tuple=local;int region=121;bool receipt=true;
            if(field==0) ++wrong.member;if(field==1) ++wrong.activity.incarnation.value;
            if(field==2) ++wrong.token.epoch;if(field==3) ++wrong.token.boss.generation;
            if(field==4) ++tuple.token;if(field==5) ++tuple.sliceSetHash;
            if(field==6) region=112;if(field==7) receipt=false;
            check(!tx.project(wrong,true,receipt,tuple,region).arrived,"foreign receipt cannot arrive");
        }
        result=tx.project(scope,true,true,local,121);
        check(result.arrived && result.host.state==3,"exact native3 and actual121 release once");
        check(!tx.project(scope,true,true,local,121).arrived,"arrival edge once");
        local.state=0;result=tx.project(scope,true,true,local,121);
        check(result.host.state==0 && !tx.pending(),"native reset acknowledges host release");
        for(auto stale:{1,2,3,0}) {local.state=static_cast<std::int8_t>(stale);check(tx.project(scope,true,true,local,121).host.state==0,"delayed native tuples cannot replay");}
    }
    namespace tr=a::membership::transactions;
    a::membership::MembershipState before{};a::membership::AuthoritativeUpdate update{};update.hasTeleport=true;
    const auto idle=tr::merge(before,update);check(idle.hasTeleportReceipt,"explicit zero preserves native provenance");
    check(!tr::equal_authoritative(before,idle),"first explicit zero commits");
    check(tr::merge(idle,{}).hasTeleportReceipt,"sparse updates preserve provenance");
    auto snapshot=tr::make_snapshot(idle,{},1),wrong=snapshot;wrong.hasTeleportReceipt=false;
    check(snapshot.hasTeleportReceipt && !tr::equal(snapshot,wrong),"snapshot exact guard includes provenance");
}
// Drive the production receipt API to accepted dunk, then verify its snapshot
// gates the actual encoded portal body. Each fixture retains the full arm path.
void portal_receipts() {
    for(unsigned cycle=1;cycle<=2;++cycle) {
        Run run;
        run.wave(0,m::Action::left,false);run.wave(1,m::Action::right,false);run.depart(1,false);
        run.wave(2,m::Action::left,false);run.depart(2,false);
        run.wave(3,m::Action::right,false);run.depart(3,false);
        run.wave(4,m::Action::left,false);run.depart(4,false);
        run.wave(5,m::Action::startCycle,false);run.wave(6,m::Action::left,false);run.wave(7,m::Action::right,false);
        if(cycle==2) {
            run.mechanic(1,false,false,0);
            run.wave(8,m::Action::startCycle,false);run.wave(9,m::Action::left,false);run.wave(10,m::Action::right,false);
        }
        auto token=run.claim(m::Action::deletion);
        const std::uint16_t scene=cycle==1?9:27;
        check(run.state.animation(token,m::Animation::started),"portal fixture native deletion starts");
        check(run.state.rescue_started(token,scene),"portal fixture native rescue arrives");
        check(run.state.animation(token,m::Animation::deletionHold),"portal fixture native deletion holds");
        check(run.state.rescue_ready(token,scene),"portal fixture native rescue ready");
        check(run.state.route_arrival(token,false,0x12),"portal fixture native charge platform arrival");
        const m::ChargeReceipt charge{token,0x122,2,0x233,0x12,0x344,cycle==1?0x40BF06U:0x40BF05U,
            static_cast<std::uint16_t>(cycle==1?18:1),static_cast<std::uint16_t>(cycle==1?20:3)};
        check(run.state.pickup(charge) && run.state.dunk(charge),"portal fixture accepted native pickup and dunk");
        token=run.state.snapshot().command.token;
        for(bool backFirst:{false,true}) {
            auto state=run.state;
            for(unsigned count=0;count<3;++count) {
                if(count) {
                    const bool back=count==1?backFirst:!backFirst;
                    const auto slot=static_cast<std::uint16_t>(back?28:27);
                    const auto generation=2U+(back?2U*cycle-1U:1U);
                    auto stale=token;++stale.epoch;
                    check(!state.eye_object(stale,slot,generation,0x567,back?0x678:0x789),"stale eye source receipt rejected");
                    check(!state.eye_object(token,slot,generation+1,0x567,back?0x678:0x789),"wrong eye source generation rejected");
                    check(state.eye_object(token,slot,generation,0x567,back?0x678:0x789),"exact native eye source creation accepted");
                }
                wire::Snapshot snapshot{};snapshot.omegaMission=state.snapshot();
                for(const auto& row:t::sources) if(row.role==t::Role::portal && row.cycle==cycle) {
                    std::array<std::byte,32> bytes{};bits::Writer writer(bytes);
                    check(wire::write_auth_body(writer,snapshot,row.registry,4,row.slot,false),"receipt-backed portal packet encodes");
                    bits::Reader reader(bytes);
                    check(take(reader,32)==0x80000000ULL+(count==2?3U:2U),"only second native platform receipt advances source generation");
                    check(take(reader,32)==0x80000000ULL && take(reader,1)==static_cast<unsigned>(count==2),"missing or single native platform keeps portal inactive");
                }
            }
        }
    }
}
void packets() {
    wire::Snapshot snapshot{};snapshot.omegaEndingSelected=true;snapshot.omegaEndingRevision=2;snapshot.omegaEndingPlay=true;
    std::array<std::byte,256> bytes{};bits::Writer writer(bytes);
    check(wire::auth_body_bits(snapshot,e::kRegistry,6,0,false)==263,"exact cinematic schema width");
    check(wire::write_auth_body(writer,snapshot,e::kRegistry,6,0,false) && writer.bit_count()==263,"production cinematic body");
    bits::Reader reader(bytes);
    check(take(reader,64)==UINT64_MAX && take(reader,64)==0 && take(reader,32)==2,"cinematic original clock defaults and revision");
    check(take(reader,1)==1 && take(reader,1)==0,"play without preload override");
    check(take(reader,32)==0x811C9DC5 && take(reader,7)==0 && take(reader,16)==0x7FFF,"native absent reference");
    check(take(reader,6)==2 && take(reader,5)==0 && take(reader,3)==0 && take(reader,32)==0,"native modes and empty collections");
    for(bool restricted:{false,true}) {
        snapshot={};snapshot.omegaMission.generation=2;snapshot.omegaMission.restriction=restricted;
        bits::Writer director(bytes);check(wire::write_auth_body(director,snapshot,0x4786C0E0,35,1,false),"mission director emits");
        check(director.bit_count()==359,"native director width");bits::Reader r(bytes);
        check(take(r,1)==static_cast<unsigned>(restricted) && take(r,1)==0 && take(r,2)==1 && take(r,2)==0 && take(r,1)==0,"restriction and native selectors");
        check(take(r,64)==0 && take(r,64)==0x134F00C00000ULL && take(r,64)==0 && take(r,64)==0 && take(r,64)==UINT64_MAX && take(r,32)==0x3F800000,"native no-countdown sentinels");
        bits::Writer lifetime(bytes);check(wire::write_auth_body(lifetime,snapshot,0x4786C0E0,17,3,false),"lifetime emits");bits::Reader lr(bytes);check(lr.skip(72),"lifetime prefix");check(take(lr,32)==0x80000000ULL+(restricted?14:0),"scenario ordinal filter14, not packed112");
    }
    for(const auto& row:t::sources) for(unsigned cycle=1;cycle<=3;++cycle) for(unsigned phase=0;phase<3;++phase) {
        snapshot={};auto& s=snapshot.omegaMission;s.generation=2;s.command.cycle=static_cast<std::uint8_t>(cycle);s.phase=m::Phase::route;
        s.chargeEnabled=phase==1;s.chargeDunked=phase==2;s.chargePickedUp=phase>0;s.transitPrepared=0x7F;
        const auto width=row.role==t::Role::sink?375U:252U;
        bits::Writer source(bytes);check(wire::auth_body_bits(snapshot,row.registry,4,row.slot,false)==width
            && wire::write_auth_body(source,snapshot,row.registry,4,row.slot,false) && source.bit_count()==width,"all40 transit source widths match production framing");
        if(row.role==t::Role::sink) {
            check(source.write(0xABCD,16),"following record sentinel fits");
            bits::Reader sr(bytes);check(sr.skip(250),"source prefix precedes native dynamic array");
            check(take(sr,2)==1 && take(sr,1)==1 && take(sr,32)==0x80804FB8U,"sink carries exactly one present interaction override");
            const bool unlocked=cycle==row.cycle && phase==1;
            check(take(sr,2)==(unlocked?3U:2U),"only current active sink unlocked; dormant retired and other cycles locked");
            check(take(sr,32)==0x811C9DC5U && take(sr,7)==0 && take(sr,16)==0x7FFFU,"override retains absent additional predicate");
            check(take(sr,32)==0x80000000U && take(sr,1)==0 && take(sr,16)==0xABCD,"no synthetic use and following record remains aligned");
        }
        if(row.gate) {bits::Writer gate(bytes);check(wire::write_auth_body(gate,snapshot,row.registry,23,row.gate,false) && gate.bit_count()==147,"all18 gate bodies production147");}
    }
    for(const auto& row:t::sources) if(row.role==t::Role::portal) for(unsigned receipts=0;receipts<4;++receipts) for(unsigned step=0;step<6;++step) {
        snapshot={};auto& s=snapshot.omegaMission;s.generation=2;s.command.cycle=row.cycle;
        s.chargeEnabled=step==1 || step==2;s.chargePickedUp=step>=2;s.chargeDunked=step>=3;
        s.phase=step>=3?m::Phase::shield:m::Phase::carrying;s.eyePlatform=step==4;
        s.dpsFrontCreated=(receipts&1)!=0;s.dpsBackCreated=(receipts&2)!=0;
        const bool ready=row.cycle==3 || receipts==3;
        if(step==5) ++s.command.cycle;
        bits::Writer contact(bytes);
        check(wire::write_auth_body(contact,snapshot,row.registry,4,row.slot,false) && contact.bit_count()==252,"all three contact portals retain valid native source framing");
        bits::Reader cr(bytes);
        check(take(cr,32)==0x80000000ULL+(step>=4?4U:step==3 && ready?3U:2U),"transport generation changes only after qualified creation receipts or retirement");
        check(take(cr,32)==0x80000000ULL && take(cr,1)==static_cast<unsigned>(step==3 && ready),"cycles1/2 require both platforms; cycle3 needs no such creation receipts");
    }
    for(const auto& row:t::sources) if(row.role==t::Role::endFx) for(unsigned step=0;step<6;++step) {
        snapshot={};auto& s=snapshot.omegaMission;s.generation=2;s.command.cycle=row.cycle;
        // Dormant, rescue route, pickup, drop, accepted dunk, next cycle.
        s.chargeEnabled=step>=1 && step<=3;s.chargePickedUp=step==2 || step==4;s.chargeDunked=step==4;
        if(step==5) ++s.command.cycle;
        bits::Writer endpoint(bytes);
        check(wire::write_auth_body(endpoint,snapshot,row.registry,4,row.slot,false) && endpoint.bit_count()==252,"all three endpoint sources encode native creation body");
        bits::Reader er(bytes);
        check(take(er,32)==0x80000000ULL+(step==0?2U:step==5?4U:3U),"endpoint created before pickup; pickup/drop/dunk do not respawn it");
        check(take(er,32)==0x80000000ULL && take(er,1)==static_cast<unsigned>(step>=1 && step<=4),"endpoint candidate zero active throughout route and dunk");
        bits::Writer endpointGate(bytes);
        check(wire::write_auth_body(endpointGate,snapshot,row.registry,23,row.gate,false) && endpointGate.bit_count()==147,"endpoint gate available with its source");
        bits::Reader eg(bytes);
        check(take(eg,32)==std::bit_cast<std::uint32_t>(step==5?1.F:0.F),"endpoint keeps original active presentation position until retirement");
        check(take(eg,16)==(step==0?0x7FFFU:step==5?0x8002U:0x8001U),"endpoint gate has stable active revision across pickup and dunk");
    }
    for(const auto& row:t::sources) if(row.role==t::Role::bridge) for(unsigned step=0;step<5;++step) {
        snapshot={};auto& s=snapshot.omegaMission;s.generation=2;s.command.cycle=row.cycle;
        s.chargeEnabled=step==1 || step==2;s.chargePickedUp=step>=2;s.chargeDunked=step==3;
        if(step==4) ++s.command.cycle;
        bits::Writer bridgeGate(bytes);
        check(wire::write_auth_body(bridgeGate,snapshot,row.registry,23,row.gate,false) && bridgeGate.bit_count()==147,"all seven platform gates encode complete channels");
        bits::Reader bridgeReader(bytes);
        const bool visible=step>=1 && step<=3;
        check(take(bridgeReader,32)==std::bit_cast<std::uint32_t>(visible?1.F:0.F),"platform rises for route and stays materialized through pickup and dunk");
        check(take(bridgeReader,16)==(step==0?0x7FFFU:step==4?0x8002U:0x8001U),"stable platform revision until retirement");
        check(take(bridgeReader,1)==0,"platform uses native smooth transition");
        check(take(bridgeReader,32)==std::bit_cast<std::uint32_t>(1.F) && take(bridgeReader,16)==0x7FFFU,"platform power remains untouched");
        check(take(bridgeReader,1)==0 && take(bridgeReader,32)==0 && take(bridgeReader,16)==0x7FFFU && take(bridgeReader,1)==0,"platform lock remains untouched");
    }
    snapshot={};snapshot.omegaMission.generation=2;snapshot.omegaMission.command.cycle=1;snapshot.omegaMission.chargeEnabled=true;
    bits::Writer gate(bytes);check(wire::write_auth_body(gate,snapshot,0x0040BF06,23,34,false),"first ring active");bits::Reader r(bytes);check(take(r,32)==std::bit_cast<std::uint32_t>(.1F) && take(r,16)==0x8001,"ring position .1 and active revision");
    snapshot.omegaMission.chargeEnabled=false;snapshot.omegaMission.chargeDunked=true;bits::Writer retired(bytes);check(wire::write_auth_body(retired,snapshot,0x0040BF06,23,34,false),"ring retired");bits::Reader rr(bytes);check(take(rr,32)==std::bit_cast<std::uint32_t>(.2F) && take(rr,16)==0x8002,"ring retires at .2");
}
m::State ending_state() {
    const unsigned order=0;
    Run run;
    // Independent documented walkthrough: 21 initial, 12 chase, 99 Crown,
    // 11 living escape actors; no clock or synthetic disappearance advances it.
    run.wave(0,m::Action::left,order&1);
    run.wave(1,m::Action::right,!(order&1));
    run.depart(1,order&2); check(run.state.snapshot().cannons==7,"first three cannons activate together");
    run.wave(2,m::Action::left,order&1); run.depart(2,!(order&2));
    run.wave(3,m::Action::right,order&1); run.depart(3,order&2);
    run.wave(4,m::Action::left,order&1); run.depart(4,!(order&2));
    check(run.state.snapshot().cannons==15 && run.state.snapshot().restriction,"Crown arrival enables fourth cannon/restriction");
    run.wave(5,m::Action::startCycle,order&1); run.wave(6,m::Action::left,order&1); run.wave(7,m::Action::right,order&1);
    run.mechanic(1,order&4,order&8,order%3);
    run.wave(8,m::Action::startCycle,order&1); run.wave(9,m::Action::left,order&1); run.wave(10,m::Action::right,order&1);
    run.mechanic(2,!(order&4),!(order&8),order%3);
    auto token=run.claim(m::Action::left);
    check(run.state.animation(token,m::Animation::started),"escape starts from native summon");
    const auto escape=run.actors(11);
    check(run.state.release_arm(token),"escape native clip wrap requests low control");
    check(run.state.arm_applied(token,run.state.snapshot().arm.revision),"living escape permits departure after native low receipt");
    check(!run.state.snapshot().restriction,"second recovery releases restriction");
    run.depart(5,order&2);
    check(run.state.snapshot().restriction,"actual final arrival restores restriction");
    run.wave(12,m::Action::startCycle,order&1); run.wave(13,m::Action::left,order&1); run.wave(14,m::Action::right,order&1);
    run.mechanic(3,order&4,order&8,order%3);
    return run.state;
}
void runtime() {
    m::runtime::state=ending_state();
    const auto state=m::runtime::snapshot(1);p::queue={};
    auto sparse=state;sparse.rescueReadyMask=0;sparse.rescueStartedMask=0;sparse.scenes={};
    auto speech=p::observe(sparse,100);
    check(speech.pending==33,"typed final death requests D20");
    check(!p::dispatch(1,33,2,101),"wrong generation not submitted");
    auto view=e::runtime::update(1,100);check(view.requested && !view.transition,"ending waits for actual D20 submission");
    check(p::dispatch(1,33,1,200),"exact D20 generation submitted");
    check(!p::dispatch(1,33,1,201),"duplicate cannot postpone D20 window");
    check(!e::runtime::update(1,9301).transition,"authored delay plus duration still running");
    view=e::runtime::update(1,9302);check(view.transition,"D20 complete window enables native command");
    const a::ActivityInstanceKey activity{42,{7}};
    auto host=e::runtime::project(1,activity,55,true,true,{},112);
    check(host.present && !e::runtime::snapshot(1).arrived,"advertisement not an arrival receipt");
    check(!e::runtime::resource(view.token,0x10001234),"resource alone cannot start before arrival");
    auto local=host.host;local.state=3;
    check(e::runtime::project(1,activity,55,true,true,local,121).arrived,"native arrival gates resource");
    check(e::runtime::resource(view.token,0x10001234),"full resource owner accepted");
    check(!e::runtime::native(view.token,0x10001234,0x10002345,2,true,false,9400),"inactive before active not completion");
    check(!e::runtime::native(view.token,0x10001234,0x10002345,3,true,true,9400),"foreign play revision rejected");
    check(e::runtime::native(view.token,0x10001234,0x10002345,2,true,true,9400),"native active starts mission ending");
    check(!e::runtime::native(view.token,0x10001234,0x10002345,2,false,false,9500),"unload not movie completion");
    check(!e::runtime::native(view.token,0x10001234,0x10002346,2,true,false,9500),"new component cannot complete old movie");
    check(e::runtime::native(view.token,0x10001234,0x10002345,2,true,false,9500),"same registered native active-to-inactive completes");
    check(m::runtime::snapshot(1).phase==m::Phase::finished,"production ending forwards actual mission completion");
    local.state=0;check(e::runtime::project(1,activity,55,true,true,local,121).host.state==0,"host release survives movie completion");
}
void presentation() {
    p::Queue queue;m::Snapshot s{};s.command.token={boss,7};s.command.cycle=1;s.generation=2;s.phase=m::Phase::deletion;s.scenes[0].generation=2;
    queue.observe(s,100);check(queue.view(1).pending==14,"actual deletion requests panic");
    check(queue.dispatch(1,14,1,100),"panic native submission");s.rescueStartedMask=1;s.phase=m::Phase::rescue;
    queue.observe(s,5959);check(queue.view(1).pending==255,"bank duration separates rescue exchange");
    queue.observe(s,5960);check(queue.view(1).pending==15,"actual Scene readiness requests Osiris");
    check(queue.dispatch(1,15,1,6000),"Osiris submits before hold");check(!(queue.view(1).requested&(UINT64_C(1)<<16)),"hold dialogue needs native holding child");s.rescueReadyMask=1;queue.observe(s,12399);check(queue.view(1).pending==255,"authored exchange interval retained");queue.observe(s,12400);check(queue.view(1).pending==16,"second Osiris line follows accepted interval");
    queue.observe(s,27400);check(queue.view(1).timeouts==1 && queue.view(1).pending==255,"missing native dispatch retires once without retry");queue.observe(s,100000);check(queue.view(1).timeouts==1,"missing dispatch cannot create retry storm");
    s.phase=m::Phase::route;s.chargeEnabled=true;s.chargePlatform=true;queue.observe(s,100001);check(queue.view(1).objective==0x85A8F583 && queue.view(1).pending==18,"Arc objective and actual route line");
    s.phase=m::Phase::eye;queue.observe(s,100002);check(queue.view(1).pending==22 && queue.view(1).objective==0xA41DE99B,"eye retires stale charge prompts");
    s.phase=m::Phase::recovery;queue.observe(s,100003);check(queue.view(1).pending==255,"recovery retires obsolete eye prompt");
    s.endingStarted=true;queue.observe(s,100004);check(queue.view(1).cinematic && queue.view(1).pending==255,"native movie owns remaining audio");
}
int main() {handshake();packets();portal_receipts();presentation();runtime();std::cout<<checks<<" ending/transit checks, "<<failures<<" failures\n";return failures?1:0;}
