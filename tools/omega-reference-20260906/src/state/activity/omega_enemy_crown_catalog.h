#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace sunrise::state::activity::omega_enemy_crown {

inline constexpr std::uint32_t kRegistry=0x0040BF06U;
inline constexpr std::uint32_t kRegistryDefinition=0x80F476C6U;
inline constexpr std::uint16_t kTacticalSlot=16;
inline constexpr std::uint32_t kTacticalDefinition=0x80F4766CU;
inline constexpr std::uint32_t kHiveRegistry=0x0040BF05U;
inline constexpr std::uint32_t kHiveRegistryDefinition=0x80F4779FU;
inline constexpr std::uint32_t kVexRegistry=0x0040BF03U;
inline constexpr std::uint32_t kVexRegistryDefinition=0x80F478BDU;
inline constexpr std::uint32_t kCabalRegistry=0x0040BF04U;
inline constexpr std::uint32_t kCabalRegistryDefinition=0x80F47807U;

struct Spawner final {
    std::uint16_t slot;
    std::uint16_t ruleSlot;
    std::uint32_t resource;
    std::array<std::uint32_t,2> entities;
    std::uint8_t categories;
    std::uint16_t memberSlot;
};

/** Exact authored source/rule/category identities. Catalog membership does not
 * activate a source. The anchor and later waves remain dormant in this slice. */
inline constexpr std::array<Spawner,15> kSpawners{{
    {0,77,0x80F4763C,{0x80F44FAA,0},1,1},
    {2,85,0x80F47642,{0x80F45004,0},1,0},
    {3,71,0x80F47645,{0x80F45004,0},1,0},
    {4,75,0x80F47648,{0x80F45004,0x80F45056},2,0},
    {5,79,0x80F4764B,{0x80F45004,0x80F45056},2,0},
    {6,83,0x80F4764E,{0x80F45004,0x80F45056},2,0},
    {7,68,0x80F47651,{0x80F45004,0x80F45056},2,0},
    {8,70,0x80F47654,{0x80F45004,0x80F45056},2,0},
    {9,72,0x80F47657,{0x80F45004,0x80F45056},2,0},
    {10,74,0x80F4765A,{0x80F45004,0},1,0},
    {11,76,0x80F4765D,{0x80F4507C,0},1,0},
    {12,78,0x80F47660,{0x80F45004,0},1,0},
    {13,80,0x80F47663,{0x80F45004,0},1,0},
    {14,82,0x80F47666,{0x80F4507C,0},1,0},
    {15,84,0x80F47669,{0x80F4507C,0},1,0},
}};

inline constexpr std::array<Spawner,15> kHiveSpawners{{
    {5,84,0x80F47724,{0x80F45033,0},1,6},
    {7,96,0x80F4772A,{0x80F44FA6,0},1,0},
    {8,78,0x80F4772D,{0x80F45059,0},1,0},
    {9,82,0x80F47730,{0x80F44FA6,0},1,0},
    {10,86,0x80F47733,{0x80F44FA6,0},1,0},
    {11,90,0x80F47736,{0x80F44FA6,0},1,0},
    {12,75,0x80F47739,{0x80F44FA6,0},1,0},
    {13,77,0x80F4773C,{0x80F44FA6,0},1,0},
    {14,79,0x80F4773F,{0x80F44FA6,0},1,0},
    {15,81,0x80F47742,{0x80F44FA6,0x80F45083},2,0},
    {16,83,0x80F47745,{0x80F44FA6,0x80F45083},2,0},
    {17,85,0x80F47748,{0x80F45059,0},1,0},
    {18,87,0x80F4774B,{0x80F44FA6,0},1,0},
    {19,89,0x80F4774E,{0x80F44FA6,0},1,0},
    {20,91,0x80F47751,{0x80F44FA6,0},1,0},
}};
inline constexpr std::array<Spawner,14> kVexSpawners{{
    {4,62,0x80F47854,{0x80F4505B,0},1,5},
    {6,70,0x80F4785A,{0x80F45014,0},1,0},
    {7,66,0x80F4785D,{0x80F45014,0},1,0},
    {8,46,0x80F47860,{0x80F45014,0},1,0},
    {9,64,0x80F47863,{0x80F4501A,0},1,0},
    {10,68,0x80F47866,{0x80F45014,0x80F4501A},2,0},
    {11,72,0x80F47869,{0x80F45014,0},1,0},
    {12,61,0x80F4786C,{0x80F45009,0},1,0},
    {13,63,0x80F4786F,{0x80F45009,0},1,0},
    {14,65,0x80F47872,{0x80F45014,0x80F4501A},2,0},
    {15,67,0x80F47875,{0x80F45014,0},1,0},
    {16,69,0x80F47878,{0x80F4501A,0},1,0},
    {17,71,0x80F4787B,{0x80F4500D,0},1,0},
    {18,74,0x80F4787E,{0x80F4500D,0},1,0},
}};
inline constexpr std::array<Spawner,11> kCabalSpawners{{
    {0,20,0x80F477D1,{0x80F45053,0},1,0},
    {1,18,0x80F477D4,{0x80F45053,0},1,0},
    {2,32,0x80F477D7,{0x80F45053,0},1,0},
    {3,51,0x80F477DA,{0x80F45053,0},1,0},
    {4,22,0x80F477DD,{0x80F4504F,0},1,5},
    {6,45,0x80F477E3,{0x80F45080,0},1,0},
    {7,50,0x80F477E6,{0x80F45080,0},1,0},
    {8,52,0x80F477E9,{0x80F45080,0},1,0},
    {9,19,0x80F477EC,{0x80F45080,0},1,0},
    {10,21,0x80F477EF,{0x80F45080,0},1,0},
    {11,23,0x80F477F2,{0x80F45080,0},1,0},
}};
struct Registry final {
    std::uint32_t key,definition,tacticalDefinition;
    std::uint16_t tacticalSlot;
    std::span<const Spawner> spawners;
};
// BF04 is the post-second-DPS escape wave, not a fourth DPS cycle.
inline constexpr std::array<Registry,4> kRegistries{{
    {kRegistry,kRegistryDefinition,kTacticalDefinition,kTacticalSlot,kSpawners},
    {kHiveRegistry,kHiveRegistryDefinition,0x80F47754U,21,kHiveSpawners},
    {kVexRegistry,kVexRegistryDefinition,0x80F47881U,19,kVexSpawners},
    {kCabalRegistry,kCabalRegistryDefinition,0x80F477F5U,12,kCabalSpawners},
}};
[[nodiscard]] constexpr const Registry* find_registry(std::uint32_t key) noexcept {
    for(const auto& registry:kRegistries) {if(registry.key==key) {return &registry;}}
    return nullptr;
}

struct Group final {
    std::uint16_t source;
    std::array<std::uint8_t,2> requested;
    bool member;
};

/** First Crown opening only. Retail establishes Dregs and Shanks in the opening;
 * these exact per-source counts reconstruct missing host policy. Later Dregs,
 * Vandals and Taldriks near retail384s require separate future summon stages. */
inline constexpr std::array<Group,8> kGroups{{
    {2,{3,0},false},{3,{3,0},false},
    {4,{1,1},false},{5,{1,1},false},{6,{1,1},false},
    {7,{1,1},false},{8,{1,1},false},{9,{1,1},false},
}};

[[nodiscard]] constexpr const Spawner* find_spawner(std::uint16_t slot,std::uint32_t key=kRegistry) noexcept {
    const auto* registry=find_registry(key);if(registry==nullptr) {return nullptr;}
    for(const auto& source:registry->spawners) { if(source.slot==slot) { return &source; } }
    return nullptr;
}

} // namespace sunrise::state::activity::omega_enemy_crown
