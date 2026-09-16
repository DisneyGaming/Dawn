#include "../src/server/runtime/activity/lost_sector_runtime.h"
#include "../src/server/runtime/activity/lost_sector_catalog.h"
#include "../src/middleware/bap/activity_message/sense_update.h"
#include <array>
#include <cstdio>

namespace activity=sunrise::server::runtime::activity;
namespace ls=activity::lost_sector;
namespace registry=sunrise::state::activity::coo::registry;

namespace {
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)
inline constexpr std::array<registry::Slot,3> kSlots{{
    {0,1,0x80809A3BU,0x80807ECCU,0x80807EC9U,0x81000001U},
    {1,1,0x80809A3BU,0x80807ECCU,0x80807EC9U,0x81000002U},
    {2,1,0x80809A3BU,0x80807ECCU,0x80807EC9U,0x81000003U},
}};
inline constexpr registry::Definition kRegistry{"sector",0x80F00001U,0x10000001U,
    0x82000001U,0x20000001U,7,kSlots};
inline constexpr std::array<activity::population::Capability,3> kCapabilities{{
    {&kRegistry,0,0,{},false,0,1},{&kRegistry,1,0,{},false,0,1},
    {&kRegistry,2,0,{},false,0,1},
}};
inline constexpr std::array<ls::Stage,2> kStages{{{0,2},{2,1}}};
inline constexpr std::array<ls::Sector,1> kSectors{{{"proof sector",7,0,2}}};
inline constexpr std::array<ls::SourcePolicy,3> kPolicies{{{2,0,false},{1,0,false},{1,0,true}}};
inline constexpr ls::Definition kDefinition{kSectors,kStages,kPolicies,0};
}

int main() {
    CHECK(ls::valid(kDefinition,kCapabilities));
    auto never=[](std::size_t,std::uint16_t,const ls::Stage&) {return false;};

    namespace catalog=activity::lost_sector::catalog;
    std::size_t catalogSectors{},catalogBosses{};
    const auto validateCatalog=[&](const auto& capabilities,const auto& sectors,
        const auto& stages,const auto& policies) {
        const ls::Definition definition{sectors,stages,policies,0};
        if(!ls::valid(definition,capabilities))return false;
        catalogSectors+=sectors.size();
        for(const auto& sector:sectors) {
            std::size_t bosses{};
            for(std::size_t stageIndex=sector.firstStage;
                stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
                const auto& stage=stages[stageIndex];
                for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source)
                    bosses+=policies[source].boss?1U:0U;
            }
            if(bosses!=1)return false;
            catalogBosses+=bosses;
        }
        return true;
    };
    CHECK(validateCatalog(catalog::mercury::kCapabilities,catalog::mercury::kSectors,
        catalog::mercury::kStages,catalog::mercury::kPolicies));
    CHECK(validateCatalog(catalog::mars::kCapabilities,catalog::mars::kSectors,
        catalog::mars::kStages,catalog::mars::kPolicies));
    CHECK(validateCatalog(catalog::io::kCapabilities,catalog::io::kSectors,
        catalog::io::kStages,catalog::io::kPolicies));
    CHECK(validateCatalog(catalog::titan::kCapabilities,catalog::titan::kSectors,
        catalog::titan::kStages,catalog::titan::kPolicies));
    CHECK(validateCatalog(catalog::nessus::kCapabilities,catalog::nessus::kSectors,
        catalog::nessus::kStages,catalog::nessus::kPolicies));
    CHECK(validateCatalog(catalog::tangled_shore::kCapabilities,catalog::tangled_shore::kSectors,
        catalog::tangled_shore::kStages,catalog::tangled_shore::kPolicies));
    CHECK(catalogSectors==19 && catalogBosses==19);
    CHECK(catalog::mercury::kCapabilities.size()==22 && catalog::mars::kCapabilities.size()==52
        && catalog::io::kCapabilities.size()==68 && catalog::titan::kCapabilities.size()==81
        && catalog::nessus::kCapabilities.size()==126 && catalog::tangled_shore::kCapabilities.size()==106);

    // Mercury's ordinary surface is bubble15; a qualified bubble16 preload
    // must request only Pariah's Refuge stage zero and never require main-bubble arrival.
    {
        const activity::population::Owner mercuryOwner{0x4D455243U,{1}};
        activity::population::Service mercuryService;
        CHECK(mercuryService.begin(mercuryOwner,catalog::mercury::kCapabilities,91));
        auto definition=catalog::mercury::definition(0);
        ls::Director mercuryDirector;
        CHECK(mercuryDirector.begin(mercuryOwner,91,definition,catalog::mercury::kCapabilities));
        CHECK(mercuryDirector.update(15,true,mercuryService,never,16));
        CHECK(mercuryService.project(16).count==4);
        CHECK(mercuryDirector.diagnostics(0).stage==0 && mercuryDirector.diagnostics(0).runs==1);
    }

    // A qualified incoming region can queue only stage zero. It is not an
    // arrival, cannot advance waves, and repeated publication is idempotent.
    {
        const activity::population::Owner preloadOwner{0x4321U,{3}};
        activity::population::Service preload;CHECK(preload.begin(preloadOwner,kCapabilities,55));
        ls::Director prewarm;CHECK(prewarm.begin(preloadOwner,55,kDefinition,kCapabilities));
        CHECK(prewarm.update(6,true,preload,never,7));
        CHECK(preload.target(0)==2 && preload.target(1)==1 && preload.target(2)==0);
        const auto revision=preload.revision();
        CHECK(prewarm.update(6,true,preload,never,7));
        CHECK(preload.revision()==revision);
        auto remoteClear=[](std::size_t,std::uint16_t,const ls::Stage&) {return true;};
        CHECK(prewarm.update(6,true,preload,remoteClear,UINT32_MAX));
        CHECK(preload.target(2)==0 && prewarm.diagnostics(0).stage==0);
    }

    const activity::population::Owner owner{0x1234U,{9}};
    activity::population::Service service;CHECK(service.begin(owner,kCapabilities,77));
    ls::Director director;CHECK(director.begin(owner,77,kDefinition,kCapabilities));
    CHECK(director.update(7,true,service,never));
    CHECK(service.target(0)==2 && service.target(1)==1 && service.target(2)==0);
    const auto firstRevision=service.revision();
    CHECK(director.update(7,true,service,never));
    CHECK(service.revision()==firstRevision); // boundary noise cannot duplicate a resident boss/stage
    CHECK(director.update(6,true,service,never));
    CHECK(director.update(7,true,service,never));
    CHECK(service.revision()==firstRevision); // uncleared exit/re-entry is not a reset

    const auto consume=[&](std::size_t index,std::uint32_t nativeRevision) {
        sunrise::middleware::bap::activity_message::sense_update::SenseObject value{};
        value.registryKey=kCapabilities[index].registry->key;value.slotIndex=kCapabilities[index].slot;
        value.slotType=1;value.hasNativeSchema=true;value.nativeSchema=0x80807ECC;
        value.nativeRevision=nativeRevision;value.hasRootDelta=true;value.sourceDelta.present=1;
        value.sourceDelta.scalar[0]=service.generation(index);value.sourceDelta.consumedPresent=true;
        value.sourceDelta.consumedCount=1;value.sourceDelta.consumed[0]=service.target(index);
        return service.observe(7,value)!=nullptr;
    };
    CHECK(consume(0,1) && consume(1,1));
    auto clear=[](std::size_t,std::uint16_t,const ls::Stage&) {return true;};
    CHECK(director.update(7,true,service,clear));
    CHECK(service.target(2)==1);
    CHECK(director.diagnostics(0).phase==ls::Phase::active);
    CHECK(consume(2,1));
    CHECK(director.update(7,true,service,clear));
    CHECK(director.diagnostics(0).phase==ls::Phase::cleared);
    const auto clearRevision=service.revision();
    CHECK(director.update(7,true,service,clear));
    CHECK(service.revision()==clearRevision); // standing in a cleared sector stays cleared
    CHECK(director.update(7,false,service,never));
    CHECK(director.diagnostics(0).phase==ls::Phase::cleared); // loading is not an exit
    CHECK(director.update(6,true,service,never,7));
    CHECK(director.diagnostics(0).phase==ls::Phase::resetReady);
    CHECK(service.revision()==clearRevision); // outgoing/stale prefetch cannot rearm on the exit tick

    unsigned staleClearCalls{};
    const auto staleClear=[&](std::size_t,std::uint16_t,const ls::Stage&) {++staleClearCalls;return true;};
    CHECK(director.update(7,true,service,staleClear));
    CHECK(director.diagnostics(0).phase==ls::Phase::active);
    CHECK(director.diagnostics(0).runs==2);
    CHECK(service.renewal(0).pending && service.renewal(1).pending);
    CHECK(staleClearCalls==0); // old-generation consumed/quiescent state cannot clear the renewal
    CHECK(service.revision()==clearRevision);
    CHECK(service.commit_renewal(0) && service.commit_renewal(1));
    CHECK(director.update(7,true,service,staleClear));
    CHECK(staleClearCalls==0); // the new generation has not been consumed/admitted

    std::printf("PASS %u lost-sector runtime checks\n",checks);
}
