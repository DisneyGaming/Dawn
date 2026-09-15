#include "native_population_events.h"
#include <mutex>
namespace sunrise::state::activity::native_population {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const Lease& lease) noexcept {std::lock_guard lock(mutex);return mailbox.bind(lease);}
void unbind(const Lease& lease) noexcept {std::lock_guard lock(mutex);mailbox.unbind(lease);}
void release(ActivityInstanceKey owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
std::uint64_t epoch() noexcept {std::lock_guard lock(mutex);return mailbox.epoch();}
bool pending(ActivityInstanceKey owner) noexcept {std::lock_guard lock(mutex);return mailbox.pending(owner);}
Lease lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot,std::uint32_t generation) noexcept {
    std::lock_guard lock(mutex);return mailbox.lookup(definition,registry,slot,generation);
}
bool submit(const Event& event,std::uint64_t epochValue) noexcept {std::lock_guard lock(mutex);return mailbox.submit(event,epochValue);}
void observation_lost() noexcept {std::lock_guard lock(mutex);mailbox.observation_lost();}
std::size_t drain(ActivityInstanceKey owner,std::span<Event> output,bool& overflow) noexcept {
    std::lock_guard lock(mutex);overflow=mailbox.overflow();return mailbox.drain(owner,output);
}
}
