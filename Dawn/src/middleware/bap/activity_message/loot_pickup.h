#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace dawn::middleware::bap::activity_message::loot_pickup {

/** Native incident target 3539, schema 0x808087F0, in build 86657. */
inline constexpr std::uint32_t kIncidentTarget = 3539;

/** The Tower arm: 640 content bits in 80 bytes. */
inline constexpr std::uint32_t kTowerPayloadBytes = 80;

/**
 * The Haunted Forest arm: 656 bits in 82 bytes, i.e. the same 640-bit body plus one extra 16-bit
 * field at a position that has not been recovered. Target 3539 has fired three times in the
 * archived logs, always in the Forest and always at this length.
 */
inline constexpr std::uint32_t kForestPayloadBytes = 82;

/** Haunted Forest bubble (package "infinite_abyss"). [authored] */
inline constexpr std::int32_t kForestBubble = 13;

/** @return True for a payload length this module can decode. Every other length fails closed. */
[[nodiscard]] constexpr bool known_payload_length(std::uint32_t bytes) noexcept {
    return bytes == kTowerPayloadBytes || bytes == kForestPayloadBytes;
}

/** Which arm produced one decoded pickup. */
enum class Variant : std::uint8_t {
    tower = 0,
    forest = 1,
};

/** Pickup::extraFieldIndex when the body carried no extra field. */
inline constexpr std::uint8_t kNoExtraField = 0xFFU;

/** Authenticated fields recovered from the native placed-loot incident body. */
struct Pickup final {
    std::uint32_t nonce{};
    std::uint64_t accountSoid{};
    std::uint64_t characterSoid{};
    std::uint32_t sourceHash{};
    std::array<float, 3> position{};
    std::int32_t bubble{-1};
    Variant variant{Variant::tower};
    /** Index of the known field the extra 16-bit field precedes; kNoExtraField when absent. */
    std::uint8_t extraFieldIndex{kNoExtraField};
    /** Bit offset of the extra field inside the body. */
    std::uint16_t extraFieldBitOffset{};
    /** Raw value of the extra 16-bit field. */
    std::uint16_t extraField{};
};

/**
 * Identity the caller expects one body to carry. Supplying it is what lets the 82-byte arm pick a
 * layout: a mis-decode shifts every following field, so an insertion point that still reproduces
 * the account SOID, the selected character and the bubble is the decode, and anything ambiguous
 * is refused.
 */
struct Identity final {
    std::uint64_t accountSoid{};
    std::uint64_t characterSoid{};
    /** Negative disables the bubble constraint. */
    std::int32_t bubble{-1};
};

/**
 * Decodes the exact placed-loot form, including canonical identity and padding fields.
 * @param payload Complete incident body; 80 or 82 bytes, every other length fails closed.
 * @param output Cleared first. Receives the accepted layout only.
 * @param expected Session identity used to choose between 82-byte layouts. Ignored by the 80-byte
 *        arm. Without it an 82-byte body is accepted only when exactly one insertion point decodes.
 */
[[nodiscard]] bool parse(std::span<const std::byte> payload, Pickup& output,
                         const Identity* expected = nullptr) noexcept;

} // namespace dawn::middleware::bap::activity_message::loot_pickup
