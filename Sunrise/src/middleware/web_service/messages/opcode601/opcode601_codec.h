#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../web_service_envelope.h"

namespace sunrise::middleware::web_service::messages::opcode601 {

/** Web Service opcode for a loot pickup the Client cannot do on its own. */
inline constexpr std::uint16_t kOpcode = 601;
/** The whole response is 27 bytes: 6 header, 165 body bits, 2 trailer bits. */
inline constexpr std::size_t kResponseSize = 27;

/**
 * Reports whether this request is the loot pickup.
 * No response field reads the request body, so its width and content are not tested.
 * @param message Parsed Web Service envelope.
 * @return True when the opcode matches.
 */
[[nodiscard]] bool parse_request(const Message& message) noexcept;

/**
 * The pickup report body: schema 0x8080760B = {5-bit kind, bias 1} then the nested 0x80807BB8
 * source reference {i32 type tag (bias 0x80000000), i64 object handle, i32 client drop sequence}.
 * The client never puts the item or quantity on the wire; the server decides the payout.
 * Widths are the documented ones; `exact` reports whether the payload held exactly those bits
 * plus byte padding, so a live capture can correct the layout if it differs.
 */
struct Request {
    std::uint8_t rawKind{};
    std::int32_t kind{-1};
    std::int32_t sourceTag{};
    std::uint32_t rawSourceTag{};
    std::uint64_t sourceHandle{};
    std::uint32_t rawSequence{};
    std::int32_t sequence{};
    std::uint32_t payloadBits{};
    std::uint32_t consumedBits{};
    bool exact{};
};

/**
 * Decodes the pickup report as far as the payload allows.
 * @param message Opcode-601 request.
 * @param output Receives every field reached; `exact` is true only for a fully consumed body.
 * @return True when the opcode matches and at least the kind and source reference were read.
 */
[[nodiscard]] bool parse_request(const Message& message, Request& output) noexcept;

/**
 * Appends the three-field response tail (source tag, handle, sequence) after a status pair,
 * echoing the request's own values so the client can correlate its pending drop record.
 * Writes zeros when the request body did not parse, which is the historical neutral reply.
 * @param writer Payload writer positioned after the status pair.
 * @param message The opcode-601 request being answered.
 * @return True when the tail fits.
 */
template <class Writer>
[[nodiscard]] bool write_tail(Writer& writer, const Message& message) noexcept {
    Request request{};
    static_cast<void>(parse_request(message, request));
    return writer.write(request.rawSourceTag, 32U) && writer.write(request.sourceHandle, 64U)
           && writer.write(request.rawSequence, 32U);
}

/**
 * Encodes the fixed-width response that completes the request.
 * @param message Opcode-601 request whose envelope fields are echoed.
 * @param output Caller-owned response storage.
 * @param written Receives the exact response byte count on success.
 * @return True when the opcode matches and the response fits.
 */
[[nodiscard]] bool
encode_response(const Message& message, std::span<std::byte> output, std::size_t& written) noexcept;

} // namespace sunrise::middleware::web_service::messages::opcode601
