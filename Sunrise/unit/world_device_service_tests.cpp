#include "server/runtime/activity/world_device_service.h"
#include "server/runtime/activity/haunted_forest_registries.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace service=sunrise::server::runtime::activity::world_device;
namespace wd=service::wire;
namespace hf=sunrise::server::runtime::activity::haunted_forest::mode;
namespace roster=sunrise::middleware::bap::activity_message::sensor_auth_update;
unsigned checks{};
void expect(bool value){++checks;if(!value){std::fprintf(stderr,"failed check %u\n",checks);std::exit(1);}}
service::Action position(std::uint32_t id,float value) {service::Action action{id,service::Position,{}};action.state.position.value=value;return action;}
struct Writer final {std::vector<std::uint8_t> bytes{};std::size_t bits{};
    bool write(std::uint64_t value,std::size_t width) {
        for(std::size_t i=width;i>0;--i){if(bits%8==0)bytes.push_back(0);
            bytes.back()|=static_cast<std::uint8_t>(((value>>(i-1))&1)<<(7-bits%8));++bits;}return true;}};
int main(int argc,char** argv) {
    std::array<service::Action,7> actions{position(1,.2F),position(2,.1F)};
    actions[2]={3,service::Power,{}};actions[2].state.power.value=.5F;
    actions[3]={4,service::Lock,{}};actions[3].state.lock.value=1;
    actions[4]={5,service::Lock,{}};actions[4].state.lock.value=0;
    actions[5]={6,service::Power|service::Lock,{}};actions[5].state.power.value=.25F;
    actions[6]=position(7,.2F);actions[6].state.position.snap=true;
    const service::Capability cap{&hf::kRegistries[0],33,actions};expect(service::valid(cap));
    service::Service empty;expect(empty.begin({1,{2}},3,{}));expect(empty.project(13).count==0);
    expect(empty.request({{1,{2}},3,1,1,0x34D23982,1,33},13)==service::Result::unsupported);
    empty.release();expect(!empty.owner());expect(empty.revision()==0);
    service::Service source;
    expect(!source.begin({},3,{&cap,1}));expect(!source.begin({1,{2}},0,{&cap,1}));
    expect(source.begin({1,{2}},3,{&cap,1}));expect(!source.begin({1,{2}},3,{&cap,1}));
    actions[0].state.position.value=.4F; // Runtime owns copied trusted actions.
    expect(source.project(13).count==0);
    service::Command request{{1,{2}},3,1,1,0x34D23982,1,33};
    for(unsigned field=0;field<7;++field) {
        auto bad=request;
        if(field==0)bad.owner.sessionId=7;
        if(field==1)bad.owner.incarnation.value=7;
        if(field==2)bad.boot=7;
        if(field==3)bad.expectedRevision=7;
        if(field==4)bad.registry=0x34D23983;
        if(field==5)bad.slot=32;
        if(field==6)bad.action=999;
        const auto verdict=source.request(bad,13);
        expect(verdict==service::Result::stale || verdict==service::Result::unsupported);
        expect(source.project(13).count==0);expect(source.revision()==1);expect(source.last_request()==0);
    }
    expect(source.request(request,12)==service::Result::stale);
    auto noRequest=request;noRequest.request=0;expect(source.request(noRequest,13)==service::Result::duplicate);
    expect(source.request(request,13)==service::Result::accepted);
    const auto first=source.project(13);expect(first.count==1);
    expect(first.entries[0].state.position==wd::Channel{.2F,1,false});
    expect(first.entries[0].state.power==wd::Channel{1,0,false});expect(first.entries[0].state.lock==wd::Channel{});
    expect(source.project(12).count==0);expect(source.revision()==2);
    expect(source.request(request,13)==service::Result::stale); // old server revision
    request.expectedRevision=2;expect(source.request(request,13)==service::Result::duplicate);
    request.request=2;expect(source.request(request,13)==service::Result::unchanged);
    expect(source.project(13).entries[0].state==first.entries[0].state);
    request.expectedRevision=3;request.request=3;request.action=2;
    expect(source.request(request,13)==service::Result::accepted);
    const auto occupied=source.project(13);
    expect(occupied.entries[0].state.position==wd::Channel{.1F,2,false});
    expect(occupied.entries[0].state.power.revision==0 && occupied.entries[0].state.lock.revision==0);
    if(argc==2) {
        const std::filesystem::path output(argv[1]);std::filesystem::create_directories(output);
        for(const auto& item:std::array<std::pair<const char*,wd::State>,2>{{{"service_initial_red",first.entries[0].state},{"service_occupied_capture",occupied.entries[0].state}}}) {
            Writer writer;expect(wd::write_payload(writer,item.second));expect(writer.bits==147);
            std::ofstream file(output/(std::string(item.first)+".wire.bin"),std::ios::binary);
            file.write(reinterpret_cast<const char*>(writer.bytes.data()),static_cast<std::streamsize>(writer.bytes.size()));expect(file.good());
        }
    }
    request.expectedRevision=4;request.request=4;request.action=1;
    expect(source.request(request,13)==service::Result::accepted);
    expect(source.project(13).entries[0].state.position==wd::Channel{.2F,3,false});
    request.expectedRevision=5;request.request=5;request.action=3;
    expect(source.request(request,13)==service::Result::accepted);
    auto powered=source.project(13).entries[0].state;
    expect(powered.position==wd::Channel{.2F,3,false});expect(powered.power==wd::Channel{.5F,1,false});
    expect(powered.lock.revision==0);
    request.expectedRevision=6;request.request=6;request.action=7;
    expect(source.request(request,13)==service::Result::accepted);
    const auto snapped=source.project(13).entries[0].state;
    expect(snapped.position==wd::Channel{.2F,4,true});expect(snapped.power==powered.power && snapped.lock==powered.lock);
    source.release();expect(source.project(13).count==0);expect(source.request(request,13)==service::Result::stale);
    actions[0].state.position.value=.2F;
    expect(source.begin({1,{4}},4,{&cap,1}));expect(source.request(request,13)==service::Result::stale);
    service::Command fresh{{1,{4}},4,1,1,0x34D23982,1,33};
    expect(source.request(fresh,13)==service::Result::accepted);
    expect(source.project(13).entries[0].state.position.revision==1);

    // Actual HF roster admission, including auth/sense slot flags and bubble.
    std::array<std::uint8_t,126> types{},flags{};std::array<std::uint16_t,126> indices{};
    expect(hf::kRegistries[0].slots.size()==types.size());
    for(std::size_t i=0;i<types.size();++i){const auto& slot=hf::kRegistries[0].slots[i];types[i]=slot.type;flags[i]=slot.flags();indices[i]=slot.index;}
    const std::array<std::uint32_t,1> keys{0x34D23982};std::array<std::uint8_t,1> present{1};
    roster::BubbleSubBlock local{13,keys,present};roster::Roster table;
    table.groups[0]={0x34D23982,types,flags,indices};table.groupCount=1;table.bubbleSubBlocks={&local,1};
    expect(wd::valid(first,table,104));expect(!wd::valid(first,table,96));
    expect(wd::find(first,0x34D23982,23,33)!=nullptr);expect(wd::find(first,0x34D23982,23,32)==nullptr);
    expect(wd::find(first,0x34D23982,4,33)==nullptr);expect(wd::find(first,0x34D23983,23,33)==nullptr);
    for(unsigned field=0;field<12;++field) {
        auto invalid=first;
        if(field==0)invalid.entries[0].registry=0;
        if(field==1)invalid.entries[0].slot=127;
        if(field==2)invalid.entries[0].bubble=12;
        if(field==3)invalid.entries[0].state.position.value=2;
        if(field==4)invalid.count=wd::kCapacity+1;
        if(field==5){invalid.entries[1]=invalid.entries[0];invalid.count=2;}
        if(field==6)present[0]=0;
        if(field==7)local.bubble=12;
        if(field==8)types[33]=4;
        if(field==9)flags[33]=1;
        if(field==10){table.groups[1]=table.groups[0];table.groupCount=2;}
        if(field==11)table.groupCount=0;
        expect(!wd::valid(invalid,table,104));present[0]=1;local.bubble=13;types[33]=23;flags[33]=3;table.groupCount=1;
    }
    for(unsigned field=0;field<6;++field){auto wrong=cap;auto badActions=actions;wrong.actions=badActions;
        if(field==0)wrong.slot=32;
        if(field==1)wrong.slot=999;
        if(field==2)badActions[0].channels=0;
        if(field==3)badActions[0].state.position.revision=1;
        if(field==4)badActions[1].id=badActions[0].id;
        if(field==5)wrong.actions={};
        expect(!service::valid(wrong));}
    auto duplicate=std::array<service::Capability,2>{cap,cap};service::Service invalid;
    expect(!invalid.begin({1,{2}},3,duplicate));expect(invalid.revision()==0);

    // A wrap would make native106AD60 ignore the next command. Exhaustion is
    // therefore a hard refusal, preserving even other selected channels.
    service::Service exhausted;expect(exhausted.begin({2,{1}},1,{&cap,1}));
    for(std::uint64_t i=1;i<=32767;++i){service::Command step{{2,{1}},1,exhausted.revision(),i,0x34D23982,(i&1)?4U:5U,33};
        expect(exhausted.request(step,13)==service::Result::accepted);}
    const auto before=exhausted.project(13).entries[0].state;
    expect(before.lock.revision==32767 && before.lock.value==1);
    service::Command overflow{{2,{1}},1,exhausted.revision(),32768,0x34D23982,6,33};
    expect(exhausted.request(overflow,13)==service::Result::exhausted);
    expect(exhausted.project(13).entries[0].state==before);expect(exhausted.last_request()==32767);
    expect(exhausted.revision()==32768);expect(before.power.revision==0);
    std::printf("world device service: %u checks, zero failures\n",checks);
}
