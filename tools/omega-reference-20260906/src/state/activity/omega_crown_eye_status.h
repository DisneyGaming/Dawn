#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::state::activity::omega_crown_eye_status {

struct Source final {
    std::uint32_t registry, definition;
    std::uint16_t slot;
    std::uint8_t cycle;
};
// ho_damage_phase, native host-object-filter8080953F. Each definition's +B20
// is entity80F44F1F; resource80F6661C exposes HUD string E7B21E28.
inline constexpr std::array<Source,3> kSources{{
    {0x0040BF06U,0x80F47681U,23,1},
    {0x0040BF05U,0x80F4775AU,23,2},
    {0x0040BF03U,0x80F47896U,26,3},
}};
inline constexpr std::size_t kInactiveBits=186;
inline constexpr std::size_t kActiveBits=251;

[[nodiscard]] constexpr const Source* source(std::uint32_t registry,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=26) { return nullptr; }
    for(const auto& item:kSources) {
        if(item.registry==registry && item.slot==slot) { return &item; }
    }
    return nullptr;
}
[[nodiscard]] constexpr bool active(const Source& item,std::uint8_t cycle,
    bool eyeStatusActive) noexcept {
    return cycle>=1 && cycle<=3 && item.cycle==cycle && eyeStatusActive;
}

template<class Writer>
[[nodiscard]] bool write_authority(Writer& writer,bool enabled) noexcept {
    // Original4BEE90 decodes8080954B into0x70bytes. Both state-control bools
    // stay0 while enabled; inactive suppresses enumeration and removes the
    // dynamic filter. Original9F1F10 then removes each exact player/effect pair.
    // Do not advance the broader clear or subscriber channels for this lifetime.
    if(!writer.write(0,1) || !writer.write(enabled?0U:1U,1)) { return false; }
    for(unsigned channel=0;channel<4;++channel) {
        if(!writer.write(0x80000000U,32)) { return false; }
    }
    if(!writer.write(0x811C9DC5U,32) || !writer.write(0,7)
        || !writer.write(0x7FFFU,16) || !writer.write(enabled?1U:0U,1)) { return false; }
    if(!enabled) { return true; }
    // Runtime schema80809155 selects player entities through the nested
    // all-current-players selector8080915D. Native9EF940/A58420/A566C0 keep
    // full player keys and resolve full entity handles; no client HUD injection.
    return writer.write(0x80809155U,32) && writer.write(1,1)
        && writer.write(0x8080915DU,32);
}

} // namespace sunrise::state::activity::omega_crown_eye_status
