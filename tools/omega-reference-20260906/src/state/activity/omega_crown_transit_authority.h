#pragma once

#include "omega_first_mancannon_authority.h"
#include <bit>

namespace dawn::state::activity::omega_crown_transit {

// Only authenticated encounter state supplies these latches. Pickup/dunk are
// native receipts; player position is never used to manufacture them.
struct TransitAuthority final {
    std::uint8_t cycle{};
    bool launches{}, bridge{}, target{}, finalTraversal{};
    // Bit i set once the native creation receipt (9EFFC0) for kSources[i] was
    // observed this run. DPS platforms wait for creation before requesting
    // phase-in; native device interpolation owns the actual visual transition.
    std::uint64_t created{};
    bool dpsPlatform{};
    bool returnLaunch{};
};
// platform: the deferred slab the carry object stands on (o_dunk_runway BF06/BF05
// slot24 = model 80F4AD93/80F4ADCF, world X[-1417.8,-1401.7]/[-1578.9,-1562.8]
// Y[-96.7,-70.3] top z-39.4 under carry z-39.33; o_finale_stairs BF03 slot21 =
// model 80F4AD3D top z-23.81 under carry z-23.79, plus its o_finale_railing 22).
// The rings deliver the player onto it and the pickup happens on it, so it must
// exist with the launches latch, not after the first pickup like the bridge to
// the sink (o_dunk_runway_end 25 / o_finale_bridge 20, retail materialises them
// only once the charge is carried: retail frames 415-416, 510).
enum class Role : std::uint8_t { core, ring, bridge, portal, destinationA,
    destinationB, finalCore, finalFx, finalDisk, platform, dpsPlatformA, dpsPlatformB, returnLauncher };
struct Source final {
    std::uint32_t registry, definition, entity;
    std::uint16_t slot, gateSlot;
    std::uint32_t gateDefinition;
    std::uint8_t cycle, preparationIndex;
    Role role;
    bool deferred;
};
inline constexpr std::uint16_t kNoGate=UINT16_MAX;
inline constexpr std::uint8_t kNoPreparation=UINT8_MAX;
// Package candidate0 transforms remain wholly native. Gates bind by candidate
// GUID, never the misleading shared definition+38 value.
inline constexpr std::array<Source,36> kSources{{
    {0x0040BF06U,0x80F47690U,0x80F44F3CU,28,kNoGate,0,1,0,Role::core,false},
    {0x0040BF06U,0x80F47693U,0x80F44F41U,29,kNoGate,0,1,1,Role::core,false},
    {0x0040BF06U,0x80F47696U,0x80F44F37U,30,kNoGate,0,1,2,Role::core,false},
    {0x0040BF06U,0x80F47699U,0x80C1156FU,31,34,0x80F476A2U,1,kNoPreparation,Role::ring,true},
    {0x0040BF06U,0x80F4769CU,0x80C1156FU,32,35,0x80F476A5U,1,kNoPreparation,Role::ring,true},
    {0x0040BF06U,0x80F4769FU,0x80C1156FU,33,36,0x80F476A8U,1,kNoPreparation,Role::ring,true},
    {0x0040BF06U,0x80F47684U,0x80F4AD97U,24,26,0x80F4768AU,1,kNoPreparation,Role::platform,true},
    {0x0040BF06U,0x80F47687U,0x80F4ADB5U,25,27,0x80F4768DU,1,kNoPreparation,Role::bridge,true},
    {0x0040BF06U,0x80F4766FU,0x80F44FA3U,17,kNoGate,0,1,kNoPreparation,Role::portal,true},
    {0x0040BF05U,0x80F47769U,0x80F44F41U,28,kNoGate,0,2,3,Role::core,false},
    {0x0040BF05U,0x80F4776CU,0x80F44F3CU,29,kNoGate,0,2,4,Role::core,false},
    {0x0040BF05U,0x80F4776FU,0x80F44F37U,30,kNoGate,0,2,5,Role::core,false},
    {0x0040BF05U,0x80F47772U,0x80C1156FU,31,34,0x80F4777BU,2,kNoPreparation,Role::ring,true},
    {0x0040BF05U,0x80F47775U,0x80C1156FU,32,35,0x80F4777EU,2,kNoPreparation,Role::ring,true},
    {0x0040BF05U,0x80F47778U,0x80C1156FU,33,36,0x80F47781U,2,kNoPreparation,Role::ring,true},
    {0x0040BF05U,0x80F4775DU,0x80F4ADD3U,24,26,0x80F47763U,2,kNoPreparation,Role::platform,true},
    {0x0040BF05U,0x80F47760U,0x80F4ADF1U,25,27,0x80F47766U,2,kNoPreparation,Role::bridge,true},
    {0x0040BF05U,0x80F47715U,0x80F44FA3U,0,kNoGate,0,2,kNoPreparation,Role::portal,true},
    {0x0040BF03U,0x80F47884U,0x80F4ACF1U,20,23,0x80F4788DU,3,kNoPreparation,Role::bridge,true},
    {0x0040BF03U,0x80F47887U,0x80F4AD42U,21,24,0x80F47890U,3,kNoPreparation,Role::platform,true},
    {0x0040BF03U,0x80F4788AU,0x80F4AD29U,22,25,0x80F47893U,3,kNoPreparation,Role::platform,true},
    {0x0040BF03U,0x80F478A5U,0x80F44FA3U,31,kNoGate,0,3,kNoPreparation,Role::portal,true},
    {0x95FB2E01U,0x80F475ACU,0x80F44F32U,22,kNoGate,0,0,6,Role::finalCore,false},
    {0x95FB2E01U,0x80F475AFU,0x80F4AE26U,23,24,0x80F475B2U,0,kNoPreparation,Role::finalFx,true},
    {0x95FB2E01U,0x80F475B5U,0x80F4AD79U,25,26,0x80F475B8U,0,kNoPreparation,Role::finalDisk,true},
    {0x95FB2E01U,0x80F47576U,0x80C00869U,4,kNoGate,0,0,kNoPreparation,Role::destinationA,true},
    {0x95FB2E01U,0x80F47579U,0x80C00869U,5,kNoGate,0,0,kNoPreparation,Role::destinationA,true},
    {0x95FB2E01U,0x80F4757CU,0x80C00869U,6,kNoGate,0,0,kNoPreparation,Role::destinationA,true},
    {0x95FB2E01U,0x80F4757FU,0x80C00869U,7,kNoGate,0,0,kNoPreparation,Role::destinationB,true},
    {0x95FB2E01U,0x80F47582U,0x80C00869U,8,kNoGate,0,0,kNoPreparation,Role::destinationB,true},
    {0x95FB2E01U,0x80F47585U,0x80C00869U,9,kNoGate,0,0,kNoPreparation,Role::destinationB,true},
    {0x95FB2E01U,0x80F475BBU,0x80F4AD54U,27,29,0x80F475C1U,0,kNoPreparation,Role::dpsPlatformA,true},
    {0x95FB2E01U,0x80F475BEU,0x80F4ACD4U,28,kNoGate,0,0,kNoPreparation,Role::dpsPlatformA,false},
    {0x0040BF03U,0x80F4789CU,0x80F4AD0AU,28,30,0x80F478A2U,3,kNoPreparation,Role::dpsPlatformB,true},
    {0x0040BF03U,0x80F4789FU,0x80F4ACD4U,29,kNoGate,0,3,kNoPreparation,Role::dpsPlatformB,false},
    // o_boss_ping_pushback_phantom: the native spline launcher on the shared
    // eye platform. Mode0 removes/recreates on active edges at the same G; it
    // must retire before the second eye visit, then launch again on recovery.
    {0x95FB2E01U,0x80F475C4U,0x80F44F5AU,30,kNoGate,0,0,kNoPreparation,Role::returnLauncher,false},
}};
[[nodiscard]] constexpr const Source* source(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& item:kSources) { if(item.registry==registry && item.slot==slot) { return &item; } }
    return nullptr;
}
/** Every source with the given role in the given cycle (platform pieces, bridges). */
[[nodiscard]] constexpr std::size_t sources_with_role(std::uint8_t cycle,Role role,
    std::span<const Source*> out) noexcept {
    std::size_t count{};
    for(const auto& item:kSources) {
        if(item.cycle==cycle && item.role==role) { if(count<out.size()) { out[count]=&item; } ++count; }
    }
    return count;
}
[[nodiscard]] constexpr const Source* gate(std::uint32_t registry,std::uint16_t slot) noexcept {
    if(slot==kNoGate) { return nullptr; }
    for(const auto& item:kSources) { if(item.registry==registry && item.gateSlot==slot) { return &item; } }
    return nullptr;
}
[[nodiscard]] constexpr bool dps_platform_created(std::uint8_t cycle,std::uint64_t mask) noexcept {
    if(cycle<1 || cycle>3) { return false; }
    const auto first=cycle==3?33U:31U;
    const auto required=UINT64_C(3)<<first;
    return (mask&required)==required;
}
[[nodiscard]] constexpr bool active(const Source& item,const TransitAuthority& state) noexcept {
    if(state.cycle>3) { return false; }
    if(item.role==Role::returnLauncher) { return (state.cycle==1 || state.cycle==2) && state.returnLaunch; }
    // The deferred front survives both visits at G+1; its gate phases it out
    // during recovery. The immediate collision back retires on recovery, and
    // only the next actual dunk creates it again at G before player transport.
    if(item.role==Role::dpsPlatformA) {
        if(item.deferred) { return state.cycle==2 || (state.cycle==1 && state.dpsPlatform); }
        return (state.cycle==1 || state.cycle==2) && state.dpsPlatform && !state.returnLaunch;
    }
    if(item.role==Role::dpsPlatformB) { return state.cycle==3 && state.dpsPlatform; }
    // The disk is the barrier behind Panoptes during the first two Crown
    // mechanics. Keep its native carrier; its device phases it out for the
    // final chase, rather than creating a fresh obstruction with the cannon.
    if(item.role==Role::finalDisk) { return state.cycle>=1; }
    if(item.role==Role::finalCore || item.role==Role::finalFx) {
        return state.cycle>=2 && state.finalTraversal;
    }
    // The encounter qualifies actual dunk plus native DPS front/back creation.
    // This transient source graph and its destinations arm together; there is
    // no carrying-only guessed contact point between dunk and native transport.
    // A's destinations survive the first recovery and second rescue, just like
    // its shared landing platform. Reverting a deferred source to generation G
    // here would contradict its already committed G+1 creation.
    if(item.role==Role::destinationA) { return state.cycle==2 || (state.cycle==1 && state.target); }
    if(item.role==Role::destinationB) { return state.cycle==3 && state.target; }
    if(state.cycle==0 || item.cycle!=state.cycle) { return false; }
    if(item.role==Role::portal) { return state.target; }
    // The platform is created with the rings that land the player on it; only
    // the bridge to the sink follows pickup. The teleport sequence waits above.
    if(item.role==Role::core || item.role==Role::ring || item.role==Role::platform) { return state.launches; }
    return state.bridge;
}
[[nodiscard]] constexpr bool created(const Source& item,const TransitAuthority& state) noexcept {
    const auto index=static_cast<std::size_t>(&item-kSources.data());
    return index<kSources.size() && index<64 && ((state.created>>index)&UINT64_C(1))!=0;
}
[[nodiscard]] constexpr bool retired(const Source& item,const TransitAuthority& state) noexcept {
    if(state.cycle>3) { return false; }
    if(item.role==Role::destinationA || item.role==Role::dpsPlatformA) { return state.cycle==3; }
    return item.cycle!=0 && state.cycle>item.cycle;
}

// Native mode0 cores consume the previously prepared state; mode1 objects
// require a newer generation to create. Retirement is another newer generation
// so old destinationA cannot compete with destinationB in the final arena.
template<class Writer>
[[nodiscard]] bool write_source(Writer& writer,const Source& item,
    std::uint32_t generation,const TransitAuthority& state) noexcept {
    if(generation==0 || generation>=0x7FFFFFFDU || state.cycle>3) { return false; }
    const bool enabled=active(item,state);
    const auto revision= generation+(item.deferred?(retired(item,state)?2U:enabled?1U:0U):0U);
    return omega_first_mancannon::write_authority(writer,revision,enabled);
}

struct Preparation final { std::uint32_t generation{};std::uint8_t index{}; };
[[nodiscard]] inline bool preparation(std::span<const std::byte,16> prefix,
    std::span<const std::byte,0x44> state,Preparation& receipt) noexcept {
    receipt={};std::uint32_t definition{};std::memcpy(&definition,prefix.data(),4);
    const Source* found{};
    for(const auto& item:kSources) {
        if(item.definition==definition && item.preparationIndex!=kNoPreparation) { found=&item;break; }
    }
    if(!found) { return false; }
    // Same exact native authority schema; reuse its bounded field validation
    // only after matching one of this catalog's seven physical core definitions.
    std::array<std::byte,16> schemaPrefix{};
    std::memcpy(schemaPrefix.data(),prefix.data(),schemaPrefix.size());
    const auto canonical=omega_first_mancannon::kCannons[0].coreDefinition;
    std::memcpy(schemaPrefix.data(),&canonical,4);
    omega_first_mancannon::Preparation checked{};
    if(!omega_first_mancannon::preparation(schemaPrefix,state,checked)
        || checked.generation>=0x7FFFFFFDU) { return false; }
    receipt={checked.generation,found->preparationIndex};return true;
}

inline constexpr std::size_t kGateBits=147;
template<class Writer>
[[nodiscard]] bool write_channels(Writer& writer,float position,std::uint16_t revision) noexcept {
    const auto channel=[&](float value,std::uint16_t rev) noexcept {
        return writer.write(std::bit_cast<std::uint32_t>(value),32)
            && writer.write(static_cast<std::uint32_t>(rev)+0x8000U,16)
            && writer.write(0U,1);
    };
    // Zero revisions leave native power1/lock0 untouched. Smooth native device
    // interpolation owns the animation; no host timing or manual model phase.
    return channel(position,revision) && channel(1.F,0) && channel(0.F,0);
}
template<class Writer>
[[nodiscard]] bool write_gate(Writer& writer,const Source& item,const TransitAuthority& state) noexcept {
    if(item.gateSlot==kNoGate || state.cycle>3) { return false; }
    const bool enabled=active(item,state),done=retired(item,state);
    float position{};
    if(item.role==Role::finalDisk) {
        // Original DF1F70 + this entity's EA0/ED0 bindings: rising starts
        // phase_in, falling starts phase_out. Second recovery opens the path
        // before the later native Cabal summon and boss departure release the
        // final cannon. The inherited first return cannot open it: starting
        // cycle2 resets the dunk latch. No mid-motion creation reversal.
        const bool opening=state.cycle==3 || (state.cycle==2
            && (state.finalTraversal || (state.dpsPlatform && state.returnLaunch)));
        const bool built=enabled && created(item,state);
        return write_channels(writer,opening?0.F:built?1.F:0.F,opening?2U:built?1U:0U);
    }
    if(item.role==Role::dpsPlatformA) {
        // Original DF1F70 positive START binds EA0 ->80BFD0FD phase_in;
        // negative START binds ED0 ->80BFD0FE phase_out. The runtime starts
        // at raw90: treating raw callback410 as a runtime offset inverted the
        // earlier interpretation. Start dormant at0, rise only after creation,
        // fall on recovery, then rise on the second visit with newer revisions.
        if(state.cycle==3) { return write_channels(writer,0.F,6U); }
        if(state.cycle==2) {
            if(!state.dpsPlatform) { return write_channels(writer,0.F,3U); }
            if(state.returnLaunch) { return write_channels(writer,0.F,5U); }
            const bool built=enabled && created(item,state);
            return write_channels(writer,built?1.F:0.F,built?4U:3U);
        }
        if(state.cycle==1 && state.returnLaunch) { return write_channels(writer,0.F,3U); }
        const bool built=enabled && created(item,state);
        return write_channels(writer,built?1.F:0.F,built?2U:enabled?1U:0U);
    }
    if(item.role==Role::dpsPlatformB) {
        const bool built=enabled && created(item,state);
        return write_channels(writer,built?1.F:0.F,done?3U:built?2U:enabled?1U:0U);
    }
    // 80FB8766: opening branch0.0901..0.1001 ramps its visible scalar0->1;
    // closing branch0.1901..0.2001 ramps1->0. >=0.2901 is removal.
    if(item.role==Role::ring) { position=done?0.2F:enabled?0.1F:0.F; return write_channels(writer,position,done?2U:enabled?1U:0U); }
    // Existing runway/bridge startup pulse is retained here. The DPS policies
    // above use the verified native polarity; an initial rise/fall pulse can
    // mask the inverse direction because DF1F70 skips start callbacks when the
    // previous movement still has nonzero velocity.
    if(item.role==Role::bridge || item.role==Role::platform
        || item.role==Role::dpsPlatformA || item.role==Role::dpsPlatformB) {
        const bool built=enabled && created(item,state);
        position=done?1.F:built?0.F:1.F;
        return write_channels(writer,position,done?3U:built?2U:enabled?1U:0U);
    }
    if(item.role==Role::finalFx) { position=enabled?1.F:0.F; return write_channels(writer,position,done?2U:enabled?1U:0U); }
    return false;
}
struct DunkGate final { std::uint32_t registry,definition;std::uint16_t slot;std::uint8_t cycle; };
inline constexpr std::array<DunkGate,3> kDunkGates{{
    {0x0040BF06U,0x80F4767BU,21,1},
    {0x0040BF05U,0x80F47721U,4,2},
    {0x0040BF03U,0x80F47851U,3,3},
}};
[[nodiscard]] constexpr const DunkGate* dunk_gate(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& item:kDunkGates) { if(item.registry==registry && item.slot==slot) { return &item; } }
    return nullptr;
}
template<class Writer>
[[nodiscard]] bool write_dunk_gate(Writer& writer,const DunkGate& item,
    std::uint8_t cycle,bool dunked,bool bridgeReady) noexcept {
    if(cycle>3) { return false; }
    // Exact80F58F0A starts its effect at nativeposition0 and fades out at1.
    // Its carrier appears with the pickup/bridge latch, independently of the
    // actual-dunk teleport source. Dunk advances this device into its own fade.
    const bool done=cycle>item.cycle;
    const bool current=cycle==item.cycle;
    return write_channels(writer,done || (current && dunked)?1.F:0.F,
        done?3U:current && dunked?2U:current && bridgeReady?1U:0U);
}

} // namespace dawn::state::activity::omega_crown_transit
