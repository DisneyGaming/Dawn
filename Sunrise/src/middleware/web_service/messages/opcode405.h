#pragma once
#include "../../encoding/bit_reader.h"
#include "../web_service_envelope.h"

namespace sunrise::middleware::web_service::messages::opcode405 {
// Native transfer constructor5306F0, schema8080761B; UI senderF2BC30.
struct Request {std::int16_t context{};std::int8_t option{};std::uint64_t instance{};std::int16_t item{};std::int32_t quantity{};};
inline bool parse(const Message& message,Request& out) noexcept {
    out={};if(message.opcode!=405 || (message.payload.size()!=17 && message.payload.size()!=18)) {return false;}
    encoding::bits::Reader reader(message.payload);std::uint64_t context{},option{},instance{},item{},quantity{};
    if(!reader.read(16,context) || !reader.read(8,option) || !reader.read(64,instance)
        || !reader.read(16,item) || !reader.read(32,quantity)) {return false;}
    if(reader.remaining_bits()) {std::uint64_t tail{};if(!reader.read(8,tail) || tail) {return false;}}
    out={static_cast<std::int16_t>(int(context)-32768),static_cast<std::int8_t>(int(option)-128),instance,
        static_cast<std::int16_t>(int(item)-32768),static_cast<std::int32_t>(static_cast<std::int64_t>(quantity)-2147483648LL)};
    return true;
}
}
