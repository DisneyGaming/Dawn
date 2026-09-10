#pragma once
#include "../../../state/activity/native_population_events.h"

namespace sunrise::client::hooks::bootflow {
// Value-only notification after the existing native admission has been accepted.
// This observer never requests animation, changes locomotion or publishes authority.
void observe_vance_contact_admission(const state::activity::native_population::Event&,
                                    std::uint32_t parent) noexcept;
void observe_vance_contact_retirement(const state::activity::native_population::Event&) noexcept;
[[nodiscard]] bool install_vance_contact_observer() noexcept;
void quiesce_vance_contact_observer() noexcept;
[[nodiscard]] bool uninstall_vance_contact_observer() noexcept;
}
