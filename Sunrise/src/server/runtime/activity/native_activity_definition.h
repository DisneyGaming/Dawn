#pragma once
#include "population_service.h"
#include "placement_service.h"
#include "native_npc_animation_service.h"
#include "adventure_start_plan.h"
#include "ambient_population_definition.h"
#include "public_event_rally_runtime.h"
#include "public_event_initial_runtime.h"
#include "adventure_opening_runtime.h"
#include "native_occupancy_wait.h"
#include "native_capture_runtime.h"
#include "forest_generator_service.h"
#include "cue_presentation_service.h"
#include "world_device_service.h"
#include "../../../state/activity/coo/mission_script.h"

namespace sunrise::server::runtime::activity {
namespace coo=state::activity::coo;
namespace open_world { struct Definition; }
// Trusted package/profile data. Files select registered operations; they do
// not define wire layouts, native pointers or destinations for teleports.
struct NativeAction final {
    coo::CommandSpec command{};
    std::uint16_t capability{};
    std::string_view countParameter{};
};
struct NativeActivityDefinition final {
    std::string_view activity;
    const wchar_t* scriptFile{};
    std::uint8_t bubble{};
    const coo::script::Profile* profile{};
    std::span<const registry::Definition> registries;
    std::span<const population::Capability> populations;
    std::span<const placement::Capability> placements;
    std::span<const NativeAction> actions;
    coo::ModuleBinding persistentModule{};
    // Optional native type-42 controllers. Empty preserves existing activity
    // behavior; capability presence alone never requests an animation.
    std::span<const npc_animation::Capability> animations{};
    /** Authored in-world selections allowed by already-requested native devices. */
    std::span<const adventure_start::Route> startRoutes{};
    /** Definition-counted initial populations gated by native monitor occupancy. */
    std::span<const ambient_population::InitialBinding> ambientInitial{};
    /** Optional profile-selected rally placements with qualified native receipts. */
    std::span<const public_event::RallyBinding> publicEventRallies{};
    /** Selected in-world activity openings using authored co-resident groups. */
    std::span<const adventure::OpeningBinding> adventureOpenings{};
    /** Qualified native occupancy waits available to the activity's UE graph. */
    std::span<const occupancy_wait::Binding> occupancyWaits{};
    /** Empty permits any ordinal for this package; explicit entries restrict variants. */
    std::span<const std::int32_t> activityOrdinals{};
    /** Optional authored registry dependencies selected by retained UE parameters. */
    std::span<const ambient_population::RegistryBinding> optionalRegistries{};
    /** Empty preserves legacy timing bytes. Positive policy selects native type2/type5 clock authority. */
    std::string_view clockFrequencyParameter{};
    std::span<const native_capture::Binding> captures{};
    /** Optional authored type-37 generators, activated only by UE requests. */
    std::span<const forest_generator::Capability> generators{};
    /** Authored directive variants; actual top-level/local admission is checked by the wire roster. */
    std::span<const cue_presentation::Action> directives{};
    coo::Asset directiveSource{};
    /** Optional native type-23 channels; actions retain authored device animation. */
    std::span<const world_device::Capability> devices{};
    std::span<const public_event::InitialDefinition> publicEventInitials{};
    bool retainRosterOrdinals{false};
    /** Shared package-derived free-roam behavior. Empty preserves specialized profiles. */
    const open_world::Definition* openWorld{};
};
}
