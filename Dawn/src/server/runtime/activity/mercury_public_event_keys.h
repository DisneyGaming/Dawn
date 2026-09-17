#pragma once
#include "mercury_public_event_registries.h"
#include "public_event_key_runtime.h"
namespace dawn::server::runtime::activity::mercury::public_events {
inline constexpr std::array<public_event::keys::Item,4> kMainlandKeys{{
    {{{{},0,0,0,0,0xC8229B2B,0x80F5E42D,1,0x4C8,34,15,0x80F58EAA,0xA1868317222FBA26ULL},
      {{},0,0,0,0,0xC8229B2B,0x80F5E3F1,1,0x4C8,14,15,0x80F58EF8,0x584A32DDD9A799EDULL},0x80F7A855,0x80F7A88B},{&kRegistries[1],13,1},0},
    {{{{},0,0,0,0,0xC8229B2B,0x80F5E430,1,0x4C8,35,15,0x80F58EAA,0xEB9F7FDDAB3308FEULL},
      {{},0,0,0,0,0xC8229B2B,0x80F5E3FA,1,0x4C8,17,15,0x80F58EFC,0x5E5E883ABAC82A09ULL},0x80F7A855,0x80F7A88D},{&kRegistries[1],16,1},0},
    {{{{},0,0,0,0,0xC8229B2B,0x80F5E433,1,0x4C8,36,15,0x80F58EAA,0x750267715857E63FULL},
      {{},0,0,0,0,0xC8229B2B,0x80F5E403,1,0x4C8,20,15,0x80F58F00,0xF9D73D1F06751C38ULL},0x80F7A855,0x80F7A88F},{&kRegistries[1],19,1},1},
    {{{{},0,0,0,0,0xC8229B2B,0x80F5E436,1,0x4C8,37,15,0x80F58EAA,0xE1110FD14BF34632ULL},
      {{},0,0,0,0,0xC8229B2B,0x80F5E40C,1,0x4C8,23,15,0x80F58F04,0x284BB27461788249ULL},0x80F7A855,0x80F7A89C},{&kRegistries[1],22,1},1}
}};
inline constexpr world_device::Action kDunkActions[]{
    // Authored effect80F58F0A shows the waiting charge at endpoint0 and
    // withdraws it at endpoint1. Native position revisions still advance.
    {1,world_device::Position,{{0.0F,0,false},{},{}}},
    {2,world_device::Position,{{1.0F,0,false},{},{}}}
};
inline constexpr std::array<world_device::Capability,4> kMainlandDunkDevices{{
    {&kRegistries[1],15,kDunkActions},{&kRegistries[1],18,kDunkActions},
    {&kRegistries[1],21,kDunkActions},{&kRegistries[1],24,kDunkActions}
}};
inline constexpr public_event::keys::Definition kMainlandKeyDefinition{kMainlandKeys,kMainlandDunkDevices};
}
