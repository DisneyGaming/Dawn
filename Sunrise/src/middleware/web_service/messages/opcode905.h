#pragma once
#include "../../encoding/bit_reader.h"
#include "../web_service_envelope.h"

namespace sunrise::middleware::web_service::messages::opcode905 {
struct Request {std::int8_t location{};std::uint64_t instance{};std::int16_t item{};std::int64_t clock{};bool hasClock{};};
// Native schema 808075D8 / constructor 530BE0, shared UI action 2.
inline bool parse(const Message& message,Request& out) noexcept {
    out={};if(message.opcode!=905 || (message.payload.size()!=11 && message.payload.size()!=19)) {return false;}
    encoding::bits::Reader reader(message.payload);std::uint64_t location{},instance{},item{},hasClock{},clock{};
    if(!reader.read(2,location) || !reader.read(64,instance) || !reader.read(16,item)
        || !reader.read(1,hasClock) || (hasClock && !reader.read(64,clock)) || reader.remaining_bits()>7) {return false;}
    while(reader.remaining_bits()) {std::uint64_t bit{};if(!reader.read(1,bit) || bit) {return false;}}
    out={static_cast<std::int8_t>(location-1),instance,static_cast<std::int16_t>(static_cast<int>(item)-32768),static_cast<std::int64_t>(clock),hasClock!=0};
    return true;
}
}
