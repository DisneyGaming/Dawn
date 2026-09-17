#include "client/hooks/bootflow/omega_boss_eye_diagnostics.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace eye = dawn::client::hooks::bootflow::omega_boss_eye_diagnostics;
namespace omega_boss_eye_diagnostics = eye;
namespace graph = dawn::client::hooks::bootflow::omega_boss_graph;
namespace observation = dawn::client::hooks::bootflow::omega_boss_graph_observation;
namespace fixture {
unsigned checks{};
void require(bool value,const char* label) {
    ++checks;
    if (!value) { std::fprintf(stderr,"FAIL: %s (%u)\n",label,checks); std::exit(1); }
}
template<class T,std::size_t N>
void put(std::array<std::byte,N>& b,std::size_t at,T value) { std::memcpy(b.data()+at,&value,sizeof value); }
std::uint64_t run{1};
std::uint64_t now{100};
graph::Owner owner{1,2,3,4,5,6,7,8,9,0,10};
alignas(16) std::array<std::byte,0x60> bytes{};
alignas(16) std::array<std::array<float,4>,67> vectors{};
alignas(16) std::array<std::byte,0x1474> animation{};
alignas(16) std::array<std::byte,47*0x30> providers{};
std::byte member{};
unsigned getters{};
bool findAvailable{true},bound{true},freshOwner{true},memberEnabled{true};
bool changeSelf{},changeRun{},changeOwner{},changeMember{};
std::vector<std::string> lines;
void reset(std::uint64_t epoch) {
    run=epoch; owner.run=epoch; now=100; bytes={}; vectors={}; getters=0;
    findAvailable=bound=freshOwner=memberEnabled=true; changeSelf=changeRun=changeOwner=changeMember=false;
    put(bytes,0,std::uint32_t{0x80F6695E}); put(bytes,4,std::uint32_t{0x80809790});
    put(bytes,8,std::int64_t{0x1768}); put(bytes,0x24,std::uint32_t{20});
    put(bytes,0x2C,owner.entity); put(bytes,0x50,std::uint64_t{67});
    put(bytes,0x58,reinterpret_cast<std::uintptr_t>(vectors.data())
        -reinterpret_cast<std::uintptr_t>(bytes.data())-0x68);
    for (std::size_t i=0;i<eye::kInputs.size();++i)
        vectors[static_cast<std::size_t>(eye::kInputs[i].index)].fill(static_cast<float>(i+1));
    animation={}; providers={};
    put(animation,0,std::uint32_t{0x80F6690A});put(animation,4,std::uint32_t{0x808082EC});
    put(animation,8,std::int64_t{0x3038});put(animation,0x24,owner.parent);
    put(animation,0x2C,owner.entity);put(animation,0x1470,owner.actor);
    put(animation,0x1328,std::uint64_t{47});
    put(animation,0x1330,reinterpret_cast<std::uintptr_t>(providers.data())
        -reinterpret_cast<std::uintptr_t>(animation.data())-0x1340);
    constexpr auto at=eye::kAnimationGlowProvider*0x30;
    put(providers,at,std::uint32_t{0x80F6690A});put(providers,at+4,std::uint32_t{0x80807EEB});
    put(providers,at+8,std::int64_t{0x4340});put(providers,at+0x20,0.5F);
    put(providers,at+0x10,reinterpret_cast<std::uintptr_t>(animation.data())
        -reinterpret_cast<std::uintptr_t>(providers.data()+at)-0x10);
}
observation::Snapshot snapshot(observation::Phase phase) {
    observation::Snapshot s{};
    s.entity=owner.entity; s.character=owner.character; s.biped=owner.biped;
    s.graphAsset=observation::kGraphAsset; s.phase=phase; s.nativeResult=s.active=true;
    return s;
}
}
namespace state::activity { std::uint64_t mission_run_generation() noexcept {return fixture::run;} }
std::uint64_t GetTickCount64() noexcept {return fixture::now;}
struct alignas(16) ScalarValue { std::array<float,4> lanes{}; };
struct ScalarView { ScalarValue value{}; std::uint32_t self{20}; unsigned failure{}; bool bound{}; };
bool left_scalar(std::uint32_t,ScalarView& out) noexcept {
    out.bound=fixture::bound;
    if (!fixture::findAvailable) out.failure=1;
    return fixture::findAvailable;
}
bool scalar_equals(const ScalarValue& value,float expected) noexcept {
    return std::all_of(value.lanes.begin(),value.lanes.end(),[expected](float v){return v==expected;});
}
std::byte* resolve_handle(std::uint32_t self) noexcept {
    if (self==fixture::owner.member) return &fixture::member;
    if (self==fixture::owner.parent) return fixture::animation.data();
    return self==20 ? fixture::bytes.data() : nullptr;
}
bool readable(const void* p,std::size_t bytes) noexcept {
    const auto at=reinterpret_cast<std::uintptr_t>(p);
    for (const auto& range : {std::pair{reinterpret_cast<std::uintptr_t>(fixture::bytes.data()),fixture::bytes.size()},
                              std::pair{reinterpret_cast<std::uintptr_t>(fixture::vectors.data()),sizeof fixture::vectors},
                              std::pair{reinterpret_cast<std::uintptr_t>(fixture::animation.data()),fixture::animation.size()},
                              std::pair{reinterpret_cast<std::uintptr_t>(fixture::providers.data()),fixture::providers.size()}})
        if (at>=range.first && at-range.first<=range.second && bytes<=range.second-(at-range.first)) return true;
    return false;
}
bool copy_native(const void* p,void* out,std::size_t n) noexcept {
    if (!readable(p,n)) return false; std::memcpy(out,p,n); return true;
}
struct MemberView { bool enabled{true},exactQueue{true}; int head{},count{1}; };
bool member_view(std::byte*,MemberView& out) noexcept {out.enabled=fixture::memberEnabled; return true;}
bool character_owner(const MemberView&,std::uint64_t,std::uint32_t,graph::Owner& out) noexcept {
    out=fixture::owner; return fixture::freshOwner;
}
void getter(std::byte*,std::int32_t index,ScalarValue* out) noexcept {
    ++fixture::getters;
    out->lanes=fixture::vectors[static_cast<std::size_t>(index)];
    if (fixture::changeSelf) fixture::put(fixture::bytes,0x24,std::uint32_t{21});
    if (fixture::changeRun) ++fixture::run;
    if (fixture::changeOwner) ++fixture::owner.character;
    if (fixture::changeMember) fixture::memberEnabled=false;
}
template<class Function> Function native(std::uintptr_t rva) noexcept {
    fixture::require(rva==0x57A770,"only existing read-only getter used");
    return reinterpret_cast<Function>(&getter);
}
void log(const char* format,...) noexcept {
    char line[1600]{}; va_list args; va_start(args,format);
    std::vsnprintf(line,sizeof line,format,args); va_end(args); fixture::lines.emplace_back(line);
}
#include "../src/client/hooks/bootflow/omega_boss_eye_diagnostics.inl"

int main(int argc,char** argv) {
    using fixture::require;
    require(argc==3,"extracted80F6695E and80F6690A asset arguments required");
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<char> raw{std::istreambuf_iterator<char>(input),{}};
    const auto asset=std::as_bytes(std::span(raw));
    require(asset.size()==18752,"complete extracted controller asset");
    require(eye::field<std::uint32_t>(asset,0x4A0)==0x80F6695E,"asset controller identity");
    require(eye::field<std::uint32_t>(asset,0x4A4)==0x80809790,"asset controller class");
    require(eye::field<std::uint64_t>(asset,0x4A8)==0x1768,"asset definition offset");
    require(eye::field<std::uint64_t>(asset,0x4F0)==67,"asset vector count");
    constexpr std::array<std::size_t,9> records{0x45AC,0x45C4,0x44F8,0x4504,0x451C,0x45E8,0x45F4,0x48A4,0x44E0};
    for (std::size_t i=0;i<records.size();++i) {
        require(eye::field<std::uint32_t>(asset,records[i])==eye::kInputs[i].hash,"asset native input hash");
        require(eye::field<std::int32_t>(asset,records[i]+4)==eye::kInputs[i].index,"asset native input index");
        const auto values=0x508+eye::field<std::uint64_t>(asset,0x4F8);
        for (unsigned lane=0;lane<4;++lane)
            require(eye::field<float>(asset,values+16U*static_cast<unsigned>(eye::kInputs[i].index)+lane*4U)==0.F,
                    "asset inputs begin zero before native producers");
    }
    std::ifstream animationInput(argv[2],std::ios::binary);
    std::vector<char> animationRaw{std::istreambuf_iterator<char>(animationInput),{}};
    const auto animationAsset=std::as_bytes(std::span(animationRaw));
    require(animationAsset.size()==17600,"complete extracted animation component asset");
    require(eye::field<std::uint32_t>(animationAsset,0x90)==0x80F6690A,"original animation component");
    require(eye::field<std::uint32_t>(animationAsset,0x94)==0x808082EC,"original animation runtime type");
    require(eye::field<std::uint64_t>(animationAsset,0x98)==0x3038,"original animation definition");
    require(eye::field<std::uint64_t>(animationAsset,0x13B8)==eye::kAnimationProviderCount,"original provider count");
    const auto providerOffset=0x13D0+eye::field<std::uint64_t>(animationAsset,0x13C0)
        +eye::kAnimationGlowProvider*0x30;
    require(providerOffset==0x2EE0,"original CE provider ordinal");
    require(eye::animation_glow_provider(animationAsset.subspan(providerOffset,0x30)),"original CE provider identity");
    require(eye::field<std::uint32_t>(animationAsset,0x4368)==0xCE0BA42D,"original CE provider name");
    require(eye::field<float>(animationAsset,providerOffset+0x20)==0.F,"original provider starts zero");
    fixture::reset(1);
    float producer{};
    require(read_animation_glow(fixture::owner,producer) && producer==0.5F,"read current animation provider");
    for (const auto at : {0U,4U,8U,0x24U,0x2CU,0x1328U,0x1330U,0x1470U}) {
        const auto saved=fixture::animation;fixture::animation[at]^=std::byte{1};
        require(!read_animation_glow(fixture::owner,producer),"wrong animation owner/layout refused");
        fixture::animation=saved;
    }
    for (const auto at : {0U,4U,8U,0x10U}) {
        const auto saved=fixture::providers;fixture::providers[eye::kAnimationGlowProvider*0x30+at]^=std::byte{1};
        require(!read_animation_glow(fixture::owner,producer),"wrong provider definition/backlink refused");
        fixture::providers=saved;
    }
    require(eye::controller(fixture::bytes,20,fixture::owner.entity),"exact controller admitted");
    for (const auto at : {0U,4U,8U,0x24U,0x2CU,0x50U}) {
        const auto before=fixture::bytes; fixture::bytes[at]^=std::byte{1};
        require(!eye::controller(fixture::bytes,20,fixture::owner.entity),"wrong controller field rejected");
        fixture::bytes=before;
    }
    for (std::size_t n=0;n<fixture::bytes.size();++n)
        require(!eye::controller(std::span(fixture::bytes).first(n),20,fixture::owner.entity),"truncated controller rejected");
    auto s=fixture::snapshot(observation::Phase::leadIn);
    auto wrong=fixture::owner; ++wrong.run;
    observe_eye_inputs(wrong,s); require(fixture::lines.empty(),"wrong run cannot claim phase");
    wrong=fixture::owner; ++wrong.entity;
    observe_eye_inputs(wrong,s); require(fixture::lines.empty(),"wrong graph owner cannot claim phase");
    for (unsigned phase=1;phase<=5;++phase) {
        s=fixture::snapshot(static_cast<observation::Phase>(phase));
        observe_eye_inputs(fixture::owner,s); observe_eye_inputs(fixture::owner,s);
        require(fixture::lines.size()==2*phase,"one pair of samples per graph phase");
        require(fixture::lines.back().find("available=1")!=std::string::npos,"phase input sample available");
        const auto& inputs=fixture::lines[fixture::lines.size()-2];
        require(inputs.find("body_health=1 eye_health=2 alive=3")!=std::string::npos,"indices read in native order");
        require(inputs.find("illumination=7")!=std::string::npos,"illumination remains independent input");
        require(fixture::lines.back().find("input_CE0BA42D=8 computed_20AA7FC1=9")!=std::string::npos,
                "animation producer and computed visual result sampled separately");
        require(fixture::lines.back().find("animation_available=1 animation_CE0BA42D=0.5")!=std::string::npos,
                "original animation producer precedes bound script input");
    }
    require(fixture::getters==45,"exactly nine read-only getters per phase");
    wrong=fixture::owner; ++wrong.revision;
    observe_eye_inputs(wrong,fixture::snapshot(observation::Phase::leadIn));
    require(fixture::lines.size()==10,"same-run changed command cannot reset phase claims");
    fixture::now=1099;
    observe_eye_inputs(fixture::owner,fixture::snapshot(observation::Phase::leadIn));
    require(fixture::lines.size()==10,"settled sample waits a full second");
    fixture::now=1100;
    for (unsigned phase=1;phase<=5;++phase) {
        s=fixture::snapshot(static_cast<observation::Phase>(phase));
        observe_eye_inputs(fixture::owner,s); observe_eye_inputs(fixture::owner,s);
        require(fixture::lines.size()==10+2*phase,"one settled pair per phase");
        require(fixture::lines.back().find("sample=1")!=std::string::npos,"settled sample identified");
    }
    fixture::now=10000;
    for (unsigned phase=1;phase<=5;++phase)
        observe_eye_inputs(fixture::owner,fixture::snapshot(static_cast<observation::Phase>(phase)));
    require(fixture::lines.size()==20 && fixture::getters==90,"ten available snapshots are the maximum per owner");
    const auto firstOwner=fixture::owner;
    for (unsigned scenario=0;scenario<10;++scenario) {
        fixture::reset(2+scenario); const auto before=fixture::lines.size();
        if (scenario==0) fixture::findAvailable=false;
        if (scenario==1) fixture::bound=false;
        if (scenario==2) fixture::put(fixture::bytes,0,std::uint32_t{});
        if (scenario==3) fixture::put(fixture::bytes,0x58,std::uintptr_t{1});
        if (scenario==4) fixture::changeSelf=true;
        if (scenario==5) fixture::freshOwner=false;
        if (scenario==6) fixture::changeRun=true;
        if (scenario==7) fixture::put(fixture::bytes,0x50,std::uint64_t{66});
        if (scenario==8) fixture::changeOwner=true;
        if (scenario==9) fixture::changeMember=true;
        s=fixture::snapshot(observation::Phase::leadIn);
        const auto issued=fixture::owner;
        observe_eye_inputs(issued,s);
        require(fixture::lines.size()==before+1,"unavailable sample reported once");
        require(fixture::lines.back().find("available=0")!=std::string::npos,"unsafe sample not reported as available");
        const auto getters=fixture::getters;
        observe_eye_inputs(issued,s);
        require(fixture::lines.size()==before+1 && fixture::getters==getters,"unavailable phase never retried");
    }
    const auto total=fixture::lines.size(); fixture::run=firstOwner.run;
    s.entity=firstOwner.entity; s.character=firstOwner.character; s.biped=firstOwner.biped;
    observe_eye_inputs(firstOwner,s); require(fixture::lines.size()==total,"older run cannot reset diagnostic ledger");
    for (const auto& line : fixture::lines) require(line.size()<512,"diagnostic fits native log limit");
    std::printf("PASS: %u eye-input diagnostic checks\n",fixture::checks);
}
