#include "adventure_native_bridge.h"
#include <mutex>
namespace dawn::server::runtime::activity::adventure::native_bridge {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const feedback::Ticket& t) noexcept {std::lock_guard lock(mutex);return mailbox.bind(t);}
bool clear(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
    std::lock_guard lock(mutex);return mailbox.clear(active,next);
}
bool advance(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
    std::lock_guard lock(mutex);return mailbox.advance(active,next);
}
bool pending(feedback::Owner owner) noexcept {std::lock_guard lock(mutex);return mailbox.pending(owner);}
void release(feedback::Owner owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
Binding lookup(std::uint32_t definition) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(definition);}
bool submit(const Event& e) noexcept {std::lock_guard lock(mutex);return mailbox.submit(e);}
std::size_t drain(feedback::Owner owner,std::span<Event> out) noexcept {
    std::lock_guard lock(mutex);return mailbox.drain(owner,out);
}
std::size_t drain(const feedback::Ticket& ticket,std::span<Event> out) noexcept {
    std::lock_guard lock(mutex);return mailbox.drain(ticket,out);
}
}
