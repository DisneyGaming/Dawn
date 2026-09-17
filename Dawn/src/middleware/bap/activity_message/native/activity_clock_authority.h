#pragma once
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dawn::middleware::bap::activity_message::native::activity_clock {
// Service-9 notification2: 8080867E wraps808086E8 without extra framing.
// Event26 ->3CB7A0 stores f0 at scenario+14, f1 at+18, and marks+10 ready.
// f1 is the native timing configuration: 4E4CA0 computes1000/f1, while
// 3CB030 permits elapsed time only when f1>0. It is NOT a rate multiplier.
// Field0's gameplay meaning is still unproven; preserve it as an explicit
// wire field. This codec makes no default running configuration or transport.
inline constexpr std::uint32_t kMessageType=2;
inline constexpr std::uint32_t kSchema=0x8080867E;
inline constexpr std::size_t kBits=33;
struct Configuration final {
    bool field0{};
    float timing{};
};
[[nodiscard]] inline bool valid(Configuration value) noexcept {
    return std::isfinite(value.timing) && value.timing>=0.0F;
}
template<class Writer> [[nodiscard]] bool write(Writer& writer,Configuration value) noexcept {
    return valid(value) && writer.write(value.field0?1:0,1)
        && writer.write(std::bit_cast<std::uint32_t>(value.timing),32);
}
// Type5 uses351070(reader,&native_u64,64), a raw memory-bit copy rather
// than a reflected numeric scalar. Its eight wire bytes must therefore be
// little endian even though each byte is written MSB first by the bitstream.
// Keeping this separate avoids changing reflected shared-clock fields.
template<class Writer> [[nodiscard]] bool write_elapsed(Writer& writer,std::uint64_t ticks) noexcept {
    for(unsigned shift=0;shift<64;shift+=8)
        if(!writer.write((ticks>>shift)&0xFF,8))return false;
    return true;
}
// Native35F080 defines673200 ticks/second; fractional milliseconds therefore
// cannot be treated as GetTickCount ticks or copied directly into an anchor.
inline constexpr std::uint64_t kTicksPerSecond=673200;
[[nodiscard]] constexpr bool from_milliseconds(std::uint64_t milliseconds,
    std::uint64_t& ticks) noexcept {
    const auto whole=milliseconds/1000;
    const auto fraction=(milliseconds%1000)*kTicksPerSecond/1000;
    if(whole>(UINT64_MAX-fraction)/kTicksPerSecond)return false;
    ticks=whole*kTicksPerSecond+fraction;return true;
}
}
