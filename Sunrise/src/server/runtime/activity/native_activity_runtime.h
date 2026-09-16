#pragma once
#include "persistent_activity.h"
#include "../../../state/activity/coo/native_player_trigger.h"
namespace sunrise::server::runtime::activity::native_activity {
using Owner=population::Owner;
// Uses the same retained document as update, before the roster is published.
[[nodiscard]] bool optional_registries(const NativeActivityDefinition& definition,
    ambient_population::RegistryBatch& output) noexcept;
// Caller has already validated creator-root ownership and admitted the profile's
// exact package registries. A sent frame is not a native readiness receipt.
// experimentalRewardPlacements admits the round definition's gated reward placements. It stays
// false unless the host switch is on: placement authority validates a frame as a whole, so one
// roster slot without the auth flag voids every placement in it.
[[nodiscard]] NativeActivityFrame update(Owner owner,std::uint32_t bubble,bool arrived,
    const NativeActivityDefinition& definition,const adventure_start::wire::Request& selected={},
    bool openingAdmissionReady=true,std::uint32_t populationPrefetchBubble=UINT32_MAX,bool experimentalRewardPlacements=false) noexcept;
void observe(Owner owner,std::uint32_t bubble,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept;
bool observe_player_trigger(Owner owner,
    const state::activity::coo::native_player_trigger::Receipt& receipt) noexcept;
// Read-only round phase, completed-branch count and population boot for a live round activity.
// The boot is the round service's own token, not a second clock-domain read.
// Never starts a script, an entry or an update tick.
[[nodiscard]] bool round_progress(Owner owner,timed_round::Phase& phase,
    std::uint64_t& completedRounds,std::uint64_t& boot) noexcept;
[[nodiscard]] bool publication_pending(Owner owner) noexcept;
/** Read-only respawn-suppression view of one live round activity. Never starts or ticks anything. */
struct RespawnState final {
    bool valid{};
    bool suppressed{};
    bool restricted{};
    bool travelArrivalQualified{};
    bool latched{};
    timed_round::Phase phase{timed_round::Phase::entry};
};
[[nodiscard]] RespawnState respawn_state(Owner owner) noexcept;
void observe_player_life(Owner owner,std::uint64_t boot,std::uint32_t entity,bool alive) noexcept;
/** Read-only snapshot; never starts a script or executes an update tick. */
[[nodiscard]] bool snapshot_placements(Owner owner,const NativeActivityDefinition*& definition,
    placement::wire::Batch& output) noexcept;
// Project an already admitted creator clock for another qualified native
// activity recipient. Never initializes an Entry, script or clock domain.
[[nodiscard]] bool snapshot_clock(Owner owner,activity_clock::Publication& output) noexcept;
}
