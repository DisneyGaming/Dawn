#include "client/hooks/bootflow/omega_mission_crown.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
namespace c=dawn::client::hooks::bootflow::omega_mission_crown;
unsigned checks{};
void check(bool v,const char* what) {++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
std::vector<std::byte> load(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(in.good(),"asset opens");
    std::vector<std::byte> b(static_cast<std::size_t>(in.tellg()));in.seekg(0);
    in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));check(in.good(),"asset reads");return b;
}
template<class T> T get(std::span<const std::byte> b,std::size_t at) {return c::observation::field<T>(b,at);}
template<class T> void put(std::vector<std::byte>& b,std::size_t at,T value) {std::memcpy(b.data()+at,&value,sizeof value);}
int main(int argc,char** argv) {
    check(argc==3,"graph and bank assets required");const auto graphs=load(argv[1]),bank=load(argv[2]);
    for(unsigned cycle=1;cycle<=3;++cycle) {
        const auto graph=c::cycles[cycle-1].graph;
        const auto row=0x30+static_cast<std::size_t>(graph)*0x38;
        const auto count=get<std::uint64_t>(graphs,row+8);
        const auto nodes=row+0x20+get<std::uint64_t>(graphs,row+0x10);
        check(count==(cycle==3?7U:8U),"packaged node count");
        for(std::uint32_t index=0;index<count;++index) {
            const auto node=nodes+index*0x50;
            std::vector<std::byte> b(0xB8);
            put(b,0x10,c::observation::kGraphAsset);put(b,0x21,std::uint8_t{1});
            put(b,0xA4,index);put(b,0xB0,index);put(b,0xA8,graph);put(b,0xB4,graph);
            put(b,0x30,get<std::int32_t>(graphs,node+0x10));
            put(b,0x20,get<std::uint8_t>(graphs,node+8));put(b,0x38,3.F);put(b,0x3C,1.F);
            c::Observation observed{};
            check(c::decode(b,bank,cycle,true,observed),"all packaged nodes admitted");
            if(index==get<std::uint32_t>(graphs,row+4)) check(observed.node==c::Node::summon,"authored initial node summons both arms");
            const bool death=observed.node==c::Node::death;
            check(c::decode(b,bank,cycle,false,observed)==death,"only terminal death admits stopped result");
            if(death) check(observed.terminal,"native terminal death");
            const auto original=b;
            put(b,0x21,std::uint8_t{0});check(!c::decode(b,bank,cycle,true,observed),"inactive graph rejected");
            b=original;put(b,0xA8,graph+1);check(!c::decode(b,bank,cycle,true,observed),"foreign requested graph rejected");
            b=original;put(b,0xB0,std::int32_t{-1});check(!c::decode(b,bank,cycle,true,observed),"unloaded node rejected");
            b=original;put(b,0x30,std::int32_t{24});check(!c::decode(b,bank,cycle,true,observed),"foreign clip rejected");
            b=original;put(b,0x3C,4.F);check(!c::decode(b,bank,cycle,true,observed),"invalid playback rejected");
            b=original;put(b,0x20,static_cast<std::uint8_t>(get<std::uint8_t>(b,0x20)^1));
            check(!c::decode(b,bank,cycle,true,observed),"wrong looping policy rejected");
        }
    }
    std::printf("%u Crown graph checks passed\n",checks);
}
