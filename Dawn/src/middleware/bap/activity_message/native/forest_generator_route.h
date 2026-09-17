#pragma once
#include "forest_generator_authority.h"

namespace dawn::middleware::bap::activity_message::native::forest_generator {

/** The four endpoint groups in the native type-37 order. */
enum class Side : std::uint8_t {positiveX,negativeX,positiveY,negativeY};
inline constexpr std::size_t kSideCount=4;

struct Grid final {
    std::uint8_t columns{},heights{};
    friend constexpr bool operator==(const Grid&,const Grid&)=default;
};

struct Endpoint final {
    Side side{};
    std::int8_t column{},height{};
    bool active{true};
    friend constexpr bool operator==(const Endpoint&,const Endpoint&)=default;
};

/** A complete two-ended route contract for a type-37 generator recipe. */
struct Route final {
    Grid grid{};
    Endpoint entrance{},exit{};
    // The native solver receives -1 columns for these groups, but their authored
    // heights are retained because the worker still reads the full anchor block.
    std::array<std::int8_t,kSideCount> unusedHeights{};
    friend constexpr bool operator==(const Route&,const Route&)=default;
};

[[nodiscard]] constexpr std::size_t side_index(Side side) noexcept {
    const auto value=static_cast<std::uint8_t>(side);
    return value<kSideCount?value:kSideCount;
}
[[nodiscard]] constexpr bool valid_side(Side side) noexcept { return side_index(side)<kSideCount; }
[[nodiscard]] constexpr bool in_grid(const Grid& grid,std::int8_t column,std::int8_t height) noexcept {
    return column>=0 && height>=0
        && static_cast<std::uint8_t>(column)<grid.columns
        && static_cast<std::uint8_t>(height)<grid.heights;
}
[[nodiscard]] constexpr bool has_side(const Route& route,std::size_t index) noexcept {
    return side_index(route.entrance.side)==index || side_index(route.exit.side)==index;
}

[[nodiscard]] constexpr bool valid(const Route& route) noexcept {
    // Nonnegative endpoint coordinates are stored in signed bytes. A dimension
    // of 128 therefore covers the complete representable nonnegative range.
    if(!route.grid.columns || !route.grid.heights
        || route.grid.columns>128 || route.grid.heights>128
        || !valid_side(route.entrance.side) || !valid_side(route.exit.side)
        || route.entrance.side==route.exit.side
        || !in_grid(route.grid,route.entrance.column,route.entrance.height)
        || !in_grid(route.grid,route.exit.column,route.exit.height))return false;
    return true;
}

/** Resolve a route without exposing a partially changed State on failure. */
[[nodiscard]] constexpr bool resolve_route(const Route& route,State& destination) noexcept {
    if(!valid(route))return false;
    auto resolved=destination;
    for(std::size_t index=0;index<kSideCount;++index)
        resolved.primary.anchors[index]={-1,route.unusedHeights[index],0.F,false};
    resolved.primary.anchors[side_index(route.entrance.side)]={
        route.entrance.column,route.entrance.height,0.F,route.entrance.active};
    resolved.primary.anchors[side_index(route.exit.side)]={
        route.exit.column,route.exit.height,1.F,route.exit.active};
    resolved.primary.overrides=static_cast<std::uint8_t>(resolved.primary.overrides|Anchors);
    destination=resolved;
    return true;
}

} // namespace dawn::middleware::bap::activity_message::native::forest_generator
