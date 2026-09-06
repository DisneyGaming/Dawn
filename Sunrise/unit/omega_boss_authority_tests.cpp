#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string_view>

#include "state/activity/omega/omega_boss_authority.h"
#include "middleware/encoding/bit_writer.h"

namespace authority=sunrise::state::activity::omega::boss_authority;
namespace bits=sunrise::middleware::encoding::bits;
namespace {
unsigned checks=0;
void check(bool result,const char* message) {
    ++checks;
    if(!result){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
struct Reader {
    std::span<const std::byte> data;
    std::size_t position=0;
    std::uint64_t get(unsigned width) {
        check(position+width<=data.size()*8,"independent reader has complete field");
        std::uint64_t result=0;
        for(unsigned i=0;i<width;++i,++position)
            result=(result<<1)|((std::to_integer<unsigned>(data[position/8])>>(7-position%8))&1U);
        return result;
    }
};
void hex_matches(std::span<const std::byte> actual,std::string_view expected) {
    check(actual.size()*2==expected.size(),"native fixture byte count");
    const auto nibble=[](char c){return static_cast<unsigned>(c<='9'?c-'0':c-'a'+10);};
    for(std::size_t i=0;i<actual.size();++i)
        check(std::to_integer<unsigned>(actual[i])==16*nibble(expected[2*i])+nibble(expected[2*i+1]),
              "C++ export matches independent pinned-schema fixture");
}
void absent_reference(Reader& r) {
    check(r.get(1)==1,"parent optional reference is explicit");
    check(r.get(32)==0x811C9DC5U,"canonical native FNV sentinel");
    check(r.get(7)==0,"absent reference type decodes to minus one");
    check(r.get(16)==32767,"absent reference index decodes to minus one");
}
void parent_fields(std::span<const std::byte> bytes,std::uint32_t generation) {
    Reader r{bytes};
    absent_reference(r);absent_reference(r);
    check(r.get(1)==1&&r.get(3)==0,"empty native hash array");
    check(r.get(1)==1&&r.get(4)==1,"one authored actor category");
    check(r.get(32)==0x80000000U,"member ownership requests zero additional loose actors");
    check(r.get(1)==1&&r.get(4)==0,"secondary category array stays empty");
    check(r.get(1)==1&&r.get(3)==1&&r.get(2)==1,"safe template variant and localized name tier both zero");
    check(r.get(3)==0&&r.get(2)==0&&r.get(3)==0,"remaining actor overrides stay unset");
    check(r.get(1)==1&&r.get(31)==generation,"parent generation");
    check(r.get(1)==1&&r.get(32)==0,"native scalar default");
    check(r.get(1)==1&&r.get(32)==0x811C9DC5U,"native hash default");
    absent_reference(r);absent_reference(r);absent_reference(r);
    check(r.get(1)==1&&r.get(32)==0x95FB2E01U&&r.get(7)==67&&r.get(16)==0x8039U,
          "auth+A0 selects exact authored spawn rule66/57");
    check(r.get(1)==1&&r.get(31)==0&&r.get(1)==1&&r.get(31)==0,"remaining counters default zero");
    check(r.get(1)==1&&r.get(6)==1&&r.get(1)==1&&r.get(5)==1,"biased counters default zero");
    check(r.get(1)==1&&r.get(31)==0,"independent Scene revision stays zero");
    check(r.get(2)==1&&r.get(3)==1,"native retirement and request modes zero");
    check(r.get(1)==1&&r.get(32)==0x811C9DC5U,"terminal native hash default");
    check(r.position==641,"entire independently decoded parent consumes641bits");
    check(r.get(7)==0,"unused byte padding stays zero");
}
void member_fields(std::span<const std::byte> bytes,bool active,std::uint32_t generation) {
    Reader r{bytes};
    check(r.get(1)==1&&r.get(31)==generation,"member shares exact source generation");
    check(r.get(2)==1&&r.get(3)==1,"member retirement/location modes decode zero");
    check(r.get(1)==static_cast<unsigned>(active),"member enabled only for active authority");
    // Schema37D5AC0 fields4..7:+008,+04C,+100,+910. In particular the
    // command record+100 stays at native revision/head/count0; never resend a queue.
    for(unsigned i=0;i<4;++i)check(r.get(1)==0,"nested field absent preserves native registration defaults");
    check(r.position==42&&r.get(6)==0,"member framing and trailing padding");
}
}

int main(int argc,char** argv) {
    // Generated independently from pinned reflection and native default command
    // programs by authority/verify_authority_schema.py, not from this header.
    constexpr std::string_view parentGolden=
        "c08e4ee2807fffc08e4ee2807fff88c000000042500800000018000000060472771702393b8a01ffff"
        "02393b8a01ffff02393b8a01ffff2bf65c030e00e600000002000000020c3000000009c08e4ee280";
    std::array<std::byte,81> firstParent{};
    std::array<std::byte,6> firstMember{};
    for(bool active:{false,true}) {
        for(std::uint32_t generation:{1U,17U,0x7FFFFFFFU}) {
            std::array<std::byte,81> parent{};std::array<std::byte,6> member{};
            bits::Writer p(parent),m(member);
            check(authority::write_parent(p,active,generation)&&p.bit_count()==641,"parent serializes full native body");
            check(authority::write_member(m,active,generation)&&m.bit_count()==42,"member serializes full native body");
            parent_fields(parent,active?generation:0U);member_fields(member,active,active?generation:0U);
            if(active&&generation==1){firstParent=parent;firstMember=member;hex_matches(parent,parentGolden);hex_matches(member,"800000014c00");}
            if(!active)hex_matches(member,"800000004800");
        }
    }
    for(std::uint32_t invalid:{0U,0x80000000U,0xFFFFFFFFU}) {
        std::array<std::byte,81> buffer{};bits::Writer w(buffer);
        check(!authority::write_parent(w,true,invalid)&&w.bit_count()==0,"invalid parent activation rejected before writing");
        check(!authority::write_member(w,true,invalid)&&w.bit_count()==0,"invalid member activation rejected before writing");
    }
    std::array<std::byte,80> shortParent{};bits::Writer p(shortParent);
    std::array<std::byte,5> shortMember{};bits::Writer m(shortMember);
    check(!authority::write_parent(p,true,1),"640bits cannot hold parent641bits");
    check(!authority::write_member(m,true,1),"40bits cannot hold member42bits");
    for(auto type:std::array<std::uint8_t,5>{0,1,2,6,66})
    for(auto index:std::array<std::uint16_t,4>{0,1,22,57}) {
        check(authority::parent_slot(0x95FB2E01U,type,index)==(type==1&&index==0),"parent exact slot scope");
        check(authority::member_slot(0x95FB2E01U,type,index)==(type==2&&index==1),"member exact slot scope");
        check(!authority::parent_slot(0x95FB2E00U,type,index)&&!authority::member_slot(0x95FB2E00U,type,index),"unrelated registry rejected");
    }
    if(argc==3) {
        const auto save=[](const char* path,std::span<const std::byte> data){
            FILE* f=nullptr;check(fopen_s(&f,path,"wb")==0&&f,"open export");
            check(std::fwrite(data.data(),1,data.size(),f)==data.size(),"write export");check(std::fclose(f)==0,"close export");
        };
        save(argv[1],firstParent);save(argv[2],firstMember);
    }
    std::printf("omega_boss_authority: %u checks passed\n",checks);
}
