#include "public_event_deferred_placement_bridge.h"
#include <mutex>
namespace sunrise::server::runtime::activity::public_event::deferred_bridge {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const feedback::Ticket& value) noexcept {std::lock_guard lock(mutex);return mailbox.bind(value);}
State lookup(std::uint32_t definition) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(definition);}
bool created(const Binding& binding,const feedback::Observation& value) noexcept {std::lock_guard lock(mutex);return mailbox.created(binding,value);}
bool point_ready(const Binding& binding,const feedback::Observation& value,std::uint64_t sequence,std::uint64_t weak) noexcept {
    std::lock_guard lock(mutex);return mailbox.point_ready(binding,value,sequence,weak);
}
void release(feedback::Owner owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
}
