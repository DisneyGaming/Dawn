#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "../../../../middleware/bap/activity_message/sense_update.h"
#include "../../../../middleware/bap/activity_message/sensor_auth_update.h"

namespace sunrise::server::bap::encrypted::diagnostics::omega_trace {

namespace message = middleware::bap::activity_message;

/** Appends one raw type-6 packet and every decoded object to Omega's JSONL timeline. */
void record_sense(std::uint64_t sessionId,
                  std::uint64_t packetSequence,
                  std::uint64_t packetHash,
                  std::string_view validation,
                  std::string_view transition,
                  std::string_view hostAction,
                  bool rosterReady,
                  bool openingTriggered,
                  bool parsed,
                  const message::sense_update::SenseUpdate& update,
                  std::span<const std::byte> payload) noexcept;

/** Records one successfully encoded Omega type-5 body and returns its delivery correlation id. */
[[nodiscard]] std::uint64_t
record_auth_staged(std::uint64_t sessionId,
                   const message::sensor_auth_update::Snapshot& snapshot,
                   std::span<const std::byte> payload) noexcept;

/** Records whether a staged Omega authority body reached the caller or was discarded. */
void record_auth_delivery(std::uint64_t sessionId,
                          std::uint64_t publicationId,
                          std::uint8_t scriptState,
                          bool delivered) noexcept;

} // namespace sunrise::server::bap::encrypted::diagnostics::omega_trace
