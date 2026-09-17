#pragma once
#include "../../../server/runtime/activity/ambient_population_named_points.h"
#include <cstring>

namespace dawn::client::hooks::bootflow::ambient_named_identity {
namespace points=server::runtime::activity::ambient_population::named_points;
template<class T> [[nodiscard]] T field(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};if(offset<=bytes.size() && sizeof(T)<=bytes.size()-offset)std::memcpy(&value,bytes.data()+offset,sizeof(T));return value;
}
[[nodiscard]] inline bool placement(const points::Binding& binding,std::uint32_t index,
    std::span<const std::byte> bytes,points::Point& point) noexcept {
    point={};if(!binding.epoch || bytes.size()!=144 || index>=binding.ticket.placementCount)return false;
    const points::Point actual{index,field<std::uint32_t>(bytes,0),field<std::uint64_t>(bytes,0x70)};
    for(std::size_t i=0;i<binding.ticket.count;++i)if(actual==binding.ticket.points[i]) {point=actual;return true;}
    return false;
}
[[nodiscard]] inline bool resident(const points::Binding& binding,const points::Point& point,
    std::uint32_t handle,std::span<const std::byte> bytes) noexcept {
    return binding.epoch && handle!=UINT32_MAX && bytes.size()>=0x98
        && field<std::uint32_t>(bytes,0x0C)==handle
        && field<std::uint32_t>(bytes,0x88)==binding.ticket.list
        && field<std::uint32_t>(bytes,0x8C)==point.index && field<std::uint64_t>(bytes,0x90)==point.guid;
}
}
