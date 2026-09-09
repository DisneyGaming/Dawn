#pragma once
#include "frame.h"
#include "../coo/native_device_authority.h"
namespace sunrise::state::activity::beyond_infinity::shields {
inline constexpr coo::Asset kBackEffect{0x0FF26BCCU,0x80F4606DU,26,15};
inline constexpr coo::Asset kFrontEffect{0x0FF26BCCU,0x80F46070U,26,16};
inline constexpr coo::Asset kFrontFilter{0x0FF26BCCU,0x80F46018U,34,19};
inline constexpr coo::Asset kBackFilter{0x0FF26BCCU,0x80F4601BU,34,20};
inline constexpr std::array<std::uint16_t,6> kFrontSources{6,7,8,9,10,11};
inline constexpr std::array<std::uint16_t,3> kBackSources{12,13,14};
inline constexpr std::size_t bits(coo::Asset asset,const NativeState& state) noexcept {
    if(!state.managed) { return 0; }
    if(asset==kFrontEffect || asset==kBackEffect) { return 186; }
    if(asset==kFrontFilter) { return 4+(state.active?kFrontSources.size()*90:0); }
    if(asset==kBackFilter) { return 4+(state.active?kBackSources.size()*90:0); }
    return 0;
}
template<class Writer> bool write(Writer& writer,coo::Asset asset,const NativeState& state) noexcept {
    if(!bits(asset,state)) { return false; }
    // Both packaged effect definitions retain80F4596D at+B20. Only their
    // linked native collections and enabled state are supplied by authority.
    if(asset==kFrontEffect || asset==kBackEffect) {
        return coo::native_device::linked_effect(writer,asset.registry,
            asset==kFrontEffect?kFrontFilter.slot:kBackFilter.slot,state.active);
    }
    const std::span<const std::uint16_t> selected=asset==kFrontFilter
        ?std::span<const std::uint16_t>(kFrontSources):std::span<const std::uint16_t>(kBackSources);
    return coo::native_device::collection_sources(writer,asset.registry,
        state.active?selected:std::span<const std::uint16_t>{});
}
}
