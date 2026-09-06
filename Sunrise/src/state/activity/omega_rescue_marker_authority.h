#pragma once

#include <array>
#include <cstdint>

#include "omega_first_mancannon_authority.h"
#include "omega_rescue_scene_authority.h"

namespace sunrise::state::activity::omega_rescue_markers {

/** These nine authored activity objects contain only the 80F573E7 transform
 * helper. Native Scene child creation binds their world entities to timeline
 * roles; an inline source transform alone cannot satisfy DB4630/9F0D40. */
struct Marker final {
    std::uint16_t slot;
    std::uint16_t scene;
    std::uint32_t definition;
};
inline constexpr std::array<Marker,9> kMarkers{{
    {101,83,0x80F47AE7U},{102,83,0x80F47AEAU},{103,83,0x80F47AEDU},
    {70,66,0x80F47A7FU},{71,66,0x80F47A82U},{72,66,0x80F47A85U},
    {104,84,0x80F47AF0U},{105,84,0x80F47AF3U},{106,84,0x80F47AF6U},
}};
inline constexpr std::uint16_t kAllReady=0x1FFU;

[[nodiscard]] constexpr const Marker* find(std::uint16_t slot) noexcept {
    for(const auto& marker:kMarkers) { if(marker.slot==slot) { return &marker; } }
    return nullptr;
}
[[nodiscard]] constexpr const Marker* definition(std::uint32_t resource) noexcept {
    for(const auto& marker:kMarkers) { if(marker.definition==resource) { return &marker; } }
    return nullptr;
}
[[nodiscard]] constexpr std::uint16_t bit(std::uint16_t slot) noexcept {
    for(std::size_t i=0;i<kMarkers.size();++i) {
        if(kMarkers[i].slot==slot) { return static_cast<std::uint16_t>(1U<<i); }
    }
    return 0;
}
[[nodiscard]] constexpr std::uint16_t required(std::uint16_t scene) noexcept {
    std::uint16_t result{};
    for(const auto& marker:kMarkers) {
        if(marker.scene==scene) { result=static_cast<std::uint16_t>(result|bit(marker.slot)); }
    }
    return result;
}
[[nodiscard]] constexpr bool requested(const omega_rescue_npc::Commands& commands,
                                     const Marker& marker) noexcept {
    const auto* value=omega_rescue_npc::command(commands,marker.scene);
    return value!=nullptr && value->generation!=0 && omega_rescue_npc::valid(*value);
}
/** Projection does not alter the desired generation or retained events. Once
 * all exact native entities exist, the original command is published unchanged. */
[[nodiscard]] constexpr omega_rescue_npc::SceneCommand project(
    std::uint16_t scene,const omega_rescue_npc::SceneCommand& desired,
    std::uint16_t ready) noexcept {
    const auto mask=required(scene);
    return (ready&mask)==mask?desired:omega_rescue_npc::SceneCommand{};
}
[[nodiscard]] constexpr bool created(std::uint32_t generation,
                                    std::uint32_t applied,std::uint32_t committed,
                                    bool active,std::uint32_t entity) noexcept {
    return generation!=0 && generation<0x7FFFFFFFU && applied==generation+1U
        && committed==applied && active && entity!=UINT32_MAX;
}
/** Every marker definition has +94=1: deferred creation requires the newer
 * active generation. Markers remain alive after Scene stop, until run teardown,
 * so nested children cannot lose their bound entity during their native exit. */
template<class Writer>
[[nodiscard]] bool write_authority(Writer& writer,std::uint32_t generation,
                                   bool active) noexcept {
    if(generation>=0x7FFFFFFFU || (active && generation==0)) { return false; }
    return omega_first_mancannon::write_authority(writer,generation+(active?1U:0U),active);
}

} // namespace sunrise::state::activity::omega_rescue_markers
