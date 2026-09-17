#pragma once
#include "public_event_engagement_capture.h"
namespace dawn::client::hooks::bootflow::public_event_engagement_observer {
using Context=public_event_engagement_capture::Context;
// Wrap existing omega_engagement_apply only. Original call/return stays intact.
[[nodiscard]] Context begin(void* component,const void* packet) noexcept;
[[nodiscard]] bool finish(void* component,const Context& before) noexcept;
}
