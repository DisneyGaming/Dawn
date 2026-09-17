#include "../src/state/activity/coo/native_mission_forest_authority.h"
#include "../src/state/activity/strike_bond/frame.h"
#include "../src/state/activity/beyond_infinity/authority.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>
namespace generator=dawn::state::activity::coo::native_generator;
namespace garden=dawn::state::activity::strike_bond;
namespace {
unsigned checks{};
void check(bool value,const char* reason) {++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",reason);std::exit(1);}}
struct Writer {
    std::vector<unsigned char> bytes;
    std::size_t bits{};
    std::size_t bit_count() const noexcept {return bits;}
    bool write(std::uint64_t value,std::size_t width) {
        if(width>64 || (width<64 && value>>width))return false;
        for(auto i=width;i>0;--i) {
            if(bits%8==0)bytes.push_back(0);
            bytes.back()|=static_cast<unsigned char>(((value>>(i-1))&1)<<(7-bits%8));++bits;
        }
        return true;
    }
};
void emit(const std::filesystem::path& dir,const char* name,const generator::Request& request) {
    Writer writer;check(generator::write_activation(writer,request),"production generator publication");
    check(writer.bit_count()==1750 && writer.bytes.size()==219,"variable tile list retains native minimum width");
    std::ofstream file(dir/name,std::ios::binary);
    file.write(reinterpret_cast<const char*>(writer.bytes.data()),static_cast<std::streamsize>(writer.bytes.size()));
    check(file.good(),"write native replay input");
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const std::filesystem::path directory(argv[1]);std::filesystem::create_directories(directory);
    emit(directory,"omega.bin",generator::omega_request(12345));
    namespace beyond=dawn::state::activity::beyond_infinity;
    for(std::uint8_t pass=1;pass<=2;++pass) {
        beyond::Frame frame{};frame.enabled=true;frame.spawnGeneration=513;
        frame.forestSeed=pass==1?12345U:67890U;frame.forestPass=pass;frame.forestReady=true;
        Writer writer;check(beyond::body_bits(frame,0x8E70632BU,37,3)==1750,"Beyond active producer width");
        check(beyond::write_body(writer,frame,0x8E70632BU,37,3),"Beyond active producer body");
        std::ofstream file(directory/(pass==1?"beyond1.bin":"beyond2.bin"),std::ios::binary);
        file.write(reinterpret_cast<const char*>(writer.bytes.data()),static_cast<std::streamsize>(writer.bytes.size()));
        check(file.good(),"Beyond actual native body fixture");
        check(!beyond::body_bits(frame,0x8E70632BU,37,4),"Beyond foreign generator slot rejected");
    }
    auto request=garden::forest_request(12345);
    check(request.topology==std::array<float,2>{0.F,0.F},"Garden server selects both solver inputs");
    check(request.values[0]==6 && request.selectAnchors && request.selectSeed,"existing recipe ownership retained");
    emit(directory,"garden.bin",request);
    auto disabled=request;disabled.enabled=false;emit(directory,"disabled.bin",disabled);
    auto authored=request;authored.topology=generator::kAuthoredTopologies;emit(directory,"authored.bin",authored);
    auto tree=generator::forest_request(12345);
    check(tree.topology==generator::kAuthoredTopologies,"Tree retains authored solver inputs");
    emit(directory,"tree.bin",tree);
    for(const auto value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        for(std::size_t i=0;i<2;++i) {
            auto invalid=request;invalid.topology[i]=value;Writer writer;
            check(!generator::write_activation(writer,invalid) && writer.bits==0,"nonfinite density rejected before output");
        }
    }
    std::printf("native generator topology: %u checks, zero failures\n",checks);
}
