#pragma once
#include "authored_registry.h"
#include "mercury_ambient_catalog.h"
#include "adventure_mercury.h"
#include "mercury_public_event_rally_registry.h"
#include <array>

namespace dawn::state::activity::coo::mercury {
// Package-derived capability identities, re-read 7 September 2026 with
// Dawn-work/mercury_inventory/descriptors.py. These objects are owned
// by mercury_freeroam bubble 15. This is the initial admission subset, not
// a claim that all Mercury content or its native activation policy is recovered.
using registry::Slot;
inline constexpr std::array<Slot,7> kPond{{
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80F5B68B},
    {1,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80F5B68E},
    {2,3,0x80808348,0x80807F04,0x80807F0C,0x80F5B69E},
    {3,70,0x808094EE,0x808094F0,0x808094F1,0x80F5B695},
    {4,30,0x8080952F,0x80809531,0x80809532,0x80F5B698},
    {8,66,0x808094CF,UINT32_MAX,UINT32_MAX,0x80F5B67C},
    {9,66,0x808094CF,UINT32_MAX,UINT32_MAX,0x80F5B682},
}};
inline constexpr std::array<Slot,3> kVance{{
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80F5B9CC},
    {1,70,0x808094EE,0x808094F0,0x808094F1,0x80F5B9CF},
    {2,42,0x80809583,UINT32_MAX,0x80809586,0x80F5BA28},
}};
inline constexpr std::array<Slot,11> kTeleporters{{
    {0,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5AC},
    {1,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5AF},
    {2,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5B2},
    {3,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5B5},
    {4,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5B8},
    {5,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5BB},
    {6,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5BE},
    {7,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5C1},
    {8,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5C4},
    {9,4,0x80809927,0x8080992E,0x8080992F,0x80F5E5C7},
    {10,70,0x808094EE,0x808094F0,0x808094F1,0x80F5E56A},
}};
// These package-authored groups cover each distinct Lighthouse combat zone
// without simultaneously admitting regular/hotspot variants at one transform.
// The profile keeps its twenty recovered core groups, including Vance,
// traversal, adventure and rally groups.
inline constexpr std::array<registry::Definition,20> kRegistries{{
    {"mercury_freeroam",0x80F4696A,0x74337EDD,0x80F5B6A7,0xA83A9175,15,kPond},
    {"mercury_freeroam",0x80F4696A,0x564C6ECE,0x80F5BA2B,0xA83A9175,15,kVance},
    {"mercury_freeroam",0x80F4696A,0xF25B938B,0x80F5E5CD,0xA83A9175,15,kTeleporters},
    adventure::mercury::kRegistry,
    *ambient::find(0xEB1E8934)->registry, // Vex, center left
    public_event_rally::kRegistry,
    *ambient::find(0xB3CBA385)->registry, // Cabal, blocks right
    *ambient::find(0x2571C34D)->registry, // Cabal, forest cannon
    *ambient::find(0x9D083869)->registry, // Cabal, cannon tower
    *ambient::find(0x9692BB5E)->registry, // Cabal, crater right
    *ambient::find(0x1ED6087A)->registry, // Vex, center right
    *ambient::find(0xFB7F2889)->registry, // Vex, crater cannon
    *ambient::find(0x1780D86F)->registry, // Vex, steps
    *ambient::find(0x90EFDE28)->registry, // Cabal hotspot, cannon middle
    *ambient::find(0xCF2196EA)->registry, // Vex hotspot, cannon middle
    *ambient::find(0x8C756CC3)->registry, // Cabal hotspot, crater cannon
    *ambient::find(0xBBF1BA51)->registry, // Vex hotspot, crater left
    *ambient::find(0x9B219BF3)->registry, // Cabal hotspot, forest
    *ambient::find(0xCCF03E8D)->registry, // Vex hotspot, forest
    *ambient::find(0x0EFE61CB)->registry, // Vex hotspot, pond
}};
} // namespace dawn::state::activity::coo::mercury
