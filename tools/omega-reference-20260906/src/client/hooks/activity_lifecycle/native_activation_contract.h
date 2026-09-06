#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "native_activation_validation.h"

namespace sunrise::client::hooks::activity_lifecycle::contract {

inline constexpr std::uintptr_t kActivityActivateRva = 0x3CDB80U;
inline constexpr std::uintptr_t kActivityCloseRva = 0x3CDB20U;
inline constexpr std::uintptr_t kActivityReinstantiateRva = 0x3CDDC0U;
inline constexpr std::uintptr_t kActivityCleanupRva = 0x3CA680U;
inline constexpr std::uintptr_t kActivityGlobalDropRva = 0x3C8EB0U;
inline constexpr std::size_t kHookCount = 5U;

inline constexpr std::array<std::byte, 24U> kActivityActivatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20},
};
inline constexpr std::array<std::byte, 24U> kActivityClosePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0x79},
    std::byte{0x20}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xF9}, std::byte{0x74}, std::byte{0x41}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xC1}, std::byte{0x28}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x30},
};
inline constexpr std::array<std::byte, 24U> kActivityReinstantiatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x41}, std::byte{0x18},
};
inline constexpr std::array<std::byte, 23U> kActivityCleanupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0x79},
    std::byte{0x20}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x74}, std::byte{0x2E}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xC1}, std::byte{0x28},
};
inline constexpr std::array<std::byte, 17U> kActivityGlobalDropPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x18}, std::byte{0x4C},
    std::byte{0x89},
};

/** Exact installed packed executable admitted by the on-disk build-identity gate. */
inline constexpr ImageSha256 kPinnedInstalledPackedImageSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED},
};

/**
 * Provenance identity of the reconstructed unpacked RE image from which the mapped prefixes were
 * recovered. This digest is never compared with GetModuleFileNameW(mainModule)'s packed file.
 */
inline constexpr ImageSha256 kPinnedUnpackedReReferenceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD},
};

static_assert(kPinnedInstalledPackedImageSha256 != kPinnedUnpackedReReferenceSha256);

} // namespace sunrise::client::hooks::activity_lifecycle::contract
