#pragma once

#include "nightfall_player.h"
#include "../hooks/bootflow/coo_native_components.h"

namespace sunrise::client::activity::eater_player_health::native_fallback {
namespace native=hooks::bootflow::gateway_native;
namespace coo_native=hooks::bootflow::coo_native;

inline constexpr std::uint32_t kHealthKind=0x80804B8AU;
inline constexpr std::uint64_t kDiscoveryIntervalMs=250;

/** Bounds the expensive reflected-component walk while retaining a valid lease. */
struct DiscoveryGate {
    std::uint64_t run{},last{};
    void bind(std::uint64_t next) noexcept {
        if(run==next) return;
        run=next;last=0;
    }
    [[nodiscard]] bool claim(std::uint64_t now) noexcept {
        if(!run || !now || (last && (now<=last || now-last<kDiscoveryIntervalMs))) return false;
        last=now;return true;
    }
};

struct RetainedDecision {
    bool publish{},dead{},discover{};
};

/** Publishes both edges and keeps looking when native control names a replacement. */
[[nodiscard]] constexpr RetainedDecision retained_decision(bool liveQualified,bool retained,
    bool dead,std::uint32_t cachedPlayer,std::uint32_t currentPlayer) noexcept {
    const bool replacement=currentPlayer!=UINT32_MAX && currentPlayer!=cachedPlayer;
    return {liveQualified && retained,dead,!retained || dead || replacement};
}

/**
 * Finds the exact health component in the local entity's resource bundle.
 * The native weak-reference maker supplies salts; every handle, owner, row and
 * component field is checked again after discovery before the lease escapes.
 */
template<class Read,class MakeWeak>
[[nodiscard]] bool discover(Read& read,std::uint32_t player,MakeWeak&& makeWeak,
                            nightfall_player::Observation& out) noexcept {
    out={};
    if(player==UINT32_MAX) return false;

    native::Weak entityWeak{},healthWeak{};
    makeWeak(entityWeak,player);
    std::uintptr_t row{};std::uint32_t actual{},flags{},bundle{};
    if(entityWeak.handle!=player || !read.weak(entityWeak) || !read.entity_row(entityWeak,row)
        || !read.value(row+12,actual) || actual!=player
        || !read.value(row+4,flags) || (flags&4U)
        || !read.value(row+0x4C,bundle) || bundle==UINT32_MAX) return false;

    std::uintptr_t health{};
    if(!coo_native::component<Read,1024>(read,bundle,player,kHealthKind,health)
        || health<0x10000 || health>UINTPTR_MAX-0x339) return false;

    native::Ref definition{};std::uint32_t self{},owner{};std::uint8_t deadFlags{};
    if(!read.value(health,definition) || definition.kind!=kHealthKind
        || !read.value(health+0x24,self) || self==UINT32_MAX
        || !read.value(health+0x2C,owner) || owner!=player
        || !read.value(health+0x338,deadFlags)) return false;
    std::uintptr_t resolved{};
    if(!read.resolve(self,resolved) || resolved!=health) return false;
    makeWeak(healthWeak,self);
    if(healthWeak.handle!=self || !read.weak(healthWeak)) return false;

    // Recheck both salted leases and the exact entity/bundle/component join.
    std::uintptr_t finalRow{},finalHealth{};native::Ref finalDefinition{};
    std::uint32_t finalActual{},finalFlags{},finalBundle{},finalSelf{},finalOwner{};
    std::uint8_t finalDeadFlags{};
    if(!read.weak(entityWeak) || !read.entity_row(entityWeak,finalRow) || finalRow!=row
        || !read.value(finalRow+12,finalActual) || finalActual!=actual
        || !read.value(finalRow+4,finalFlags) || finalFlags!=flags || (finalFlags&4U)
        || !read.value(finalRow+0x4C,finalBundle) || finalBundle!=bundle
        || !read.value(health,finalDefinition) || finalDefinition.handle!=definition.handle
        || finalDefinition.kind!=definition.kind || finalDefinition.offset!=definition.offset
        || !read.value(health+0x24,finalSelf) || finalSelf!=self
        || !read.resolve(finalSelf,finalHealth) || finalHealth!=health
        || !read.value(health+0x2C,finalOwner) || finalOwner!=owner
        || !read.value(health+0x338,finalDeadFlags) || finalDeadFlags!=deadFlags
        || !read.weak(healthWeak) || !read.weak(entityWeak)) return false;

    out={player,(deadFlags&1U)!=0,healthWeak};return true;
}

} // namespace sunrise::client::activity::eater_player_health::native_fallback
