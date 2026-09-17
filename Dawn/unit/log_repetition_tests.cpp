#include <cstdint>
#include <iostream>
#include <string>

#include "../src/client/hooks/retail_log/channel_name_repetition.h"
#include "../src/server/gameplay/peer/traffic_log_repetition.h"

namespace logs = dawn::core::log;
namespace retail = dawn::client::hooks::retail_log;
namespace traffic = dawn::server::gameplay::peer;

namespace {
int failures{};
int checks{};
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << label << '\n';
    }
}

std::string name_change(std::string oldName, std::string newName,
                        std::string endpoint = "127.0.0.1:30976:127.0.0.1:30976") {
    return "networking:channel: Channel name change from " + endpoint + "#" + oldName
           + " to " + endpoint + " HASHMARK" + (newName.empty() ? "" : " " + newName);
}

void repetition_counter() {
    logs::RepetitionCounter counter{};
    check(counter.observe(0).emit, "first observation at tick zero");
    for (std::uint64_t tick = 1; tick < 30'000; ++tick) {
        check(!counter.observe(tick).emit, "unchanged observation is counted");
    }
    auto summary = counter.observe(30'000);
    check(summary.emit && summary.suppressed == 29'999 && summary.windowMs == 30'000,
          "summary counts omitted observations exactly");
    check(!counter.observe(30'001).emit, "new summary window");
    summary = counter.observe(30'002, false);
    check(summary.emit && summary.suppressed == 1 && summary.windowMs == 2,
          "meaningful transition flushes omitted counts immediately");
    check(counter.observe(0).emit, "backwards clock fails open");
}

void native_names() {
    const auto empty = name_change("<UNKNOWN PEER NAME>", "");
    const auto unknown = name_change("", "<UNKNOWN PEER NAME>");
    retail::ChannelNameRepetition filter{};
    check(retail::is_placeholder_name_change(empty), "native unknown-to-empty format");
    check(retail::is_placeholder_name_change(unknown), "native empty-to-unknown format");
    check(filter.observe(219, empty, 0).emit, "first empty label is preserved");
    check(filter.observe(219, unknown, 16).emit, "first unknown label is preserved");
    std::uint64_t omitted = 0;
    for (std::uint64_t tick = 32; tick < 30'000; tick += 16) {
        const auto result = filter.observe(219, (tick / 16) % 2 == 0 ? empty : unknown, tick);
        check(!result.emit, "alternating label churn is bounded independently");
        ++omitted;
    }
    const auto a = filter.observe(219, empty, 30'000);
    const auto b = filter.observe(219, unknown, 30'016);
    check(a.emit && b.emit && a.suppressed + b.suppressed == omitted,
          "both directions report their exact omitted counts");
    check(filter.observe(219, name_change("", "<UNKNOWN PEER NAME>", "10.0.0.1:10"),
                         30'017).emit, "new endpoint is visible");
    check(filter.observe(220, empty, 30'018).emit, "new native site is visible");
    const auto actualName = name_change("<UNKNOWN PEER NAME>", "Guardian");
    check(!retail::is_placeholder_name_change(actualName), "actual name excluded");
    check(filter.observe(219, actualName, 30'019).emit
              && filter.observe(219, actualName, 30'020).emit,
          "every actual name observation passes");
    check(filter.observe(219, empty, 30'021).emit,
          "return to placeholders after actual name is visible");
    const auto changedEndpoint =
        "networking:channel: Channel name change from 127.0.0.1:1# to "
        "127.0.0.1:2 HASHMARK <UNKNOWN PEER NAME>";
    check(!retail::is_placeholder_name_change(changedEndpoint), "endpoint change excluded");
    check(!retail::is_placeholder_name_change(empty + " error=lost"), "unexpected tail excluded");
    check(!retail::is_placeholder_name_change(name_change("", "")), "other transitions excluded");
    for (std::uint64_t tick = 0; tick < 10; ++tick) {
        check(filter.observe(219, "networking:channel: failed to connect", tick).emit,
              "unrelated native failures never suppressed");
    }
    for (int port = 1; port < 24; ++port) {
        check(filter.observe(219, name_change("", "<UNKNOWN PEER NAME>",
                                             "127.0.0.1:" + std::to_string(port)),
                             31'000).emit, "bounded table fails open on new endpoints");
    }
}

void gameplay() {
    const auto inbound = [](unsigned cursor, unsigned entries, std::size_t large,
                            std::size_t small, std::size_t dropped, std::size_t delivered,
                            std::size_t retired, std::size_t queued) {
        return traffic::routine_inbound(true, true, cursor, entries, large, small,
                                         dropped, delivered, retired, queued);
    };
    check(inbound(1, 0, 0, 0, 0, 0, 0, 0), "pure empty inbound qualifies");
    check(!inbound(0, 0, 0, 0, 0, 0, 0, 0), "unexpected cursor retained");
    check(!inbound(1, 1, 0, 0, 0, 0, 0, 0), "ack entries retained");
    check(!inbound(1, 0, 1, 0, 0, 0, 0, 0), "large records retained");
    check(!inbound(1, 0, 0, 1, 0, 0, 0, 0), "small records retained");
    check(!inbound(1, 0, 0, 0, 1, 0, 0, 0), "dropped records retained");
    check(!inbound(1, 0, 0, 0, 0, 1, 0, 0), "delivery retained");
    check(!inbound(1, 0, 0, 0, 0, 0, 1, 0), "queue retirement retained");
    check(!inbound(1, 0, 0, 0, 0, 0, 0, 1), "pending queue evidence retained");
    check(!traffic::routine_inbound(false, true, 1, 0, 0, 0, 0, 0, 0, 0),
          "establishment retained");
    check(!traffic::routine_inbound(true, false, 1, 0, 0, 0, 0, 0, 0, 0),
          "missing packet head retained");
    check(traffic::routine_outbound(true, 0, 0, 6), "six-byte empty reply qualifies");
    check(!traffic::routine_outbound(false, 0, 0, 6), "send failure retained");
    check(!traffic::routine_outbound(true, 1, 1, 38), "payload send retained");
    check(!traffic::routine_outbound(true, 0, 1, 6), "empty send with stalled queue retained");
    check(!traffic::routine_outbound(true, 0, 0, 7), "other reply size retained");

    traffic::TrafficLogRepetition filter{};
    traffic::TrafficLogKey key{0x7F000001, 30976, 1, 0, 2, true, false};
    check(filter.observe(key, 1022, false, 0).emit, "payload before idle retained");
    check(filter.observe(key, 1023, true, 16).emit, "first idle after payload retained");
    check(!filter.observe(key, 0, true, 32).emit, "ten-bit sequence wraps normally");
    auto result = filter.observe(key, 2, true, 48);
    check(result.emit && result.suppressed == 1, "sequence gap retained with omitted count");
    check(filter.observe(key, 2, true, 64).emit, "duplicate sequence retained");
    check(filter.observe(key, 1, true, 80).emit, "regressing sequence retained");
    check(filter.observe(key, 2, false, 96).emit, "failure or payload always passes");
    key.outbound = true;
    check(filter.observe(key, 3, true, 112).emit, "direction tracked independently");
    key.applicationReady = false;
    check(filter.observe(key, 4, true, 128).emit, "readiness transition retained");
    key.stage = 1;
    check(filter.observe(key, 5, true, 144).emit, "stage transition retained");
    key.localConnection = 2;
    check(filter.observe(key, 6, true, 160).emit, "reconnect retained");
    filter.clear();
    check(filter.observe(key, 7, true, 176).emit, "reset restores first occurrence");

    traffic::TrafficLogRepetition steady{};
    std::uint64_t emitted = 0;
    std::uint64_t suppressed = 0;
    for (std::uint64_t tick = 0; tick <= 60'000; tick += 16) {
        result = steady.observe(key, static_cast<std::uint16_t>((tick / 16) % 1024), true, tick);
        emitted += result.emit ? 1U : 0U;
        suppressed += result.suppressed;
    }
    check(emitted == 3 && suppressed == 3748, "60-second 62.5Hz replay emits first plus two summaries");
}
} // namespace

int main() {
    repetition_counter();
    native_names();
    gameplay();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
