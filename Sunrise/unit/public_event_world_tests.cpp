#include "server/runtime/activity/mercury_public_event_world.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
namespace runtime=sunrise::server::runtime::activity::world_object;
namespace data=sunrise::server::runtime::activity::mercury::public_events::world;
namespace place=sunrise::middleware::bap::activity_message::native::placement;
namespace device=sunrise::middleware::bap::activity_message::native::world_device;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};void check(bool yes,const char* text){++checks;if(!yes){std::fprintf(stderr,"FAIL %u %s\n",checks,text);std::exit(1);}}
int main(int argc,char** argv) {
    if(argc!=2)return 2;const std::filesystem::path output(argv[1]);std::filesystem::create_directories(output);
    for(const auto generation:{1U,2U,65535U})for(const bool active:{false,true}) {
        place::Request request{0xC8229B2B,74,15};request.generation=generation;request.active=active;
        std::array<std::byte,32> bytes{};bits::Writer writer(bytes);
        check(place::body_bits(request)==252 && place::write(writer,request) && writer.bit_count()==252,"bounded positive-generation source codec");
        const auto file=output/(std::string(active?"on-":"off-")+std::to_string(generation)+".body");
        std::ofstream stream(file,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(stream.good(),"source fixture exported");
        auto counterpart=request;counterpart.active=!active;std::array<std::byte,32> other{};bits::Writer second(other);check(place::write(second,counterpart),"paired authority encodes");
        bytes[8]^=std::byte{128};check(bytes==other,"active changes exactly native field2 bit64");
    }
    place::Request legacy;legacy.active=false;check(!place::body_bits(legacy),"compatibility source cannot silently withdraw");
    check(runtime::Runtime::valid(data::kDefinition),"authored world definitions resolve native sources and device contracts");
    runtime::Context c{{44,{3}},5,6,7,8,29,15,true,true};runtime::Runtime service;
    check(service.begin(data::kDefinition,c),"world event owns reusable service");
    const auto advance=[&](){for(unsigned i=0;i<8;++i){if(!service.update(c))return false;if(service.requested())return true;}return false;};
    auto stale=c;stale.owner.incarnation.value++;
    check(!service.request(data::kOpening,stale),"foreign owner cannot request world objects");
    check(service.request(data::kOpening,c) && advance(),"UE requests opening objects and controls");
    place::Batch placements;device::Batch devices;check(service.append(placements,devices),"opening authority projects");
    check(placements.count==7 && devices.count==5,"both shells central effect and four central physics sources exist");
    for(const auto slot:{27U,28U}) {
        const auto* p=place::find(placements,0xB1EAC5B6,4,static_cast<std::uint16_t>(slot));
        check(p && p->generation==1 && p->active,"deferred shell gets positive generation");
    }
    const auto* left=device::find(devices,0xB1EAC5B6,23,1);check(left && left->state.position.value==0 && left->state.position.revision==1
        && left->state.power.value==1 && left->state.power.revision==1,"native shell control uses channel revisions");
    check(!place::find(placements,0xC8229B2B,4,70),"island launch physics remains absent before route change");
    auto duplicate=placements;auto originalDevices=devices;check(!service.append(duplicate,originalDevices)
        && duplicate.count==placements.count && originalDevices.count==devices.count,"conflicting merge is atomic");
    stale=c;stale.bubble=16;check(service.update(stale),"player region movement pauses control requests");
    check(!service.request(data::kCannonsOff,stale),"outside actor scope cannot mutate world route");
    place::Batch retained;device::Batch retainedDevices;check(service.append(retained,retainedDevices) && retained.count==placements.count,"region movement retains source authority");
    check(service.request(data::kOpening,c) && service.update(c),"same scene request is idempotent");
    check(service.request(data::kCannonsOff,c) && service.update(c),"UE requests native power and central physics off");
    placements={};devices={};check(service.append(placements,devices),"off authority projects");
    for(const auto slot:{74U,75U,76U,77U}) {
        const auto* p=place::find(placements,0xC8229B2B,4,static_cast<std::uint16_t>(slot));check(p && !p->active && p->generation==1,"mode0 physics withdrawal retains generation");
    }
    check(service.request(data::kReorient,c) && service.update(c),"UE requests native orientation endpoint while power is off");
    placements={};devices={};check(service.append(placements,devices),"orientation authority projects");
    left=device::find(devices,0xB1EAC5B6,23,1);check(left && left->state.position.value==1 && left->state.position.revision==2 && !left->state.position.snap
        && left->state.power.value==0,"rotation retains disabled power and authored interpolation");
    check(service.request(data::kIslands,c) && advance(),"UE requests island physics and rings");
    placements={};devices={};check(service.append(placements,devices) && placements.count==13 && devices.count==7,"full route retains retired central sources");
    for(const auto slot:{70U,71U,72U,73U,78U,79U})check(place::find(placements,0xC8229B2B,4,static_cast<std::uint16_t>(slot))->active,"island native route source active");
    check(!service.request(data::kOpening,c),"older phase cannot reset world route");
    check(service.request(data::kRetire,c) && advance(),"UE retires final route");
    placements={};devices={};check(service.append(placements,devices),"retirement authority retained");
    for(std::size_t i=0;i<placements.count;++i)check(!placements.entries[i].active,"no launch or visual source survives world retirement request");
    check(place::find(placements,0xB1EAC5B6,4,27)->generation==2,"deferred shell retirement advances native generation");
    check(service.request(data::kRetire,c) && service.update(c),"repeat retirement is stable");
    std::printf("PASS %u public-event world authority checks; creation, visible effects and travel remain live acceptance\n",checks);
}
