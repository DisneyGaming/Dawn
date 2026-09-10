#include "public_event_participant_bridge.h"
#include <mutex>
namespace sunrise::server::runtime::activity::public_event::participant_bridge {
namespace {std::mutex mutex;Mailbox mailbox;IdentitySnapshot identity;std::uint64_t identitySequence{};}
void publish_local_identity(const feedback::LocalIdentity& value,std::uint64_t nowMs) noexcept {
    std::lock_guard lock(mutex);
    if(!feedback::valid(value) || !nowMs || identitySequence==UINT64_MAX){identity={};return;}
    identity={value,++identitySequence,nowMs};
}
void invalidate_local_identity() noexcept {std::lock_guard lock(mutex);identity={};}
IdentitySnapshot local_identity(std::uint64_t nowMs) noexcept {
    std::lock_guard lock(mutex);
    return identity.sequence && nowMs>=identity.observedAtMs && nowMs-identity.observedAtMs<=2000?identity:IdentitySnapshot{};
}
bool bind(const feedback::Ticket& ticket) noexcept {std::lock_guard lock(mutex);return mailbox.bind(ticket);}
Binding lookup(std::uint32_t definition) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(definition);}
bool submit(const Event& event) noexcept {std::lock_guard lock(mutex);return mailbox.submit(event);}
std::size_t drain(const feedback::Ticket& ticket,std::span<Event> output) noexcept {
    std::lock_guard lock(mutex);return mailbox.drain(ticket,output);
}
void release(feedback::Owner owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
}
