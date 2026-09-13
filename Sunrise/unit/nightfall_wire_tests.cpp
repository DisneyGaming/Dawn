#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "state/activity/strike_bond/authority.h"
#include "state/activity/strike_pact/authority.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
namespace wire=sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool value,const char* message){++checks;if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
unsigned source_variant(std::span<const std::byte> body) {
    bits::Reader reader(body);std::uint64_t present{},count{},value{};
    check(reader.read(1,present) && (!present || reader.skip(55)) && reader.skip(3)
        && reader.read(4,count) && count>0 && count<=2 && reader.skip(32*count)
        && reader.skip(2) && reader.read(3,value),"decode native source actor variant");
    return static_cast<unsigned>(value);
}
void strike_source_variants() {
    namespace garden=sunrise::state::activity::strike_bond;
    namespace tree=sunrise::state::activity::strike_pact;
    std::size_t gardenGm{},treeGm{};
    for(const auto variant:{0U,5U}) {
        auto g=std::make_unique<garden::Frame>();g->enabled=true;g->spawnGeneration=1;
        g->enemyVariant=static_cast<std::uint8_t>(variant);
        for(const auto& row:garden::kSpawns) {
            if(row.asset.registry==garden::kBossActor.registry && row.asset.slot==3) continue;
            if(row.asset.registry==0xC80A735BU) continue;
            auto& state=g->native[garden::asset_index(row.asset)];state.managed=state.active=true;
            std::array<std::byte,512> body{};bits::Writer writer(body);
            check(garden::write_body(writer,*g,row.asset.registry,1,row.asset.slot),"Garden source authority encodes");
            const bool substituted=variant==5 && garden::grandmaster_substitution_source(row.asset.registry,row.asset.slot);
            check(source_variant(body)==(substituted?6U:1U),"Garden frozen variant applies only to proved sources");
            if(substituted) ++gardenGm;
        }
        auto t=std::make_unique<tree::Frame>();t->enabled=true;t->spawnGeneration=1;
        t->enemyVariant=static_cast<std::uint8_t>(variant);
        for(const auto& row:tree::kAllSpawns) {
            t->cohorts|=std::uint64_t{1}<<row.cohort;
            std::array<std::byte,512> body{};bits::Writer writer(body);
            check(tree::write_body(writer,*t,row.registry,1,row.source),"Tree source authority encodes");
            const bool substituted=variant==5 && tree::grandmaster_substitution_source(row.registry,row.source);
            check(source_variant(body)==(substituted?6U:1U),"Tree frozen variant applies only to proved sources");
            if(substituted) ++treeGm;
        }
    }
    check(gardenGm==23 && treeGm==17,"all forty installed GM replacement sources reach native wire");
}
int main(){
    strike_source_variants();
    auto snapshot=std::make_unique<wire::Snapshot>();
    for(bool garden:{false,true}) for(bool completed:{false,true}) for(bool failed:{false,true}) {
        *snapshot={};snapshot->lifetime=3;snapshot->region=120;snapshot->hasRegion=true;
        snapshot->strike_bond.enabled=garden;
        snapshot->strike_pact.enabled=!garden;snapshot->strike_pact.services=!garden;
        snapshot->missionCompletion={{71,1},completed,6};snapshot->nightfallFailed=failed;
        std::array<std::byte,4096> body{};bits::Writer writer(body);
        check(wire::write_auth_body(writer,*snapshot,0x4786C0E0U,17,3,false),"strike native lifetime encodes");
        const auto head=std::to_integer<unsigned>(body[0]);
        check((head>>4)==(failed?9U:completed?7U:4U),"qualified failure uses terminal phase8 only");
        check(((head>>1)&7U)==(!failed && completed?2U:1U),"failed run never publishes native success result");
    }
    std::cout<<"nightfall wire PASS "<<checks<<'\n';
}
