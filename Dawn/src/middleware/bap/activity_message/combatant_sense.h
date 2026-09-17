#pragma once

#include <cstdint>

namespace dawn::middleware::bap::activity_message::combatant_sense {
/** Optional native actor levels. Absence means unchanged, never zero or actor death. */
struct Output final {
    std::uint32_t spawnRevision{}, programRevision{}, deliveryRevision{};
    std::uint8_t programState{}, actorQuery{};
    std::int8_t deliveryState{-1};
    bool hasSpawnRevision{}, hasProgramRevision{}, hasProgramState{};
    bool hasDeliveryRevision{}, hasActorQuery{}, detached{}, snapshotValid{};
};

/** Reads an optional scalar without losing its independent presence bit. */
template<class Reader, class Value>
[[nodiscard]] bool optional(Reader& reader, std::uint8_t width, Value& output,
                            bool& present) noexcept {
    std::uint64_t value{};
    if (!reader.read(1, value)) { return false; }
    present = value != 0;
    if (present) {
        if (!reader.read(width, value)) { return false; }
        output = static_cast<Value>(value);
    }
    return true;
}

/** Decodes the reflected type-2 delta. The caller owns root presence and transport revision. */
template<class Reader>
[[nodiscard]] bool read_delta(Reader& reader, Output& output) noexcept {
    Output result{};
    std::uint32_t ignored{};
    bool present{};
    std::uint64_t value{};
    if (!optional(reader,31,result.spawnRevision,result.hasSpawnRevision)
        || !optional(reader,9,ignored,present) || !optional(reader,31,ignored,present)
        || !reader.read(1,value)) { return false; }
    if (value && (!optional(reader,6,result.programState,result.hasProgramState)
        || !optional(reader,31,result.programRevision,result.hasProgramRevision)
        || !optional(reader,31,ignored,present) || !reader.skip(1))) { return false; }
    if (!reader.read(1,value)) { return false; }
    if (value) {
        if (!reader.read(1,value)) { return false; }
        if (value) {
            for (unsigned index=0;index<8;++index) {
                if (!optional(reader,31,ignored,present)) { return false; }
            }
        }
        if (!optional(reader,32,ignored,present)) { return false; }
    }
    if (!optional(reader,31,result.deliveryRevision,result.hasDeliveryRevision)
        || !reader.read(2,value)) { return false; }
    // The dependency state is int8 with bias one; wire 1 is logical inactive/finished.
    result.deliveryState=static_cast<std::int8_t>(static_cast<int>(value)-1);
    if (!optional(reader,31,ignored,present)
        || !optional(reader,7,result.actorQuery,result.hasActorQuery)
        || !optional(reader,7,ignored,present) || !reader.read(1,value)) { return false; }
    result.detached=value!=0;
    if (!reader.read(1,value)) { return false; }
    result.snapshotValid=value!=0;
    output=result;
    return true;
}
} // namespace dawn::middleware::bap::activity_message::combatant_sense
