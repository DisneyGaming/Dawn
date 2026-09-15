#pragma once

#include "native_activity_definition.h"
#include "open_world_definition.h"
#include <array>

namespace sunrise::server::runtime::activity::open_world::profiles {

namespace catalog=state::activity::coo::open_world;

template<std::size_t R,std::size_t P>
[[nodiscard]] consteval std::array<population::Capability,P> make_populations(
    const std::array<registry::Definition,R>& registries,
    const std::array<catalog::PopulationBinding,P>& bindings) {
    std::array<population::Capability,P> result{};
    for(std::size_t i=0;i<P;++i) {
        const auto& binding=bindings[i];const auto& registry=registries[binding.registry];
        result[i]={&registry,binding.source,binding.rule,
            binding.tacticalRow<0?population::codec::TacticalGroup{}
                :population::codec::TacticalGroup{registry.key,binding.tactical,binding.tacticalRow},
            binding.hasRule,binding.tacticalRows?(1U<<binding.tacticalRows)-1U:0U};
    }
    return result;
}

template<std::size_t R,std::size_t P>
[[nodiscard]] consteval std::array<placement::Capability,P> make_placements(
    const std::array<registry::Definition,R>& registries,
    const std::array<catalog::PlacementBinding,P>& bindings) {
    std::array<placement::Capability,P> result{};
    for(std::size_t i=0;i<P;++i)result[i]={&registries[bindings[i].registry],bindings[i].slot};
    return result;
}

template<std::size_t R,std::size_t P>
[[nodiscard]] consteval std::array<adventure_start::Route,P> make_routes(
    const catalog::Destination& destination,const std::array<registry::Definition,R>& registries,
    const std::array<catalog::AdventureBinding,P>& bindings) {
    std::array<adventure_start::Route,P> result{};
    for(std::size_t i=0;i<P;++i) {
        const auto& binding=bindings[i];const auto& registry=registries[binding.registry];
        result[i]={destination.scenario,destination.activity,registry.key,binding.slot,
            registry.bubble,static_cast<std::int16_t>(binding.activity)};
    }
    return result;
}

[[nodiscard]] consteval coo::Asset source_asset(const population::Capability& capability) {
    coo::Asset result{capability.registry->key,0,1,capability.slot};
    for(const auto& slot:capability.registry->slots)
        if(slot.index==capability.slot && slot.type==1)result.definition=slot.descriptorTag;
    return result;
}

template<const auto& Destination,const auto& Registries,const auto& PopulationBindings,
    const auto& PlacementBindings,const auto& AdventureBindings>
struct Data final {
    inline static constexpr auto populations=make_populations(Registries,PopulationBindings);
    inline static constexpr auto placements=make_placements(Registries,PlacementBindings);
    inline static constexpr auto routes=make_routes(Destination,Registries,AdventureBindings);
    inline static constexpr auto bootstrap=source_asset(populations.front());
    inline static constexpr Definition runtime{&Destination};
};

#define SUNRISE_OPEN_WORLD_PROFILE(NAME,AUTHORED_NS,SCRIPT_FILE,PROFILE_ID) \
namespace NAME { \
namespace authored=catalog::AUTHORED_NS; \
using Storage=Data<authored::kDestination,authored::kRegistries,authored::kPopulations, \
    authored::kPlacements,authored::kAdventures>; \
inline constexpr coo::ModuleBinding kPersistentModule{ \
    {authored::kDestination.scenario,authored::kDestination.scenario,0,0},1}; \
inline constexpr coo::script::Capability kCapabilities[]{ \
    {"persistent.start","composition",{coo::Operation::mechanic,kPersistentModule.asset,1,coo::Wait::requested}}, \
    {"bootstrap.population","nativeActivity",{coo::Operation::population,Storage::bootstrap,1,coo::Wait::requested}}, \
}; \
inline constexpr NativeAction kActions[]{ \
    {kCapabilities[1].spec,0,"freeroam_patrol_count"}, \
}; \
inline constexpr coo::script::ModuleCapability kModules[]{{"persistent",kPersistentModule}}; \
inline constexpr coo::script::ParameterCapability kParameters[]{ \
    {"freeroam_population_enabled",1,1,1,false}, \
    {"freeroam_respawn_ms",1000,3600000,30000,false}, \
    {"freeroam_patrol_count",1,21,1,false}, \
    {"freeroam_npc_count",1,1,1,false}, \
    {"host.tick_hz",1,120,30,false}, \
}; \
inline const coo::script::Profile kProfile{PROFILE_ID,"nativeOtherActivities", \
    coo::Schema::otherMissions,kCapabilities,kModules,{}, {}, {}, {}, {},{},kParameters}; \
inline const NativeActivityDefinition kActivity{authored::kDestination.activity,SCRIPT_FILE, \
    authored::kDestination.primaryBubble,&kProfile,authored::kRegistries,Storage::populations, \
    Storage::placements,kActions,kPersistentModule,{},Storage::routes,{},{},{},{},{},{}, \
    "host.tick_hz",{},{},{},{},{},{},true,&Storage::runtime}; \
}

SUNRISE_OPEN_WORLD_PROFILE(io,io,L"eden_freeroam.json","io.freeroam.native.v1")
SUNRISE_OPEN_WORLD_PROFILE(titan,titan,L"fleet_freeroam.json","titan.freeroam.native.v1")
SUNRISE_OPEN_WORLD_PROFILE(mars,mars,L"polaris_freeroam.json","mars.freeroam.native.v1")
SUNRISE_OPEN_WORLD_PROFILE(nessus,nessus,L"planet_x_freeroam.json","nessus.freeroam.native.v1")
SUNRISE_OPEN_WORLD_PROFILE(tangled_shore,tangled_shore,L"tangled_shore_freeroam.json","tangled_shore.freeroam.native.v1")
SUNRISE_OPEN_WORLD_PROFILE(dreaming_city,dreaming_city,L"dreaming_city_freeroam.json","dreaming_city.freeroam.native.v1")

#undef SUNRISE_OPEN_WORLD_PROFILE

inline constexpr std::array<const NativeActivityDefinition*,6> kActivities{{
    &io::kActivity,&titan::kActivity,&mars::kActivity,&nessus::kActivity,
    &tangled_shore::kActivity,&dreaming_city::kActivity,
}};

} // namespace sunrise::server::runtime::activity::open_world::profiles
