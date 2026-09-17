#pragma once
#include "public_event_participant_capture.h"

namespace dawn::client::hooks::bootflow::public_event_participant_observer {
[[nodiscard]] bool install() noexcept;
void quiesce() noexcept;
[[nodiscard]] bool uninstall() noexcept;
// Prefix-qualified, unchanged native read-only accessors. A stable pair of
// reads is required; an unqualified identity is never replaced with a guess.
[[nodiscard]] bool local_identity(public_event_participant_capture::feedback::LocalIdentity&) noexcept;
// Called only by the qualified local player's game-thread position publish.
void poll_local_identity() noexcept;
}
