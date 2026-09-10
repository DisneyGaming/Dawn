#pragma once
#include "public_event_deferred_placement_capture.h"
namespace sunrise::client::hooks::bootflow::public_event_deferred_placement_observer {
using Context=public_event_deferred_placement_capture::Context;
[[nodiscard]] Context begin(void* component) noexcept;
[[nodiscard]] bool finish(void* component,const Context&,bool created) noexcept;
void refresh(void* component) noexcept;
// Read-only full native entity/holder projection, without local-player fallback.
[[nodiscard]] bool live_entity(std::uint32_t entity) noexcept;
[[nodiscard]] std::uint32_t holder_context(const void* context) noexcept;
}
