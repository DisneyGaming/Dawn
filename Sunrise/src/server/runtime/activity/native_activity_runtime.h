#pragma once
#include "persistent_activity.h"
namespace sunrise::server::runtime::activity::native_activity {
using Owner=population::Owner;
// Uses the same retained document as update, before the roster is published.
[[nodiscard]] bool optional_registries(const NativeActivityDefinition& definition,
    ambient_population::RegistryBatch& output) noexcept;
// Caller has already validated creator-root ownership and admitted the profile's
// exact package registries. A sent frame is not a native readiness receipt.
[[nodiscard]] NativeActivityFrame update(Owner owner,std::uint32_t bubble,bool arrived,
    const NativeActivityDefinition& definition,const adventure_start::wire::Request& selected={},
    bool openingAdmissionReady=true) noexcept;
void observe(Owner owner,std::uint32_t bubble,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept;
/** Read-only snapshot; never starts a script or executes an update tick. */
[[nodiscard]] bool snapshot_placements(Owner owner,const NativeActivityDefinition*& definition,
    placement::wire::Batch& output) noexcept;
// Project an already admitted creator clock for another qualified native
// activity recipient. Never initializes an Entry, script or clock domain.
[[nodiscard]] bool snapshot_clock(Owner owner,activity_clock::Publication& output) noexcept;
}
