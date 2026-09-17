#pragma once
#include "../../src/state/activity/beyond_infinity/authority.h"
#include "../../src/state/activity/deep_storage/authority.h"
#include "../../src/state/activity/hijacked/authority.h"
#include "../../src/client/hooks/bootflow/beyond_infinity_native_receipts.h"
#include "../../src/middleware/encoding/bit_reader.h"
#include "../../src/middleware/encoding/bit_writer.h"
#include <array>
#include <cstring>

namespace native_plate_preparation_fixture {
namespace coo=dawn::state::activity::coo;
namespace bits=dawn::middleware::encoding::bits;
namespace bi=dawn::state::activity::beyond_infinity;
namespace ds=dawn::state::activity::deep_storage;
namespace hj=dawn::state::activity::hijacked;
using Capture=dawn::middleware::bap::activity_message::native::capture_controller::State;
template<class T> void put(std::span<std::byte> bytes,std::size_t offset,T value) {
    std::memcpy(bytes.data()+offset,&value,sizeof value);
}
// Independent source reflection: decoded8080992F+40 is the list count.
// Live pid71072 source80F462AD had the canonical inactive fields but count1,
// so the unchanged readiness consumer refused to acknowledge its generation.
template<class Check> std::array<std::byte,0x44> decode(std::span<const std::byte> wire,Check check) {
    bits::Reader reader(wire);std::array<std::byte,0x44> native{};
    const auto read=[&](std::uint8_t width) {std::uint64_t v{};check(reader.read(width,v),"source reflection is bounded");return v;};
    put(native,0,static_cast<std::uint32_t>(read(32))^0x80000000U);
    put(native,4,static_cast<std::uint32_t>(read(32))^0x80000000U);
    native[8]=static_cast<std::byte>(read(1));native[9]=static_cast<std::byte>(read(1));
    put(native,12,static_cast<std::uint32_t>(read(32))^0x80000000U);
    put(native,16,static_cast<std::uint32_t>(read(32)));
    native[20]=static_cast<std::byte>(static_cast<std::uint8_t>(read(7)-1));
    put(native,22,static_cast<std::uint16_t>(read(16)-0x8000U));
    for(std::size_t offset=32;offset<44;offset+=4) {put(native,offset,static_cast<std::uint32_t>(read(32)));}
    put(native,44,1.F);native[48]=static_cast<std::byte>(read(1));
    put(native,64,static_cast<std::uint32_t>(read(2)));return native;
}
template<class Frame,class Encode,class Width,class Check>
void plate(Frame& frame,coo::Asset asset,std::size_t index,Capture& capture,Encode encode,Width width,Check check) {
    frame.enabled=true;frame.spawnGeneration=1;
    auto& source=frame.native[index];source.generation=1;source.managed=true;source.desired=true;
    capture={true,{true,0,4712400,0,4712400,200,1.F},false};
    std::array<std::byte,121> bytes{};bits::Writer inactive(bytes);std::size_t written{};
    check(encode(inactive,frame,asset) && inactive.finish(written) && written==32 && inactive.bit_count()==252
        && width(frame,asset)==252,"inactive plate omits pending timer and retains canonical preparation size");
    auto decoded=decode(bytes,check);
    check(coo::native_device::inactive_state(decoded),"encoded inactive plate is accepted by real preparation consumer");
    put(decoded,64,std::uint32_t{1});
    check(!coo::native_device::inactive_state(decoded),"captured count1 startup regression is rejected without weakening readiness");
    source.prepared=true;source.active=true;source.generation=2;
    for(const bool running:{true,false}) {
        if(!running) {capture={};}
        bytes={};bits::Writer active(bytes);
        check(encode(active,frame,asset) && active.finish(written) && written==121 && active.bit_count()==961
            && width(frame,asset)==961,"active plate publishes timer including stopped command after departure");
        decoded=decode(bytes,check);
        check(decoded[8]==std::byte{1} && decoded[64]==std::byte{2},"native placement carries both capture and pose records");
        bits::Reader record(bytes);std::uint64_t ignored{},present{},schema{},timerActive{};
        // Read the preceding252bits independently in bounded chunks.
        for(unsigned i=0;i<3;++i) {check(record.read(64,ignored),"skip source prefix");}
        check(record.read(60,ignored) && record.read(1,present) && record.read(32,schema) && record.read(1,timerActive)
            && present==1 && schema==0x80804FCA && timerActive==static_cast<unsigned>(running),"native capture schema follows the actual source body");
        for(unsigned i=0;i<5;++i) {check(record.read(64,ignored),"skip remaining capture clock fields");}
        check(record.read(34,ignored) && record.read(1,present) && record.read(32,schema)
            && present==1 && schema==0x80805063,"native pose record follows the complete capture record");
        for(unsigned i=0;i<9;++i) {check(record.read(32,ignored),"all nine native pose fields fit the published source body");}
    }
    source.active=false;source.generation=3;bytes={};bits::Writer retired(bytes);
    check(encode(retired,frame,asset) && retired.bit_count()==252 && width(frame,asset)==252
        && coo::native_device::inactive_state(decode(bytes,check)),"source retirement also preserves inactive envelope");
}
template<class Check> void run(Check check) {
    bi::Frame beyond{};
    plate(beyond,bi::kPlate,bi::asset_index(bi::kPlate),beyond.plateCapture.state,
        [](auto& w,const auto& f,coo::Asset a) {return bi::write_body(w,f,a.registry,static_cast<std::uint8_t>(a.type),a.slot);},
        [](const auto& f,coo::Asset a) {return bi::body_bits(f,a.registry,static_cast<std::uint8_t>(a.type),a.slot);},check);
    ds::Frame deep{};
    for(std::size_t i=0;i<std::size(ds::kPlates);++i) {const auto a=ds::kPlates[i].source;
        plate(deep,a,ds::asset_index(a),deep.plateCaptures[i].state,
            [](auto& w,const auto& f,coo::Asset v) {return ds::write_body(w,f,v.registry,static_cast<std::uint8_t>(v.type),v.slot);},
            [](const auto& f,coo::Asset v) {return ds::body_bits(f,v.registry,static_cast<std::uint8_t>(v.type),v.slot);},check);
    }
    hj::Frame hijacked{};const auto a=hj::kPlates[0].source;
    plate(hijacked,a,hj::asset_index(a),hijacked.plateCaptures[0].state,
        [](auto& w,const auto& f,coo::Asset v) {return hj::write_body(w,f,v.registry,static_cast<std::uint8_t>(v.type),v.slot);},
        [](const auto& f,coo::Asset v) {return hj::body_bits(f,v.registry,static_cast<std::uint8_t>(v.type),v.slot);},check);
}
}
