#pragma once

#include "authored_registry.h"
#include <array>

namespace dawn::state::activity::coo::city_tower_social_d2 {

using registry::Slot;

// Root-traced from the Tower SDK/cache. The source registry has a fifth
// non-network slot (61/4); it is intentionally not part of this admission
// definition. Offsets are retained as evidence because Slot carries only the
// wire identity used by the shared admission/serialization services.
inline constexpr std::array<std::uint16_t, 4> kComponentOffsets{{
    0x878, 0x4C8, 0x358, 0x228,
}};

inline constexpr std::array<Slot, 4> kSlots{{
    {0, 1, 0x80809A3B, 0x80807ECC, 0x80807EC9, 0x80B4AF52},
    {1, 4, 0x80809927, 0x8080992E, 0x8080992F, 0x80B4AF55},
    {2, 70, 0x808094EE, 0x808094F0, 0x808094F1, 0x80B4AF58},
    {3, 42, 0x80809583, UINT32_MAX, 0x80809586, 0x80B4AF5B},
}};

inline constexpr std::array<registry::Definition, 1> kRegistries{{
    {"city_tower_social_d2", 0x80B4A0F4, 0x7C6DE64F, 0x80B4AF5E,
        0xAB28899E, // Bubble identity; distinct from the state hash.
        6, kSlots, false},
}};

} // namespace dawn::state::activity::coo::city_tower_social_d2
