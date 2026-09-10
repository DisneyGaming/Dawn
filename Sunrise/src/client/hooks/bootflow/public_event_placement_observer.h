#pragma once
#include "../../../server/runtime/activity/public_event_native_bridge.h"

namespace sunrise::client::hooks::bootflow::public_event_placement_observer {
namespace bridge=server::runtime::activity::public_event::native_bridge;
namespace feedback=server::runtime::activity::public_event::placement_feedback;
// Stack-owned across one existing9F19F0 invocation, never a retained native
// pointer. No separate hook, discovery poll, native apply or constructor call.
struct Context final {
    bridge::Binding binding{};
    std::uintptr_t component{};
    feedback::SourceReference source{};
    std::uint32_t authorityHandle{UINT32_MAX};
    std::array<std::byte,0x70> authorityObject{};
    std::array<std::byte,feedback::kAuthorityBytes> authorityBody{};
    std::array<std::byte,feedback::kComponentBytes> priorComponent{};
    std::uint32_t priorEntity{UINT32_MAX},priorEntityFlags{};
};
[[nodiscard]] Context begin(void* component) noexcept;
// Returns only whether a qualified value was queued. Failure changes no native
// state, cannot prevent forwarding, and cannot substitute a readiness receipt.
[[nodiscard]] bool finish(void* component,const Context&) noexcept;
}
