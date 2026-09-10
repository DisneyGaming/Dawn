#include "public_event_engagement_bridge.h"
#include <mutex>
namespace sunrise::server::runtime::activity::public_event::engagement_bridge {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const feedback::Ticket& ticket) noexcept {std::lock_guard lock(mutex);return mailbox.bind(ticket);}
bool join(const feedback::Ticket& a,const feedback::Ticket& b) noexcept {std::lock_guard lock(mutex);return mailbox.join(a,b);}
Binding lookup(std::uint32_t definition) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(definition);}
bool submit(const Event& event) noexcept {std::lock_guard lock(mutex);return mailbox.submit(event);}
std::size_t drain(const feedback::Ticket& ticket,std::span<Event> output) noexcept {
    std::lock_guard lock(mutex);return mailbox.drain(ticket,output);
}
void release(feedback::Owner owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
}
