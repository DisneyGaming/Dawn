#pragma once
#include <array>
#include <cstdint>

namespace dawn::client::hooks::bootflow::native_hook_ownership {
// Physical detour targets consumed by the installers. Shared observers call
// through these owners; callable native helpers are not additional detours.
inline constexpr std::array<std::uintptr_t,3> kAmbientNamedPoints{
    0x569D10,0x4E25D0,0x34F790};
inline constexpr std::array<std::uintptr_t,4> kHijackedPlacements{
    0x575690,0x4EC1A0,0x424D10,0x557690};
inline constexpr std::array<std::uintptr_t,9> kArcCharge{
    0xD99620,0xF36640,0x9EFFC0,0xF32CD0,0x9F0750,0xB804E0,0xCDCB60,0xB7E3C0,0x1006F20};
inline constexpr std::array<std::uintptr_t,0> kNativeCapture{};
inline constexpr std::array<std::uintptr_t,1> kCleanupOwner{0xF9C150};
inline constexpr std::array<std::uintptr_t,1> kPropertyList{0x4D86B0};
inline constexpr std::array<std::uintptr_t,1> kLocalReconnect{0x17CC1A0};
}
