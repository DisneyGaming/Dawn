#pragma once
#include "../coo/native_generator_authority.h"

namespace dawn::state::activity::strike_bond {

namespace forest_wire=middleware::bap::activity_message::native::forest_generator;
namespace forest_native=coo::native_generator;

// Forest C's recovered native order is +X, -X, +Y, -Y. Only the south
// entrance and north exit are selected; the side heights remain authored data
// even though their columns are intentionally absent from the solver.
inline constexpr forest_wire::Route kForestRoute{
    {3,3},
    {forest_wire::Side::negativeY,1,0,true},
    {forest_wire::Side::positiveY,1,2,true},
    {1,0,0,0},
};
static_assert(forest_wire::valid(kForestRoute));

// Garden's native generator budget and authored density sentinels are data;
// the run seed is supplied by the existing chronology at publication time.
[[nodiscard]] consteval forest_native::Request make_forest_request_base() {
    forest_native::Request request{};
    request.values[0]=6;
    request.topology=forest_native::kAuthoredTopologies;
    forest_native::Request resolved{};
    if(!forest_native::build_route_request(request,kForestRoute,resolved))
        throw "invalid Forest route";
    return resolved;
}
inline constexpr forest_native::Request kForestRequestBase=make_forest_request_base();
static_assert(kForestRequestBase.selectAnchors);

inline forest_native::Request forest_request(std::uint32_t seed) noexcept {
    auto request=kForestRequestBase;
    request.seed=seed;
    return request;
}

} // namespace dawn::state::activity::strike_bond
