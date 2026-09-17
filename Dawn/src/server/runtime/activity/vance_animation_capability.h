#pragma once
#include "native_npc_animation_service.h"
#include "mercury_registries.h"

namespace dawn::server::runtime::activity::mercury {
// Actual bank 80EC120B, group AFB11A12, sole key at A0. The native
// runtime uses this bank; the shared behavior graph's `idle` key is different.
// Verified on r16, 2026-09-09: source and idle in the first publication frame
// produced native active type39 with changing state and visible vendor motion.
// Request it at startup: the later contact-derived policy rejects new idles.
inline constexpr std::array<npc_animation::Action,1> kVanceAnimationActions{{
    {1,0x010B0F07U,0x811C9DC5U,true},
}};
inline constexpr std::array<npc_animation::Capability,1> kVanceAnimationCapabilities{{
    {&kRegistries[1],2,kVanceAnimationActions},
}};
} // namespace dawn::server::runtime::activity::mercury
