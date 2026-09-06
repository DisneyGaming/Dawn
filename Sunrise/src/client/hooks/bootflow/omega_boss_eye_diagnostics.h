#pragma once

#include "omega_boss_graph.h"
#include "omega_boss_graph_observation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::client::hooks::bootflow::omega_boss_eye_diagnostics {
namespace graph = omega_boss_graph;
namespace observation = omega_boss_graph_observation;

struct Input { std::uint32_t hash; std::int32_t index; };
// Package80F6695E bound mappings at44D0/47B0. These observations are not
// writable encounter controls. CE0BA42D is an animation output;20AA7FC1 is
// computed by the original script and feeds the material, particles and lights.
inline constexpr std::array<Input, 9> kInputs{{
    {0xBE57CE2EU,6}, {0xDD46E308U,7}, {0x270E8F60U,8},
    {0x28D3F669U,21}, {0x47180E9AU,22}, {0xF33CEA78U,23}, {0xF4ECD569U,24},
    {0xCE0BA42DU,0}, {0x20AA7FC1U,32}}};

template<class T>
T field(std::span<const std::byte> bytes, std::size_t at) noexcept {
    T value{};
    if (at <= bytes.size() && sizeof value <= bytes.size()-at)
        std::memcpy(&value,bytes.data()+at,sizeof value);
    return value;
}

inline bool controller(std::span<const std::byte> bytes, std::uint32_t self,
                       std::uint32_t entity) noexcept {
    return bytes.size() >= 0x60 && self != UINT32_MAX && entity != UINT32_MAX
        && field<std::uint32_t>(bytes,0) == 0x80F6695EU
        && field<std::uint32_t>(bytes,4) == 0x80809790U
        && field<std::int64_t>(bytes,8) == 0x1768
        && field<std::uint32_t>(bytes,0x24) == self
        && field<std::uint32_t>(bytes,0x2C) == entity
        && field<std::uint64_t>(bytes,0x50) == 67;
}

inline constexpr std::size_t kAnimationProviderCount=47;
inline constexpr std::size_t kAnimationGlowProvider=40;
inline bool animation_parent(std::span<const std::byte> bytes,
                             const graph::Owner& owner) noexcept {
    return bytes.size()>=0x1474
        && field<std::uint32_t>(bytes,0)==0x80F6690AU
        && field<std::uint32_t>(bytes,4)==0x808082ECU
        && field<std::int64_t>(bytes,8)==0x3038
        && field<std::uint32_t>(bytes,0x24)==owner.parent
        && field<std::uint32_t>(bytes,0x2C)==owner.entity
        && field<std::uint64_t>(bytes,0x1328)==kAnimationProviderCount
        // A0E8E0 indexes+1470 through the AI actor pool, whose+50 A8CB20
        // resolves back to this parent. It is not the character's+5C0 state.
        && field<std::uint32_t>(bytes,0x1470)==owner.actor;
}
inline bool animation_glow_provider(std::span<const std::byte> bytes) noexcept {
    return bytes.size()>=0x30
        && field<std::uint32_t>(bytes,0)==0x80F6690AU
        && field<std::uint32_t>(bytes,4)==0x80807EEBU
        && field<std::int64_t>(bytes,8)==0x4340;
}

class Claims final {
public:
    bool claim(std::uint64_t currentRun, const graph::Owner& owner,
               const observation::Snapshot& snapshot, std::uint64_t now,
               unsigned& sample) noexcept {
        const auto phase = static_cast<unsigned>(snapshot.phase);
        if (!graph::valid_owner(owner) || currentRun == UINT64_MAX
            || currentRun != owner.run || currentRun < run_
            || !snapshot.active || !snapshot.nativeResult
            || snapshot.graphAsset != observation::kGraphAsset
            || snapshot.entity != owner.entity || snapshot.character != owner.character
            || snapshot.biped != owner.biped || phase < 1 || phase > 5) return false;
        if (currentRun != run_) { run_=currentRun; owner_=owner; phases_=settled_=0; first_={}; }
        if (owner != owner_) return false;
        const auto bit=1U<<phase;
        if (!(phases_&bit)) {
            phases_|=bit; first_[phase]=now; sample=0;
            return true;
        }
        // The authored glow filter uses state. A later read distinguishes
        // a first-frame zero from a settled value without driving that filter.
        if ((settled_&bit) || now<first_[phase] || now-first_[phase]<1000) return false;
        settled_|=bit; sample=1;
        return true; // Unavailable samples consume their finite slot too.
    }
private:
    std::uint64_t run_{};
    graph::Owner owner_{};
    unsigned phases_{},settled_{};
    std::array<std::uint64_t,6> first_{};
};
}
