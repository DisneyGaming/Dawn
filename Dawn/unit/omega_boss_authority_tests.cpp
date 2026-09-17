#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string_view>

#include "state/activity/omega/omega_boss_authority.h"
#include "state/activity/omega/omega_mission_state.h"
#include "middleware/encoding/bit_writer.h"

namespace authority=dawn::state::activity::omega::boss_authority;
namespace bits=dawn::middleware::encoding::bits;
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
    for(bool active:{false,true}) {
        const authority::ArmControl previous{17,29,false,true};
        const authority::ArmControl reset{};
        std::array<std::byte,39> bytes{};bits::Writer w(bytes);
        const auto& control=active?reset:previous;
        check(authority::write_member(w,active,17,control,true) && w.bit_count()==180,"boss startup/dormancy explicitly resets native control");
        check(authority::member_bits(active,17,control,true)==180,"reset control catalog size");
        Reader r{bytes};r.get(39);
        check(r.get(1)==1 && r.get(31)==0,"explicit reset defeats previous optional control revision");
        r.get(6);r.get(6);r.get(3);r.get(32);r.get(7);r.get(16);r.get(32);
        check(r.get(5)==0 && r.get(1)==0 && r.get(1)==0 && r.position==180,"reset emits no scalar action or replacement graph queue");
    }
    for(bool right:{false,true}) for(bool high:{false,true}) {
        const authority::ArmControl control{17,29,right,high};
        std::array<std::byte,39> bytes{};bits::Writer w(bytes);
        check(authority::write_member(w,true,17,control) && w.bit_count()==308,"native control body is308bits");
        check(authority::member_bits(true,17,control)==308,"catalog length includes exact native control");
        Reader r{bytes};
        check(r.get(1)==1 && r.get(31)==17 && r.get(2)==1 && r.get(3)==1 && r.get(1)==1,"arm update preserves boss lifetime and placement");
        check(r.get(1)==0 && r.get(1)==1 && r.get(31)==29,"independent control revision");
        check(r.get(6)==0 && r.get(6)==0 && r.get(3)==0,"control flags and hash list remain untouched");
        check(r.get(32)==0x811C9DC5U && r.get(7)==0 && r.get(16)==32767 && r.get(32)==0,"control target and index remain native defaults");
        check(r.get(5)==2 && r.get(32)==0xA2AE120FU && r.get(32)==(high&&!right?0x3F800000U:0U),"left exact authored scalar row");
        check(r.get(32)==0x8496ABD2U && r.get(32)==(high&&right?0x3F800000U:0U),"right exact authored scalar row");
        check(r.get(1)==0 && r.get(1)==0 && r.position==308,"control cannot replace animation queue or transport payload");
        std::array<std::byte,6> dormant{};bits::Writer d(dormant);
        check(authority::write_member(d,false,17,control) && d.bit_count()==42,"inactive boss never publishes float control");
        bits::Writer wrong(bytes);
        check(!authority::write_member(wrong,true,18,control) && wrong.bit_count()==0,"foreign spawn generation rejected before mutation");
    }
    {
        namespace mission=dawn::state::activity::omega::mission;
        mission::State state;const mission::Boss owner{1,17,11,12,13,14,15,0};
        check(state.bind(owner),"bind arm authority owner");
        auto token=state.snapshot().command.token;auto stale=token;++stale.boss.generation;
        check(!state.prepare_arm(stale) && !state.release_arm(token),"stale and unordered arm requests rejected");
        check(state.prepare_arm(token) && state.snapshot().arm.revision==1,"native readiness schedules rising left control");
        check(!state.prepare_arm(token) && !state.release_arm(token),"pending publication cannot repeat or release");
        check(!state.arm_applied(stale,1) && !state.arm_applied(token,2),"foreign owner and future revision cannot acknowledge");
        check(state.arm_applied(token,1) && !state.arm_applied(token,1),"native apply accepted exactly once");
        check(!state.release_arm(token),"scalar application alone cannot finish animation");
        check(state.animation(token,mission::Animation::started),"real playback starts wave");
        check(state.release_arm(token) && state.snapshot().arm.revision==2 && !state.snapshot().arm.high,"native wrap schedules both arms low");
        check(state.snapshot().command.token==token && state.snapshot().command.wave==0,"release publication cannot advance wave");
        check(!state.arm_applied(token,1) && state.arm_applied(token,2),"only low control receipt advances initial wave");
        check(state.snapshot().command.wave==1 && !state.prepare_arm(token),"prior epoch cannot rearm subsequent wave");
        token=state.snapshot().command.token;
        check(state.prepare_arm(token) && state.snapshot().arm.right && state.snapshot().arm.revision==3,"next right arm has fresh independent revision");
        check(state.snapshot().command.token.boss.revision==0,"arm publications preserve exact graph owner revision");
        auto nextOwner=owner;++nextOwner.run;++nextOwner.generation;
        check(state.bind(nextOwner) && state.snapshot().arm.revision==0 && !state.arm_applied(token,3),"new run invalidates all prior control receipts");
    }
    for(const auto sequence:{0x65D2379FU,0x65D2379CU,0x65D2379EU,0x65D2379DU,0x65D2379BU}) {
        for(std::uint32_t revision:{1U,2U,4U,0x7FFFFFFFU}) {
            const authority::IntroProgram program{17,revision,true,sequence};
            for(bool armed:{false,true}) {
                const authority::ArmControl arm=armed?authority::ArmControl{17,29,false,false}:authority::ArmControl{};
                std::array<std::byte,65> bytes{};bits::Writer writer(bytes);
                const auto width=armed?520U:392U;
                check(authority::write_member(writer,true,17,arm,true,program,true) && writer.bit_count()==width,"active archive composes .5 and independent .6");
                check(authority::member_bits(true,17,arm,true,program,true)==width,"active catalog matches composed body width");
                Reader reader{bytes};reader.position=armed?306:178;
                check(reader.get(1)==1 && reader.get(31)==revision && reader.get(6)==0 && reader.get(6)==1,"program revision, resume head and single command");
                check(reader.get(1)==1 && reader.get(4)==10 && reader.get(2)==1,"native kind9 and default secondary condition");
                check(reader.get(32)==0xAFB11A12U && reader.get(32)==sequence && reader.get(32)==0x811C9DC5U,"exact named intro/crown/cycle program");
                check(reader.get(32)==0x811C9DC5U && reader.get(7)==0 && reader.get(16)==32767,"native queue target remains absent");
                check(reader.get(3)==1 && reader.get(8)==128,"native original queue mode0 and marker0, not generic ability defaults");
                check(reader.get(1)==0 && reader.position==width,"program cannot change delivery authority");
            }
        }
    }
    for(std::int8_t marker=0;marker<5;++marker) {
        const authority::IntroProgram program{17,10,true,0xCBFDCA32U,marker};
        std::array<std::byte,65> bytes{};bits::Writer writer(bytes);
        check(authority::write_member(writer,true,17,{17,29,false,false},true,program,true) && writer.bit_count()==520,"targeted movement composes current low control");
        check(authority::member_bits(true,17,{17,29,false,false},true,program,true)==520,"targeted movement body length");
        Reader reader{bytes};reader.position=306;
        check(reader.get(1)==1 && reader.get(31)==10 && reader.get(6)==0 && reader.get(6)==1,"movement revision and queue count");
        check(reader.get(1)==1 && reader.get(4)==10 && reader.get(2)==1,"movement uses native kind9");
        check(reader.get(32)==0x1F992208U && reader.get(32)==0xCBFDCA32U && reader.get(32)==0x811C9DC5U,"authored teleport selector hashes");
        check(reader.get(32)==0x95FB2E01U && reader.get(7)==49 && reader.get(16)==0x8037U,"movement references actual type48 path slot55");
        check(reader.get(3)==1 && reader.get(8)==static_cast<unsigned>(128+marker),"native targeting mode and exact path milestone");
        check(reader.get(1)==0 && reader.position==520,"movement consumes exact body");
    }
    for(const authority::IntroProgram program:{authority::IntroProgram{17,1,true,0xCBFDCA32U,-1},authority::IntroProgram{17,1,true,0xCBFDCA32U,5},authority::IntroProgram{17,1,true,0x65D2379FU,0},authority::IntroProgram{17,1,true,0x65D2379FU,-2}}) {
        std::array<std::byte,65> bytes{};bits::Writer writer(bytes);
        check(!authority::write_member(writer,true,17,{},true,program,true) && writer.bit_count()==0,"invalid target/program pairing rejected before encoding");
        check(!authority::member_bits(true,17,{},true,program,true),"invalid target rejected by catalog");
    }
    for(const authority::IntroProgram program:{authority::IntroProgram{},authority::IntroProgram{17,2,false}}) {
        std::array<std::byte,28> bytes{};bits::Writer writer(bytes);
        check(authority::write_member(writer,true,17,{},true,program,true) && writer.bit_count()==223,"explicit startup/reset queue and pending cancellation are empty");
        Reader reader{bytes};reader.position=178;
        check(reader.get(1)==1 && reader.get(31)==program.revision && reader.get(6)==0 && reader.get(6)==0 && reader.get(1)==0,"empty program carries only control revision");
    }
    for(const authority::IntroProgram program:{authority::IntroProgram{18,1,true},authority::IntroProgram{17,0x80000000U,true},authority::IntroProgram{17,1,true,0x12345678U}}) {
        std::array<std::byte,65> bytes{};bits::Writer writer(bytes);
        check(!authority::write_member(writer,true,17,{},true,program,true) && !writer.bit_count(),"foreign or unknown program is rejected before encoding");
        check(!authority::member_bits(true,17,{},true,program,true),"catalog rejects invalid program too");
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
