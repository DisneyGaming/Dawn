#pragma once
#include <array>
#include <bit>
#include <cstdint>
namespace sunrise::middleware::bap::activity_message::device_sense {
// 80804F47: three optional (float32, biased int32 revision) channel pairs.
struct Output {std::array<float,3> values{};std::array<std::int32_t,3> revisions{};std::uint8_t present{};};
template<class R> bool read(R& r,Output& out) noexcept {
    Output value{};std::uint64_t present{},raw{};
    for(unsigned i=0;i<6;++i) {
        if(!r.read(1,present)) {return false;}
        if(!present) {continue;}
        if(!r.read(32,raw)) {return false;}
        value.present|=static_cast<std::uint8_t>(1U<<i);
        if(i%2) {value.revisions[i/2]=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(raw)^0x80000000U);}
        else {value.values[i/2]=std::bit_cast<float>(static_cast<std::uint32_t>(raw));}
    }
    out=value;return true;
}
}
