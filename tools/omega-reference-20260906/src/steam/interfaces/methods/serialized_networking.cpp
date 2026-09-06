#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../internal.h"

namespace sunrise::steam::interfaces::methods {
namespace {

/** Enough calls to cover initialization and three channel-security retries without flooding. */
constexpr unsigned kMaximumReports = 128;

std::atomic<unsigned> g_reports{0};

/** Reads a tiny stable fingerprint without trusting a provider-owned message pointer. */
void fingerprint(const void* message,
                 DWORD messageSize,
                 std::uint32_t& first,
                 std::uint64_t& hash) noexcept {
    first = 0;
    hash = 14695981039346656037ULL;
    if (message == nullptr || messageSize == 0) {
        return;
    }
    __try {
        const auto* const bytes = static_cast<const unsigned char*>(message);
        const DWORD firstSize = static_cast<DWORD>(sizeof(first));
        const DWORD prefix = messageSize < firstSize ? messageSize : firstSize;
        for (DWORD index = 0; index < prefix; ++index) {
            first |= static_cast<std::uint32_t>(bytes[index]) << (index * 8U);
        }
        for (DWORD index = 0; index < messageSize; ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        first = 0;
        hash = 0;
    }
}

/** Emits one bounded serialized-networking method observation. */
void report(const char* method,
            std::uint64_t remoteId,
            DWORD value0,
            DWORD value1,
            const void* message,
            DWORD messageSize,
            const char* behavior = "stub") noexcept {
    if (g_reports.fetch_add(1, std::memory_order_relaxed) >= kMaximumReports) {
        return;
    }
    std::uint32_t first = 0;
    std::uint64_t hash = 0;
    fingerprint(message, messageSize, first, hash);
    std::array<char, 320> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=steam_networking stage=serialized method=%s remote=0x%016llX value0=%u "
        "value1=%u bytes=%u first=0x%08X hash=0x%016llX behavior=%s",
        method,
        static_cast<unsigned long long>(remoteId),
        static_cast<unsigned>(value0),
        static_cast<unsigned>(value1),
        static_cast<unsigned>(messageSize),
        static_cast<unsigned>(first),
        static_cast<unsigned long long>(hash),
        behavior);
    if (written > 0) {
        const std::size_t length = static_cast<std::size_t>(written) < line.size()
                                       ? static_cast<std::size_t>(written)
                                       : line.size() - 1;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         std::string_view(line.data(), length));
    }
}

} // namespace

/** Drops a rendezvous notification. Nothing is sent. */
void serialized_send_rendezvous([[maybe_unused]] void* self,
                                std::uint64_t remoteId,
                                DWORD sourceConnectionId,
                                const void* message,
                                DWORD messageSize) noexcept {
    report("rendezvous", remoteId, sourceConnectionId, 0, message, messageSize);
}

/** Drops a connection failure message. Nothing is sent. */
void serialized_send_failure([[maybe_unused]] void* self,
                             std::uint64_t remoteId,
                             DWORD destinationConnectionId,
                             DWORD reason,
                             [[maybe_unused]] const char* reasonText) noexcept {
    report("failure", remoteId, destinationConnectionId, reason, nullptr, 0);
}

/** @return Zero. This shim delivers no serialized certificates. */
ApiCall serialized_get_certificate([[maybe_unused]] void* self) noexcept {
    report("certificate", 0, 0, 0, nullptr, 0);
    return 0;
}

/**
 * Writes an empty network config.
 * @param output Optional text buffer.
 * @return Zero config bytes.
 */
int serialized_get_network_config([[maybe_unused]] void* self,
                                  void* output,
                                  DWORD capacity,
                                  [[maybe_unused]] const char* launcherPartner) noexcept {
    report("config", 0, capacity, 0, nullptr, 0);
    if (output != nullptr && capacity > 0) {
        *static_cast<char*>(output) = '\0';
    }
    return 0;
}

/** Drops a relay ticket. Nothing is kept. */
void serialized_cache_relay_ticket([[maybe_unused]] void* self,
                                   const void* ticket,
                                   DWORD ticketSize) noexcept {
    report("cache_ticket", 0, 0, 0, ticket, ticketSize);
}

/** @return Zero. No relay tickets are kept. */
DWORD serialized_relay_ticket_count([[maybe_unused]] void* self) noexcept {
    report("ticket_count", 0, 0, 0, nullptr, 0);
    return 0;
}

/** @return Zero. There is no ticket at any index. */
int serialized_get_relay_ticket([[maybe_unused]] void* self,
                                DWORD index,
                                [[maybe_unused]] void* output,
                                DWORD capacity) noexcept {
    report("get_ticket", 0, index, capacity, nullptr, 0);
    return 0;
}

/** Drops a connection-state message. Nothing is kept. */
void serialized_post_connection_state([[maybe_unused]] void* self,
                                      const void* message,
                                      DWORD messageSize) noexcept {
    report("connection_state", 0, 0, 0, message, messageSize);
}

} // namespace sunrise::steam::interfaces::methods
