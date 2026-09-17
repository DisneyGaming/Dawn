#include "native_activation_global_drop_fanout.h"

#include <utility>

namespace dawn::client::hooks::activity_lifecycle {

NativeActivationGlobalDropFanout::Dispatch::Dispatch(
    NativeActivationGlobalDropFanout& owner,
    const NativeActivationGlobalDropObserverTable& table,
    std::uint64_t generation) noexcept
    : owner_(&owner), table_(&table), generation_(generation) {
}

NativeActivationGlobalDropFanout::Dispatch::~Dispatch() noexcept {
    finish();
}

NativeActivationGlobalDropFanout::Dispatch::Dispatch(Dispatch&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      table_(std::exchange(other.table_, nullptr)),
      generation_(std::exchange(other.generation_, 0U)),
      preCalled_(std::exchange(other.preCalled_, false)),
      postCalled_(std::exchange(other.postCalled_, false)) {
}

NativeActivationGlobalDropFanout::Dispatch&
NativeActivationGlobalDropFanout::Dispatch::operator=(Dispatch&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    finish();
    owner_ = std::exchange(other.owner_, nullptr);
    table_ = std::exchange(other.table_, nullptr);
    generation_ = std::exchange(other.generation_, 0U);
    preCalled_ = std::exchange(other.preCalled_, false);
    postCalled_ = std::exchange(other.postCalled_, false);
    return *this;
}

void NativeActivationGlobalDropFanout::Dispatch::notify_pre(
    const NativeActivationGlobalDropCohort& cohort) noexcept {
    if (owner_ == nullptr || table_ == nullptr || preCalled_) {
        return;
    }
    preCalled_ = true;
    table_->pre(table_->context, cohort);
}

void NativeActivationGlobalDropFanout::Dispatch::notify_post(
    const NativeActivationGlobalDropCohort& cohort) noexcept {
    if (owner_ == nullptr || table_ == nullptr || postCalled_) {
        return;
    }
    postCalled_ = true;
    table_->post(table_->context, cohort);
}

void NativeActivationGlobalDropFanout::Dispatch::finish() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    NativeActivationGlobalDropFanout* const owner = std::exchange(owner_, nullptr);
    const std::uint64_t generation = std::exchange(generation_, 0U);
    table_ = nullptr;
    owner->release_dispatch(generation);
}

bool NativeActivationGlobalDropFanout::register_observer(
    const NativeActivationGlobalDropObserverTable* table,
    NativeActivationGlobalDropObserverHandle& output) noexcept {
    output = {};
    if (table == nullptr
        || table->abiVersion != kNativeActivationGlobalDropObserverAbiVersion
        || table->pre == nullptr || table->post == nullptr) {
        return false;
    }

    std::uint64_t observed = control_.load(std::memory_order_acquire);
    if (control_state(observed) != kFree) {
        return false;
    }
    const std::uint64_t current = control_generation(observed);
    const std::uint64_t reserved = make_control(current, kReserved);
    if (!control_.compare_exchange_strong(
            observed, reserved, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return false;
    }

    if (current == kMaximumObserverGeneration) {
        control_.store(make_control(current, kFree), std::memory_order_release);
        return false;
    }
    const std::uint64_t next = current + 1U;
    table_.store(table, std::memory_order_release);
    control_.store(make_control(next, kActive), std::memory_order_release);
    output.generation = next;
    return true;
}

bool NativeActivationGlobalDropFanout::unregister_observer(
    NativeActivationGlobalDropObserverHandle handle) noexcept {
    if (!handle) {
        return false;
    }
    return retire_current(handle.generation, true);
}

void NativeActivationGlobalDropFanout::accept() noexcept {
    accepting_.store(true, std::memory_order_release);
}

void NativeActivationGlobalDropFanout::quiesce() noexcept {
    accepting_.store(false, std::memory_order_release);
}

NativeActivationGlobalDropFanout::Dispatch NativeActivationGlobalDropFanout::acquire() noexcept {
    if (!accepting_.load(std::memory_order_acquire)) {
        return {};
    }

    const std::uint64_t control = control_.load(std::memory_order_acquire);
    const std::uint64_t generation = control_generation(control);
    if (generation == 0U || control_state(control) != kActive) {
        return {};
    }

    inFlight_.fetch_add(1U, std::memory_order_acq_rel);
    if (!accepting_.load(std::memory_order_acquire)
        || control_.load(std::memory_order_acquire) != control) {
        inFlight_.fetch_sub(1U, std::memory_order_release);
        return {};
    }

    const NativeActivationGlobalDropObserverTable* const table =
        table_.load(std::memory_order_acquire);
    if (table == nullptr
        || table->abiVersion != kNativeActivationGlobalDropObserverAbiVersion
        || table->pre == nullptr || table->post == nullptr) {
        inFlight_.fetch_sub(1U, std::memory_order_release);
        return {};
    }
    return Dispatch{*this, *table, generation};
}

bool NativeActivationGlobalDropFanout::clear_after_removed() noexcept {
    quiesce();
    return retire_current(0U, false);
}

bool NativeActivationGlobalDropFanout::idle() const noexcept {
    return inFlight_.load(std::memory_order_acquire) == 0U;
}

bool NativeActivationGlobalDropFanout::accepting() const noexcept {
    return accepting_.load(std::memory_order_acquire);
}

bool NativeActivationGlobalDropFanout::has_registration() const noexcept {
    return control_state(control_.load(std::memory_order_acquire)) != kFree
           || table_.load(std::memory_order_acquire) != nullptr;
}

void NativeActivationGlobalDropFanout::release_dispatch(std::uint64_t generation) noexcept {
    (void)generation;
    inFlight_.fetch_sub(1U, std::memory_order_release);
}

bool NativeActivationGlobalDropFanout::retire_current(std::uint64_t expectedGeneration,
                                                       bool requireGeneration) noexcept {
    std::uint64_t control = control_.load(std::memory_order_acquire);
    for (;;) {
        const std::uint64_t generation = control_generation(control);
        const std::uint64_t state = control_state(control);
        if (requireGeneration && generation != expectedGeneration) {
            return false;
        }
        if (state == kFree || state == kReserved) {
            return state == kFree && !requireGeneration;
        }
        if (state == kActive) {
            const std::uint64_t retiring = make_control(generation, kRetiring);
            if (!control_.compare_exchange_weak(
                    control, retiring, std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            control = retiring;
        } else if (state != kRetiring) {
            return false;
        }
        if (inFlight_.load(std::memory_order_acquire) != 0U) {
            return false;
        }
        const std::uint64_t clearing = make_control(generation, kClearing);
        if (!control_.compare_exchange_strong(
                control, clearing, std::memory_order_acq_rel, std::memory_order_acquire)) {
            continue;
        }
        table_.store(nullptr, std::memory_order_release);
        const std::uint64_t free = make_control(generation, kFree);
        control_.store(free, std::memory_order_release);
        return true;
    }
}

bool register_global_drop_observer(
    const NativeActivationGlobalDropObserverTable* table,
    NativeActivationGlobalDropObserverHandle& output) noexcept {
    return detail::native_activation_global_drop_fanout().register_observer(table, output);
}

bool unregister_global_drop_observer(
    NativeActivationGlobalDropObserverHandle handle) noexcept {
    return detail::native_activation_global_drop_fanout().unregister_observer(handle);
}

namespace detail {

NativeActivationGlobalDropFanout& native_activation_global_drop_fanout() noexcept {
    static NativeActivationGlobalDropFanout fanout{};
    return fanout;
}

__declspec(noinline) void notify_native_activation_global_drop_pre(
    NativeActivationGlobalDropFanout::Dispatch& dispatch,
    const NativeActivationGlobalDropCohort& cohort) noexcept {
    dispatch.notify_pre(cohort);
}

__declspec(noinline) void notify_native_activation_global_drop_post(
    NativeActivationGlobalDropFanout::Dispatch& dispatch,
    const NativeActivationGlobalDropCohort& cohort) noexcept {
    dispatch.notify_post(cohort);
}

} // namespace detail

} // namespace dawn::client::hooks::activity_lifecycle
