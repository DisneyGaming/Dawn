#pragma once
#include "../../../src/state/activity/beyond_infinity/authority.h"
#include "../../../src/middleware/encoding/bit_writer.h"
#include "ai_wire.h"
namespace beyond_ai_fixture {
template<class Check> void run(Check check) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace wire=beyond_ai_wire_fixture;
    bi::Frame frame{};frame.enabled=true;frame.spawnGeneration=17;
    const auto verify=[&](std::uint16_t slot,const auto& expected) {
        const auto* source=bi::find(0x0FF26BCCU,1,slot);check(source!=nullptr,"native ambush source exists");
        frame.native[bi::asset_index(source->asset)]={17,true,true,true,true};
        std::array<std::byte,90> bytes{};
        sunrise::middleware::encoding::bits::Writer writer(bytes);
        check(bi::write_body(writer,frame,0x0FF26BCCU,1,slot),"write native tactical source body");
        check((writer.bit_count()+7)/8==expected.size(),"source matches independent reflected wire length");
        for(std::size_t i=0;i<expected.size();++i) {
            check(std::to_integer<std::uint8_t>(bytes[i])==expected[i],"ambush source wire matches native tactical row and authored spawn rule");
        }
    };
    verify(6,wire::source6);verify(7,wire::source7);verify(8,wire::source8);
    verify(9,wire::source9);verify(10,wire::source10);verify(11,wire::source11);
    verify(12,wire::source12);verify(13,wire::source13);verify(14,wire::source14);
    check(bi::tactical_group({0x15FFBE16U,0,1,6}).row==-1,"Panoptes is not assigned an ambush combat task");
    check(bi::tactical_group({0x85742F3EU,0,1,6}).row==-1,"Gateway assignments are unaffected");
}
}
