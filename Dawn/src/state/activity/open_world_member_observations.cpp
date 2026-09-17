#include "open_world_member_observations.h"
#include <atomic>
#include <mutex>

namespace dawn::state::activity::open_world_members {
namespace {
std::mutex mutex;
Mailbox<256> mailbox;
std::atomic<bool> active{};
std::atomic<std::uint64_t> lost{};
}
void start() noexcept {
    active.store(false, std::memory_order_release);
    std::lock_guard lock(mutex);
    mailbox.clear();
    lost.store(0);
    active.store(true, std::memory_order_release);
}
void stop() noexcept {
    active.store(false, std::memory_order_release);
    std::lock_guard lock(mutex);
    mailbox.clear();
}
bool enabled() noexcept { return active.load(std::memory_order_acquire); }
std::uint64_t losses() noexcept { return lost.load(); }

void capture(events::Receipt receipt, const events::Event& event, MemberRef member) noexcept {
    if (!enabled()) return;
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) { ++lost; return; }
    if (!enabled()) return;
    if (event.kind != events::Kind::admitted || receipt.lease != event.lease) { ++lost; return; }
    std::size_t discarded{};
    const auto result = mailbox.offer({receipt, event.actor, event.sourceHandle, member}, &discarded);
    lost.fetch_add(discarded);
    if (result == Intake::invalid || result == Intake::conflict) ++lost;
}
Take take(events::Receipt receipt, const events::Event& event, Observation& output) noexcept {
    output = {};
    if (!enabled()) return Take::inactive;
    std::unique_lock lock(mutex, std::defer_lock);
    if (!detail::try_bounded([&lock]() noexcept { return lock.try_lock(); })) { ++lost; return Take::busy; }
    if (!enabled()) return Take::inactive;
    return mailbox.take(receipt, event, output) ? Take::found : Take::missing;
}
void discard(events::Receipt receipt, const events::Event& event) noexcept {
    Observation ignored;
    static_cast<void>(take(receipt, event, ignored));
}
void release(ActivityInstanceKey owner) noexcept {
    // Cleanup takes only this bounded, memory-only lock. It calls no native
    // mailbox API, so it cannot invert the authoritative receipt lock order.
    std::lock_guard lock(mutex);
    mailbox.release(owner);
}
void release_source(const coo::PopulationOwner& owner) noexcept {
    std::lock_guard lock(mutex);
    mailbox.release_source(owner);
}
}
