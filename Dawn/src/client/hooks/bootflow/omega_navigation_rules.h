#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <span>

#include "../../../state/activity/omega_presentation_rules.h"

namespace dawn::client::hooks::bootflow::omega_navigation {
inline constexpr std::size_t kFallbackOffset=0xA80;
inline constexpr std::size_t kComponentSize=0xB00;
inline constexpr std::size_t kGatewaySize=0x360;
inline constexpr std::size_t kGatewayOpenOffset=0x355;
struct Reference final {
    std::uint32_t handle{}, type{};
    std::int64_t offset{};
};
static_assert(sizeof(Reference)==16);

/** Destination scenario bubbles, recovered from the authored transition selectors.
 * These are neither map-global bubble numbers nor slice-state ids. In particular,
 * ap_inside_forest is owned by Lighthouse but targets Forest D; ap_phase targets Lair. */
[[nodiscard]] inline constexpr std::uint32_t destination_bubble(
    state::activity::omega_presentation::NavigationGoal goal) noexcept {
    using Goal=state::activity::omega_presentation::NavigationGoal;
    switch (goal) {
    case Goal::ikora: case Goal::portal: return 15;
    case Goal::forestEntrance: return 11;
    case Goal::lairApproach: case Goal::lairEntry: return 14;
    default: return UINT32_MAX;
    }
}

template <typename T>
[[nodiscard]] inline T read(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    T value{};
    if (offset<=bytes.size() && sizeof value<=bytes.size()-offset) {
        std::memcpy(&value,bytes.data()+offset,sizeof value);
    }
    return value;
}

/** FFB850's terminal branch: a gateway node at maximum route progress has opened.
 * A native result of zero alone also means no route/current node and is insufficient.
 * The caller has already validated the exact Omega worker identity. */
[[nodiscard]] inline bool terminal_gateway_index(std::span<const std::byte> worker,
    std::span<const std::byte> node, std::uint32_t nativeResult,
    std::uint8_t& index) noexcept {
    if (worker.size()<0x930 || node.size()<0x30 || nativeResult!=0
        || read<std::uint8_t>(node,0x18)!=3
        || read<std::int8_t>(worker,0x89A)<0) { return false; }
    const auto count=read<std::int32_t>(worker,0x92C);
    const auto gateway=read<std::int8_t>(node,0x24);
    const float maximum=read<float>(worker,0x89C);
    const float progress=read<float>(node,0x2C);
    // The accepted Omega recipe assigns the exit progress 1, entrance 0. Requiring
    // that maximum rejects an uninitialized/equal-zero graph as a completion event.
    if (count<=0 || count>128 || gateway<0 || gateway>=count
        || !std::isfinite(progress) || maximum!=1.0F || progress!=maximum) { return false; }
    index=static_cast<std::uint8_t>(gateway);
    return true;
}

[[nodiscard]] inline bool gateway_open(std::span<const std::byte> gateway) noexcept {
    return gateway.size()==kGatewaySize
        && read<std::uint8_t>(gateway,kGatewayOpenOffset)==1;
}

[[nodiscard]] inline bool relative_address(std::uintptr_t base, std::int64_t displacement,
    std::size_t suffix, std::uintptr_t& address) noexcept {
    if (base>UINTPTR_MAX-suffix) { return false; }
    address=base+suffix;
    if (displacement>=0) {
        if (static_cast<std::uint64_t>(displacement)>UINTPTR_MAX-address) { return false; }
        address+=static_cast<std::uintptr_t>(displacement);
    } else {
        const auto magnitude=static_cast<std::uint64_t>(-(displacement+1))+1;
        if (magnitude>address) { return false; }
        address-=static_cast<std::uintptr_t>(magnitude);
    }
    return address!=0;
}

/** FFB4B0 returns an element from this worker's own 0x38-byte node array.
 * Do not latch completion from another worker or a transient/unrelated pointer. */
[[nodiscard]] inline bool owns_node(std::span<const std::byte> worker,
    std::uintptr_t workerAddress, std::uintptr_t nodeAddress) noexcept {
    if (worker.size()<0x928) { return false; }
    const auto count=read<std::int32_t>(worker,0x924);
    std::uintptr_t first{};
    if (count<=0 || count>4096
        || !relative_address(workerAddress,read<std::int64_t>(worker,0x858),0x868,first)
        || nodeAddress<first) { return false; }
    const auto offset=nodeAddress-first;
    return offset%0x38==0 && offset/0x38<static_cast<std::uintptr_t>(count);
}

[[nodiscard]] inline constexpr state::activity::omega_presentation::NavigationGoal
forward_goal(state::activity::omega_presentation::NavigationGoal goal,
             bool terminalGateOpened) noexcept {
    using Goal=state::activity::omega_presentation::NavigationGoal;
    return terminalGateOpened && goal==Goal::forestGates ? Goal::lairApproach : goal;
}

/** Exact live-proven Omega global directive component, not the shared class alone.
 * +48 is the component's runtime self reference; +A80 is its fallback waypoint.
 * Rebuild the marker reference from that owner, never from a HUD index or cached pointer. */
[[nodiscard]] inline bool fallback_reference(std::span<const std::byte> component,
                                              Reference& reference) noexcept {
    if (component.size()<kComponentSize
        || read<std::uint32_t>(component,0)!=0x80F47BD4U
        || read<std::uint32_t>(component,4)!=0x80804F54U
        || read<std::int64_t>(component,8)!=0xB88) { return false; }
    reference=read<Reference>(component,0x48);
    if (reference.handle==0 || reference.handle==UINT32_MAX
        || (reference.handle&0x80000000U)!=0 || reference.type!=0x80804F53U
        || reference.offset<0 || reference.offset>0x20000) { return false; }
    reference.type=0x80804F55U;
    reference.offset+=kFallbackOffset;
    return true;
}

/** Change only the fallback's kind, display mode, destination bubble and world target. Native
 * registration consumes kind0 as removal and invalidates its selected-route cache.
 * All twelve authored objective target slots and the forest worker remain independent. */
[[nodiscard]] inline bool update_fallback(std::span<std::byte> component,
    state::activity::omega_presentation::NavigationGoal goal) noexcept {
    Reference reference{};
    if (!fallback_reference(component,reference)) { return false; }
    auto* record=component.data()+kFallbackOffset;
    bool changed=false;
    const auto put=[&changed,record](std::size_t offset, const auto& value) noexcept {
        if (std::memcmp(record+offset,&value,sizeof value)!=0) {
            std::memcpy(record+offset,&value,sizeof value);
            changed=true;
        }
    };
    state::activity::omega_presentation::Point point{};
    if (state::activity::omega_presentation::navigation_point(goal,point)) {
        put(4,std::uint8_t{3});
        put(0xC,std::uint8_t{2});
        // C73150 copies this into the resolved point +40; C75E70/BEF7A0 pass it
        // to A22B90 as the destination-bubble mask. Updating XYZ alone retains
        // the stale opening bubble and routes backwards through Lighthouse.
        put(0x18,destination_bubble(goal));
        put(0x20,std::array<float,4>{point.x,point.y,point.z,1.0F});
    } else {
        put(4,std::uint8_t{0});
    }
    return changed;
}
} // namespace dawn::client::hooks::bootflow::omega_navigation
