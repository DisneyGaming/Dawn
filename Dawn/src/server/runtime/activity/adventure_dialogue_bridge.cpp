#include "adventure_dialogue_bridge.h"
#include <mutex>
namespace dawn::server::runtime::activity::adventure::dialogue_bridge {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const feedback::Ticket& t) noexcept {std::lock_guard lock(mutex);return mailbox.bind(t);}
bool advance(const feedback::Ticket& a,const feedback::Ticket& b) noexcept {std::lock_guard lock(mutex);return mailbox.advance(a,b);}
Binding lookup(std::uint32_t definition) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(definition);}
bool submit(const Event& e) noexcept {std::lock_guard lock(mutex);return mailbox.submit(e);}
std::size_t drain(const feedback::Ticket& ticket,std::span<Event> out) noexcept {std::lock_guard lock(mutex);return mailbox.drain(ticket,out);}
void release(feedback::Owner owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
}
