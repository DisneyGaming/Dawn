#pragma once
#include "definition.h"
#include <array>
namespace sunrise::state::build_data::scenarios::omega_crown_roster {
// Full descriptor fingerprints from the installed v51 catalog. Each includes
// all native types, flags, ordinals, descriptor identities and sense/auth schemas.
// Provenance and independent packed fixtures: build/omega-full-20260905/crown-roster.
struct Group { std::uint32_t key, tag; std::uint16_t count; std::uint64_t fingerprint; };
inline constexpr std::array<Group,5> groups{{
    {0x0040BF06U,0x80F476C6U,64,0xB89861F542233BABULL},
    {0x0040BF05U,0x80F4779FU,64,0xD6FD653249DC0378ULL},
    {0x0040BF04U,0x80F47807U,29,0x9A72811A8747E73BULL},
    {0x0040BF03U,0x80F478BDU,55,0xE394648A19F7639CULL},
    {0x99BD2FEBU,0x80F47B1DU,119,0x3D560B2E4A5C84E8ULL},
}};
inline const Group* find(std::uint32_t key) noexcept {
    for (const auto& group:groups) if (group.key==key) return &group;
    return nullptr;
}
inline bool valid(const RosterGroup& group, std::uint32_t key) noexcept {
    const auto* expected=find(key);
    if (!expected || group.registryKey!=key || group.objectTag!=expected->tag
        || group.slotCount!=expected->count) return false;
    std::uint64_t fingerprint=14695981039346656037ULL;
    for (std::size_t i=0;i<group.slotCount;++i) {
        const std::array<std::uint32_t,8> fields{group.slotTypes[i],group.slotFlags[i],group.slotIndices[i],
            group.descriptorTags[i],group.descriptorOffsets[i],group.componentClasses[i],
            group.senseSchemas[i],group.authSchemas[i]};
        for (auto value:fields) fingerprint=(fingerprint^value)*1099511628211ULL;
    }
    return fingerprint==expected->fingerprint;
}
}
