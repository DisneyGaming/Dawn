#include "../src/middleware/bap/activity_message/native/capture_controller_authority.h"
#include "../src/middleware/encoding/bit_reader.h"
#include "../src/middleware/encoding/bit_writer.h"
#include "../src/server/runtime/activity/native_activity_clock.h"
#include "../src/server/runtime/activity/mission_capture_service.h"
#include "../src/server/runtime/activity/mission_observation_queue.h"
#include "../src/state/activity/coo/native_device_authority.h"
#include "../src/client/hooks/bootflow/beyond_infinity_plate_timer.h"
#include "fixtures/native_plate_preparation_tests.h"
#include <array>
#include <cstdio>
#include <limits>

namespace clockwire=dawn::middleware::bap::activity_message::native::activity_clock;
namespace capture=dawn::middleware::bap::activity_message::native::capture_controller;
namespace bits=dawn::middleware::encoding::bits;
namespace hostclock=dawn::server::runtime::activity::activity_clock;
int checks{},failures{};
void check(bool value,const char* label) {++checks;if(!value){++failures;std::printf("FAIL %s\n",label);}}
std::uint64_t read(bits::Reader& reader,std::uint8_t width) {
    std::uint64_t value{};check(reader.read(width,value),"bounded reflected read");return value;
}
int main() {
    native_plate_preparation_fixture::run(check);
    namespace mission=dawn::server::runtime::activity::mission_capture;
    namespace oldtimer=dawn::client::hooks::bootflow::beyond_infinity_plate_timer;
    mission::Publication timer{};
    check(mission::update(timer,1,false,false,3366000,100),"server publishes native stopped capture before occupancy");
    check(!timer.state.active && timer.published,"stopped capture is explicit");
    check(mission::update(timer,2,true,false,3366000,200),"occupancy publishes a native five second charge");
    const auto initial=timer;
    check(mission::update(timer,2,true,false,3366000,900) && timer.state==initial.state,"publication cadence cannot restart native timer");
    oldtimer::Command old{};check(oldtimer::start(old,3366000,200),"independent legacy native tuple available");
    check(mission::matches(timer,2,std::span(old.bytes).subspan(0x20,0x48)),"server publication matches previously verified native apply tuple");
    check(!mission::matches(timer,3,std::span(old.bytes).subspan(0x20,0x48)),"later occupancy cannot consume old revision");
    check(mission::update(timer,3,false,false,3366000,1000) && !timer.state.clock.running,"departure stops capture through native authority");
    check(!mission::matches(timer,3,std::span(old.bytes).subspan(0x20,0x48)),"stopped command rejects old running native state");
    check(mission::update(timer,4,true,false,3366000,1200) && timer.state.clock.anchor==1200,"new occupancy gets new server clock anchor");
    const auto charged=timer.state;
    check(mission::update(timer,5,false,true,3366000,2000) && timer.state==charged,"confirmed completion retains exact native timer across departure");
    check(!mission::update(timer,6,true,false,UINT64_MAX,100),"unrepresentable duration rejected");
    auto measured=bits::Writer::measuring();
    check(dawn::state::activity::coo::native_device::object(measured,129,true,&charged) && measured.bit_count()==640,"live mission source contains one388bit capture record");
    dawn::server::runtime::activity::MissionObservationQueue<unsigned,3> queue;
    unsigned consumed{};
    check(queue.push(1) && queue.push(2) && consumed==0,"observation intake cannot advance server state");
    check(queue.drain([&](unsigned e) {consumed=consumed*10+e;}) && consumed==12,"server consumes start before completion in original order");
    check(queue.push(1) && queue.push(2) && queue.push(3) && !queue.push(4),"bounded queue reports overflow");
    check(!queue.drain([&](unsigned) {++consumed;}) && consumed==12,"overflow cannot drop receipts then continue mission");
    queue.reset();check(queue.drain([&](unsigned) {++consumed;}) && consumed==12,"new owner clears previous receipts and overflow");

    // Independent reflection vectors: bool f0 then raw IEEE754 float f1.
    const std::array<std::array<std::byte,5>,2> expected{{
        {std::byte{0x1F},std::byte{0xC0},std::byte{},std::byte{},std::byte{}},
        {std::byte{0x9F},std::byte{0xC0},std::byte{},std::byte{},std::byte{}},
    }};
    for(unsigned flag=0;flag<2;++flag) {
        std::array<std::byte,5> bytes{};bits::Writer writer(bytes);std::size_t n{};
        check(clockwire::write(writer,{flag!=0,1.0F}),"type2 tuple encode");
        check(writer.bit_count()==33 && writer.finish(n) && n==5,"type2 reflected width");
        check(bytes==expected[flag],"type2 exact independent IEEE754 vector");
    }
    for(const float invalid:{-1.0F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        auto writer=bits::Writer::measuring();check(!clockwire::write(writer,{false,invalid}),"reject invalid type2 timing");
        check(writer.bit_count()==0,"invalid type2 does not partially publish");
    }
    check(clockwire::valid({false,0.0F}),"native stopped configuration allowed");
    std::uint64_t ticks=9;
    check(clockwire::from_milliseconds(8000,ticks) && ticks==5385600,"8sec native units");
    check(clockwire::from_milliseconds(1,ticks) && ticks==673,"fractional millisecond floor");
    check(clockwire::from_milliseconds(10,ticks) && ticks==6732,"exact rational10ms");
    ticks=9;check(!clockwire::from_milliseconds(UINT64_MAX,ticks) && ticks==9,"tick conversion overflow leaves result");
    capture::State state{true,{true,0,5385600,123,5385477,1000000,1.0F},false};
    for(const float rate:{1.0F,-0.5F}) {
        state.clock.rate=rate;
        std::array<std::byte,49> bytes{};bits::Writer writer(bytes);std::size_t n{};
        check(capture::write_record(writer,state),"capture record encode");
        check(writer.bit_count()==388 && writer.finish(n) && n==49,"native dynamic record width");
        bits::Reader reader(bytes);
        check(read(reader,1)==1 && read(reader,32)==0x80804FCA,"present exact class");
        check(read(reader,1)==1 && read(reader,1)==1,"active and running flags");
        check(read(reader,64)==0 && read(reader,64)==5385600,"native min max");
        check(read(reader,64)==123 && read(reader,64)==5385477,"native elapsed remaining");
        check(read(reader,64)==1000000,"native context anchor");
        check(read(reader,32)==std::bit_cast<std::uint32_t>(rate),"IEEE754 signed rate");
        check(read(reader,1)==0 && read(reader,4)==0,"final native flag and tail padding");
    }
    for(unsigned mode=0;mode<6;++mode) {
        auto bad=state;
        if(mode==0)bad.clock.minimum=bad.clock.maximum+1;
        if(mode==1)bad.clock.elapsed=bad.clock.maximum+1;
        if(mode==2)bad.clock.minimum=bad.clock.elapsed+1;
        if(mode==3)bad.clock.anchor=UINT64_MAX;
        if(mode==4)bad.clock.rate=0;
        if(mode==5)bad.clock.rate=std::numeric_limits<float>::quiet_NaN();
        auto writer=bits::Writer::measuring();check(!capture::write_record(writer,bad),"invalid capture state rejected");
        check(writer.bit_count()==0,"invalid capture leaves writer untouched");
    }
    state.clock.running=false;state.clock.rate=0;state.clock.anchor=UINT64_MAX;
    check(capture::valid(state),"paused shared-clock native sentinel permitted");
    std::array<std::byte,48> small{};bits::Writer shortWriter(small);
    check(!capture::write_record(shortWriter,state),"insufficient record storage rejected");
    hostclock::Service service;
    const hostclock::Owner owner{0x9EAA300100200001ULL,{1}};
    const hostclock::Policy policy{0x81550015,13,{false,1000.0F/30.0F}};
    hostclock::Publication publication{};
    check(!service.project(owner,7,13,5000,publication) && !publication,"absent profile has no clock publication");
    check(!service.begin({},7,1,policy,1000),"clock rejects absent owner");
    check(!service.begin(owner,0,1,policy,1000),"clock rejects absent boot");
    check(!service.begin(owner,7,0,policy,1000),"clock rejects absent allocator epoch");
    check(service.begin(owner,7,1,policy,1000),"clock begins admitted owner");
    check(!service.begin(owner,7,2,policy,1000),"clock cannot replace live domain");
    check(!service.project({owner.sessionId,{2}},7,13,1500,publication),"wrong incarnation rejected");
    check(!service.project(owner,8,13,1500,publication),"wrong boot rejected");
    check(!service.project(owner,7,12,1500,publication),"wrong bubble rejected");
    check(service.project(owner,7,13,1500,publication) && publication.elapsedTicks==336600,"server halfsecond uses native units");
    const auto firstDomain=publication.domain;
    check(!service.project(owner,7,13,1499,publication),"clock regression does not rewind synchronization");
    check(service.project(owner,7,13,9000,publication) && publication.elapsedTicks==5385600,"monotonic8sec synchronization");
    check(publication.domain==firstDomain,"time advances without changing clock domain");
    service.retire();
    check(!service.project(owner,7,13,10000,publication),"retired clock does not publish");
    check(!service.begin(owner,7,1,policy,10000),"replayed allocator epoch rejected");
    check(service.begin(owner,7,2,policy,10000),"fresh domain after retirement");
    check(service.project(owner,7,13,10000,publication) && publication.elapsedTicks==0
        && publication.domain!=firstDomain,"new domain resets time and invalidates old identity");
    std::printf("native capture authority: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
