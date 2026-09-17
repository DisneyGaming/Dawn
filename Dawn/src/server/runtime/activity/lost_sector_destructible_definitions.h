#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace dawn::server::runtime::activity::lost_sector_destructible {
struct Definition final {
    std::uint32_t source{},entity{},healthDefinition{};
    std::uint64_t healthOffset{};
};
inline constexpr std::array<Definition,4> kBindings{{
    {0x815700D5U,0x80C75CF7U,0x815B59A9U,0xB08U},
    {0x815700D8U,0x80C75CF7U,0x815B59A9U,0xB08U},
    {0x815700DBU,0x80C75CF7U,0x815B59A9U,0xB08U},
    {0x815700DEU,0x80C75CF7U,0x815B59A9U,0xB08U},
}};
[[nodiscard]] constexpr bool supported(std::uint32_t definition) noexcept {
    return std::find_if(kBindings.begin(),kBindings.end(),[definition](const Definition& item) {
        return item.source==definition;
    })!=kBindings.end();
}
[[nodiscard]] constexpr const Definition* binding(std::uint32_t definition) noexcept {
    const auto found=std::find_if(kBindings.begin(),kBindings.end(),[definition](const Definition& item) {
        return item.source==definition;
    });
    return found==kBindings.end()?nullptr:&*found;
}
}
