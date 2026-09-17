#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace dawn::client::hooks::bootflow::forest_tuner {

/**
 * Shared dial between the Forest menu page and the generator tick hook. The panel writes
 * plain values; the hook copies every field whose write flag is set into the sensor's
 * authority record (presence-mask bits included) before every worker tick, repairing the
 * wire's empty-record overwrite before native change detection. All fields are relaxed atomics: a torn read costs
 * one cycle of a stale value, never a fault.
 */
struct Group {
    std::atomic<int> a{0};
    std::atomic<int> b{0};
    std::atomic<float> weight{1.0F};
    std::atomic<bool> active{false};
    std::atomic<bool> write{false};
};

struct State {
    /** Optional manual diagnostic override; server authority owns normal activation. */
    std::atomic<bool> enable{false};
    std::atomic<bool> writeSeed{false};
    std::atomic<int> seed{9001};
    std::atomic<bool> writeMode{false};
    std::atomic<int> mode{2};
    std::array<Group, 4> groups{};
    std::atomic<bool> writeFloats{false};
    std::atomic<float> f0{0.12F};
    std::atomic<float> f1{0.16F};
    std::atomic<bool> writeInts{false};
    std::atomic<int> i0{-1};
    std::atomic<int> i1{-1};
    /** Ticking worker instances get slots in tick order; each slot has its own force
     * request and entry-count display, so both segments' workers are addressable. */
    static constexpr std::size_t kWorkerSlots = 4;
    std::array<std::atomic<int>, kWorkerSlots> forceEntryPer{-1, -1, -1, -1};
    std::array<std::atomic<int>, kWorkerSlots> entryCountPer{};
    /** Application count, shown by the panel as liveness feedback. */
    std::atomic<std::uint32_t> applies{0};
};

/** @return The one shared dial. */
[[nodiscard]] State& state() noexcept;

/** @return True while a generator sensor instance exists to receive the dial. */
[[nodiscard]] bool sensor_present() noexcept;

} // namespace dawn::client::hooks::bootflow::forest_tuner
