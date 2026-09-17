#pragma once

#include <Windows.h>
#include <dxgi.h>
#include <array>
#include <atomic>
#include <cstdio>
#include "../../../core/logging/log.h"
#include "omega_frame_samples.h"

namespace dawn::client::hooks::graphics::hijacked_frame_timing {

enum class Kind : unsigned { update, capture, readback, world_step, placement_request, roster_apply, camera_update, post_present };
inline constexpr unsigned kWorkKinds=8;
inline constexpr std::array<const char*,kWorkKinds> kWorkNames{
    "update","capture","readback","world_step","placement_request","roster_apply","camera_update","post_present"};
namespace detail {
inline std::atomic<std::uint64_t> scopeRun{}, scopeEpoch{}, expiredRun{}, droppedPresents{};
struct WorkCounter { std::atomic<std::uint64_t> calls{}, ticks{}, maximum{}; };
struct WorkSample { std::uint64_t calls{}, ticks{}, maximum{}; };
inline std::array<WorkCounter,kWorkKinds> work{};
inline std::atomic_flag presentWriter=ATOMIC_FLAG_INIT;
inline omega_frame_timing::FrameWindow window;
struct OutputCache {std::uint64_t run{},epoch{},chain{};HWND window{};};
inline OutputCache outputCache;
inline std::atomic_flag outputWriter=ATOMIC_FLAG_INIT;
// The short nonblocking guard only copies the cache. COM/window queries never
// hold it or presentWriter, including calls made from measured native callbacks.
[[nodiscard]] inline HWND output_window(std::uint64_t run,std::uint64_t epoch,std::uint64_t chain=0) noexcept {
    if(outputWriter.test_and_set(std::memory_order_acquire)) return nullptr;
    const auto cached=outputCache;
    outputWriter.clear(std::memory_order_release);
    return cached.run==run && cached.epoch==epoch && (!chain || cached.chain==chain)?cached.window:nullptr;
}
inline void refresh_output(IDXGISwapChain* chain,std::uint64_t run,std::uint64_t epoch) noexcept {
    DXGI_SWAP_CHAIN_DESC description{};
    const auto outputWindow=SUCCEEDED(chain->GetDesc(&description))?description.OutputWindow:nullptr;
    if(outputWriter.test_and_set(std::memory_order_acquire)) return;
    outputCache={run,epoch,reinterpret_cast<std::uintptr_t>(chain),outputWindow};
    outputWriter.clear(std::memory_order_release);
}
inline constexpr unsigned kSlowLimit=120;
struct SlowFrame {
    omega_frame_timing::FrameInput previous{};
    std::uint64_t epoch{},run{},failed{};
    DWORD thread{};
    unsigned captured{};
};
struct SlowGap {
    omega_frame_timing::FrameInput previous{};
    std::uint64_t failed{};
    DWORD thread{};
    unsigned ordinal{};
};
inline SlowFrame slowFrame; // Only the nonblocking Present writer accesses this state.
struct SlowWork {
    std::uint64_t run{},start{},end{},dropped{};
    DWORD thread{};
    unsigned ordinal{};
};
struct SlowWorkSlot {
    std::atomic_flag writer=ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> dropped{};
    SlowWork event{};
    std::uint64_t run{};
    unsigned captured{};
    bool pending{};
};
inline std::array<SlowWorkSlot,kWorkKinds> slowWork{};
[[nodiscard]] inline std::uint64_t now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) && value.QuadPart>0
        ?static_cast<std::uint64_t>(value.QuadPart):0;
}
[[nodiscard]] inline std::uint64_t frequency() noexcept {
    static const auto value=[] {
        LARGE_INTEGER result{};
        return QueryPerformanceFrequency(&result) && result.QuadPart>0
            ?static_cast<std::uint64_t>(result.QuadPart):std::uint64_t{};
    }();
    return value;
}
[[nodiscard]] inline std::uint64_t active_run() noexcept {
    const auto run=scopeRun.load(std::memory_order_relaxed);
    return run && expiredRun.load(std::memory_order_relaxed)!=run?run:0;
}
inline void maximum(std::atomic<std::uint64_t>& counter,std::uint64_t value) noexcept {
    auto current=counter.load(std::memory_order_relaxed);
    // Diagnostic accounting must never spin indefinitely in a native callback.
    for(unsigned retry=0;retry<4 && value>current;++retry)
        if(counter.compare_exchange_weak(current,value,std::memory_order_relaxed)) break;
}
[[nodiscard]] inline WorkSample drain(WorkCounter& counter) noexcept {
    return {counter.calls.exchange(0,std::memory_order_relaxed),
        counter.ticks.exchange(0,std::memory_order_relaxed),
        counter.maximum.exchange(0,std::memory_order_relaxed)};
}
// Failed Presents do not become cadence endpoints. As in FrameWindow, the next
// successful entry spans intervening failures, which the event names explicitly.
[[nodiscard]] inline SlowGap gap(const omega_frame_timing::FrameInput& input,
    std::uint64_t frequency,std::uint64_t epoch,bool windowChanged,DWORD thread,bool foreground) noexcept {
    if(slowFrame.run!=input.run) {slowFrame={};slowFrame.run=input.run;}
    if(windowChanged || slowFrame.epoch!=epoch || slowFrame.previous.chain!=input.chain) {
        slowFrame.previous={};slowFrame.failed=0;slowFrame.epoch=epoch;
    }
    // Do not spend foreground evidence on the game's background frame cap, or
    // bridge an alt-tab pause into the first foreground interval.
    if(!foreground) {slowFrame.previous={};slowFrame.failed=0;return {};}
    if(slowFrame.previous.entry && input.entry<slowFrame.previous.entry) return {};
    if(!input.success) {++slowFrame.failed;return {};}
    SlowGap result{};
    const auto threshold=frequency/50U+(frequency%50U!=0); // ceil(20 ms in QPC ticks)
    if(slowFrame.previous.entry && input.entry-slowFrame.previous.entry>=threshold
        && slowFrame.captured<kSlowLimit) {
        result={slowFrame.previous,slowFrame.failed,slowFrame.thread,++slowFrame.captured};
    }
    slowFrame.previous=input;slowFrame.thread=thread;slowFrame.failed=0;
    return result;
}
inline void capture_work(unsigned kind,std::uint64_t run,std::uint64_t epoch,std::uint64_t start,std::uint64_t end) noexcept {
    const auto ticksPerSecond=frequency();
    if(!ticksPerSecond || end-start<ticksPerSecond/100U+(ticksPerSecond%100U!=0)) return;
    const auto outputWindow=output_window(run,epoch);
    if(!outputWindow || GetForegroundWindow()!=outputWindow) return;
    auto& slot=slowWork[kind];
    if(slot.writer.test_and_set(std::memory_order_acquire)) {
        slot.dropped.fetch_add(1,std::memory_order_relaxed);return;
    }
    if(slot.run!=run) {slot.run=run;slot.captured=0;slot.pending=false;slot.dropped.store(0,std::memory_order_relaxed);}
    if(slot.captured<kSlowLimit) {
        if(slot.pending) slot.dropped.fetch_add(1,std::memory_order_relaxed);
        slot.event={run,start,end,0,GetCurrentThreadId(),++slot.captured};slot.pending=true;
    }
    slot.writer.clear(std::memory_order_release);
}
[[nodiscard]] inline SlowWork drain_slow(unsigned kind,std::uint64_t run) noexcept {
    auto& slot=slowWork[kind];
    if(slot.writer.test_and_set(std::memory_order_acquire)) return {};
    SlowWork result{};
    if(slot.pending && slot.run==run) {
        result=slot.event;result.dropped=slot.dropped.exchange(0,std::memory_order_relaxed);slot.pending=false;
    }
    slot.writer.clear(std::memory_order_release);return result;
}
} // namespace detail

// Scope changes never acquire the renderer lock or reset a run's 600-second budget.
inline void set_scope(std::uint64_t run) noexcept {
    if(run && run!=UINT64_MAX && detail::scopeRun.exchange(run,std::memory_order_acq_rel)!=run)
        detail::scopeEpoch.fetch_add(1,std::memory_order_release);
}
inline void clear_scope() noexcept {
    if(detail::scopeRun.exchange(0,std::memory_order_acq_rel)) detail::scopeEpoch.fetch_add(1,std::memory_order_release);
}

class PostSpan final {
public:
    explicit PostSpan(Kind kind) noexcept:run_(detail::active_run()),epoch_(detail::scopeEpoch.load(std::memory_order_acquire)),kind_(kind) {
        if(run_) started_=detail::now();
    }
    ~PostSpan() noexcept {
        if(!started_ || detail::active_run()!=run_ || detail::scopeEpoch.load(std::memory_order_acquire)!=epoch_) return;
        const auto ended=detail::now();
        if(ended<started_) return;
        auto& counter=detail::work[static_cast<unsigned>(kind_)];
        counter.calls.fetch_add(1,std::memory_order_relaxed);
        counter.ticks.fetch_add(ended-started_,std::memory_order_relaxed);
        detail::maximum(counter.maximum,ended-started_);
        detail::capture_work(static_cast<unsigned>(kind_),run_,epoch_,started_,ended);
    }
    PostSpan(const PostSpan&)=delete;
    PostSpan& operator=(const PostSpan&)=delete;
private:
    std::uint64_t run_{},started_{},epoch_{};
    Kind kind_{};
};

class PresentSample final {
public:
    explicit PresentSample(UINT flags) noexcept {
        if(flags & DXGI_PRESENT_TEST) return;
        input_.run=detail::active_run();
        if(input_.run) {epoch_=detail::scopeEpoch.load(std::memory_order_acquire);input_.entry=detail::now();}
    }
    [[nodiscard]] bool active() const noexcept {return input_.entry!=0;}
    void before_native() noexcept {if(input_.entry) input_.nativeBegin=detail::now();}
    void after_native(HRESULT result) noexcept {
        if(!input_.entry) return;
        input_.nativeEnd=detail::now();input_.success=result==S_OK;
    }
    void finish(IDXGISwapChain* chain,bool selected,UINT syncInterval) noexcept {
        // This span includes diagnostic formatting/logging. It only enqueues
        // its own slow event after reporting returns, for a later Present.
        const PostSpan postTiming(Kind::post_present);
        if(!selected || !input_.entry || detail::active_run()!=input_.run
            || detail::scopeEpoch.load(std::memory_order_acquire)!=epoch_) return;
        input_.chain=reinterpret_cast<std::uintptr_t>(chain);input_.syncInterval=syncInterval;
        const auto cachedWindow=detail::output_window(input_.run,epoch_,input_.chain);
        const bool foregroundSample=cachedWindow && GetForegroundWindow()==cachedWindow;
        if(detail::presentWriter.test_and_set(std::memory_order_acquire)) {
            detail::droppedPresents.fetch_add(1,std::memory_order_relaxed);return;
        }
        const auto frequency=detail::frequency();
        if(!frequency || !input_.chain || input_.nativeBegin<input_.entry || input_.nativeEnd<input_.nativeBegin) {
            detail::presentWriter.clear(std::memory_order_release);return;
        }
        const auto result=detail::window.push(input_,frequency);
        const auto thread=GetCurrentThreadId();
        const auto gap=result.expired?detail::SlowGap{}:detail::gap(input_,frequency,epoch_,result.windowChanged,thread,foregroundSample);
        std::array<detail::WorkSample,kWorkKinds> work{};
        std::array<detail::SlowWork,kWorkKinds> slowWork{};
        if(result.ready || result.expired || result.windowChanged)
            for(unsigned i=0;i<work.size();++i) work[i]=detail::drain(detail::work[i]);
        const auto dropped=result.ready?detail::droppedPresents.exchange(0,std::memory_order_relaxed):0;
        const auto droppedTotal=detail::droppedPresents.load(std::memory_order_relaxed);
        bool hasSlowWork{};
        if(!result.expired) for(unsigned i=0;i<slowWork.size();++i) {
            slowWork[i]=detail::drain_slow(i,input_.run);hasSlowWork=hasSlowWork || slowWork[i].ordinal!=0;
        }
        if(result.expired) detail::expiredRun.store(input_.run,std::memory_order_relaxed);
        detail::presentWriter.clear(std::memory_order_release);
        // Sampling and renderer locks are released. The caller retains the
        // graphics lifetime guard through reporting, including out-of-line calls.
        if(result.ready || result.windowChanged) detail::refresh_output(chain,input_.run,epoch_);
        if(!result.ready && !gap.ordinal && !hasSlowWork) return;
        const auto& frame=result.report;
        const double toMs=1000./static_cast<double>(frequency);
        const double fps=frame.intervalTicks
            ?static_cast<double>(frame.intervals)*static_cast<double>(frequency)/static_cast<double>(frame.intervalTicks):0.;
        const auto outputWindow=detail::output_window(input_.run,epoch_,input_.chain);
        const bool windowKnown=outputWindow!=nullptr;
        const bool foreground=windowKnown && GetForegroundWindow()==outputWindow;
        if(gap.ordinal) {
            std::array<char,880> slow{};
            const int length=std::snprintf(slow.data(),slow.size(),
                "ev=hijacked_slow_frame run=%llu n=%u qpc_freq=%llu qpc_start=%llu qpc_end=%llu gap_ms=%.3f "
                "previous_present_start=%llu previous_present_end=%llu previous_present_ms=%.3f "
                "current_present_start=%llu current_present_end=%llu current_present_ms=%.3f "
                "previous_tid=%lu tid=%lu failures_between=%llu dropped_since_report=%llu foreground=%u window_known=%u "
                "source=successful_entry_to_entry budget=120 foreground_budget=1 current_present_after_gap=1",
                static_cast<unsigned long long>(input_.run),gap.ordinal,static_cast<unsigned long long>(frequency),
                static_cast<unsigned long long>(gap.previous.entry),static_cast<unsigned long long>(input_.entry),
                static_cast<double>(input_.entry-gap.previous.entry)*toMs,
                static_cast<unsigned long long>(gap.previous.nativeBegin),static_cast<unsigned long long>(gap.previous.nativeEnd),
                static_cast<double>(gap.previous.nativeEnd-gap.previous.nativeBegin)*toMs,
                static_cast<unsigned long long>(input_.nativeBegin),static_cast<unsigned long long>(input_.nativeEnd),
                static_cast<double>(input_.nativeEnd-input_.nativeBegin)*toMs,gap.thread,thread,
                static_cast<unsigned long long>(gap.failed),static_cast<unsigned long long>(dropped+droppedTotal),
                foreground?1U:0U,windowKnown?1U:0U);
            if(length>0 && static_cast<std::size_t>(length)<slow.size())
                core::log::write(core::log::Channel::client,core::log::Level::info,{slow.data(),static_cast<std::size_t>(length)});
        }
        for(unsigned i=0;i<slowWork.size();++i) {
            const auto& sample=slowWork[i];if(!sample.ordinal) continue;
            std::array<char,512> slow{};
            const int length=std::snprintf(slow.data(),slow.size(),
                "ev=hijacked_slow_work run=%llu kind=%s n=%u qpc_freq=%llu qpc_start=%llu qpc_end=%llu elapsed_ms=%.3f "
                "tid=%lu dropped=%llu inclusive=1 budget=120_per_kind foreground_at_capture=1 source=deferred_latest",
                static_cast<unsigned long long>(sample.run),kWorkNames[i],sample.ordinal,static_cast<unsigned long long>(frequency),
                static_cast<unsigned long long>(sample.start),static_cast<unsigned long long>(sample.end),
                static_cast<double>(sample.end-sample.start)*toMs,sample.thread,static_cast<unsigned long long>(sample.dropped));
            if(length>0 && static_cast<std::size_t>(length)<slow.size())
                core::log::write(core::log::Channel::client,core::log::Level::info,{slow.data(),static_cast<std::size_t>(length)});
        }
        if(!result.ready) return;
        std::array<char,880> line{};
        const int length=std::snprintf(line.data(),line.size(),
            "ev=hijacked_frame_timing run=%llu presents=%llu failed=%llu elapsed_ms=%.2f fps=%.2f "
            "frame_avg_ms=%.3f frame_min_ms=%.3f frame_max_ms=%.3f sync=%u..%u present_total_ms=%.3f present_max_ms=%.3f "
            "pre_present_total_ms=%.3f pre_present_max_ms=%.3f "
            "update_calls=%llu update_total_ms=%.3f update_max_ms=%.3f "
            "capture_calls=%llu capture_total_ms=%.3f capture_max_ms=%.3f "
            "readback_calls=%llu readback_total_ms=%.3f readback_max_ms=%.3f "
            "dropped_samples=%llu foreground=%u window_known=%u inclusive_work=1 source=selected_successful_present budget_s=600",
            static_cast<unsigned long long>(input_.run),static_cast<unsigned long long>(frame.presents),
            static_cast<unsigned long long>(frame.failed),static_cast<double>(frame.elapsed)*toMs,fps,
            frame.intervals?static_cast<double>(frame.intervalTicks)*toMs/static_cast<double>(frame.intervals):0.,
            static_cast<double>(frame.intervals?frame.minimum:0)*toMs,static_cast<double>(frame.maximum)*toMs,
            frame.syncMinimum,frame.syncMaximum,static_cast<double>(frame.presentTicks)*toMs,
            static_cast<double>(frame.presentMaximum)*toMs,static_cast<double>(frame.prePresentTicks)*toMs,
            static_cast<double>(frame.prePresentMaximum)*toMs,
            static_cast<unsigned long long>(work[0].calls),static_cast<double>(work[0].ticks)*toMs,static_cast<double>(work[0].maximum)*toMs,
            static_cast<unsigned long long>(work[1].calls),static_cast<double>(work[1].ticks)*toMs,static_cast<double>(work[1].maximum)*toMs,
            static_cast<unsigned long long>(work[2].calls),static_cast<double>(work[2].ticks)*toMs,static_cast<double>(work[2].maximum)*toMs,
            static_cast<unsigned long long>(dropped),foreground?1U:0U,windowKnown?1U:0U);
        if(length>0 && static_cast<std::size_t>(length)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(length)});
        std::array<char,640> extra{};
        const int extraLength=std::snprintf(extra.data(),extra.size(),
            "ev=hijacked_work_timing run=%llu qpc_end=%llu qpc_freq=%llu format=calls,total_ms,max_ms "
            "world_step=%llu,%.3f,%.3f placement_request=%llu,%.3f,%.3f roster_apply=%llu,%.3f,%.3f camera_update=%llu,%.3f,%.3f post_present=%llu,%.3f,%.3f inclusive=1",
            static_cast<unsigned long long>(input_.run),static_cast<unsigned long long>(input_.nativeEnd),static_cast<unsigned long long>(frequency),
            static_cast<unsigned long long>(work[3].calls),static_cast<double>(work[3].ticks)*toMs,static_cast<double>(work[3].maximum)*toMs,
            static_cast<unsigned long long>(work[4].calls),static_cast<double>(work[4].ticks)*toMs,static_cast<double>(work[4].maximum)*toMs,
            static_cast<unsigned long long>(work[5].calls),static_cast<double>(work[5].ticks)*toMs,static_cast<double>(work[5].maximum)*toMs,
            static_cast<unsigned long long>(work[6].calls),static_cast<double>(work[6].ticks)*toMs,static_cast<double>(work[6].maximum)*toMs,
            static_cast<unsigned long long>(work[7].calls),static_cast<double>(work[7].ticks)*toMs,static_cast<double>(work[7].maximum)*toMs);
        if(extraLength>0 && static_cast<std::size_t>(extraLength)<extra.size())
            core::log::write(core::log::Channel::client,core::log::Level::info,{extra.data(),static_cast<std::size_t>(extraLength)});
    }
private:
    omega_frame_timing::FrameInput input_{};
    std::uint64_t epoch_{};
};
} // namespace dawn::client::hooks::graphics::hijacked_frame_timing
