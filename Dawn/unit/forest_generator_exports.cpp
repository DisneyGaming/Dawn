#include "../src/middleware/bap/activity_message/native/forest_generator_authority.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
namespace fg=dawn::middleware::bap::activity_message::native::forest_generator;
struct Writer final {
    std::vector<std::uint8_t> bytes{};
    std::size_t bits{},limit{fg::kPayloadMaxBits};
    bool write(std::uint64_t value,std::size_t width) noexcept {
        if(width>64 || bits+width>limit || (width<64 && (value>>width)))return false;
        for(std::size_t i=width;i>0;--i) {
            if(bits%8==0)bytes.push_back(0);
            bytes.back()|=static_cast<std::uint8_t>(((value>>(i-1))&1)<<(7-bits%8));++bits;
        }
        return true;
    }
};
unsigned checks{};
void expect(bool value) {++checks;if(!value){std::fprintf(stderr,"failed check %u\n",checks);std::exit(1);}}
void emit(const std::filesystem::path& dir,const char* name,const fg::State& state) {
    Writer writer;expect(fg::write_payload(writer,state));expect(writer.bits==fg::body_bits(state));
    std::ofstream file(dir/(std::string(name)+".wire.bin"),std::ios::binary);
    file.write(reinterpret_cast<const char*>(writer.bytes.data()),static_cast<std::streamsize>(writer.bytes.size()));
    expect(file.good());
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const std::filesystem::path dir(argv[1]);std::filesystem::create_directories(dir);
    fg::State state;emit(dir,"authored",state);
    state.primary.overrides=fg::Enabled;state.primary.enabled=true;emit(dir,"ignition",state);
    state.primary.enabled=false;emit(dir,"disabled",state);
    fg::State varied;
    varied.primary.seed=0xFEDCBA98;varied.primary.mode=-128;varied.primary.overrides=127;
    varied.primary.enabled=true;varied.primary.densityA=0.125F;varied.primary.densityB=0.75F;
    varied.primary.scalarOverrides={-2147483647-1,-1,0,1234567,2147483647};
    varied.primary.anchors={fg::Anchor{-128,127,1.25F,false},fg::Anchor{2,-3,0.5F,true},
        fg::Anchor{0,0,-4.0F,false},fg::Anchor{127,-128,0.0F,true}};
    varied.primary.blockedCount=100;
    for(std::size_t i=0;i<100;++i)varied.primary.blockedCells[i]={static_cast<std::int16_t>(i-50),static_cast<std::int16_t>(32767-i),static_cast<std::int16_t>(-32768+static_cast<int>(i))};
    varied.secondary.mode=127;varied.secondary.seed=17;varied.secondary.blockedCount=1;
    varied.secondary.blockedCells[0]={-1,2,3};varied.secondary.blockedCells[99]={123,456,789};
    varied.reportedSeed=0xABCDEF01;
    for(std::size_t i=0;i<32;++i)varied.areas[i]=static_cast<std::uint8_t>(i*7);
    for(std::size_t i=0;i<64;++i)varied.groups[i]=static_cast<std::uint8_t>(255-i*3);
    emit(dir,"varied",varied);
    auto maximum=varied;maximum.secondary.blockedCount=100;emit(dir,"maximum",maximum);
    for(const bool second:{false,true}) {
        auto invalid=state;auto& recipe=second?invalid.secondary:invalid.primary;
        recipe.blockedCount=101;Writer writer;expect(!fg::write_payload(writer,invalid));expect(writer.bits==0);
        recipe.blockedCount=0;recipe.overrides=128;expect(!fg::write_payload(writer,invalid));expect(writer.bits==0);
        recipe.overrides=0;recipe.densityA=std::numeric_limits<float>::infinity();expect(!fg::write_payload(writer,invalid));
        recipe.densityA=-1;recipe.anchors[3].weight=std::numeric_limits<float>::quiet_NaN();expect(!fg::write_payload(writer,invalid));
    }
    Writer shortWriter;shortWriter.limit=fg::body_bits(state)-1;expect(!fg::write_payload(shortWriter,state));
    std::printf("forest generator exports: %u checks, zero failures\n",checks);
}
