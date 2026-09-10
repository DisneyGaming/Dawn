#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace sunrise::state::activity::coo::registry {
struct Slot final {
    std::uint16_t index{};
    std::uint8_t type{};
    std::uint32_t componentClass{}, senseSchema{}, authSchema{}, descriptorTag{};
    [[nodiscard]] constexpr std::uint8_t flags() const noexcept {
        return (senseSchema!=UINT32_MAX?1U:0U)|(authSchema!=UINT32_MAX?2U:0U);
    }
};
struct Definition final {
    std::string_view activity;
    std::uint32_t scenario{}, key{}, objectTag{}, bubbleHash{};
    std::uint8_t bubble{};
    std::span<const Slot> slots;
};
// This qualifies catalog extraction only. It never makes an object ordinary,
// adds it to a wire roster, or enables a native source.
[[nodiscard]] constexpr bool required(const Definition& definition,std::uint32_t scenario,
    std::uint32_t objectTag,std::uint32_t key,std::uint64_t explicitBubbleMask) noexcept {
    return definition.bubble<64 && definition.scenario==scenario && definition.objectTag==objectTag
        && definition.key==key && explicitBubbleMask==(std::uint64_t{1}<<definition.bubble);
}
} // namespace sunrise::state::activity::coo::registry
