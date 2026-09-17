#pragma once
#include "../../../server/runtime/activity/ambient_population_named_points.h"
namespace dawn::client::hooks::bootflow {
struct AmbientNamedConstruction final {
    server::runtime::activity::ambient_population::named_points::Binding binding{};
    server::runtime::activity::ambient_population::named_points::Point point{};
    std::array<std::byte,144> placement{};
};
// Value-only qualification around native575690. Dormant without an exact ticket;
// the owning hook forwards the original exactly once.
[[nodiscard]] AmbientNamedConstruction capture_ambient_named_construction(
    const void* placement,std::uint32_t list,std::uint32_t index) noexcept;
void complete_ambient_named_construction(const AmbientNamedConstruction& captured,
    const void* placement,const void* output,const void* returned) noexcept;
// Read-only original80807F35 lookup through the existing qualified trampoline.
// Caller owns exact current entity and output qualification; no named-list observation.
[[nodiscard]] bool read_native_point_interface(const void* entity, std::span<std::byte,48> output) noexcept;
[[nodiscard]] bool install_ambient_population_named_observer() noexcept;
void quiesce_ambient_population_named_observer() noexcept;
[[nodiscard]] bool uninstall_ambient_population_named_observer() noexcept;
}
