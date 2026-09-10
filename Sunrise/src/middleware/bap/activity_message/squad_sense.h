#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::middleware::bap::activity_message::squad_sense {

/** Root native schema for a slot-type-1 squad sensor, and the two arrays nested inside it:
 * 80807ECF holds the consumed-request counts, 80807ECD the task evaluator's own costs. */
inline constexpr std::uint32_t kSchema = 0x80807ECCU, kCountSchema = 0x80807ECFU,
                               kCostSchema = 0x80807ECDU;
/** The cost array is a fixed twenty-four entries, one per authored task group. */
inline constexpr std::size_t kTaskGroupCount = 24;
/** The largest count lane the nested list can name, and the array it indexes. */
inline constexpr std::size_t kCountCapacity = 8;
/** Costs are quantized into seven bits against a native maximum of 2040. The top code is that
 * maximum itself, which the evaluator uses for "this group cannot be reached from here" rather
 * than for an expensive route, so a group reporting it must never be selected. */
inline constexpr std::uint8_t kUnreachableCost = 127;
/** Root scalar ordinals: 1 is the revision the costs were evaluated at, 3 the live members. */
inline constexpr std::size_t kRevisionScalar = 1, kAliveScalar = 3;
inline constexpr std::size_t kScalarCount = 6;

/** One squad's reflected sense delta. Absent optional fields keep their published defaults, so a
 * caller merges this against what it already holds rather than treating it as a whole state. */
struct Output final {
    /** Raw seven-bit costs; only the entries named by costMask were on the wire. */
    std::array<std::uint8_t, kTaskGroupCount> cost{};
    std::uint32_t costMask{};
    std::array<std::int32_t, kCountCapacity> counts{};
    std::uint32_t revision{};
    std::uint32_t alive{};
    std::uint8_t countLength{};
    bool hasRevision{}, hasAlive{}, initialized{}, removal{}, costsPresent{}, countsPresent{};
};

/** Reads one optional reflected field, leaving an absent field's value to the caller. */
template<class Reader>
[[nodiscard]] bool optional_scalar(Reader& reader, std::size_t width, std::uint64_t& value,
                                   bool& present) noexcept {
    std::uint64_t flag{};
    if (!reader.read(1, flag)) { return false; }
    present = flag != 0;
    return !present || reader.read(static_cast<std::uint8_t>(width), value);
}

/**
 * Reads the root delta body: the caller has already consumed the root presence bit and still owns
 * the raw revision that follows. Bit consumption is exactly what the reflected schema declares, so
 * this can stand in for a skip of the same body without moving any boundary.
 */
template<class Reader>
[[nodiscard]] bool read_delta(Reader& reader, Output& output) noexcept {
    static constexpr std::size_t kScalarWidths[kScalarCount]{31, 31, 31, 6, 7, 31};
    Output result{};
    for (std::size_t index = 0; index < kScalarCount; ++index) {
        std::uint64_t value{};
        bool present{};
        if (!optional_scalar(reader, kScalarWidths[index], value, present)) { return false; }
        if (!present) { continue; }
        if (index == kRevisionScalar) {
            result.revision = static_cast<std::uint32_t>(value);
            result.hasRevision = true;
        } else if (index == kAliveScalar) {
            result.alive = static_cast<std::uint32_t>(value);
            result.hasAlive = true;
        }
    }
    // Fields 6 through 10 are required, so they carry no presence bit: two enum bits, three more,
    // then the member-removal flag, one unused flag and the initialized latch.
    std::uint64_t required{};
    if (!reader.read(2, required) || !reader.read(3, required)) { return false; }
    if (!reader.read(1, required)) { return false; }
    result.removal = required != 0;
    if (!reader.read(1, required) || !reader.read(1, required)) { return false; }
    result.initialized = required != 0;

    std::uint64_t present{};
    if (!reader.read(1, present)) { return false; }
    if (present != 0) {
        std::uint64_t count{};
        if (!reader.read(4, count) || count > kCountCapacity) { return false; }
        result.countsPresent = true;
        result.countLength = static_cast<std::uint8_t>(count);
        for (std::uint64_t index = 0; index < count; ++index) {
            std::uint64_t value{};
            if (!reader.read(32, value)) { return false; }
            // Signed counts carry the 32-bit midpoint as their wire bias.
            result.counts[index] = static_cast<std::int32_t>(
                static_cast<std::int64_t>(value) - static_cast<std::int64_t>(0x80000000LL));
        }
    }
    if (!reader.read(1, present)) { return false; }
    if (present != 0) {
        result.costsPresent = true;
        for (std::size_t index = 0; index < kTaskGroupCount; ++index) {
            std::uint64_t value{};
            bool sent{};
            if (!optional_scalar(reader, 7, value, sent)) { return false; }
            if (!sent) { continue; }
            result.cost[index] = static_cast<std::uint8_t>(value);
            result.costMask |= std::uint32_t{1} << index;
        }
    }
    output = result;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::squad_sense
