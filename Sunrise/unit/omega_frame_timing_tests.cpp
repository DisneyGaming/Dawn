#include <cmath>
#include <iostream>
#include "client/hooks/graphics/omega_frame_timing.h"

namespace sunrise::core::log {
void write(Channel, Level, std::string_view) noexcept {}
}
namespace {
namespace timing = sunrise::client::hooks::graphics::omega_frame_timing;
unsigned checks{}, failures{};
void check(bool value, const char* message) {
    ++checks;
    if (!value) { ++failures; std::cerr << message << '\n'; }
}
timing::FrameInput frame(std::uint64_t entry, std::uint32_t phase = 1, std::uint64_t chain = 7,
                         bool success = true, std::uint64_t run = 1) {
    return {run, chain, entry, entry + 100, entry + 3100, phase, 0, success};
}
void cadence() {
    // A 30 kHz test clock gives exact30 and60 Hz intervals. The native AI tick
    // counts are deliberately absent from this input, as in the Present hook.
    for (const auto step : {1000ULL, 500ULL, 125ULL}) {
        timing::FrameWindow window;
        timing::FrameResult result;
        for (std::uint64_t entry = 100; entry <= 30100; entry += step) {
            result = window.push(frame(entry), 30000);
            if (entry < 30100) check(!result.ready, "no per-frame logging");
        }
        check(result.ready, "one-second sample completes");
        const auto& report = result.report;
        const auto expected = 30000ULL / step;
        check(report.intervals == expected && report.intervalTicks == 30000,
              "30/60/240 Hz use exact successful Present intervals");
        check(report.minimum == step && report.maximum == step, "frame interval range retained");
        check(report.presents == expected + 1 && report.failed == 0, "completed Present calls counted");
        check(report.presentTicks == report.presents * 3000 && report.prePresentTicks == report.presents * 100,
              "native Present wait and Sunrise pre-Present work stay separate");
    }
    timing::FrameWindow window;
    (void)window.push(frame(100), 30000);
    (void)window.push(frame(15100, 1, 7, false), 30000);
    auto last = frame(30100); last.syncInterval = 2;
    const auto result = window.push(last, 30000);
    check(result.ready && result.report.presents == 2 && result.report.failed == 1,
          "occluded/error status is not a successful displayed-frame claim");
    check(result.report.intervals == 1 && result.report.intervalTicks == 30000,
          "failure gap remains in successful Present cadence");
    check(result.report.syncMinimum == 0 && result.report.syncMaximum == 2,
          "actual sync interval changes remain visible");
}
void ownership_and_budget() {
    timing::FrameWindow window;
    auto initial = frame(100);
    check(window.push(initial, 30000).modeChanged, "first selected chain requests one mode query");
    check(!window.push(frame(1100), 30000).modeChanged, "same chain does not query mode each frame");
    check(!window.push(frame(3100, 2), 30000).ready, "phase change begins a clean window");
    check(window.push(frame(4100, 2, 8), 30000).modeChanged, "replacement selected chain refreshes mode");
    for (std::uint32_t second = 1; second < timing::kCaptureSeconds; ++second) {
        const auto result = window.push(frame(100 + 30000ULL * second, second % 5), 30000);
        check(!result.expired, "capture remains active inside its run budget");
    }
    check(window.push(frame(timing::kCaptureSeconds*30000+100, 1), 30000).expired, "phase changes cannot reset bounded budget");
    check(window.push(frame(timing::kCaptureSeconds*30000+30100, 4, 9), 30000).expired, "chain replacement cannot reset expired run");
    check(!window.push(frame(timing::kCaptureSeconds*30000+30100, 4, 9, true, 2), 30000).expired, "new mission receives fresh budget");
    auto invalid = frame(timing::kCaptureSeconds*30000+100100, 4, 9, true, 2); invalid.nativeBegin = invalid.entry - 1;
    check(!window.push(invalid, 30000).ready, "invalid QPC ordering cannot manufacture a duration");
    check(!window.push(frame(timing::kCaptureSeconds*30000+100100), 0).ready, "unavailable clock frequency fails closed");
}
void callback_cost() {
    auto drain = [] { for (auto& counter : timing::detail::work) (void)timing::detail::drain(counter); };
    drain();
    timing::clear_scope();
    { timing::PostSpan span(timing::Kind::graph); }
    check(timing::detail::work[0].calls.load() == 0, "inactive scope collects no callback work");
    timing::set_scope(77, 1);
    { timing::PostSpan span(timing::Kind::graph); }
    { timing::PostSpan span(timing::Kind::fullbody); }
    check(timing::detail::work[0].calls.load() == 1 && timing::detail::work[1].calls.load() == 1,
          "all postwork is separated by callback kind");
    { timing::PostSpan span(timing::Kind::graph); timing::clear_scope(); }
    check(timing::detail::work[0].calls.load() == 1, "scope teardown discards an unfinished sample");
    timing::detail::expiredRun.store(77);
    timing::set_scope(77, 3);
    { timing::PostSpan span(timing::Kind::graph); }
    check(timing::detail::work[0].calls.load() == 1, "repeated phase cannot rearm expired timing span");
    timing::set_scope(78, 1);
    constexpr unsigned iterations = 100000;
    const auto begin = timing::detail::now();
    for (unsigned i = 0; i < iterations; ++i) { timing::PostSpan span(timing::Kind::graph); }
    const auto ended = timing::detail::now();
    check(timing::detail::work[0].calls.load() == iterations + 1, "fresh run records every callback sample");
    const double nanoseconds = static_cast<double>(ended - begin) * 1.0e9
        / static_cast<double>(timing::detail::frequency()) / iterations;
    std::cout << "Empty active PostSpan benchmark: " << nanoseconds << " ns/call (local test process)\n";
    timing::clear_scope();
}
}
int main() {
    cadence(); ownership_and_budget(); callback_cost();
    std::cout << (failures ? "FAIL: " : "PASS: ") << checks << " frame timing checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
