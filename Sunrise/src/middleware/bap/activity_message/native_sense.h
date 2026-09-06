#pragma once

#include <cstddef>
#include <cstdint>
#include "scene_sense.h"

namespace sunrise::middleware::bap::activity_message::native_sense {
/** Pinned executable reflection; values are raw codes, never inferred actor deaths. */
struct Output {
    std::uint32_t schema{};
    std::uint32_t revision{};
    bool root{};
    scene_sense::Output scene{};
};
[[nodiscard]] constexpr std::uint32_t schema(std::uint8_t type) noexcept {
    switch (type) {
    case 1: return 0x80807ECC;
    case 2: return 0x80807DA2;
    case 23: return 0x80804F47;
    case 30: return 0x80809531;
    case 43: return 0x8080626A;
    case 70: return 0x808094F0;
    default: return 0;
    }
}
template<class Reader> bool optional(Reader& reader, std::size_t width) noexcept {
    std::uint64_t present{};
    return reader.read(1,present) && (!present || reader.skip(width));
}
template<class Reader> bool source(Reader& reader) noexcept {
    // 80807ECC +00,+04,+08,+0C,+10,+14, then state/selection/three bools.
    if (!optional(reader,31) || !optional(reader,31) || !optional(reader,31)
        || !optional(reader,6) || !optional(reader,7) || !optional(reader,31)
        || !reader.skip(8)) return false;
    std::uint64_t present{},count{};
    // 80807ECF: count4 and at most8 raw32 consumed-count values.
    if (!reader.read(1,present)) return false;
    if (present && (!reader.read(4,count) || count>8 || !reader.skip(count*32))) return false;
    // 80807ECD: fixed24 entries, each optional quantized7. No host dequantization.
    if (!reader.read(1,present)) return false;
    if (present) for (unsigned i=0;i<24;++i) if (!optional(reader,7)) return false;
    return true;
}
template<class Reader> bool member(Reader& reader) noexcept {
    // 80807DA2, including both nested optional records and fixed8 child slots.
    if (!optional(reader,31) || !optional(reader,9) || !optional(reader,31)) return false;
    std::uint64_t present{},nested{};
    if (!reader.read(1,present)) return false;
    if (present && (!optional(reader,6) || !optional(reader,31)
                    || !optional(reader,31) || !reader.skip(1))) return false; // 80807F6E
    if (!reader.read(1,present)) return false;
    if (present) { // 80807DA3
        if (!reader.read(1,nested)) return false;
        if (nested) for (unsigned i=0;i<8;++i) if (!optional(reader,31)) return false; // 80807DA4
        if (!optional(reader,32)) return false;
    }
    return optional(reader,31) && reader.skip(2) && optional(reader,31)
        && optional(reader,7) && optional(reader,7) && reader.skip(2);
}
/** Consumes precisely root+schema+revision. The caller owns the group terminator. */
template<class Reader>
[[nodiscard]] bool read(Reader& reader, std::uint8_t type, Output& output,
                        std::size_t& width) noexcept {
    Output result{};
    result.schema=schema(type);
    if (!result.schema) return false;
    if (type==43) {
        if (!scene_sense::read(reader,result.scene,width)) return false;
        result.root=result.scene.delta; result.revision=result.scene.revision;
        output=result; return true;
    }
    const auto before=reader.remaining_bits();
    std::uint64_t value{};
    if (!reader.read(1,value)) return false;
    result.root=value!=0;
    if (result.root) {
        if (type==1 && !source(reader)) return false;
        if (type==2 && !member(reader)) return false;
        if (type==23) for (unsigned i=0;i<6;++i) if (!optional(reader,32)) return false;
        if (type==30 && !reader.skip(66)) return false;
        if (type==70) {
            // 808094F8/808094F7 codec35: both native alternatives occupy64 bits.
            if (!reader.read(5,value) || value>16 || !reader.skip(value*64)
                || !reader.skip(16)) return false;
        }
    }
    if (!reader.read(32,value)) return false;
    result.revision=static_cast<std::uint32_t>(value);
    width=before-reader.remaining_bits(); output=result; return true;
}
} // namespace sunrise::middleware::bap::activity_message::native_sense
