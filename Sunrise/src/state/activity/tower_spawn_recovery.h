#pragma once

#include "lifecycle_generation.h"
#include <cstdint>
#include <mutex>

namespace sunrise::state::activity::tower_spawn_recovery {
inline constexpr std::uint32_t kScenario=0x80B4A0F4U;
inline constexpr std::uint32_t kRoot=0x40AEBF90U;

// A camera observation can request publication, but never calls native cleanup.
struct Request {
    ActivityInstanceKey activity{};
    std::uint64_t run{},revision{};
    std::uint32_t lifetime{UINT32_MAX};
};
inline std::mutex mutex;
inline Request pending;
inline Request request(ActivityInstanceKey activity,std::uint64_t run,std::uint32_t lifetime) noexcept {
    const std::lock_guard lock(mutex);
    if(!activity || !run || lifetime==UINT32_MAX || pending.revision==UINT64_MAX) {return {};}
    if(pending.activity==activity && pending.run==run && pending.lifetime==lifetime) {return pending;}
    pending={activity,run,pending.revision+1,lifetime};return pending;
}
inline Request snapshot() noexcept {const std::lock_guard lock(mutex);return pending;}
inline void cancel(ActivityInstanceKey activity,std::uint64_t run) noexcept {
    const std::lock_guard lock(mutex);
    if(pending.activity==activity && pending.run==run) {pending.activity={};}
}

struct Observation {
    std::uint32_t lifetime{UINT32_MAX};
    bool player{},worldReady{},replacement{},loaderIdle{},localReady{};
};
// Require a previously playable world, a NEW salted lifetime object, and a
// stable unloaded-player state. Death, initial load and ordinary streaming do
// not qualify. One replacement gets at most one initialization request.
class Watch {
public:
    void reset() noexcept {*this={};}
    bool observe(Observation o,std::uint64_t now) noexcept {
        if(o.player && o.worldReady && o.lifetime!=UINT32_MAX) {
            prior=o.lifetime;candidate=UINT32_MAX;since=0;requested=false;return false;
        }
        if(requested) {return false;}
        if(!o.replacement || o.player || o.worldReady || !o.loaderIdle || !o.localReady
            || prior==UINT32_MAX || o.lifetime==UINT32_MAX || o.lifetime==prior) {
            candidate=UINT32_MAX;since=0;return false;
        }
        if(candidate!=o.lifetime) {candidate=o.lifetime;since=now;return false;}
        if(now<since || now-since<500) {return false;}
        requested=true;return true;
    }
private:
    std::uint32_t prior{UINT32_MAX},candidate{UINT32_MAX};
    std::uint64_t since{};
    bool requested{};
};
}
