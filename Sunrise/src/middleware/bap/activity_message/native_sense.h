#pragma once

#include <cstddef>
#include <cstdint>
#include "scene_sense.h"
#include "squad_sense.h"
#include "monitor_sense.h"
#include "combatant_sense.h"

namespace sunrise::middleware::bap::activity_message::native_sense {
/** Pinned executable reflection; values are raw codes, never inferred actor deaths. */
struct Output {
    std::uint32_t schema{};
    std::uint32_t revision{};
    bool root{};
    scene_sense::Output scene{};
    squad_sense::Output squad{};
    monitor_sense::Output monitor{};
    combatant_sense::Output combatant{};
    std::uint32_t generatorSeed{},generatorRegions{};
    std::uint64_t generatorGroups{};
};
[[nodiscard]] constexpr std::uint32_t schema(std::uint8_t type) noexcept {
    switch (type) {
    case 1: return 0x80807ECC;
    case 2: return 0x80807DA2;
    case 23: return 0x80804F47;
    case 30: return 0x80809531;
    case 37: return 0x80805006;
    case 43: return 0x8080626A;
    case 70: return 0x808094F0;
    default: return 0;
    }
}
template<class Reader> bool optional(Reader& reader, std::size_t width) noexcept {
    std::uint64_t present{};
    return reader.read(1,present) && (!present || reader.skip(width));
}
/** 80807ECC's six optional scalars, five required fields and two nested arrays. The reflected
 * decode consumes exactly the bits the old skip did; it retains the task-evaluator costs, which
 * are the only thing that names a reachable authored combat objective for a squad. */
template<class Reader> bool source(Reader& reader, squad_sense::Output& output) noexcept {
    return squad_sense::read_delta(reader,output);
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
        if (type==1 && !source(reader,result.squad)) return false;
        if (type==2 && !combatant_sense::read_delta(reader,result.combatant)) return false;
        if (type==23) for (unsigned i=0;i<6;++i) if (!optional(reader,32)) return false;
        if (type==30 && !monitor_sense::read(reader,result.monitor)) return false;
        if (type==37) {
            // 8080500B: two records, each with a bounded variable tile array, then 80805009.
            for(unsigned record=0;record<2;++record) {
                if(!reader.skip(468) || !reader.read(7,value) || value>100 || !reader.skip(value*48)) return false;
            }
            if(!reader.read(32,value)) return false;
            result.generatorSeed=static_cast<std::uint32_t>(value);
            for(unsigned i=0;i<32;++i) {
                if(!reader.read(8,value)) return false;
                if(value) result.generatorRegions|=std::uint32_t{1}<<i;
            }
            for(unsigned i=0;i<64;++i) {
                if(!reader.read(8,value)) return false;
                if(value) result.generatorGroups|=std::uint64_t{1}<<i;
            }
        }
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
