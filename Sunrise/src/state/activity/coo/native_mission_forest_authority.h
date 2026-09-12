#pragma once
#include "native_generator_authority.h"
namespace sunrise::state::activity::coo::native_generator {
// Host-owned recipes recovered from the former mission worker repairs. Native
// type37 apply and worker change detection deliver every input and choose when
// to regenerate. Ownership of replicated gateway entities is separate.
[[nodiscard]] constexpr std::uint32_t mission_seed(std::uint64_t run,std::uint32_t generation,std::uint8_t pass) noexcept {
    auto value=run^(static_cast<std::uint64_t>(generation)<<32)^(0x9E3779B97F4A7C15ULL*pass);
    value=(value^(value>>30))*0xBF58476D1CE4E5B9ULL;
    value=(value^(value>>27))*0x94D049BB133111EBULL;
    const auto result=static_cast<std::uint32_t>(value^(value>>31))&0x7FFFFFFFU;
    return result?result:1U;
}
[[nodiscard]] constexpr Request omega_request(std::uint32_t seed,bool enabled=true) noexcept {
    Request request{};request.enabled=enabled;request.seed=seed;request.selectSeed=true;
    request.selectAnchors=true;request.topology={0.F,0.F};
    request.anchors={{{-1,2,0.F,false},{-1,0,0.F,false},{2,0,0.F,true},{1,2,1.F,false}}};
    return request;
}
[[nodiscard]] constexpr Request beyond_request(std::uint32_t seed,std::uint8_t pass,bool ready) noexcept {
    Request request{};request.enabled=ready && pass>=1 && pass<=2;request.seed=seed;
    request.selectSeed=seed!=0;request.selectAnchors=pass>=1 && pass<=2;request.topology={0.F,0.F};
    constexpr std::array<std::int8_t,4> heights{0,0,1,0};
    const std::array<std::int8_t,4> columns=pass==1?std::array<std::int8_t,4>{-1,0,1,-1}:std::array<std::int8_t,4>{0,0,-1,-1};
    const auto entrance=pass==1?2U:1U,target=pass==1?1U:0U;
    for(std::size_t i=0;i<4;++i)request.anchors[i]={columns[i],heights[i],i==target?1.F:0.F,i==entrance};
    return request;
}
}
