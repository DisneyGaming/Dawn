#pragma once
#include "../../encoding/bit_writer.h"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
namespace sunrise::middleware::bap::activity_message::clock_state {
inline constexpr std::uint32_t kMessageType=2;
inline constexpr std::size_t kMeaningfulBits=33,kEncodedSize=5;
// Native 8080867E (39402C8) nests 808086E8 (37E2900):
// decoded +0 is one bool bit; decoded +4 is a raw 32-bit float.
struct State { bool flag{};float rate{}; };
// Preserve the native flag's zero default. Positive rate enables extrapolation
// in 3CB030; the flag's broader simulation meaning has not been established.
inline constexpr State kRunning{false,1.F};
inline bool encode(State state,std::span<std::byte> output,std::size_t& written) noexcept {
    written=0;
    if(output.size()<kEncodedSize || !std::isfinite(state.rate) || state.rate<0.F) { return false; }
    encoding::bits::Writer writer(output.first(kEncodedSize));
    return writer.write(state.flag?1U:0U,1)
        && writer.write(std::bit_cast<std::uint32_t>(state.rate),32)
        && writer.bit_count()==kMeaningfulBits && writer.finish(written) && written==kEncodedSize;
}
}
