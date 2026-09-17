#pragma once
#include "../../encoding/bit_reader.h"
#include "../web_service_envelope.h"

namespace dawn::middleware::web_service::messages::vendor_reply {
struct Request {std::int16_t vendor{},interaction{},reply{};std::int32_t selection{};};
// Native schema 808075D4: three biased i16 fields and one biased i32, exactly 80 bits.
inline bool parse(const Message& message,Request& out) noexcept {
    out={};if(message.opcode!=904 || (message.payload.size()!=10 && message.payload.size()!=11)
        || (message.payload.size()==11 && message.payload.back()!=std::byte{})) {return false;}
    // Service-10 framing leaves a zero terminator byte on the live aligned form.
    encoding::bits::Reader r(message.payload.first(10));std::uint64_t v{},i{},reply{},selection{};
    if(!r.read(16,v) || !r.read(16,i) || !r.read(16,reply) || !r.read(32,selection) || r.remaining_bits()) {return false;}
    out={static_cast<std::int16_t>(static_cast<int>(v)-32768),
        static_cast<std::int16_t>(static_cast<int>(i)-32768),
        static_cast<std::int16_t>(static_cast<int>(reply)-32768),
        static_cast<std::int32_t>(static_cast<std::int64_t>(selection)-2147483648LL)};return true;
}
}
