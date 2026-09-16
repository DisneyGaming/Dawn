#include "native_population_events.h"
#include <mutex>

namespace sunrise::state::activity::native_population {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const Lease& lease) noexcept {std::lock_guard lock(mutex);return mailbox.bind(lease);}
RenewResult renew(const Lease& prior,const Lease& next) noexcept {std::lock_guard lock(mutex);return mailbox.renew(prior,next);}
void release(ActivityInstanceKey owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
std::uint64_t epoch() noexcept {std::lock_guard lock(mutex);return mailbox.epoch();}
bool pending(ActivityInstanceKey owner) noexcept {std::lock_guard lock(mutex);return mailbox.pending(owner);}
bool pending_lease(const Lease& lease) noexcept {std::lock_guard lock(mutex);return mailbox.pending_lease(lease);}
bool quiescent(std::span<const Lease> leases) noexcept {std::lock_guard lock(mutex);return mailbox.quiescent(leases);}
Lease lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot,std::uint32_t generation) noexcept {
    std::lock_guard lock(mutex);return mailbox.lookup(definition,registry,slot,generation);
}
Receipt capture(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot,std::uint32_t generation) noexcept {
    std::lock_guard lock(mutex);return mailbox.capture(definition,registry,slot,generation);
}
Receipt capture(const Lease& lease) noexcept {std::lock_guard lock(mutex);return mailbox.capture(lease);}
Creation begin_creation() noexcept {std::lock_guard lock(mutex);return mailbox.begin_creation();}
Creation begin_creation(bool& observing) noexcept {
    std::lock_guard lock(mutex);observing=mailbox.epoch()!=0;return mailbox.begin_creation();
}
StageResult stage(Creation creation,const Event& event,Receipt& receipt) noexcept {
    std::lock_guard lock(mutex);return mailbox.stage(creation,event,receipt);
}
void cancel(Creation creation) noexcept {std::lock_guard lock(mutex);mailbox.cancel(creation);}
bool provisional(Receipt receipt,const Event& event) noexcept {
    std::lock_guard lock(mutex);return mailbox.provisional(receipt,event);
}
AdmitResult admit(Receipt receipt,const Event& event,bool publish) noexcept {
    std::lock_guard lock(mutex);return mailbox.admit(receipt,event,publish);
}
void unbind(const Lease& lease) noexcept {std::lock_guard lock(mutex);mailbox.unbind(lease);}
bool submit(const Event& event,Receipt receipt) noexcept {std::lock_guard lock(mutex);return mailbox.submit(event,receipt);}
bool submit(const Event& event,std::uint64_t epochValue) noexcept {std::lock_guard lock(mutex);return mailbox.submit(event,epochValue);}
void observation_lost() noexcept {std::lock_guard lock(mutex);mailbox.observation_lost();}
std::size_t drain(ActivityInstanceKey owner,std::span<Event> output,bool& overflow) noexcept {
    std::lock_guard lock(mutex);overflow=mailbox.overflow();return mailbox.drain(owner,output);
}
}
