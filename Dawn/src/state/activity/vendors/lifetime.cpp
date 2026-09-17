#include "lifetime.h"
#include "../runtime.h"
#include <mutex>

namespace dawn::state::activity::vendors::lifetime {
namespace {
std::mutex mutex;std::array<State,64> states;
// Native lookup has registry/slot/generation, not an activity key. Give overlapping
// activity incarnations distinct generations while the old binding drains.
std::uint32_t generation{};
std::uint64_t publication{};
}
void prune() noexcept {
    std::lock_guard lock(mutex);
    for(auto& state:states) if(state.lease.activity && !activity::contains(state.lease.activity)) {
        native_population::unbind(state.lease);state={};
    }
}
bool prepare(ActivityInstanceKey owner,coo::Asset source,std::uint8_t bubble,Authority& out) noexcept {
    if(!owner || !activity::contains(owner) || source.type!=1 || bubble>63) return false;
    std::lock_guard lock(mutex);
    State* empty{};
    for(auto& state:states) {
        if(state.lease.activity==owner && state.lease.source.source==source) {
            if(state.lease.bubble!=bubble) return false;
            out={state.lease.source.generation,state.occupied,state.suspended};return true;
        }
        if(!state.lease.activity && !empty) empty=&state;
    }
    if(!empty || generation==0x7FFFFFFFU) return false;
    const native_population::Lease lease{owner,{owner.sessionId,1,owner.incarnation.value,source,generation+1},bubble};
    if(!native_population::bind(lease)) return false;
    ++generation;*empty={lease};empty->publication=++publication;out={generation,false};return true;
}
bool owns(const native_population::Lease& lease) noexcept {
    std::lock_guard lock(mutex);
    for(const auto& state:states) if(state.lease==lease && lease.activity) return true;
    return false;
}
bool admit(const native_population::Event& event) noexcept {
    if(event.kind!=native_population::Kind::admitted) return false;
    std::lock_guard lock(mutex);
    for(auto& state:states) if(state.lease==event.lease) {
        const bool occupied=state.occupied;
        if(!state.admit(event.actor)) return false;
        if(!occupied) state.publication=++publication;
        return true;
    }
    return false;
}
bool removed(const native_population::Event& event,native_population::Lease* lease) noexcept {
    std::lock_guard lock(mutex);
    for(auto& state:states) if(state.lease==event.lease) {
        if(generation==0x7FFFFFFFU) return false;
        auto next=state;if(!next.removed(event.actor,generation+1)) return false;
        native_population::unbind(state.lease);
        if(!native_population::bind(next.lease)) {
            static_cast<void>(native_population::bind(state.lease));return false;
        }
        ++generation;state=next;state.publication=++publication;if(lease) *lease=next.lease;return true;
    }
    return false;
}
bool resume(const native_population::Lease& lease) noexcept {
    std::lock_guard lock(mutex);
    for(auto& state:states) if(state.lease==lease && lease.activity && state.suspended && !state.occupied) {
        state.suspended=false;state.publication=++publication;return true;
    }
    return false;
}
std::uint64_t revision(ActivityInstanceKey owner) noexcept {
    std::lock_guard lock(mutex);std::uint64_t result{};
    for(const auto& state:states) if(state.lease.activity==owner && state.publication>result) result=state.publication;
    return result;
}
}
