/** Bounded parser for the type-6 forms recovered from the current Omega captures. */

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

#include "../../encoding/bit_reader.h"
#include "../../encoding/byte_order.h"
#include "sense_update.h"

namespace dawn::middleware::bap::activity_message::sense_update {
namespace {

namespace bits = encoding::bits;

constexpr std::uint8_t kPresenceWidth = 1;
constexpr std::uint8_t kKeyWidth = 32;
constexpr std::uint8_t kSlotTypeWidth = 7;
constexpr std::uint8_t kSlotIndexWidth = 16;
constexpr std::uint32_t kSlotTypeBias = 1;
constexpr std::uint32_t kSlotIndexBias = 32768;
constexpr std::size_t kObjectHeaderBits =
    kPresenceWidth + kKeyWidth + kSlotTypeWidth + kSlotIndexWidth;

/** The root roster mirror uses the same widths as the host's type-5 delta. */
constexpr std::uint8_t kTopCountWidth = 9;
constexpr std::size_t kTopMaskWords = 8;
constexpr std::uint8_t kBubbleCountWidth = 7;
constexpr std::size_t kBubbleMaskWords = 3;
constexpr std::uint32_t kBubbleKeyBias = 0x80000000U;
constexpr std::uint32_t kMaximumBubble = 63;
constexpr std::size_t kMaximumTopKeys = kTopMaskWords * 32;
constexpr std::size_t kMaximumBubbleKeys = kBubbleMaskWords * 32;

/** Native sense schemas: 80807ECC, 80807DA2, 80804F47, 80809531, 8080626A, 808094F0. */
constexpr std::uint8_t kSlotTypeSpawner = 1;
constexpr std::uint8_t kSlotTypeCombatantMember = 2;
constexpr std::uint8_t kSlotTypeMission = 23;
constexpr std::uint8_t kSlotTypeVolume = 30;
constexpr std::uint8_t kSlotTypeScene = 43;
constexpr std::uint8_t kSlotTypeMonitor = 70;

/** Preserves Dawn's established diagnostic hash convention (not standard FNV-1a). */
constexpr std::uint64_t kProjectFnvBasis = 1469598103934665603ULL;
constexpr std::uint64_t kProjectFnvPrime = 1099511628211ULL;

[[nodiscard]] bool read_expected(bits::Reader& reader,
                                 std::uint8_t width,
                                 std::uint64_t expected) noexcept {
    std::uint64_t value = 0;
    return reader.read(width, value) && value == expected;
}

[[nodiscard]] bool read_masks(bits::Reader& reader,
                              std::span<std::uint32_t> masks) noexcept {
    for (std::uint32_t& mask : masks) {
        std::uint64_t value = 0;
        if (!reader.read(32, value)) {
            return false;
        }
        mask = static_cast<std::uint32_t>(value);
    }
    return true;
}

[[nodiscard]] bool append_roster_entry(SenseUpdate& update,
                                       std::uint32_t key,
                                       std::int16_t bubble,
                                       bool active) noexcept {
    if (update.rosterEntryCount >= update.rosterEntries.size()) {
        return false;
    }
    RosterEntry& entry = update.rosterEntries[update.rosterEntryCount++];
    entry.registryKey = key;
    entry.bubble = bubble;
    entry.active = active;
    return true;
}

/** Parses the optional phase-1 roster acknowledgement that precedes the changed groups. */
[[nodiscard]] bool parse_roster_acknowledgement(bits::Reader& reader,
                                                SenseUpdate& update) noexcept {
    std::uint64_t present = 0;
    if (!reader.read(kPresenceWidth, present)) {
        return false;
    }
    if (present == 0) {
        return true;
    }
    update.hasRosterAcknowledgement = true;
    if (!read_expected(reader, kPresenceWidth, 1)
        || !read_expected(reader, kPresenceWidth, 1)) {
        return false;
    }

    std::uint64_t topCountValue = 0;
    if (!reader.read(kTopCountWidth, topCountValue) || topCountValue > kMaximumTopKeys
        || topCountValue > update.rosterEntries.size()) {
        return false;
    }
    const auto topCount = static_cast<std::size_t>(topCountValue);
    update.topLevelRosterCount = static_cast<std::uint16_t>(topCount);
    for (std::size_t index = 0; index < topCount; ++index) {
        std::uint64_t key = 0;
        if (!reader.read(kKeyWidth, key)
            || !append_roster_entry(update, static_cast<std::uint32_t>(key), -1, false)) {
            return false;
        }
    }
    if (!read_expected(reader, kPresenceWidth, 1)) {
        return false;
    }
    std::array<std::uint32_t, kTopMaskWords> topMasks{};
    if (!read_masks(reader, topMasks) || !read_expected(reader, kPresenceWidth, 1)) {
        return false;
    }
    std::uint64_t topStateCount = 0;
    if (!reader.read(kTopCountWidth, topStateCount) || topStateCount != topCount) {
        return false;
    }
    for (std::size_t index = 0; index < topCount; ++index) {
        std::uint64_t state = 0;
        if (!reader.read(8, state)) {
            return false;
        }
        RosterEntry& entry = update.rosterEntries[index];
        entry.state = static_cast<std::uint8_t>(state);
        entry.active = ((topMasks[index / 32] >> (index % 32)) & 1U) != 0U;
    }

    std::uint64_t hasBubbles = 0;
    if (!reader.read(kPresenceWidth, hasBubbles)) {
        return false;
    }
    if (hasBubbles == 0) {
        return true;
    }
    std::uint64_t bubbleCountValue = 0;
    if (!reader.read(kBubbleCountWidth, bubbleCountValue)
        || bubbleCountValue > kMaximumBubble + 1U) {
        return false;
    }
    update.bubbleBlockCount = static_cast<std::uint8_t>(bubbleCountValue);
    for (std::size_t block = 0; block < bubbleCountValue; ++block) {
        std::uint64_t encodedBubble = 0;
        std::uint64_t keyCountValue = 0;
        if (!read_expected(reader, kPresenceWidth, 1)
            || !reader.read(kKeyWidth, encodedBubble) || encodedBubble < kBubbleKeyBias
            || encodedBubble - kBubbleKeyBias > kMaximumBubble
            || !read_expected(reader, kPresenceWidth, 1)
            || !read_expected(reader, kPresenceWidth, 1)
            || !reader.read(kBubbleCountWidth, keyCountValue)
            || keyCountValue > kMaximumBubbleKeys
            || keyCountValue > update.rosterEntries.size() - update.rosterEntryCount) {
            return false;
        }
        const auto bubble = static_cast<std::int16_t>(encodedBubble - kBubbleKeyBias);
        const std::size_t keyCount = static_cast<std::size_t>(keyCountValue);
        const std::size_t firstEntry = update.rosterEntryCount;
        for (std::size_t index = 0; index < keyCount; ++index) {
            std::uint64_t key = 0;
            if (!reader.read(kKeyWidth, key)
                || !append_roster_entry(
                    update, static_cast<std::uint32_t>(key), bubble, false)) {
                return false;
            }
        }
        if (!read_expected(reader, kPresenceWidth, 1)) {
            return false;
        }
        std::array<std::uint32_t, kBubbleMaskWords> masks{};
        if (!read_masks(reader, masks) || !read_expected(reader, kPresenceWidth, 1)) {
            return false;
        }
        std::uint64_t stateCount = 0;
        if (!reader.read(kBubbleCountWidth, stateCount) || stateCount != keyCount) {
            return false;
        }
        for (std::size_t index = 0; index < keyCount; ++index) {
            std::uint64_t state = 0;
            if (!reader.read(8, state)) {
                return false;
            }
            RosterEntry& entry = update.rosterEntries[firstEntry + index];
            entry.state = static_cast<std::uint8_t>(state);
            entry.active = ((masks[index / 32] >> (index % 32)) & 1U) != 0U;
        }
    }
    return true;
}

/** Optional reflection fields carry one delta-presence bit; required fields do not. */
[[nodiscard]] bool skip_optional(bits::Reader& reader,std::size_t width) noexcept {
    std::uint64_t present{};
    return reader.read(1,present) && (present==0 || reader.skip(width));
}

[[nodiscard]] bool skip_counted_words(bits::Reader& reader,std::uint8_t width,
                                     std::size_t maximum,std::size_t elementBits=32) noexcept {
    std::uint64_t count{};
    return reader.read(width,count) && count<=maximum && reader.skip(count*elementBits);
}

/** Original 4C77A0 writes root presence, reflected fields, then 4D8490 writes
 * a raw 32-bit revision. The object-list zero belongs to 4D81C0, not this body. */
[[nodiscard]] bool resolve_body_bits(const bits::Reader& input,std::uint8_t type,
                                     std::size_t remaining,SenseObject& object,
                                     std::size_t& bodyBits) noexcept {
    if(type!=kSlotTypeSpawner && type!=kSlotTypeCombatantMember && type!=kSlotTypeMission && type!=kSlotTypeVolume
        && type!=kSlotTypeScene && type!=kSlotTypeMonitor) { return false; }
    bits::Reader reader=input;
    const auto before=reader.remaining_bits();
    std::uint64_t present{};
    if(!reader.read(1,present)) { return false; }
    object.hasDelta=present!=0;
    if(object.hasDelta) {
        if(type==kSlotTypeSpawner) {
            // 80807ECC: six optional scalars, two small enums and three bools.
            for(const auto width : {31U,31U,31U,6U,7U,31U}) {
                if(!skip_optional(reader,width)) { return false; }
            }
            if(!reader.skip(2+3+1+1+1) || !reader.read(1,present)) { return false; }
            // 80807ECF -> 80809491: consumed counts, at most eight int32 values.
            if(present!=0 && !skip_counted_words(reader,4,8)) { return false; }
            if(!reader.read(1,present)) { return false; }
            // 80807ECD is a fixed array of 24 optional quantized 7-bit values.
            if(present!=0) {
                for(unsigned i=0;i<24;++i) {
                    if(!skip_optional(reader,7)) { return false; }
                }
            }
        } else if(type==kSlotTypeCombatantMember) {
            // 80807DA2: preserve the complete member delta as raw data. These
            // fields do not establish a combat kill or authorize progression.
            for(const auto width : {31U,9U,31U}) {
                if(!skip_optional(reader,width)) { return false; }
            }
            if(!reader.read(1,present)) { return false; }
            if(present!=0) {
                // 80807F6E: three optional scalars and one required bool.
                for(const auto width : {6U,31U,31U}) {
                    if(!skip_optional(reader,width)) { return false; }
                }
                if(!reader.skip(1)) { return false; }
            }
            if(!reader.read(1,present)) { return false; }
            if(present!=0) {
                // 80807DA3 -> 80807DA4: fixed eight optional 31-bit values,
                // followed by an independently optional raw 32-bit field.
                if(!reader.read(1,present)) { return false; }
                if(present!=0) {
                    for(unsigned i=0;i<8;++i) {
                        if(!skip_optional(reader,31)) { return false; }
                    }
                }
                if(!skip_optional(reader,32)) { return false; }
            }
            if(!skip_optional(reader,31) || !reader.skip(2)
                || !skip_optional(reader,31) || !skip_optional(reader,7)
                || !skip_optional(reader,7) || !reader.skip(1+1)) { return false; }
        } else if(type==kSlotTypeMission) {
            for(unsigned i=0;i<6;++i) {
                if(!skip_optional(reader,32)) { return false; }
            }
        } else if(type==kSlotTypeVolume) {
            if(!reader.skip(1+1+32+32)) { return false; }
        } else if(type==kSlotTypeScene) {
            if(!reader.skip(32+1) || !skip_optional(reader,31) || !reader.skip(2)
                || !skip_counted_words(reader,6,32)) { return false; }
        } else {
            // 808094F8 has at most 16 codec35 entries. Original 9F87E0 emits
            // exactly 64 bits in either native mode; retain them as raw data.
            if(!skip_counted_words(reader,5,16,64) || !reader.skip(16)) { return false; }
        }
    }
    std::uint64_t revision{};
    if(!reader.read(32,revision)) { return false; }
    object.revision=static_cast<std::uint32_t>(revision);
    bodyBits=before-reader.remaining_bits();
    // Every nonempty group still needs its own terminal zero after its last object.
    return bodyBits<remaining && bodyBits<=kCapturedBodyBitCapacity;
}

[[nodiscard]] bool read_object_body(bits::Reader& reader,std::size_t bodyBits,
                                    SenseObject& object) noexcept {
    if(bodyBits>kCapturedBodyBitCapacity) { return false; }
    object.bodyBits=static_cast<std::uint32_t>(bodyBits);
    auto hash=kProjectFnvBasis;
    for(unsigned shift=0;shift<32;shift+=8) {
        hash^=(object.bodyBits>>shift)&0xFFU; hash*=kProjectFnvPrime;
    }
    std::size_t left=bodyBits;
    for(std::size_t chunk=0;left!=0;++chunk) {
        const auto width=static_cast<std::uint8_t>((std::min)(left,std::size_t{64}));
        std::uint64_t value{};
        if(!reader.read(width,value)) { return false; }
        if(chunk==0) { object.bodyFirst=value; }
        else if(chunk==1) { object.bodySecond=value; }
        else if(chunk==2) { object.bodyThird=value; }
        else { object.bodyTail[chunk-3]=value; }
        object.bodySetBitCount+=static_cast<std::uint16_t>(std::popcount(value));
        for(unsigned bit=width;bit!=0;--bit) {
            hash^=(value>>(bit-1))&1U; hash*=kProjectFnvPrime;
        }
        left-=width;
    }
    object.bodyHash=hash;
    return true;
}

/** Parses every supported, schema-sized object until the required list terminator. */
[[nodiscard]] bool parse_groups(bits::Reader& reader, SenseUpdate& update) noexcept {
    for (;;) {
        std::uint64_t present = 0;
        if (!reader.read(kPresenceWidth, present)) {
            return false;
        }
        if (present == 0) {
            return true;
        }
        if (update.groupCount >= update.groups.size()) {
            return false;
        }
        std::uint64_t keyValue = 0;
        std::uint64_t groupBitsValue = 0;
        if (!reader.read(kKeyWidth, keyValue) || !reader.read(kKeyWidth, groupBitsValue)
            || groupBitsValue == 0 || groupBitsValue > reader.remaining_bits()
            || groupBitsValue > (std::numeric_limits<std::uint32_t>::max)()) {
            return false;
        }
        SenseGroup& group = update.groups[update.groupCount];
        group.registryKey = static_cast<std::uint32_t>(keyValue);
        group.bodyBits = static_cast<std::uint32_t>(groupBitsValue);
        group.firstObject = update.objectCount;
        const std::size_t groupStartRemaining = reader.remaining_bits();
        bool terminated=false;
        while (groupStartRemaining - reader.remaining_bits() < group.bodyBits) {
            const std::size_t consumed = groupStartRemaining - reader.remaining_bits();
            const std::size_t groupRemaining = group.bodyBits - consumed;
            std::uint64_t objectPresent = 0;
            if(!reader.read(kPresenceWidth,objectPresent)) { return false; }
            if(objectPresent==0) { terminated=true; break; }
            if (groupRemaining < kObjectHeaderBits + 34
                || update.objectCount >= update.objects.size()) {
                return false;
            }
            std::uint64_t objectKey = 0;
            std::uint64_t encodedType = 0;
            std::uint64_t encodedIndex = 0;
            if (!reader.read(kKeyWidth, objectKey) || objectKey != group.registryKey
                || !reader.read(kSlotTypeWidth, encodedType) || encodedType < kSlotTypeBias
                || !reader.read(kSlotIndexWidth, encodedIndex)
                || encodedIndex < kSlotIndexBias) {
                return false;
            }
            SenseObject& object = update.objects[update.objectCount];
            object.registryKey = group.registryKey;
            object.slotType = static_cast<std::uint8_t>(encodedType - kSlotTypeBias);
            object.slotIndex = static_cast<std::uint16_t>(encodedIndex - kSlotIndexBias);
            object.groupOrdinal = update.groupCount;
            object.objectOrdinal = group.objectCount;
            const std::size_t afterHeader = groupStartRemaining - reader.remaining_bits();
            if (afterHeader > group.bodyBits) {
                return false;
            }
            std::size_t bodyBits = 0;
            if (!resolve_body_bits(reader,
                                   object.slotType,
                                   group.bodyBits - afterHeader,
                                   object,
                                   bodyBits)
                || !read_object_body(reader, bodyBits, object)) {
                return false;
            }
            ++update.objectCount;
            ++group.objectCount;
        }
        if (!terminated || group.objectCount==0
            || groupStartRemaining - reader.remaining_bits() != group.bodyBits) {
            return false;
        }
        ++update.groupCount;
    }
}

[[nodiscard]] bool parse_recovered(std::span<const std::byte> input,
                                   SenseUpdate& update,
                                   std::size_t& consumedBits) noexcept {
    bits::Reader reader(input);
    std::uint64_t literal = 0;
    if (!reader.read(kEpochFieldWidth, update.epoch.first)
        || !reader.read(kEpochFieldWidth, update.epoch.second)
        || !reader.read(kLiteralZeroWidth, literal) || literal != 0
        || !parse_roster_acknowledgement(reader, update) || !parse_groups(reader, update)
        || !read_expected(reader, kPresenceWidth, 0)) {
        return false;
    }

    const std::size_t paddingBits = reader.remaining_bits();
    if (paddingBits > encoding::kBitsPerByte - 1U) {
        return false;
    }
    consumedBits = input.size() * encoding::kBitsPerByte - paddingBits;
    std::uint64_t padding = 0;
    if (!reader.read(static_cast<std::uint8_t>(paddingBits), padding) || padding != 0) {
        return false;
    }
    update.paddingBits = static_cast<std::uint8_t>(paddingBits);
    return true;
}

} // namespace

bool parse_sense_update(std::span<const std::byte> input,
                        SenseUpdate& update,
                        std::size_t& consumedBits) noexcept {
    update = {};
    consumedBits = 0;
    if (input.size() > kOuterBitCapacity / encoding::kBitsPerByte
        || !parse_recovered(input, update, consumedBits)) {
        update = {};
        consumedBits = 0;
        return false;
    }
    return true;
}

} // namespace dawn::middleware::bap::activity_message::sense_update
