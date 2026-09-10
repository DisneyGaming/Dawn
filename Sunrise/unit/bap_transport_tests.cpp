#include <WinSock2.h>
#include <WS2tcpip.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/logging/log.h"
#include "core/settings/settings.h"
#include "server/bap/runtime.h"
#include "server/transport/bap_listener.h"
#include "server/transport/internal.h"

namespace {
using namespace sunrise;
namespace transport = server::transport;
std::vector<std::uint32_t> received;
unsigned checks{};
unsigned polls{};

void require(bool condition, const char* label) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}

std::uint32_t read32(std::span<const std::byte> bytes) {
    std::uint32_t value{};
    for (const auto byte : bytes.first(4)) {
        value = (value << 8U) | std::to_integer<std::uint32_t>(byte);
    }
    return value;
}

std::vector<std::byte> frame(std::uint32_t sequence, bool reply = false,
                             std::size_t size = 3528) {
    std::vector<std::byte> bytes(size);
    bytes[0] = std::byte{0xAB};
    bytes[1] = std::byte{1};
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[2 + i] = static_cast<std::byte>((size - 6) >> (24 - 8 * i));
        bytes[7 + i] = static_cast<std::byte>(sequence >> (24 - 8 * i));
    }
    bytes[6] = static_cast<std::byte>(reply);
    return bytes;
}

void append(transport::Peer& peer, const std::vector<std::byte>& bytes,
            std::size_t begin = 0, std::size_t count = SIZE_MAX) {
    count = (std::min)(count, bytes.size() - begin);
    require(peer.streamSize + count <= peer.stream.size(), "test ingress fits");
    std::memcpy(peer.stream.data() + peer.streamSize, bytes.data() + begin, count);
    peer.streamSize += count;
}

void reset() {
    auto& peer = transport::g_listener.peers[0];
    peer.streamSize = peer.outputSize = peer.outputOffset = 0;
    received.clear();
}

void buffered_contract() {
    reset();
    auto& peer = transport::g_listener.peers[0];
    for (std::uint32_t i = 0; i < 50; ++i) append(peer, frame(i));
    require(transport::drain_stream(0), "coalesced frames accepted");
    require(received.size() == 32, "one callback drains a fair bounded batch");
    require(peer.streamSize == 18 * 3528, "remaining frames retained");
    require(transport::drain_stream(0), "next slice drains remaining burst");
    require(received.size() == 50 && peer.streamSize == 0, "burst drained completely");
    for (std::uint32_t i = 0; i < 50; ++i)
        require(received[i] == i, "burst ordering preserved");

    reset();
    append(peer, frame(1));
    append(peer, frame(2, true));
    append(peer, frame(3));
    require(transport::drain_stream(0), "mixed request burst accepted");
    require(received.size() == 2 && peer.outputSize == 4, "reply stops ingress batch");
    require(transport::advance_output(peer, 1), "short write advances output");
    require(transport::drain_stream(0) && received.size() == 2,
            "partially sent reply blocks later request");
    require(!transport::advance_output(peer, 4), "oversized write rejected");
    require(transport::advance_output(peer, 3), "reply suffix completes");
    require(transport::drain_stream(0) && received.size() == 3 && received[2] == 3,
            "later request follows reply completion");

    reset();
    const auto split = frame(7);
    append(peer, frame(6));
    append(peer, split, 0, 4);
    require(transport::drain_stream(0) && received.size() == 1 && peer.streamSize == 4,
            "partial header survives previous complete frame");
    append(peer, split, 4, 19);
    require(transport::drain_stream(0) && received.size() == 1 && peer.streamSize == 23,
            "partial payload is retained");
    append(peer, split, 23);
    require(transport::drain_stream(0) && received.size() == 2 && received[1] == 7,
            "split payload offered exactly once");

    reset();
    append(peer, frame(9, false, transport::kStreamCapacity));
    require(transport::drain_stream(0) && received.size() == 1 && peer.streamSize == 0,
            "maximum supported frame is accepted without byte-budget starvation");
}

SOCKET connect_peer(std::uint16_t port) {
    const SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    require(socket != INVALID_SOCKET, "create loopback client");
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(connect(socket, reinterpret_cast<const sockaddr*>(&target), sizeof target) == 0,
            "connect ephemeral loopback listener");
    BOOL enabled = TRUE;
    require(setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&enabled), sizeof enabled) == 0,
            "disable test client Nagle delay");
    u_long nonblocking = 1;
    require(ioctlsocket(socket, FIONBIO, &nonblocking) == 0, "nonblocking test client");
    return socket;
}

void socket_burst() {
    reset();
    require(transport::initialize_on_port(0), "start real listener on ephemeral port");
    sockaddr_in address{};
    int length = sizeof address;
    require(getsockname(transport::g_listener.acceptor,
                        reinterpret_cast<sockaddr*>(&address), &length) == 0,
            "resolve ephemeral listener port");
    const SOCKET client = connect_peer(ntohs(address.sin_port));
    const SOCKET other = connect_peer(ntohs(address.sin_port));
    std::uint64_t tick = 1;
    transport::service(tick++);
    transport::service(tick++);
    std::vector<std::byte> pending;
    std::size_t sent{};
    for (std::uint32_t i = 0; i < 400; ++i) {
        const auto packet = frame(i);
        pending.insert(pending.end(), packet.begin(), packet.end());
    }
    const auto separate = frame(1000, true);
    require(send(other, reinterpret_cast<const char*>(separate.data()),
                 static_cast<int>(separate.size()), 0) == static_cast<int>(separate.size()),
            "second peer request queued");
    bool otherReceived = false;
    std::size_t otherBytes = 0;
    std::array<char, 4> reply{};
    for (unsigned slice = 0; slice < 250 && received.size() < 401; ++slice) {
        if (sent < pending.size()) {
            const auto count = send(client, reinterpret_cast<const char*>(pending.data() + sent),
                                    static_cast<int>(pending.size() - sent), 0);
            require(count > 0 || (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK),
                    "bulk send accepts or applies backpressure");
            if (count > 0) sent += static_cast<std::size_t>(count);
        }
        transport::service(tick * 16);
        ++tick;
        if (!otherReceived) {
            const auto count = recv(other, reply.data() + otherBytes,
                                    static_cast<int>(reply.size() - otherBytes), 0);
            require(count > 0 || (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK),
                    "second peer reply accepts or remains pending");
            if (count > 0) otherBytes += static_cast<std::size_t>(count);
            otherReceived = otherBytes == reply.size();
        }
        Sleep(1);
    }
    require(sent == pending.size() && received.size() == 401,
            "real socket burst drains faster than one frame per callback");
    require(otherReceived && polls != 0, "other peer and due polls remain serviced");
    std::uint32_t next = 0;
    for (const auto sequence : received) {
        if (sequence != 1000) require(sequence == next++, "socket order exactly preserved");
    }
    require(next == 400, "all heartbeat-sized frames delivered once");
    closesocket(client);
    closesocket(other);
    transport::shutdown();
}
} // namespace

namespace sunrise::core::log {
void write(Channel, Level, std::string_view) noexcept {}
}
namespace sunrise::core::settings {
const Settings& get() noexcept { static Settings settings{}; return settings; }
}
namespace sunrise::server::bap {
bool consume(const client::network::BapRequest& request,
             client::network::BapResponse& response) noexcept {
    response.size = 0;
    if (request.event == client::network::BapEvent::poll) ++polls;
    if (request.event != client::network::BapEvent::frame) return true;
    require(request.frame.size() >= 11, "route receives complete fixture");
    const auto sequence = read32(request.frame.subspan(7));
    received.push_back(sequence);
    if (request.frame[6] == std::byte{1}) {
        std::copy_n(request.frame.begin() + 7, 4, request.response.begin());
        response.size = 4;
    }
    return true;
}
}

int main(int argc, char** argv) {
    if (argc != 2 || std::strcmp(argv[1], "--socket-only") != 0) buffered_contract();
    socket_burst();
    std::printf("PASS: %u transport checks; coalesced ingress, reply ordering, partial frames, "
                "bounded fairness and real loopback throughput\n", checks);
}
