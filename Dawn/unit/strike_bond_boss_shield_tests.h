#pragma once
#include "strike_bond_fire_trace_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_boss_shield.h"
namespace strike_bond_shield_fixture {
namespace shield=dawn::client::hooks::bootflow::strike_bond_boss_shield;
struct Fixture : strike_bond_fire_fixture::Fixture {
    static constexpr std::uintptr_t model=bundle+0x1000;
    static constexpr std::uintptr_t modelResource=0xE00000,modelDefinition=modelResource+0x790,mesh=0xF00000;
    static constexpr std::uint32_t modelSelf=0xE025;
    void authored_pass(bool present) {
        for(std::size_t i=0;i<3;++i) {
            const auto row=mesh+0xB0+i*0x88;
            put<std::uint64_t>(row+0x28,present?1:0);
            for(std::size_t pass=0;pass<24;++pass) put<std::uint16_t>(row+0x38+2*pass,static_cast<std::uint16_t>(present && pass>7));
        }
    }
    Fixture() {
        request.frame.region=136;put<std::uint32_t>(entity+12,5);
        zero(model,0x1C8);handles[modelSelf]=model;
        put<std::uint32_t>(model,0x80F4599F);put<std::uint32_t>(model+4,0x808072BD);put<std::uint64_t>(model+8,0x790);
        put(model+0x24,modelSelf);put<std::uint32_t>(model+0x2C,5);
        put<std::uint32_t>(model+0x1A0,0x80F4599F);put<std::uint32_t>(model+0x1A4,0x80807315);
        put<std::uint64_t>(model+0x1A8,0x978);put<std::int64_t>(model+0x1B0,-0x1B0);
        put<std::uint32_t>(model+0xC0,0x7DFD604B);put<std::uint32_t>(model+0x1C0,0x7FFD42F4);
        for(const auto pass:{0,1,3,7,9,12}) put<std::uint16_t>(model+0xC4+2*pass,1);
        put<std::uint64_t>(metadata+0x68,3);put<std::int32_t>(metadata+0x12C,0x1000);
        put<std::int32_t>(metadata+0x144,static_cast<std::int32_t>(health-bundle));
        put(image+shield::kDrawRva,shield::kDrawPrefix);
        handles[0x80F4599FU]=modelResource;handles[0x80F4599CU]=mesh;
        zero(modelDefinition,0x1E0);put<std::uint32_t>(modelDefinition,0x80F4599F);
        put<std::uint32_t>(modelDefinition+4,0x808072B8);put<std::uint64_t>(modelDefinition+8,0x80);
        put<std::uint32_t>(modelDefinition+0x1DC,0x80F4599C);
        zero(mesh,0x248);put<std::uint64_t>(mesh,0x26F0);put<std::uint64_t>(mesh+0x10,3);
        put<std::uint64_t>(mesh+0x18,0x98);put<std::uint64_t>(mesh+0xB0,3);put<std::uint32_t>(mesh+0xB8,0x80807378);
        for(std::size_t i=0;i<3;++i) {
            const auto row=mesh+0xB0+i*0x88;
            put<std::uint32_t>(row+0x10,0x80810010);put<std::uint32_t>(row+0x20,0x80810020);
        }
        authored_pass(true);
    }
    bool accepted() {shield::Binding out{};return shield::sample(*this,image,body,request,out);}
};
// All addresses and provider names after row zero are synthetic. This models
// the qualified per-actor binding without loading game resources or code.
struct PhaseFixture : Fixture {
    static constexpr std::uintptr_t parent=0x1000000,phaseResource=0x1100000,producer=0x1200000;
    static constexpr auto provider=parent+0x2000,phaseDefinition=phaseResource+0x2ED8,
        providerDefinitions=phaseResource+0x39F0,modelInput=model+0x138+0xE8+4*0x60;
    static constexpr std::uint32_t parentSelf=0x12031,producerSelf=0x14032;
    PhaseFixture() {
        handles[parentSelf]=parent;handles[producerSelf]=producer;handles[0x80F66F55U]=phaseResource;
        put<std::uint32_t>(actor+0x50,parentSelf);
        zero(parent,0x30);put<std::uint32_t>(parent,0x80F66F55);put<std::uint32_t>(parent+4,0x808082EC);
        put<std::uint64_t>(parent+8,0x2ED8);put(parent+0x24,parentSelf);put<std::uint32_t>(parent+0x2C,5);
        put(parent+0x1470,request.enemy.actor);put<std::uint64_t>(parent+0x1328,45);
        put<std::int64_t>(parent+0x1330,provider-parent-0x1340);
        zero(provider,0x30);put<std::uint32_t>(provider,0x80F66F55);put<std::uint32_t>(provider+4,0x80807EEB);
        put<std::uint64_t>(provider+8,0x39F0);put<std::uintptr_t>(provider+0x10,parent-provider-0x10);
        put<float>(provider+0x20,.375F);
        zero(phaseDefinition,0x330);put<std::uint32_t>(phaseDefinition,0x80F66F55);
        put<std::uint32_t>(phaseDefinition+4,0x80807EE1);put<std::uint64_t>(phaseDefinition+8,0x90);
        put<std::uint64_t>(phaseDefinition+0x320,45);put<std::uint64_t>(phaseDefinition+0x328,0x39F0-0x2ED8-0x338);
        zero(providerDefinitions,45*0x30);
        for(std::uint32_t i=0;i<45;++i) {
            const auto row=providerDefinitions+i*0x30;
            put<std::uint32_t>(row,0x80F66F55);put<std::uint32_t>(row+4,0x80807EEA);
            put<std::uint64_t>(row+8,0x2660+i*0x30);put<std::uint32_t>(row+0x18,0x80BFDDA6);
            put<std::uint32_t>(row+0x28,shield::kPhaseInput+i);
        }
        put<std::uint64_t>(model+0x120,9);put<std::int64_t>(model+0x128,0xE8);
        zero(modelInput,0x58);put<std::uint32_t>(modelInput,0x80F4599F);put<std::uint32_t>(modelInput+4,0x80809789);
        put<std::uint64_t>(modelInput+8,0x1720);put(modelInput+0x38,producerSelf);put<std::int32_t>(modelInput+0x50,23);
        zero(producer,0x30);put<std::uint32_t>(producer,0x80F66F70);put<std::uint32_t>(producer+4,0x80809790);
        put<std::uint64_t>(producer+8,0x1668);put(producer+0x24,producerSelf);put<std::uint32_t>(producer+0x2C,5);
        put<std::uint64_t>(producer+0x50,76);
    }
};
struct ChangingRead : Fixture {
    std::uintptr_t changeAfter{};std::function<void(Fixture&)> change;bool changed{};
    bool copy(std::uintptr_t address,std::span<std::byte> output) {
        const bool ok=Fixture::copy(address,output);
        if(ok && !changed && address==changeAfter) {changed=true;change(*this);}return ok;
    }
    template<class T> bool value(std::uintptr_t address,T& out) {
        return copy(address,std::as_writable_bytes(std::span{&out,1}));
    }
};
}
inline bool strike_bond_shield_contracts() {
    using namespace strike_bond_shield_fixture;
#define GARDEN_SHIELD_CHECK(expression) do {if(!(expression)){std::fprintf(stderr,"FAIL Garden shield line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    // Only an activated, live damage phase fades the blue shell. Intro and
    // every immunity/terminal phase retain the native zero input.
    using Mode=shield::mission::BossMode;
    for(const auto mode:{Mode::damage,Mode::parking,Mode::dormant,Mode::waking,Mode::dying,Mode::opening}) {
        shield::mission::Frame active{};active.enabled=true;active.bossFighting=true;active.bossCycle.mode=mode;
        GARDEN_SHIELD_CHECK(shield::phase_value(active)==(mode==Mode::damage?1.F:0.F));
        auto gated=active;gated.bossFighting=false;
        GARDEN_SHIELD_CHECK(shield::phase_value(gated)==0.F);
        gated=active;gated.enabled=false;
        GARDEN_SHIELD_CHECK(shield::phase_value(gated)==0.F);
        gated=active;gated.bossDead=true;
        GARDEN_SHIELD_CHECK(shield::phase_value(gated)==0.F);
        gated=active;gated.ending=true;
        GARDEN_SHIELD_CHECK(shield::phase_value(gated)==0.F);
        gated=active;gated.finished=true;
        GARDEN_SHIELD_CHECK(shield::phase_value(gated)==0.F);
    }
    Fixture good;shield::Binding bound{};
    GARDEN_SHIELD_CHECK(shield::sample(good,Fixture::image,Fixture::body,good.request,bound));
    GARDEN_SHIELD_CHECK(shield::boundaries(good,Fixture::image));
    GARDEN_SHIELD_CHECK(bound.model==Fixture::model && bound.health==Fixture::health && bound.modelSelf==Fixture::modelSelf);
    PhaseFixture phase;shield::Binding phaseBinding{};shield::PhaseInput phaseInput{};
    GARDEN_SHIELD_CHECK(shield::sample(phase,Fixture::image,Fixture::body,phase.request,phaseBinding));
    const auto phaseBefore=phase.bytes;
    GARDEN_SHIELD_CHECK(shield::phase_sample(phase,Fixture::image,phaseBinding,phase.request,phaseInput));
    const shield::PhaseInput expectedPhase{PhaseFixture::parent,PhaseFixture::provider,PhaseFixture::parentSelf,.375F};
    GARDEN_SHIELD_CHECK(phaseInput==expectedPhase && phase.bytes==phaseBefore);
    const auto rejectsPhase=[&](PhaseFixture bad) {
        auto untouched=expectedPhase;
        return !shield::phase_sample(bad,Fixture::image,phaseBinding,bad.request,untouched) && untouched==expectedPhase;
    };
    for(const auto value:{0.F,1.F}) {auto edge=phase;edge.put(PhaseFixture::provider+0x20,value);
        GARDEN_SHIELD_CHECK(shield::phase_sample(edge,Fixture::image,phaseBinding,edge.request,phaseInput) && phaseInput.value==value);}
    // Actor salts, native notification ownership, and the renderer's selected
    // producer are separate identities; a plausible address alone is insufficient.
    {auto bad=phase;bad.handles.erase(PhaseFixture::parentSelf);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::uint32_t>(Fixture::actor+0x48,bad.request.enemy.actor^0x2000U);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::uint32_t>(PhaseFixture::parent+0x24,PhaseFixture::parentSelf^0x2000U);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::uintptr_t>(PhaseFixture::provider+0x10,0);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::int32_t>(PhaseFixture::modelInput+0x50,22);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::uint32_t>(PhaseFixture::producer+0x2C,6);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.handles[PhaseFixture::producerSelf]+=0x100;GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::uint64_t>(PhaseFixture::phaseDefinition+0x320,44);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    {auto bad=phase;bad.put<std::uint32_t>(PhaseFixture::providerDefinitions+0x30+0x28,shield::kPhaseInput);GARDEN_SHIELD_CHECK(rejectsPhase(bad));}
    for(const auto value:{-.01F,1.01F,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        auto bad=phase;bad.put(PhaseFixture::provider+0x20,value);GARDEN_SHIELD_CHECK(rejectsPhase(bad));
    }
    const auto before=good.bytes;
    shield::BaseLease base;
    GARDEN_SHIELD_CHECK(base.bind(good.request.owner,good.request.enemy,bound));
    auto hidden=bound;hidden.passFlags=base.project(bound.passFlags,false,false);
    auto hide=shield::command(hidden),show=shield::command(bound);
    GARDEN_SHIELD_CHECK(hide.object==Fixture::model+0x1A0 && hide.renderer==0x7DFD604B && hide.pass==7 && hide.count==0);
    GARDEN_SHIELD_CHECK(show.object==hide.object && show.renderer==hide.renderer && show.pass==7 && show.count==1);
    GARDEN_SHIELD_CHECK(good.bytes==before);
    // The initial mutable word is not the authored base: native initialization
    // may not have published the mesh's nonempty pass yet.
    {auto early=good;early.put<std::uint16_t>(Fixture::model+shield::kPassFlagsOffset,0);shield::Binding sampled{};
        GARDEN_SHIELD_CHECK(shield::sample(early,Fixture::image,Fixture::body,early.request,sampled));
        GARDEN_SHIELD_CHECK(sampled.authoredBase && sampled.passFlags==0 && sampled.modelDefinition==Fixture::modelDefinition && sampled.mesh==Fixture::mesh);
        shield::BaseLease captured;GARDEN_SHIELD_CHECK(captured.bind(early.request.owner,early.request.enemy,sampled));
        GARDEN_SHIELD_CHECK(captured.project(sampled.passFlags,true,false)==1);}
    {auto absent=good;absent.authored_pass(false);shield::Binding sampled{};
        GARDEN_SHIELD_CHECK(shield::sample(absent,Fixture::image,Fixture::body,absent.request,sampled) && !sampled.authoredBase);
        shield::BaseLease captured;GARDEN_SHIELD_CHECK(captured.bind(absent.request.owner,absent.request.enemy,sampled));
        GARDEN_SHIELD_CHECK(captured.project(sampled.passFlags,true,false)==0);}
    for(const auto offset:{0x10U,0x20U}) {auto absent=good;
        for(std::size_t i=0;i<3;++i) absent.put<std::uint32_t>(Fixture::mesh+0xB0+i*0x88+offset,UINT32_MAX);
        shield::Binding sampled{};GARDEN_SHIELD_CHECK(shield::sample(absent,Fixture::image,Fixture::body,absent.request,sampled) && !sampled.authoredBase);}
    {auto one=good;
        for(std::size_t i=0;i<2;++i) one.put<std::uint32_t>(Fixture::mesh+0xB0+i*0x88+0x10,UINT32_MAX);
        shield::Binding sampled{};GARDEN_SHIELD_CHECK(shield::sample(one,Fixture::image,Fixture::body,one.request,sampled) && sampled.authoredBase);}
    GARDEN_SHIELD_CHECK(shield::enabled_count(0x1E1)==5 && shield::enabled_count(0x201)==0 && shield::enabled_count(0x3E1)==4);
    // The bridge's own write and native reference changes do not constitute a
    // new model lease. Reconstruction consumes the model word, not our last
    // queued renderer count, so it must remain hidden between callbacks.
    GARDEN_SHIELD_CHECK(!base.bind(good.request.owner,good.request.enemy,hidden) && base.originalBase);
    GARDEN_SHIELD_CHECK(shield::same_binding(bound,hidden) && shield::same_model(bound,hidden));
    good.put(Fixture::model+shield::kPassFlagsOffset,hidden.passFlags);
    for(unsigned slot=0;slot<4;++slot) {
        const auto reference=static_cast<std::uint16_t>(0x202U<<slot);
        good.put(Fixture::model+shield::kPassFlagsOffset,static_cast<std::uint16_t>(hidden.passFlags|reference));
        shield::Binding native{};GARDEN_SHIELD_CHECK(shield::sample(good,Fixture::image,Fixture::body,good.request,native));
        GARDEN_SHIELD_CHECK(!base.bind(good.request.owner,good.request.enemy,native));
        GARDEN_SHIELD_CHECK(base.project(native.passFlags,false,false)==native.passFlags);
        GARDEN_SHIELD_CHECK(shield::command(native).count==0); // native full reconstruction
        native.passFlags=static_cast<std::uint16_t>(native.passFlags&~reference);
        GARDEN_SHIELD_CHECK(shield::command(native).count==0); // native owner removal
        GARDEN_SHIELD_CHECK(base.project(native.passFlags,true,false)==1);
    }
    // Every non-base bit is preserved, including genuine native enable owners.
    // They can keep the shield drawn; the bridge must report that conflict and
    // publish the real reduction instead of oscillating between false zeros.
    for(unsigned flags=0;flags<=UINT16_MAX;++flags) {
        const auto input=static_cast<std::uint16_t>(flags);
        const auto suppressed=base.project(input,false,false),restored=base.project(input,true,false);
        GARDEN_SHIELD_CHECK((suppressed&~1U)==(flags&~1U) && (restored&~1U)==(flags&~1U));
        GARDEN_SHIELD_CHECK(!(suppressed&1U) && (restored&1U));
        GARDEN_SHIELD_CHECK((shield::enabled_count(suppressed)!=0)==(shield::native_enables(suppressed)!=0));
    }
    {auto enabled=hidden;enabled.passFlags=0x220;
        GARDEN_SHIELD_CHECK(base.project(enabled.passFlags,false,false)==0x220);
        GARDEN_SHIELD_CHECK(shield::native_enables(enabled.passFlags)==0x20 && shield::command(enabled).count==1);}
    // A renderer rebuild on the same salted model must not recapture our zero
    // as its original base. A genuinely new model or actor generation must.
    {auto rebuilt=hidden;++rebuilt.renderer;++rebuilt.displayOwner;
        GARDEN_SHIELD_CHECK(shield::same_model(bound,rebuilt) && !shield::same_binding(bound,rebuilt));
        GARDEN_SHIELD_CHECK(!base.bind(good.request.owner,good.request.enemy,rebuilt));
        GARDEN_SHIELD_CHECK(base.project(rebuilt.passFlags,true,false)==1);}
    {auto initialized=bound;auto desired=initialized;
        GARDEN_SHIELD_CHECK(!base.bind(good.request.owner,good.request.enemy,initialized));
        desired.passFlags=base.project(initialized.passFlags,false,false);
        // Initialization can publish 1 while our cached last publication is 0.
        // Correct that new native publication once, then leave stable state alone.
        GARDEN_SHIELD_CHECK(shield::publication_changed(initialized,desired,hidden,0));
        GARDEN_SHIELD_CHECK(!shield::publication_changed(desired,desired,hidden,0));}
    {auto replaced=hidden;++replaced.modelSelf;replaced.authoredBase=false;shield::BaseLease next=base;
        GARDEN_SHIELD_CHECK(next.bind(good.request.owner,good.request.enemy,replaced));
        GARDEN_SHIELD_CHECK(!next.originalBase && next.project(replaced.passFlags,true,false)==0);}
    {auto next=base;auto owner=good.request.owner;++owner.value;auto actor=good.request.enemy;++actor.generation;
        GARDEN_SHIELD_CHECK(next.bind(owner,actor,hidden) && next.originalBase);
        GARDEN_SHIELD_CHECK(next.project(hidden.passFlags,true,false)==1);}
    {auto dying=base;
        GARDEN_SHIELD_CHECK(dying.project(1,true,true)==0 && dying.retiring);
        GARDEN_SHIELD_CHECK(!dying.bind(good.request.owner,good.request.enemy,bound));
        GARDEN_SHIELD_CHECK(dying.project(1,true,false)==0);}
    good.put(Fixture::model+shield::kPassFlagsOffset,bound.passFlags);
    for(const auto address:{Fixture::modelDefinition,Fixture::modelDefinition+4,Fixture::modelDefinition+8,
                            Fixture::modelDefinition+0x1DC,Fixture::mesh,Fixture::mesh+0x10,Fixture::mesh+0x18,
                            Fixture::mesh+0xB0,Fixture::mesh+0xB8}) {
        auto bad=good;bad.bytes[address]^=std::byte{1};GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto handle:{0x80F4599FU,0x80F4599CU}) {
        auto bad=good;bad.handles.erase(handle);GARDEN_SHIELD_CHECK(!bad.accepted());
        bad=good;bad.handles[handle]=UINTPTR_MAX-0x10;GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    {auto bad=good;bad.put<std::uint64_t>(Fixture::mesh+0xD8,UINT64_MAX);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint16_t>(Fixture::mesh+0xB0+0x38+14,2);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint16_t>(Fixture::mesh+0xB0+0x38+16,2);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.bytes.erase(Fixture::mesh+0xB0+3*0x88-1);GARDEN_SHIELD_CHECK(!bad.accepted());}
    for(unsigned change=0;change<3;++change) {
        ChangingRead changing;changing.changeAfter=Fixture::mesh+0xB0;
        changing.change=[change](Fixture& f) {
            if(change==0) f.put<std::uint32_t>(Fixture::modelDefinition+0x1DC,0x80F4599D);
            else if(change==1) f.authored_pass(false);
            else f.handles[0x80F4599CU]+=0x100;
        };
        auto untouched=bound;
        GARDEN_SHIELD_CHECK(!shield::sample(changing,Fixture::image,Fixture::body,changing.request,untouched) && untouched==bound);
    }
    for(const auto address:{Fixture::model,Fixture::health}) for(const auto offset:{0U,4U,8U,0x24U,0x2CU}) {
        auto bad=good;bad.bytes[address+offset]^=std::byte{1};GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto offset:{0x1A0U,0x1A4U,0x1A8U,0x1B0U}) {
        auto bad=good;bad.bytes[Fixture::model+offset]^=std::byte{1};GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto offset:{0xC0U,0x1C0U}) {
        auto bad=good;bad.put<std::uint32_t>(Fixture::model+offset,UINT32_MAX);GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto handle:{Fixture::modelSelf,15U}) {auto bad=good;bad.handles[handle]+=0x10;GARDEN_SHIELD_CHECK(!bad.accepted());}
    for(const auto offset:{0xC0U,0xD2U,0x1B7U,0x1C0U}) {auto bad=good;bad.bytes.erase(Fixture::model+offset);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::entity+12,5^0x2000U);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::entity+4,1);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.owner.value;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.region=8;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.enabled=false;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.bossStage=3;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto dead=good;dead.request.frame.bossDead=true;GARDEN_SHIELD_CHECK(!dead.accepted());}
    for(const auto rva:{shield::kDrawRva,std::uintptr_t{0xCD6C20}}) {
        auto bad=good;bad.bytes[Fixture::image+rva]^=std::byte{1};GARDEN_SHIELD_CHECK(!shield::boundaries(bad,Fixture::image));
    }
    // A rebuilt renderer remains eligible but must invalidate the cached key.
    {auto rebuilt=good;rebuilt.put<std::uint32_t>(Fixture::model+0xC0,0x7DFD804B);shield::Binding next{};
        GARDEN_SHIELD_CHECK(shield::sample(rebuilt,Fixture::image,Fixture::body,rebuilt.request,next) && next!=bound);}
    {auto bad=good;bad.bytes.erase(Fixture::health+0x24);auto untouched=bound;
        GARDEN_SHIELD_CHECK(!shield::sample(bad,Fixture::image,Fixture::body,bad.request,untouched) && untouched==bound);}
#undef GARDEN_SHIELD_CHECK
    return true;
}
