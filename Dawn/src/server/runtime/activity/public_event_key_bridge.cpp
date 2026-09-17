#include "public_event_key_bridge.h"
#include <mutex>
namespace dawn::server::runtime::activity::public_event::keys::bridge {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const Ticket& t) noexcept {std::lock_guard lock(mutex);return mailbox.bind(t);}
State lookup(std::uint32_t d) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(d);}
State carrier(std::uint32_t d,std::uint32_t e) noexcept {std::lock_guard lock(mutex);return mailbox.carrier(d,e);}
State sink(std::uint32_t d,std::uint32_t e) noexcept {std::lock_guard lock(mutex);return mailbox.sink(d,e);}
State held(const State& s,std::uint32_t e) noexcept {std::lock_guard lock(mutex);return mailbox.held(s,e);}
bool created(const placement::Observation& o) noexcept {std::lock_guard lock(mutex);return mailbox.created(o);}
bool carry(const State& s,std::uint32_t c,std::uint32_t h,bool held) noexcept {std::lock_guard lock(mutex);return mailbox.carry(s,c,h,held);}
bool deposit(const Use& u) noexcept {std::lock_guard lock(mutex);return mailbox.deposit(u);}
void release(placement::Owner o) noexcept {std::lock_guard lock(mutex);mailbox.release(o);}
}
