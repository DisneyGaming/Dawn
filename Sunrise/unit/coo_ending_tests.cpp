#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include "state/activity/coo/omega_ending_controller.h"
#include "state/activity/coo/omega_projection.h"
#include "middleware/encoding/bit_writer.h"
#include "fixtures/coo_ending_legacy.h"

namespace coo=sunrise::state::activity::coo;
namespace ending=sunrise::state::activity::omega_ending;
namespace old=sunrise::state::activity::frozen_ending;
namespace wire=sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits=sunrise::middleware::encoding::bits;
static unsigned checks{},comparisons{},bodies{};
#define CHECK(value) do { ++checks;if(!(value)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#value);std::exit(1); } } while(false)
constexpr ending::Token kOwner{1,20,0x77F4200CU,1};
old::Token legacy_token(ending::Token token) { return std::bit_cast<old::Token>(token); }
struct Pair final {
    coo::ending::Controller current;
    old::Ending reference;
    std::uint64_t now{};
    void same() {
        ++comparisons;
        const auto a=reference.authority();const auto b=current.authority();
        CHECK(std::bit_cast<ending::Token>(a.token)==b.token);CHECK(a.revision==b.revision);
        CHECK(a.bookendState==b.bookendState);CHECK(a.play==b.play);CHECK(a.started==b.started);
        CHECK(a.complete==b.complete);CHECK(a.failed==b.failed);CHECK(a.arrived==b.arrived);CHECK(a.retireRoster==b.retireRoster);
        CHECK(static_cast<unsigned>(reference.phase())==static_cast<unsigned>(current.phase()));
        CHECK(static_cast<unsigned>(reference.handoff())==static_cast<unsigned>(current.handoff()));
        CHECK(std::bit_cast<ending::Token>(reference.retirement_request())==current.retirement_request());
        CHECK(std::bit_cast<ending::Token>(reference.handoff_request())==current.handoff_request());
        wire::Snapshot x{},y{};x.archiveOmega=y.archiveOmega=true;
        x.omegaEndingRevision=a.revision;x.omegaEndingPlay=a.play;x.omegaEndingState=a.bookendState?1U:0U;
        x.omegaEndingRetire=a.retireRoster;
        x.omegaEndingSeedRuntime=wire::ending_runtime_seed_required(a.bookendState,a.arrived,a.play,a.started,a.failed);
        coo::omega::Frame frame;frame.ending=b;coo::omega::project(frame,y);
        for(const auto registry:{ending::kRegistry,0x82FB58B7U,0x95FB2E01U}) {
            for(const auto type:{std::uint8_t{6},std::uint8_t{18},std::uint8_t{35}}) {
                std::array<std::byte,4096> bx{},by{};bits::Writer wx(bx),wy(by);
                CHECK(wire::write_auth_body(wx,x,registry,type,0,false));CHECK(wire::write_auth_body(wy,y,registry,type,0,false));
                CHECK(wx.bit_count()==wy.bit_count());std::size_t nx{},ny{};
                CHECK(wx.finish(nx) && wy.finish(ny) && nx==ny);CHECK(bx==by);++bodies;
            }
        }
    }
    void drain() { reference.advance(now,false);current.advance(now,false);same(); }
    void request(std::uint64_t time) {
        now=time;CHECK(reference.request(legacy_token(kOwner),now)==current.request(kOwner,now,true));same();
    }
    void advance(std::uint64_t time,bool dialogue) {
        now=time;reference.advance(now,dialogue);current.advance(now,dialogue);same();
    }
    void retire(std::uint64_t time,ending::Token token=kOwner) {
        now=time;CHECK(reference.observe_retirement(legacy_token(token),now)==current.observe_retirement(token,now));drain();
    }
    void arrive(std::uint64_t time,int region=ending::kSlice,ending::Token token=kOwner) {
        now=time;CHECK(reference.observe_arrival(legacy_token(token),region)==current.observe_arrival(token,region,now));drain();
    }
    void camera(std::uint64_t time,std::uint32_t revision,bool active,bool ready,ending::Token token=kOwner) {
        now=time;reference.observe(legacy_token(token),revision,active,ready,now);current.observe(token,revision,active,ready,now);drain();
    }
    void skip(std::uint64_t time) { now=time;CHECK(reference.skip()==current.skip(now));drain(); }
    void claim() { CHECK(reference.claim_handoff(legacy_token(kOwner))==current.claim_handoff(kOwner));same(); }
    void launch(bool queued) {
        CHECK(reference.note_handoff_result(legacy_token(kOwner),queued)==current.note_handoff_result(kOwner,queued,now));drain();
    }
    void prepared(std::uint64_t time=100) {
        request(time);advance(time+1,true);retire(time+2);arrive(time+3);
    }
};
void accepted_timeline() {
    // Reconstructed API inputs from the accepted run's phase/revision log,
    // not a claim that native camera callback packets were captured verbatim.
    Pair p;p.request(723266);p.advance(732516,true);p.retire(733578);p.arrive(736344);
    p.camera(736375,0,false,true);CHECK(p.current.authority().revision==9);
    p.camera(737438,9,true,true);p.skip(761407);CHECK(p.current.authority().revision==10);
    p.camera(762735,10,false,true);CHECK(p.current.authority().revision==11);
    p.claim();p.launch(true);CHECK(p.current.diagnostics().phase==coo::Phase::complete);
    CHECK(p.current.diagnostics().complete==0x7FU);
}
void paths() {
    for(unsigned attempt=0;attempt<3;++attempt) for(unsigned variant=0;variant<8;++variant) {
        Pair p;p.request(100);p.claim();p.skip(101);p.arrive(102);p.retire(103);
        p.advance(104,true);auto stale=kOwner;++stale.epoch;p.retire(105,stale);p.retire(106);p.arrive(107,120);
        p.camera(108,0,false,true);CHECK(!p.current.authority().play);p.arrive(109);
        std::uint64_t now=110;
        for(unsigned n=0;n<attempt;++n) {
            p.camera(now,0,false,true);const auto revision=p.current.authority().revision;
            p.camera(now+999,revision,false,true);p.camera(now+1000,revision,false,true);now+=1001;
            CHECK(p.current.phase()==ending::Phase::preparing);
        }
        p.camera(now,0,true,true);p.camera(now+1,0,false,false);p.camera(now+2,0,false,true);
        const auto revision=p.current.authority().revision;
        p.camera(now+3,revision+1,true,true);p.camera(now+4,revision,true,false);p.camera(now+5,revision,true,true);
        CHECK(p.current.authority().started);
        if(variant&1U) { p.skip(now+6);p.skip(now+7);p.camera(now+8,revision,false,true); }
        const auto terminal=p.current.authority().revision;
        p.camera(now+9,terminal,false,false);CHECK(!p.current.authority().complete);
        if(variant&2U) { p.camera(now+10,terminal,false,true,stale);CHECK(!p.current.authority().complete); }
        p.camera(now+11,terminal,false,true);CHECK(p.current.authority().complete);
        p.camera(now+12,terminal,false,true);p.claim();p.claim();p.launch((variant&4U)==0);
        CHECK(p.current.diagnostics().phase==((variant&4U)?coo::Phase::failed:coo::Phase::complete));
    }
    for(unsigned phase=0;phase<5;++phase) {
        Pair p;p.request(100);
        if(phase>0) { p.advance(101,true); }
        if(phase>1) { p.retire(102); }
        if(phase>2) { p.arrive(103);p.camera(104,0,false,true); }
        if(phase>3) { p.camera(105,p.current.authority().revision,true,true); }
        p.advance(phase==4?300105:120102,false);CHECK(p.current.authority().failed);CHECK(!p.current.handoff_request().valid());
    }
    Pair rejected;rejected.prepared();
    for(unsigned i=0;i<3;++i) {
        const auto now=200U+i*2000U;rejected.camera(now,0,false,true);
        rejected.camera(now+1000,rejected.current.authority().revision,false,true);
    }
    CHECK(rejected.current.authority().failed);
}
void delayed_receipts() {
    coo::ending::Controller c;CHECK(c.request(kOwner,100,true));
    CHECK(!c.observe_retirement(kOwner,101));c.advance(102,true);
    CHECK(c.observe_retirement(kOwner,103));CHECK(!c.authority().bookendState);
    CHECK(!c.observe_arrival(kOwner,ending::kSlice,104));c.advance(105,false);CHECK(c.authority().bookendState);
    // A ready camera recorded before native arrival cannot become eligible later.
    c.observe(kOwner,0,false,true,106);CHECK(c.observe_arrival(kOwner,ending::kSlice,107));c.advance(108,false);
    CHECK(c.authority().arrived && !c.authority().play);
    for(unsigned i=0;i<10000;++i) { c.observe(kOwner,0,false,true,109); }
    CHECK(c.diagnostics().queued==1);c.advance(110,false);CHECK(c.authority().play);
    auto revision=c.authority().revision;
    c.observe(kOwner,revision,true,true,111);c.advance(112,false);CHECK(c.authority().started);
    CHECK(c.skip(113));CHECK(!c.skip(114));c.observe(kOwner,revision,false,true,115);c.advance(116,false);
    CHECK(!c.authority().complete);revision=c.authority().revision;
    c.observe(kOwner,revision,false,true,117);c.advance(118,false);CHECK(c.authority().complete);
    CHECK(!c.note_handoff_result(kOwner,true,119));CHECK(c.claim_handoff(kOwner));
    CHECK(c.note_handoff_result(kOwner,true,120));CHECK(!c.note_handoff_result(kOwner,true,121));
    CHECK(c.handoff()==ending::Handoff::claimed);c.finish_handoff(122);
    CHECK(c.handoff()==ending::Handoff::queued);CHECK(c.diagnostics().phase==coo::Phase::complete);
    const auto incarnation=c.diagnostics().incarnation;c.reset();CHECK(c.request(kOwner,200,true));
    CHECK(c.diagnostics().incarnation>incarnation);CHECK(!c.authority().started);CHECK(c.diagnostics().queued==0);
    c.advance(201,true);CHECK(c.observe_retirement(kOwner,202));c.advance(203,false);
    for(unsigned i=0;i<=coo::ending::Controller::kCapacity;++i) { c.observe(kOwner,0,(i&1U)!=0,true,204+i); }
    c.advance(400,false);CHECK(c.authority().failed);CHECK(c.diagnostics().failure==coo::Failure::queueOverflow);
    c.reset();CHECK(c.request_preview(2,500));CHECK(!c.selected());CHECK(c.authority().bookendState);
}
int main() {
    CHECK(coo::Executor::valid(coo::ending::kDefinition));CHECK(coo::Executor::valid(coo::ending::kRetry));
    accepted_timeline();paths();delayed_receipts();
    std::printf("PASS: %u checks; %u frozen ending comparisons; %u wire bodies; skip/retry/deadline/retirement/handoff/FIFO/reset parity\n",checks,comparisons,bodies);
}
