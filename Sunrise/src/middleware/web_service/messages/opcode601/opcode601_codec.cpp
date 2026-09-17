/**
 * Opcode 601 is a loot pickup. The Client sends it when it cannot pick the loot up on its own.
 * The reply is 165 bits and no field can be left out, so the bare echo under-runs the decoder.
 * Nothing reads the three tail fields. The request carries no completion handle.
 */

#include "opcode601_codec.h"

#include <algorithm>
#include <array>

#include "../../../encoding/bit_reader.h"
#include "../../../encoding/bit_writer.h"
#include "../../../encoding/byte_order.h"
#include "../../status_fields.h"

namespace sunrise::middleware::web_service::messages::opcode601 {
namespace {

/** The three tail integers after the status pair are 32, 64 and 32 bits. */
constexpr std::uint8_t kTailIntegerWidth = 32;
constexpr std::uint8_t kTailLongWidth = 64;
/** Required fields nothing reads still take their width. Zero is the neutral value. */
constexpr std::uint64_t kUnusedValue = 0;

} // namespace

/** Reports whether this request is the remote loot pickup. */
bool parse_request(const Message& message) noexcept {
    return message.opcode == kOpcode;
}

/** Decodes the pickup report body; tolerant of a longer body, exact only when fully consumed. */
bool parse_request(const Message& message, Request& output) noexcept {
    output = {};
    if (message.opcode != kOpcode) {
        return false;
    }
    output.payloadBits = static_cast<std::uint32_t>(message.payload.size() * 8U);
    encoding::bits::Reader reader(message.payload);
    std::uint64_t value{};
    if (!reader.read(5U, value)) {
        return false;
    }
    output.rawKind = static_cast<std::uint8_t>(value);
    output.kind = static_cast<std::int32_t>(value) - 1;
    if (!reader.read(32U, value)) {
        return false;
    }
    output.rawSourceTag = static_cast<std::uint32_t>(value);
    output.sourceTag = static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x80000000LL);
    if (!reader.read(64U, output.sourceHandle) || !reader.read(32U, value)) {
        return false;
    }
    output.rawSequence = static_cast<std::uint32_t>(value);
    output.sequence = static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x80000000LL);
    output.consumedBits = 5U + 32U + 64U + 32U;
    output.exact = reader.remaining_bits() < 8U;
    return true;
}

/** Encodes the status pair and its three-field tail in descriptor order. */
bool encode_response(const Message& message,
                     std::span<std::byte> output,
                     std::size_t& written) noexcept {
    written = 0;
    if (!parse_request(message) || output.size() < kResponseSize) {
        return false;
    }

    std::array<std::byte, kResponseSize> staged{};
    encoding::write_u16_be(std::span(staged).first<encoding::kU16Size>(), message.opcode);
    encoding::write_u32_be(std::span(staged).subspan<encoding::kU16Size, encoding::kU32Size>(),
                           message.transactionId);

    // A neutral answer: status 0 and an unset version, so it names a version the Client already
    // has. The three tail fields echo the request's source reference and drop sequence.
    encoding::bits::Writer writer(std::span(staged).subspan(kEnvelopeHeaderSize));
    bool encoded = status::write_fields(writer, ResponseShape::statusPair, StatusResponse{});
    static_cast<void>(kUnusedValue);
    static_cast<void>(kTailIntegerWidth);
    static_cast<void>(kTailLongWidth);
    encoded = encoded && write_tail(writer, message) && writer.write(0U, kAbsentTrailerWidth);

    std::size_t payloadSize = 0;
    if (!encoded || !writer.finish(payloadSize)) {
        return false;
    }
    const std::size_t total = kEnvelopeHeaderSize + payloadSize;
    if (output.size() < total) {
        return false;
    }
    std::copy_n(staged.begin(), total, output.begin());
    written = total;
    return true;
}

} // namespace sunrise::middleware::web_service::messages::opcode601
