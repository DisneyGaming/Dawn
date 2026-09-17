#pragma once

#include <Windows.h>
#include <dxgi.h>
#include <array>
#include <atomic>
#include <cstdio>
#include "../../../core/logging/log.h"
#include "omega_frame_samples.h"

namespace dawn::client::hooks::graphics::omega_frame_timing {

enum class Kind : unsigned { graph, fullbody, scope_check, member_view, member_auth,
    character_owner, parent_context, biped_lookup, motion_pump, motion_view,
    crown_pump, crown_observe, eye_inputs, cannon_delivery, directive,
    graph_native, fullbody_native, count };
inline constexpr unsigned kWorkKinds=static_cast<unsigned>(Kind::count);
inline constexpr std::array<const char*,kWorkKinds> kWorkNames{
    "graph","fullbody","scope","member","auth","character","parent","biped",
    "motion","motion_view","crown","crown_observe","eye","cannons","directive","graph_native","fullbody_native"};
namespace detail {
inline std::atomic<std::uint64_t> scopeRun{}, expiredRun{};
inline std::atomic<std::uint32_t> scopePhase{};
struct WorkCounter {
    std::atomic<std::uint64_t> calls{}, ticks{}, maximum{};
};
inline std::array<WorkCounter, kWorkKinds> work{};
inline std::atomic_flag presentWriter = ATOMIC_FLAG_INIT;
inline FrameWindow window;
inline std::atomic<std::uint64_t> droppedPresents{};
[[nodiscard]] inline std::uint64_t now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) && value.QuadPart > 0
        ? static_cast<std::uint64_t>(value.QuadPart) : 0;
}
[[nodiscard]] inline std::uint64_t frequency() noexcept {
    static const auto value = [] {
        LARGE_INTEGER result{};
        return QueryPerformanceFrequency(&result) && result.QuadPart > 0
            ? static_cast<std::uint64_t>(result.QuadPart) : std::uint64_t{};
    }();
    return value;
}
[[nodiscard]] inline std::uint64_t active_run() noexcept {
    const auto run = scopeRun.load(std::memory_order_relaxed);
    return run && expiredRun.load(std::memory_order_relaxed) != run ? run : 0;
}
inline void maximum(std::atomic<std::uint64_t>& counter, std::uint64_t value) noexcept {
    auto current = counter.load(std::memory_order_relaxed);
    // Diagnostic maximum is best effort under contention; never spin in game work.
    for (unsigned retry = 0; retry < 4 && value > current; ++retry)
        if (counter.compare_exchange_weak(current, value, std::memory_order_relaxed)) break;
}
struct WorkSample { std::uint64_t calls{}, ticks{}, maximum{}; };
[[nodiscard]] inline WorkSample drain(WorkCounter& counter) noexcept {
    return {counter.calls.exchange(0, std::memory_order_relaxed),
            counter.ticks.exchange(0, std::memory_order_relaxed),
            counter.maximum.exchange(0, std::memory_order_relaxed)};
}
inline void mode(IDXGISwapChain* chain, std::uint64_t run) noexcept {
    DXGI_SWAP_CHAIN_DESC swap{};
    DXGI_OUTPUT_DESC output{};
    DEVMODEW display{};
    display.dmSize = sizeof display;
    IDXGIOutput* monitor{};
    const bool described = SUCCEEDED(chain->GetDesc(&swap));
    bool outputKnown{}, displayKnown{};
    if (SUCCEEDED(chain->GetContainingOutput(&monitor)) && monitor) {
        outputKnown = SUCCEEDED(monitor->GetDesc(&output));
        if (outputKnown) displayKnown = EnumDisplaySettingsW(output.DeviceName, ENUM_CURRENT_SETTINGS, &display) != FALSE;
        monitor->Release();
    }
    std::array<char, 384> line{};
    const int length = std::snprintf(line.data(), line.size(),
        "ev=omega_frame_mode run=%llu chain=%p described=%u windowed=%u buffer=%ux%u "
        "swap_refresh=%u/%u output_known=%u display_known=%u display=%lux%lu display_hz=%lu source=dxgi_and_windows",
        static_cast<unsigned long long>(run), static_cast<void*>(chain), described ? 1U : 0U,
        swap.Windowed ? 1U : 0U, swap.BufferDesc.Width, swap.BufferDesc.Height,
        swap.BufferDesc.RefreshRate.Numerator, swap.BufferDesc.RefreshRate.Denominator,
        outputKnown ? 1U : 0U, displayKnown ? 1U : 0U,
        display.dmPelsWidth, display.dmPelsHeight, display.dmDisplayFrequency);
    if (length > 0 && static_cast<std::size_t>(length) < line.size())
        core::log::write(core::log::Channel::client, core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
}
} // namespace detail

// These scope updates contain only atomics and are safe while the reveal owner
// holds its own lock. Repeating a phase or clearing/reopening the same run does
// not reset FrameWindow's600-second budget.
inline void set_scope(std::uint64_t run, std::uint32_t phase) noexcept {
    if (!run || run == UINT64_MAX) return;
    detail::scopePhase.store(phase, std::memory_order_relaxed);
    detail::scopeRun.store(run, std::memory_order_release);
}
inline void clear_scope() noexcept { detail::scopeRun.store(0, std::memory_order_release); }

// Place after the original callback, before the active/identity filters. Totals
// then cover all Dawn graph/fullbody postwork, including early-return checks.
class PostSpan final {
public:
    explicit PostSpan(Kind kind) noexcept : run_(detail::active_run()), kind_(kind) {
        if (run_) started_ = detail::now();
    }
    ~PostSpan() noexcept {
        if (!started_ || detail::active_run() != run_) return;
        const auto ended = detail::now();
        if (ended < started_) return;
        auto& counter = detail::work[static_cast<unsigned>(kind_)];
        counter.calls.fetch_add(1, std::memory_order_relaxed);
        counter.ticks.fetch_add(ended - started_, std::memory_order_relaxed);
        detail::maximum(counter.maximum, ended - started_);
    }
    PostSpan(const PostSpan&) = delete;
    PostSpan& operator=(const PostSpan&) = delete;
private:
    std::uint64_t run_{}, started_{};
    Kind kind_{};
};

class PresentSample final {
public:
    explicit PresentSample(UINT flags) noexcept {
        if ((flags & DXGI_PRESENT_TEST) != 0) return;
        input_.run = detail::active_run();
        if (!input_.run) return;
        input_.phase = detail::scopePhase.load(std::memory_order_relaxed);
        input_.entry = detail::now();
    }
    void before_native() noexcept { if (input_.entry) input_.nativeBegin = detail::now(); }
    void after_native(HRESULT result) noexcept {
        if (!input_.entry) return;
        input_.nativeEnd = detail::now();
        input_.success = result == S_OK;
    }
    void finish(IDXGISwapChain* chain, bool selected, UINT syncInterval) noexcept {
        if (!selected || !input_.entry || detail::active_run() != input_.run) return;
        if (detail::presentWriter.test_and_set(std::memory_order_acquire)) {
            detail::droppedPresents.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        input_.chain = reinterpret_cast<std::uintptr_t>(chain);
        input_.syncInterval = syncInterval;
        const auto frequency = detail::frequency();
        const auto result = detail::window.push(input_, frequency);
        std::array<detail::WorkSample, kWorkKinds> work{};
        if (result.ready || result.expired || result.windowChanged)
            for (unsigned i = 0; i < work.size(); ++i) work[i] = detail::drain(detail::work[i]);
        const auto dropped = result.ready
            ? detail::droppedPresents.exchange(0, std::memory_order_relaxed) : 0;
        if (result.expired) detail::expiredRun.store(input_.run, std::memory_order_relaxed);
        detail::presentWriter.clear(std::memory_order_release);
        // Neither COM/Win32 discovery nor logging holds the sampling guard or a
        // renderer/hook lock. Output discovery runs once per run/selected chain.
        if (result.modeChanged) detail::mode(chain, input_.run);
        if (!result.ready || !frequency) return;
        const auto& frame = result.report;
        const double toMs = 1000. / static_cast<double>(frequency);
        const double fps = frame.intervalTicks
            ? static_cast<double>(frame.intervals) * static_cast<double>(frequency) / static_cast<double>(frame.intervalTicks) : 0.;
        std::array<char, 960> line{};
        const int length = std::snprintf(line.data(), line.size(),
            "ev=omega_frame_timing run=%llu phase=%u presents=%llu failed=%llu elapsed_ms=%.2f "
            "fps=%.2f frame_ms=%.3f..%.3f sync=%u..%u present_total_ms=%.3f present_max_ms=%.3f "
            "pre_present_total_ms=%.3f pre_present_max_ms=%.3f "
            "graph_post_calls=%llu graph_post_total_ms=%.3f graph_post_max_ms=%.3f "
            "fullbody_post_calls=%llu fullbody_post_total_ms=%.3f fullbody_post_max_ms=%.3f "
            "dropped_samples=%llu source=selected_successful_present budget_s=600",
            static_cast<unsigned long long>(input_.run), input_.phase,
            static_cast<unsigned long long>(frame.presents), static_cast<unsigned long long>(frame.failed),
            static_cast<double>(frame.elapsed) * toMs, fps,
            static_cast<double>(frame.intervals ? frame.minimum : 0) * toMs,
            static_cast<double>(frame.maximum) * toMs, frame.syncMinimum, frame.syncMaximum,
            static_cast<double>(frame.presentTicks) * toMs, static_cast<double>(frame.presentMaximum) * toMs,
            static_cast<double>(frame.prePresentTicks) * toMs, static_cast<double>(frame.prePresentMaximum) * toMs,
            static_cast<unsigned long long>(work[0].calls), static_cast<double>(work[0].ticks) * toMs,
            static_cast<double>(work[0].maximum) * toMs,
            static_cast<unsigned long long>(work[1].calls), static_cast<double>(work[1].ticks) * toMs,
            static_cast<double>(work[1].maximum) * toMs, static_cast<unsigned long long>(dropped));
        if (length > 0 && static_cast<std::size_t>(length) < line.size())
            core::log::write(core::log::Channel::client, core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        std::array<char,2048> breakdown{};
        auto used=std::snprintf(breakdown.data(),breakdown.size(),"ev=omega_work_timing run=%llu phase=%u inclusive=1 format=calls,total_ms,max_ms",
            static_cast<unsigned long long>(input_.run),input_.phase);
        for(unsigned i=2;i<work.size() && used>0;++i) {
            const auto remaining=breakdown.size()-static_cast<std::size_t>(used);
            const auto n=std::snprintf(breakdown.data()+used,remaining," %s=%llu,%.3f,%.3f",kWorkNames[i],
                static_cast<unsigned long long>(work[i].calls),static_cast<double>(work[i].ticks)*toMs,
                static_cast<double>(work[i].maximum)*toMs);
            if(n<0 || static_cast<std::size_t>(n)>=remaining) {used=-1;break;}
            used+=n;
        }
        if(used>0) core::log::write(core::log::Channel::client,core::log::Level::info,
            {breakdown.data(),static_cast<std::size_t>(used)});
    }
private:
    FrameInput input_{};
};
} // namespace dawn::client::hooks::graphics::omega_frame_timing
