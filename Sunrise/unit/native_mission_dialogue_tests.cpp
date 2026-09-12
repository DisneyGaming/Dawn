#include "state/activity/deadly_trial/authority.h"
#include "state/activity/hijacked/authority.h"
#include "middleware/encoding/bit_writer.h"
#include "state/activity/coo/objective_delivery.h"
#include "state/activity/deadly_trial/bindings.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
namespace trial=sunrise::state::activity::deadly_trial;
namespace hijacked=sunrise::state::activity::hijacked;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool ok,const char* what) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
template<class Frame,class Encode,class Measure>
void mission(const char* name,Frame frame,Encode encode,Measure measure,const std::filesystem::path& out) {
    frame.enabled=true;frame.spawnGeneration=513;
    for(unsigned row=0;row<frame.generations.size();++row) {
        frame.generations[row]=1;frame.activeRow=static_cast<std::uint8_t>(row);
        for(bool retired:{false,true}) {
            auto f=frame;if(retired)f.activeRow=255;
            std::array<std::byte,4096> bytes{};bits::Writer writer(bytes);
            check(encode(writer,f),"actual mission authority encodes");
            check(writer.bit_count()==measure(f),"actual mission width and publication agree");
            const auto filename=std::string(name)+"-"+std::to_string(row)+(retired?"-retired":"-active");
            std::ofstream body(out/(filename+".bin"),std::ios::binary);
            body.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>((writer.bit_count()+7)/8));
            check(bool(body),"wire fixture exported");
            std::ofstream meta(out/(filename+".txt"));
            meta<<frame.generations.size()<<' '<<row<<' '<<retired<<' '<<writer.bit_count();check(bool(meta),"fixture metadata exported");
        }
    }
}
namespace coo=sunrise::state::activity::coo;
template<class Frame,class Encode,class Measure>
void objective(const char* name,Frame frame,coo::MarkerTarget marker,std::uint32_t event,Encode encode,Measure measure,const std::filesystem::path& out) {
    coo::ObjectiveDelivery delivery;const coo::Generation owner{41,513};
    coo::ObjectiveService service;service.set(event,marker);
    frame.enabled=true;frame.spawnGeneration=owner.value;
    frame.presentation=delivery.project(owner,service.state());
    check(!frame.presentation.published && measure(frame)==0,"unready native content cannot consume objective");
    check(!delivery.observe({42,513},10,0x100000,0x200000,true),"foreign run receipt rejected");
    check(!delivery.observe(owner,UINT32_MAX,0x100000,0x200000,true),"invalid native handle rejected");
    check(delivery.observe(owner,10,0x100000,0x200000,true),"native content ready");
    std::uint32_t previous{};
    for(unsigned stage=0;stage<6;++stage) {
        if(stage==1) {
            check(!delivery.observe(owner,10,0x100000,0x200000,true),"duplicate receipt does not rotate");
        } else if(stage==2 || stage==3) {
            check(delivery.observe(owner,10+stage,0x100000+stage*0x1000,0x200000,true),"physical rebind rotates ring");
        } else if(stage==4) {
            for(unsigned churn=0;churn<3;++churn) {
                check(delivery.observe(owner,13,0x103000,0,false),"readiness loss observed");
                const auto pending=delivery.project(owner,service.state());check(!pending.published,"unready body withheld");
                check(delivery.observe(owner,13,0x103000,0x200000,true),"readiness recovered");
            }
        } else if(stage==5) {service.clear();}
        frame.presentation=delivery.project(owner,service.state());
        check(frame.presentation.published,"ready native publication");
        if(stage==1)check(frame.presentation.revision==previous,"replay retains selector");
        else if(stage)check(frame.presentation.revision==previous+1,"churn coalesces to one ring rotation");
        previous=frame.presentation.revision;
        const auto repeated=delivery.project(owner,service.state());check(repeated.revision==previous,"snapshot repeats stable ring");
        std::array<std::byte,1024> bytes{};bits::Writer writer(bytes);
        check(encode(writer,frame) && writer.bit_count()==measure(frame),"actual native objective encoder");
        const auto path=out/(std::string(name)+"-objective-"+std::to_string(stage));
        std::ofstream body(path.string()+".bin",std::ios::binary);
        body.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>((writer.bit_count()+7)/8));
        check(bool(body),"objective wire fixture written");
        std::ofstream meta(path.string()+".txt");
        meta<<event<<' '<<frame.presentation.active<<' '<<(previous-1)%3<<' '<<writer.bit_count()<<' '
            <<marker.asset.registry<<' '<<marker.asset.type<<' '<<marker.asset.slot;
        for(auto word:marker.locator)meta<<' '<<word;
        check(bool(meta),"objective fixture metadata written");
    }
    check(!delivery.project({42,514},service.state()).published,"new run discards old readiness");
    check(!delivery.observe(owner,13,0x103000,0x200000,true),"old generation remains rejected");
}
int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"usage: native_mission_dialogue_tests export-directory\n");return 2;}
    const std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
    mission("trial",trial::Frame{},[](auto& w,const auto& f){return trial::write_body(w,f,trial::kRoot,53,2);},
        [](const auto& f){return trial::body_bits(f,trial::kRoot,53,2);},out);
    mission("hijacked",hijacked::Frame{},[](auto& w,const auto& f){return hijacked::write_body(w,f,hijacked::kDialogueAsset.registry,53,hijacked::kDialogueAsset.slot);},
        [](const auto& f){return hijacked::body_bits(f,hijacked::kDialogueAsset.registry,53,hijacked::kDialogueAsset.slot);},out);
    objective("trial",trial::Frame{},trial::kNativeMarkers[0].target,trial::kNativeMarkers[0].event,
        [](auto& w,const auto& f){return trial::write_body(w,f,trial::kRoot,68,0);},
        [](const auto& f){return trial::body_bits(f,trial::kRoot,68,0);},out);
    objective("hijacked",hijacked::Frame{},hijacked::marker(hijacked::kObjectives[0].event),hijacked::kObjectives[0].event,
        [](auto& w,const auto& f){return hijacked::write_body(w,f,hijacked::kRoot,68,0);},
        [](const auto& f){return hijacked::body_bits(f,hijacked::kRoot,68,0);},out);
    std::printf("Native mission dialogue: %u encoder checks passed\n",checks);
}
