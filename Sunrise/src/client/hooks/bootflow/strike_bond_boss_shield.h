#pragma once
#include "strike_bond_intro_release.h"
#include "omega_boss_health_identity.h"
#include <bit>
#include <cmath>
namespace sunrise::client::hooks::bootflow::strike_bond_boss_shield {
namespace mission=state::activity::strike_bond;
namespace trace=strike_bond_fire_trace;
// Live isolation identifies pass 7 as an effect layer, leaving the body intact.
// Its combat presentation must not follow damage immunity: the purple face
// effect is present while Dendron takes damage. Native visibility owners retain
// control of phase-specific suppression over this base contribution.
inline constexpr std::uint8_t kShieldPass=7;
inline constexpr std::uintptr_t kPassFlagsOffset=0xC4+2*kShieldPass;
inline constexpr std::uintptr_t kDrawRva=0x1150420;
inline constexpr std::array<std::uint8_t,16> kDrawPrefix{
    0x48,0x83,0xEC,0x48,0x8B,0x41,0x20,0x83,0xF8,0xFF,0x74,0x2D,0x89,0x54,0x24,0x20};
inline bool visible(const mission::Frame& f) noexcept {
    return f.enabled && f.bossFighting && !f.finished && !f.ending && !f.bossDead
        && f.bossCycle.mode!=mission::BossMode::dying;
}
inline bool wanted(const mission::BossRequest& r) noexcept {
    return trace::admitted(r) && r.frame.region==136 && r.frame.bossStage<=2;
}
// 1225BB0 initializes bit 0 from the mesh's base pass mask. Native reference
// owners add/remove only 0x222 << slot through 12244D0/1228D30. All native
// publications, including full reconstruction at 1227130, reduce this word.
// Own only the base bit; never overwrite those separate reference contributions.
inline std::uint8_t enabled_count(std::uint16_t flags) noexcept {
    return static_cast<std::uint8_t>(std::popcount(static_cast<unsigned>(flags&((flags&0x1E00U)?0x1E0U:0x1E1U))));
}
struct Binding {
    trace::Identity character{};
    std::uintptr_t model{},health{},modelDefinition{},mesh{};
    std::uint32_t modelSelf{},healthSelf{},renderer{},displayOwner{};
    std::uint16_t passFlags{};bool authoredBase{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
inline bool same_model(const Binding& a,const Binding& b) noexcept {
    return a.character==b.character && a.model==b.model && a.modelSelf==b.modelSelf;
}
inline bool same_binding(Binding a,Binding b) noexcept {
    a.passFlags=b.passFlags=0;return a==b;
}
struct BaseLease {
    state::activity::coo::Generation owner{};mission::EnemyReceipt enemy{};Binding binding{};
    bool captured{},originalBase{},retiring{};
    bool bind(state::activity::coo::Generation current,const mission::EnemyReceipt& actor,const Binding& model) noexcept {
        if(captured && owner==current && enemy==actor && same_model(binding,model)) return false;
        owner=current;enemy=actor;binding=model;captured=true;originalBase=model.authoredBase;retiring=false;
        return true;
    }
    std::uint16_t project(std::uint16_t flags,bool show,bool terminal) noexcept {
        retiring|=terminal;
        return static_cast<std::uint16_t>((flags&~1U)|(show && !retiring && originalBase?1U:0U));
    }
};
inline bool terminal(const mission::Frame& f) noexcept {
    return f.bossCycle.mode==mission::BossMode::dying || f.bossDead || f.ending || f.finished;
}
// Authored animation output -> bound controller input8 -> filtered output23
// (9F184F85) -> model input4 -> material80F66FEF opacity. 1257730 publishes
// model inputs in order; 11B53EC/118BAE3/118A2DF supply them to the renderer.
// Its native spline maps0 to alpha.5 and1 to alpha0. Other face materials do
// not consume input4. Use the native provider setter, never a material edit.
inline constexpr std::uint32_t kPhaseInput=0x0958590CU;
inline constexpr std::uintptr_t kPhaseSetterRva=0xA0FE60;
inline float phase_value(const mission::Frame& f) noexcept {
    return f.enabled && f.bossFighting && !terminal(f) && f.bossCycle.mode==mission::BossMode::damage?1.F:0.F;
}
struct PhaseInput {
    std::uintptr_t parent{},provider{};std::uint32_t parentSelf{};float value{};
    friend bool operator==(const PhaseInput&,const PhaseInput&)=default;
};
template<class Read> bool phase_sample(Read& read,std::uintptr_t image,const Binding& b,
    const mission::BossRequest& request,PhaseInput& out) noexcept {
    if(!wanted(request) || terminal(request.frame)) return false;
    std::uintptr_t table{},parent{},resource{},producer{},again{};std::uint32_t stride{},self{},value{};
    std::uint64_t count{};std::int64_t relative{};
    if(!read.value(image+0x1F9D7F8,table) || table<0x10000 || !read.value(image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000) return false;
    const auto actor=table+static_cast<std::uintptr_t>(request.enemy.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x48,value) || value!=request.enemy.actor
        || !read.value(actor+0x4C,value) || value!=b.character.entity || !read.value(actor+0x50,self)
        || self==UINT32_MAX || !read.resolve(self,parent) || parent<0x10000
        || !strike_bond_intro_release::component_header(read,parent,0x80F66F55U,0x808082ECU,0x2ED8,b.character.entity,value)
        || value!=self || !read.value(parent+0x1470,value) || value!=request.enemy.actor
        || !read.value(parent+0x1328,count) || count!=45 || !read.value(parent+0x1330,relative)
        || relative<0 || relative>0x100000) return false;
    const auto provider=parent+0x1340+static_cast<std::uintptr_t>(relative);
    std::array<std::byte,0x30> row{};
    if(!read.copy(provider,row) || trace::field<std::uint32_t>(row,0)!=0x80F66F55U
        || trace::field<std::uint32_t>(row,4)!=0x80807EEBU || trace::field<std::uint64_t>(row,8)!=0x39F0
        || provider+0x10+trace::field<std::uintptr_t>(row,0x10)!=parent) return false;
    const auto current=trace::field<float>(row,0x20);
    if(!std::isfinite(current) || current<0.F || current>1.F || !read.resolve(0x80F66F55U,resource) || resource<0x10000) return false;
    std::array<std::byte,0x330> definition{};std::array<std::byte,45*0x30> providers{};
    if(!read.copy(resource+0x2ED8,definition) || trace::field<std::uint32_t>(definition,0)!=0x80F66F55U
        || trace::field<std::uint32_t>(definition,4)!=0x80807EE1U || trace::field<std::uint64_t>(definition,8)!=0x90
        || trace::field<std::uint64_t>(definition,0x320)!=45
        || 0x2ED8+0x338+trace::field<std::uint64_t>(definition,0x328)!=0x39F0
        || !read.copy(resource+0x39F0,providers)) return false;
    std::uint32_t previous{};
    for(std::size_t i=0;i<45;++i) {
        const auto at=i*0x30;const auto name=trace::field<std::uint32_t>(providers,at+0x28);
        if(trace::field<std::uint32_t>(providers,at)!=0x80F66F55U
            || trace::field<std::uint32_t>(providers,at+4)!=0x80807EEAU
            || trace::field<std::uint64_t>(providers,at+8)!=0x2660+i*0x30
            || trace::field<std::uint32_t>(providers,at+0x18)!=0x80BFDDA6U
            || (i==0?name!=kPhaseInput:name<=previous)) return false;
        previous=name;
    }
    // Qualify the renderer's actual bound producer, not just an identically
    // named provider on another actor or a stale resource allocation.
    if(!read.value(b.model+0x120,count) || count!=9 || !read.value(b.model+0x128,relative)
        || relative<0 || relative>0x100000) return false;
    const auto input=b.model+0x138+static_cast<std::uintptr_t>(relative)+4*0x60;
    std::array<std::byte,0x58> external{};
    if(!read.copy(input,external) || trace::field<std::uint32_t>(external,0)!=0x80F4599FU
        || trace::field<std::uint32_t>(external,4)!=0x80809789U || trace::field<std::uint64_t>(external,8)!=0x1720
        || trace::field<std::int32_t>(external,0x50)!=23) return false;
    const auto producerSelf=trace::field<std::uint32_t>(external,0x38);
    if(producerSelf==UINT32_MAX || !read.resolve(producerSelf,producer) || producer<0x10000
        || !strike_bond_intro_release::component_header(read,producer,0x80F66F70U,0x80809790U,0x1668,b.character.entity,value)
        || value!=producerSelf || !read.value(producer+0x50,count) || count!=76
        || !read.resolve(self,again) || again!=parent || !read.value(actor+0x48,value) || value!=request.enemy.actor
        || !read.value(actor+0x4C,value) || value!=b.character.entity || !read.value(actor+0x50,value) || value!=self) return false;
    out={parent,provider,self,current};return true;
}
template<class Read> bool phase_boundaries(Read& read,std::uintptr_t image) noexcept {
    constexpr std::array<std::uint8_t,16> setter{0x40,0x53,0x48,0x83,0xEC,0x20,0x44,0x8B,0x09,0x48,0x8B,0xD9,0x41,0x8B,0xC1,0x4C};
    constexpr std::array<std::uint8_t,16> notify{0x48,0x83,0xEC,0x28,0xF3,0x0F,0x10,0x41,0x20,0x4C,0x8D,0x51,0x20,0xF3,0x0F,0x5C};
    std::array<std::uint8_t,16> before{},after{};
    return read.value(image+kPhaseSetterRva,before) && before==setter && read.value(image+0xA10180,after) && after==notify;
}
inline std::uint16_t native_enables(std::uint16_t flags) noexcept {return static_cast<std::uint16_t>(flags&0x1E0U);}
struct Draw {std::uintptr_t object{};std::uint32_t renderer{};std::uint8_t pass{},count{};};
inline Draw command(const Binding& b) noexcept {
    return {b.model+0x1A0,b.renderer,kShieldPass,enabled_count(b.passFlags)};
}
inline bool publication_changed(const Binding& current,const Binding& desired,const Binding& published,int lastCount) noexcept {
    const auto count=enabled_count(desired.passFlags);
    return !same_binding(current,published) || enabled_count(current.passFlags)!=count || lastCount!=count;
}
template<class Read> bool boundaries(Read& read,std::uintptr_t image) noexcept {
    std::array<std::uint8_t,16> draw{};std::array<std::byte,16> fraction{};
    return read.value(image+kDrawRva,draw) && draw==kDrawPrefix
        && read.value(image+0xCD6C20,fraction) && omega_boss_health::fraction_getter_prefix(fraction);
}
// Native 1225BB0 resolves model definition +1DC, then 1185A70 enables a
// base pass when any valid mesh record has a nonempty draw range for it.
// The first mutable pass word can still be zero before that initialization.
template<class Read> bool authored_base(Read& read,Binding& b) noexcept {
    constexpr std::uint32_t modelTag=0x80F4599FU,meshTag=0x80F4599CU;
    constexpr std::size_t definitionOffset=0x790,meshBytes=0x26F0,recordsOffset=0xB0,recordBytes=0x88;
    std::uintptr_t resource{},mesh{},again{};
    std::array<std::byte,0x1E0> definition{},checkedDefinition{};
    std::array<std::byte,0x20> header{},checkedHeader{};
    std::array<std::byte,3*recordBytes> records{},checkedRecords{};
    if(!read.resolve(modelTag,resource) || resource<0x10000 || resource>UINTPTR_MAX-definitionOffset-definition.size()
        || !read.copy(resource+definitionOffset,definition)
        || trace::field<std::uint32_t>(definition,0)!=modelTag || trace::field<std::uint32_t>(definition,4)!=0x808072B8U
        || trace::field<std::uint64_t>(definition,8)!=0x80 || trace::field<std::uint32_t>(definition,0x1DC)!=meshTag
        || !read.resolve(meshTag,mesh) || mesh<0x10000 || mesh>UINTPTR_MAX-meshBytes
        || !read.copy(mesh,header) || trace::field<std::uint64_t>(header,0)!=meshBytes
        || trace::field<std::uint64_t>(header,0x10)!=3 || trace::field<std::uint64_t>(header,0x18)!=0x98
        || !read.copy(mesh+recordsOffset,records) || trace::field<std::uint64_t>(records,0)!=3
        || trace::field<std::uint32_t>(records,8)!=0x80807378U) return false;
    bool enabled=false;
    for(std::size_t i=0;i<3;++i) {
        const auto row=i*recordBytes;
        const auto count=trace::field<std::uint64_t>(records,row+0x28);
        if(count>UINT16_MAX) return false;
        for(std::size_t pass=0;pass<23;++pass) {
            const auto first=trace::field<std::uint16_t>(records,row+0x38+2*pass);
            const auto last=trace::field<std::uint16_t>(records,row+0x3A+2*pass);
            if(first>last || last>count) return false;
            if(pass==kShieldPass && first<last && trace::field<std::uint32_t>(records,row+0x10)!=UINT32_MAX
                && trace::field<std::uint32_t>(records,row+0x20)!=UINT32_MAX) enabled=true;
        }
    }
    // Reject relocation or authored-data changes across the read, and include
    // these resource identities in the existing pre-write binding comparison.
    if(!read.resolve(modelTag,again) || again!=resource || !read.resolve(meshTag,again) || again!=mesh
        || !read.copy(resource+definitionOffset,checkedDefinition) || checkedDefinition!=definition
        || !read.copy(mesh,checkedHeader) || checkedHeader!=header
        || !read.copy(mesh+recordsOffset,checkedRecords) || checkedRecords!=records) return false;
    b.modelDefinition=resource+definitionOffset;b.mesh=mesh;b.authoredBase=enabled;return true;
}
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t character,
    const mission::BossRequest& r,Binding& out) noexcept {
    if(!wanted(r)) return false;
    Binding b{};std::uintptr_t rows{};std::uint32_t stride{},bundle{},value{};
    if(!trace::sample(read,image,character,false,r,b.character)
        || !read.value(image+0x1F93428,rows) || !read.value(image+0x1F93430,stride) || stride<0xE0 || stride>0x1000) return false;
    const auto row=rows+static_cast<std::uintptr_t>(b.character.entity&0x1FFFU)*stride;
    if(!read.value(row+12,value) || value!=b.character.entity || !read.value(row+4,value) || (value&5U)
        || !read.value(row+0x4C,bundle)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x808072BDU,b.model)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x80804B8AU,b.health)
        || !strike_bond_intro_release::component_header(read,b.model,0x80F4599FU,0x808072BDU,0x790,b.character.entity,b.modelSelf)
        || !strike_bond_intro_release::component_header(read,b.health,0x815B5A47U,0x80804B8AU,0xF28,b.character.entity,b.healthSelf)) return false;
    std::array<std::byte,0x28> display{};
    if(!read.copy(b.model+0x1A0,display) || trace::field<std::uint32_t>(display,0)!=0x80F4599FU
        || trace::field<std::uint32_t>(display,4)!=0x80807315U || trace::field<std::uint64_t>(display,8)!=0x978
        || trace::field<std::int64_t>(display,0x10)!=-0x1B0
        || !read.value(b.model+0xC0,b.renderer) || b.renderer==UINT32_MAX
        || !read.value(b.model+kPassFlagsOffset,b.passFlags)) return false;
    b.displayOwner=trace::field<std::uint32_t>(display,0x20);
    if(b.displayOwner==UINT32_MAX || !authored_base(read,b)) return false;
    out=b;return true;
}
}
