#include "../src/server/runtime/activity/mission_device_pose_service.h"
#include "../src/server/runtime/activity/mission_capture_service.h"
#include "../src/state/activity/coo/native_device_authority.h"
#include "../src/middleware/encoding/bit_writer.h"
#include "../src/middleware/encoding/bit_reader.h"
#include <array>
#include <cstdio>
#include <limits>
namespace pose=sunrise::server::runtime::activity::mission_device_pose;
namespace wire=sunrise::middleware::bap::activity_message::native::generic_device;
namespace bits=sunrise::middleware::encoding::bits;
int checks{},failures{};
void check(bool value,const char* label) {++checks;if(!value){++failures;std::printf("FAIL %s\n",label);}}
int main() {
    pose::Publication p;
    check(pose::desire(p,.1F) && p.state.position.revision==0 && p.state.position.snapRevision==0,"initial native signed revisions");
    const auto first=p.state;
    check(pose::desire(p,.1F) && p.state==first,"cadence does not advance command");
    check(pose::observe(p,{0.F,0.F,700,900}) && p.state.position.revision==701 && p.state.position.snapRevision==901,"seed from larger authored native revisions");
    const auto seeded=p.state;
    for(unsigned i=0;i<10000;++i)check(pose::observe(p,{0.F,0.F,700,900}) && p.state==seeded,"pending revision survives duplicate drift");
    check(pose::observe(p,{.1F,.1F,701,901}) && p.state==seeded,"native acceptance does not churn revisions");
    check(pose::observe(p,{.1F,0.F,701,901}) && p.state.position.revision==702 && p.state.position.snapRevision==902,"target drift repaired even when current matches");
    check(pose::observe(p,{.1F,.1F,800,950}) && p.state.position.revision==702,"matching native pose records high water without redundant correction");
    check(pose::desire(p,.2F) && p.state.position.revision==801 && p.state.position.snapRevision==951,"desired edge exceeds acknowledged native high water");
    const auto edge=p.state;
    check(pose::observe(p,{0.F,0.F,700,900}) && p.state==edge,"out of order receipt cannot cancel pending desired edge");
    check(!pose::observe(p,{std::numeric_limits<float>::quiet_NaN(),0.F,801,951}) && p.state==edge,"invalid native value rejected");
    check(pose::observe(p,{.2F,.2F,INT32_MAX,INT32_MAX}),"signed maximum acknowledgement retained");
    check(!pose::desire(p,0.F) && p.fault && p.state==edge,"revision exhaustion stops publication without wrap");
    check(!pose::observe(p,{0.F,0.F,0,0}),"fault is sticky until lifecycle reset");
    p={};check(pose::desire(p,0.F) && p.state.position.revision==0,"new owner resets pose revision state");
    pose::Inbox<unsigned> inbox;unsigned deliveries{},owner{};
    for(unsigned i=0;i<10000;++i)inbox.submit(123,{.1F,.1F,1,1});
    inbox.drain([&](unsigned r,pose::Sample){++deliveries;owner=r;});
    inbox.drain([&](unsigned,pose::Sample){++deliveries;});
    check(deliveries==1 && owner==123,"world frame repeats coalesce into bounded inbox");
    inbox.submit(123,{0.F,0.F,0,0});check(!inbox.dirty,"older native revision cannot overwrite newer sample");
    inbox.submit(123,{.1F,0.F,1,1});check(inbox.dirty,"post original target drift is retained");
    inbox={};check(!inbox.present && !inbox.dirty,"retired owner inbox cleared");
    sunrise::server::runtime::activity::mission_capture::Publication capture;
    check(pose::desire(capture.pose,.2F),"capture pose initialized independently");
    const auto saved=capture.pose.state;
    check(sunrise::server::runtime::activity::mission_capture::update(capture,1,true,false,100,20)
        && capture.pose.state==saved,"capture start cannot erase pose revision history");
    check(sunrise::server::runtime::activity::mission_capture::update(capture,2,false,false,100,25)
        && capture.pose.state==saved,"capture stop cannot erase pose revision history");
    std::array<std::byte,41> bytes{};bits::Writer writer(bytes);std::size_t count{};
    wire::State state;state.position={700,900,.1F};
    check(wire::write_record(writer,state) && writer.bit_count()==321 && writer.finish(count) && count==41,"nine32bit reflected payload plus record header");
    bits::Reader reader(bytes);std::uint64_t value{};
    check(reader.read(1,value) && value==1 && reader.read(32,value) && value==0x80805063U,"exact native dynamic schema");
    const std::array<std::uint32_t,9> expected{0x7FFFFFFFU,0x7FFFFFFFU,0x3F800000U,0x7FFFFFFFU,0x7FFFFFFFU,0,0x800002BCU,0x80000384U,0x3DCCCCCDU};
    for(auto field:expected)check(reader.read(32,value) && value==field,"independent native reflected field vector");
    namespace object=sunrise::state::activity::coo::native_device;
    auto measure=bits::Writer::measuring();
    check(object::object(measure,7,true,&capture.state,object::interaction::Mode::unchanged,&state) && measure.bit_count()==961,"capture and pose share live dynamic list");
    auto inactive=bits::Writer::measuring();
    check(object::object(inactive,7,false,&capture.state,object::interaction::Mode::unchanged,&state) && inactive.bit_count()==252,"preparation excludes all component records");
    std::printf("native device pose: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
