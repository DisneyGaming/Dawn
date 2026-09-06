#include "client/hooks/bootflow/omega_mission_motion.h"
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
namespace m=sunrise::client::hooks::bootflow::omega_mission_motion;
unsigned checks{};
void check(bool value,const char* what) { ++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);} }
template<class T> void put(std::vector<std::byte>& b,std::size_t at,T value) { std::memcpy(b.data()+at,&value,sizeof value); }
int main(int argc,char** argv) {
    check(argc==2,"provide actual captured motion-component.bin");
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(in.good(),"capture opens");
    std::vector<std::byte> b(static_cast<std::size_t>(in.tellg()));in.seekg(0);
    in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));check(in.good(),"capture reads");
    m::Arena a{};check(m::arena(b,a),"actual full motion component layout");
    check(a.self==0x08F9EA23 && a.entity==0x30FAA22C && a.movement==0x6AF9EB72,"capture has distinct full owners");
    check(a.count==1 && a.slots[0].opcode==0x39 && a.slots[0].size==0x150,"actual persistent intro allocation");
    check(!m::departure_ready(b,a),"intro blocks departure");
    const auto original=b;
    const auto slots=0x68+static_cast<std::size_t>(m::field<std::int64_t>(b,0x58));
    const auto blocks=0x78+static_cast<std::size_t>(m::field<std::int64_t>(b,0x68));
    put(b,slots+2,std::uint8_t{0x6C});put(b,blocks+2,std::uint16_t{0x330});
    put(b,a.data+0x304,a.entity);
    check(m::arena(b,a)&&m::departure_ready(b,a),"owned default locomotion admitted");
    put(b,a.data+0x304,a.entity^0x2000);check(!m::departure_ready(b,a),"recycled owner blocks default");
    put(b,a.data+0x304,a.entity);put(b,0xE0,std::uint16_t{1});
    check(m::arena(b,a)&&!m::departure_ready(b,a),"pending native request blocks departure");
    b=original;put(b,0x58,INT64_MAX);check(!m::arena(b,a),"invalid relative array");
    b=original;put(b,blocks+2,std::uint16_t{0xFFFF});check(!m::arena(b,a),"invalid allocation length");
    m::mission::Token token{{7,2,3,4,5,6,7,0},8};
    m::Identity id{token,0x08F9EA23,0x6AF9EB72,0x5DF9EA3B,0};
    for(unsigned change=0;change<8;++change) {
        m::Track t{};t.token=token;t.submitted=true;
        check(!t.observe(id,2,m::destinations[0],19),"no skipped initial stage");
        check(t.observe(id,1,m::destinations[0],19),"native stage1");
        auto wrong=id;
        switch(change) {
        case 0:++wrong.token.epoch;break;case 1:++wrong.token.boss.run;break;
        case 2:++wrong.token.boss.generation;break;case 3:wrong.component^=0x2000;break;
        case 4:wrong.movement^=0x2000;break;case 5:wrong.selector^=0x2000;break;
        case 6:++wrong.slot;break;default:wrong.token.boss.entity^=0x2000;break;
        }
        check(!t.observe(wrong,2,m::destinations[0],19),"wrong binding cannot advance");
        check(!t.cleanup(id),"early cleanup cannot finish");
        check(t.observe(id,2,m::destinations[0],19),"native stage2");
        check(t.observe(id,3,m::destinations[0],19),"native stage3");
        check(!t.cleanup(wrong),"stale cleanup rejected");check(t.cleanup(id),"native cleanup");
        check(!t.cleanup(id),"cleanup only once");
        check(!t.finish(token,false,true,m::destinations[0]),"live motion not retired");
        check(!t.finish(token,true,false,m::destinations[0]),"selector remains active");
        check(!t.finish(token,true,true,m::destinations[1]),"actual destination required");
        check(t.finish(token,true,true,m::destinations[0]),"ordered lifecycle plus actual position");
        check(!t.finish(token,true,true,m::destinations[0]),"one completion per logical command");
    }
    std::printf("PASS %u motion ownership, captured arena and lifecycle checks\n",checks);
}
