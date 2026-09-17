#include "loot_pickup.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>

#include "../../encoding/bit_reader.h"

namespace sunrise::middleware::bap::activity_message::loot_pickup {
namespace {

/** What one field of the placed-loot body contributes to the decode. */
enum class Op : std::uint8_t {
    nonce,
    literal,
    characterSoid,
    characterEcho,
    accountSoid,
    sourceHash,
    position,
    bubble,
};

struct Field final {
    Op op{};
    std::uint8_t width{};
    std::uint64_t literal{};
};

/**
 * The 640-bit placed-loot body, field by field. This is the exact sequence the 80-byte Tower arm
 * has always decoded; the 82-byte Forest arm is this same list with one 16-bit field inserted.
 */
constexpr std::array<Field, 22> kFields{{
    {Op::nonce, 32U, 0U},
    {Op::literal, 3U, 2U},
    {Op::characterSoid, 64U, 0U},
    // Nullable actor selector 18, absent subject -1, and the three false flags.
    {Op::literal, 6U, 18U},
    {Op::literal, 5U, 0U},
    {Op::literal, 32U, 0x811C9DC5U},
    {Op::literal, 32U, 0x811C9DC5U},
    {Op::literal, 32U, 0x811C9DC5U},
    {Op::characterEcho, 64U, 0U},
    {Op::literal, 6U, 3U},
    {Op::accountSoid, 64U, 0U},
    {Op::literal, 3U, 1U},
    {Op::literal, 2U, 1U},
    {Op::characterEcho, 64U, 0U},
    {Op::literal, 32U, 0x80000001U},
    {Op::sourceHash, 32U, 0U},
    {Op::position, 32U, 0U},
    {Op::position, 32U, 0U},
    {Op::position, 32U, 0U},
    {Op::bubble, 32U, 0U},
    {Op::literal, 32U, 0x811C9DC5U},
    {Op::literal, 7U, 0U},
}};

/** Width of the one extra field the 82-byte Forest arm adds: 656 - 640 bits. */
constexpr std::uint8_t kExtraFieldWidth = 16U;

/** Insertion point meaning "this body carries no extra field". */
constexpr std::size_t kNoInsertion = (std::numeric_limits<std::size_t>::max)();

/**
 * Decodes the body once, with the extra 16-bit field read immediately before kFields[extraAt].
 * extraAt == kFields.size() puts it after the trailing pad; kNoInsertion omits it entirely.
 * Every literal, the three echoes of the character SOID and the exact bit length must all hold,
 * so a wrong insertion point almost always fails here rather than producing a plausible pickup.
 */
[[nodiscard]] bool decode(std::span<const std::byte> payload, std::size_t extraAt,
                          Pickup& output) noexcept {
    Pickup parsed{};
    encoding::bits::Reader reader(payload);
    std::size_t coordinate = 0U;
    std::size_t offset = 0U;
    std::uint64_t value = 0U;
    for (std::size_t index = 0U; index <= kFields.size(); ++index) {
        if (index == extraAt) {
            if (!reader.read(kExtraFieldWidth, value)) {
                return false;
            }
            parsed.variant = Variant::forest;
            parsed.extraFieldIndex = static_cast<std::uint8_t>(index);
            parsed.extraFieldBitOffset = static_cast<std::uint16_t>(offset);
            parsed.extraField = static_cast<std::uint16_t>(value);
            offset += kExtraFieldWidth;
        }
        if (index == kFields.size()) {
            break;
        }
        const Field& field = kFields[index];
        if (!reader.read(field.width, value)) {
            return false;
        }
        offset += field.width;
        switch (field.op) {
        case Op::nonce:
            parsed.nonce = static_cast<std::uint32_t>(value);
            break;
        case Op::literal:
            if (value != field.literal) {
                return false;
            }
            break;
        case Op::characterSoid:
            parsed.characterSoid = value;
            break;
        case Op::characterEcho:
            if (value != parsed.characterSoid) {
                return false;
            }
            break;
        case Op::accountSoid:
            parsed.accountSoid = value;
            break;
        case Op::sourceHash:
            parsed.sourceHash = static_cast<std::uint32_t>(value);
            break;
        case Op::position: {
            const float component = std::bit_cast<float>(static_cast<std::uint32_t>(value));
            if (!std::isfinite(component) || coordinate >= parsed.position.size()) {
                return false;
            }
            parsed.position[coordinate] = component;
            ++coordinate;
            break;
        }
        case Op::bubble:
            parsed.bubble =
                static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x80000000LL);
            break;
        }
    }
    if (reader.remaining_bits() != 0U || parsed.accountSoid == 0U || parsed.characterSoid == 0U) {
        return false;
    }
    output = parsed;
    return true;
}

[[nodiscard]] bool identity_matches(const Pickup& pickup, const Identity& expected) noexcept {
    return expected.accountSoid != 0U && expected.characterSoid != 0U
        && pickup.accountSoid == expected.accountSoid
        && pickup.characterSoid == expected.characterSoid
        && (expected.bubble < 0 || pickup.bubble == expected.bubble);
}

} // namespace

bool parse(std::span<const std::byte> payload, Pickup& output,
           const Identity* expected) noexcept {
    output = {};
    if (payload.size() == kTowerPayloadBytes) {
        Pickup parsed{};
        if (!decode(payload, kNoInsertion, parsed)) {
            return false;
        }
        output = parsed;
        return true;
    }
    if (payload.size() != kForestPayloadBytes) {
        return false;
    }

    // 82 bytes is 656 bits: the same body plus one 16-bit field whose position is unknown. Try
    // every insertion point and keep only the layouts whose identity fields validate against the
    // session. Anything ambiguous - no survivor, or more than one - is a mis-decode and fails
    // closed, because paying on a mis-decoded body would pay on someone else's identity.
    Pickup chosen{};
    std::size_t accepted = 0U;
    for (std::size_t at = 0U; at <= kFields.size(); ++at) {
        Pickup candidate{};
        if (!decode(payload, at, candidate)) {
            continue;
        }
        if (expected != nullptr && !identity_matches(candidate, *expected)) {
            continue;
        }
        if (accepted == 0U) {
            chosen = candidate;
        }
        ++accepted;
    }
    if (accepted != 1U) {
        return false;
    }
    output = chosen;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::loot_pickup
