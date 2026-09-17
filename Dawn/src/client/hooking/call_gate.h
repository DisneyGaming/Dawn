#pragma once

#include <atomic>
#include <cstdint>

namespace dawn::client::hooking {

class CallGate;

namespace call_gate_detail {

/** Protected counter ingress/egress are out-of-line so their unwind ranges can be inspected. */
void enter(CallGate& gate) noexcept;
void leave(CallGate& gate) noexcept;

} // namespace call_gate_detail

/**
 * Coordinates non-blocking hook calls with protected detour removal.
 *
 * A replacement enters before it reads any lifecycle-owned pointer and keeps the scope alive
 * through the native trampoline call and all post-work. Quiescing disables Dawn-owned work but
 * deliberately does not stop the replacement from forwarding to the native function.
 */
class CallGate final {
public:
    class Scope final {
    public:
        __forceinline explicit Scope(CallGate& gate) noexcept
            : gate_(gate) {
            call_gate_detail::enter(gate_);
        }

        __forceinline ~Scope() noexcept {
            call_gate_detail::leave(gate_);
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        /** @return True when this call may perform Dawn-owned observation or mutation work. */
        [[nodiscard]] __forceinline bool accepts_side_effects() const noexcept {
            return gate_.accepting_.load(std::memory_order_acquire);
        }

    private:
        CallGate& gate_;
    };

    /** Allows newly entered calls to perform their scoped Dawn-owned work. */
    void accept() noexcept {
        accepting_.store(true, std::memory_order_release);
    }

    /** Stops newly entered calls doing Dawn-owned work while native forwarding remains live. */
    void quiesce() noexcept {
        accepting_.store(false, std::memory_order_release);
    }

    /** @return True when no replacement or directly protected sampler owns lifecycle state. */
    [[nodiscard]] bool idle() const noexcept {
        return activeCalls_.load(std::memory_order_acquire) == 0U;
    }

    /** @return True while this hook is running rather than retained in quiescing state. */
    [[nodiscard]] bool accepting() const noexcept {
        return accepting_.load(std::memory_order_acquire);
    }

    /** @return Number of full calls currently holding lifecycle-owned state. */
    [[nodiscard]] std::uint32_t active_calls() const noexcept {
        return activeCalls_.load(std::memory_order_acquire);
    }

private:
    friend void call_gate_detail::enter(CallGate& gate) noexcept;
    friend void call_gate_detail::leave(CallGate& gate) noexcept;

    std::atomic_bool accepting_{};
    std::atomic<std::uint32_t> activeCalls_{};
};

namespace call_gate_detail {

__declspec(noinline) inline void enter(CallGate& gate) noexcept {
    gate.activeCalls_.fetch_add(1U, std::memory_order_acq_rel);
}

__declspec(noinline) inline void leave(CallGate& gate) noexcept {
    gate.activeCalls_.fetch_sub(1U, std::memory_order_release);
}

} // namespace call_gate_detail

/**
 * Waits out the narrow Detours commit-to-caller publication window.
 *
 * Detours resumes enlisted threads before its install wrapper returns. A replacement can therefore
 * run before the hook owner stores the returned trampoline in its atomic slot. Once attached, the
 * owner is guaranteed to publish; waiting here preserves exactly-once native forwarding.
 */
template <class Function>
[[nodiscard]] Function await_original(std::atomic<Function>& slot) noexcept {
    Function original = slot.load(std::memory_order_acquire);
    while (original == nullptr) {
        slot.wait(Function{}, std::memory_order_acquire);
        original = slot.load(std::memory_order_acquire);
    }
    return original;
}

/** Publishes a committed Detours trampoline and wakes any replacement admitted during commit. */
template <class Function>
void publish_original(std::atomic<Function>& slot, Function original) noexcept {
    slot.store(original, std::memory_order_release);
    slot.notify_all();
}

} // namespace dawn::client::hooking
