#pragma once
#include "../native_population_events.h"

namespace sunrise::state::activity::vendors::lifetime {
// This is an activity/source lifetime, not a bubble-visit latch. Native network
// identity must be gone before another request is permitted.
struct State {
    native_population::Lease lease{};
    coo::PopulationActor actor{};
    bool occupied{},suspended{};
    std::uint64_t publication{};
    bool admit(const coo::PopulationActor& value) noexcept {
        if(suspended || !value.valid() || value.owner!=lease.source) return false;
        if(occupied) return actor==value;
        actor=value;occupied=true;return true;
    }
    bool removed(const coo::PopulationActor& value,std::uint32_t generation) noexcept {
        if(!occupied || actor!=value || generation<=lease.source.generation || generation>0x7FFFFFFFU) return false;
        lease.source.generation=generation;actor={};occupied=false;suspended=true;return true;
    }
};
struct Authority {std::uint32_t generation{1};bool occupied{},suspended{};};
// Only the creator publisher registers these exact vendor sources. The existing
// native population observer supplies admissions; its actor-pool retirement
// event is deliberately insufficient to clear occupied.
bool prepare(ActivityInstanceKey,coo::Asset,std::uint8_t bubble,Authority&) noexcept;
bool owns(const native_population::Lease&) noexcept;
bool admit(const native_population::Event&) noexcept;
// Destruction rearms a new generation with zero requests. Only a reloaded native
// source that has accepted that generation can resume normal creation.
bool removed(const native_population::Event&,native_population::Lease* next=nullptr) noexcept;
bool resume(const native_population::Lease&) noexcept;
// Sample before preparing a roster and acknowledge only after successful send.
std::uint64_t revision(ActivityInstanceKey) noexcept;
void prune() noexcept;
}
