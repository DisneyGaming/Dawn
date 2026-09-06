#pragma once
#include <array>
#include <cstddef>

// Reconstructed losslessly from the three header fields in the archived live log.
// SHA256 FB59CD923E7BBB6F7171296ACB72CFDF44450429BA5ECA092FB77FCC56E63EB3
namespace omega_reveal_runtime_fixture {
inline constexpr std::array<std::byte, 16> boss{
    std::byte{0x6A}, std::byte{0x75}, std::byte{0xF4}, std::byte{0x80}, std::byte{0x8F}, std::byte{0x94}, std::byte{0x80}, std::byte{0x80}, std::byte{0x28}, std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 16> intro{
    std::byte{0xD3}, std::byte{0x78}, std::byte{0xF4}, std::byte{0x80}, std::byte{0x07}, std::byte{0x4F}, std::byte{0x80}, std::byte{0x80}, std::byte{0xE8}, std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 16> anchor{
    std::byte{0x19}, std::byte{0x79}, std::byte{0xF4}, std::byte{0x80}, std::byte{0x8F}, std::byte{0x94}, std::byte{0x80}, std::byte{0x80}, std::byte{0x28}, std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}
