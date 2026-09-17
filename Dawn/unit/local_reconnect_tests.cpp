#include "../src/client/hooks/bootflow/local_reconnect_policy.h"
#include "../src/middleware/gameplay/descriptor/join_descriptor.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace p = dawn::client::hooks::bootflow::local_reconnect;
namespace d = dawn::middleware::gameplay::descriptor;
static unsigned checks{};
static void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
int main() {
    p::Address expected{};
    d::write_net_addr(0x7F000001, 30976, expected);
    p::Snapshot before{4, 2, 9, 10000, 73000, 0, expected};
    p::Snapshot after{1, 2, 9, 10000, 163484, 1, expected};
    std::uint64_t result{};
    check(p::shortened_time(before, after, true, expected, result), "normal embedded close qualifies");
    check(result == 153734, "captured close leaves exactly 250 ms");
    std::printf("native_fixture_stamp=%llu\n", static_cast<unsigned long long>(result));
    auto waiting = before; waiting.managerState = 1;
    check(p::shortened_time(waiting, after, true, expected, result), "second owners-released reset qualifies");
    check(result == 153734, "second native reset cannot restore ten-second wait");
    std::printf("repeat_fixture_stamp=%llu\n", static_cast<unsigned long long>(result));
    const auto rejected = [&](const p::Snapshot& b, const p::Snapshot& a, bool ready = true) {
        std::uint64_t sentinel = 42;
        check(!p::shortened_time(b, a, ready, expected, sentinel), "unproven transition unchanged");
        check(sentinel == 42, "rejection does not publish a timestamp");
    };
    rejected(before, after, false); // Quiesce, external topology, unready endpoint.
    for (unsigned state = 0; state < 10; ++state) {
        auto b = before; b.managerState = state;
        if (state != 4 && state != 1) rejected(b, after);
        auto a = after; a.managerState = state;
        if (state != 1) rejected(before, a);
        b = before; b.channelState = state;
        if (state > 2) rejected(b, after);
    }
    for (unsigned reason = 0; reason < 256; ++reason) {
        auto b = before; b.closeReason = reason;
        if (reason != 9) rejected(b, after);
        b.managerState = 1;
        if (reason != 9) rejected(b, after);
    }
    for (unsigned retry = 0; retry < 256; ++retry) {
        auto a = after; a.retry = static_cast<std::uint8_t>(retry);
        if (retry != 1) rejected(before, a);
    }
    for (std::uint32_t cooldown : {0U, 1U, 249U, 250U, 9999U, 10001U, 0xFFFFFFFFU}) {
        auto b = before; b.cooldown = cooldown; rejected(b, after);
        auto a = after; a.cooldown = cooldown; rejected(before, a);
    }
    for (std::size_t byte = 0; byte < expected.size(); ++byte) {
        auto b = before; b.address[byte] ^= std::byte{1}; rejected(b, after);
        auto a = after; a.address[byte] ^= std::byte{1}; rejected(before, a);
    }
    for (std::uint64_t time = 0; time < 9750; ++time) {
        auto a = after; a.stateTime = time; rejected(before, a);
    }
    for (std::uint64_t time : {std::uint64_t{9750}, std::uint64_t{10000}, std::uint64_t{163484},
                               (std::numeric_limits<std::uint64_t>::max)()}) {
        auto a = after; a.stateTime = time;
        check(p::shortened_time(before, a, true, expected, result), "safe timestamp subtraction");
        check(time - result == 9750, "only reconnect wait changes");
        for (std::uint64_t elapsed = 0; elapsed <= 10000; ++elapsed)
            check((time - result + elapsed >= 10000) == (elapsed >= 250), "native elapsed threshold");
    }
    std::printf("PASS local reconnect: %u checks\n", checks);
}
