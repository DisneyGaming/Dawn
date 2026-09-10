#include "public_event_native_bridge.h"
#include <mutex>

namespace sunrise::server::runtime::activity::public_event::native_bridge {
namespace { std::mutex mutex;Mailbox mailbox; }
bool bind(const feedback::Ticket& ticket) noexcept {std::lock_guard lock(mutex);return mailbox.bind(ticket);}
void release(state::activity::ActivityInstanceKey owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
Binding lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot) noexcept {
    std::lock_guard lock(mutex);return mailbox.lookup(definition,registry,slot);
}
Binding lookup_definition(std::uint32_t definition) noexcept {
    std::lock_guard lock(mutex);return mailbox.lookup_definition(definition);
}
bool submit(const Event& event) noexcept {std::lock_guard lock(mutex);return mailbox.submit(event);}
rally_use::Binding lookup_use(std::uint32_t definition,std::uint32_t entity) noexcept {
    std::lock_guard lock(mutex);return mailbox.lookup_use(definition,entity);
}
bool submit_use(const rally_use::Receipt& receipt) noexcept {std::lock_guard lock(mutex);return mailbox.submit_use(receipt);}
bool used(const Lease& lease) noexcept {std::lock_guard lock(mutex);return mailbox.used(lease);}
std::size_t drain(state::activity::ActivityInstanceKey owner,std::span<Event> output,bool& overflow) noexcept {
    std::lock_guard lock(mutex);overflow=mailbox.overflow();return mailbox.drain(owner,output);
}
} // namespace sunrise::server::runtime::activity::public_event::native_bridge
