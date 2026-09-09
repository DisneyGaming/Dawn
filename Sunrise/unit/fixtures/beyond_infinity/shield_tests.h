#pragma once
#include "../../../src/state/activity/beyond_infinity/authority.h"
#include "../../../src/middleware/encoding/bit_writer.h"
#include "shield_wire.h"
namespace beyond_shield_fixture {
template<class Check> void run(Check check) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace coo=sunrise::state::activity::coo;
    namespace wire=beyond_shield_wire_fixture;
    bi::Frame frame{};frame.enabled=true;frame.spawnGeneration=17;
    const auto verify=[&](coo::Asset asset,bool active,unsigned bits,const auto& expected) {
        frame.native[bi::asset_index(asset)]={17,true,active,true,active};
        std::array<std::byte,80> bytes{};
        sunrise::middleware::encoding::bits::Writer writer(bytes);
        check(bi::body_bits(frame,asset.registry,static_cast<std::uint8_t>(asset.type),asset.slot)==bits,
            "shield authority width matches independent native reflection");
        check(bi::write_body(writer,frame,asset.registry,static_cast<std::uint8_t>(asset.type),asset.slot)
            && writer.bit_count()==bits,"shield authority writes the complete native body");
        for(std::size_t i=0;i<expected.size();++i) {
            check(std::to_integer<std::uint8_t>(bytes[i])==expected[i],"shield wire equals independently reflected selector/effect fixture");
        }
    };
    verify(bi::shields::kFrontFilter,true,wire::frontBits,wire::front);
    verify(bi::shields::kBackFilter,true,wire::backBits,wire::back);
    verify(bi::shields::kFrontEffect,true,wire::frontEffectBits,wire::frontEffect);
    verify(bi::shields::kBackEffect,true,wire::backEffectBits,wire::backEffect);
    verify(bi::shields::kFrontFilter,false,wire::emptyBits,wire::empty);
    verify(bi::shields::kBackFilter,false,wire::emptyBits,wire::empty);
    auto disabled=wire::frontEffect;disabled[0]|=0x40;
    verify(bi::shields::kFrontEffect,false,wire::frontEffectBits,disabled);
    check(bi::body_bits(frame,0x85742F3EU,26,15)==0,"Beyond shield authority cannot target Gateway actors");
    check(bi::body_bits(frame,bi::shields::kFrontEffect.registry,26,17)==0,"unrelated component receives no shield body");
    for(const auto slots:{std::span<const std::uint16_t>(bi::shields::kFrontSources),std::span<const std::uint16_t>(bi::shields::kBackSources)}) {
        unsigned actors{};
        for(auto slot:slots) {
            const auto* binding=bi::find(0x0FF26BCCU,1,slot);check(binding!=nullptr,"every shield selector resolves an authored enemy source");
            const auto* source=bi::source(binding->asset);check(source && !source->sceneOwned,"shield sources are native ambush squads");
            actors+=source->categories;
        }
        check(actors<=32 && actors>=slots.size(),"all authored squad categories fit the native collection actor limit");
    }
    frame.enabled=false;
    check(bi::body_bits(frame,bi::shields::kFrontEffect.registry,26,16)==0,"inactive mission cannot publish shields");
    std::array<std::uint16_t,9> oversized{};
    auto writer=sunrise::middleware::encoding::bits::Writer::measuring();
    check(!coo::native_device::collection_sources(writer,0x0FF26BCCU,oversized),"oversized selector set is rejected");
}
}
