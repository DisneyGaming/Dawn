#pragma once

#include "server/runtime/activity/open_world_definitions.h"
#include "server/runtime/activity/open_world_runtime.h"
#include "state/activity/coo/native_combatant_authority.h"
#include "middleware/encoding/bit_writer.h"
#include <array>
#include <fstream>

inline void open_world_cases() {
    namespace activity=sunrise::server::runtime::activity;
    namespace profiles=activity::open_world::profiles;
    namespace authored=sunrise::state::activity::coo::open_world;
    constexpr std::array<const char*,6> scripts{{
        "Sunrise/scripts/eden_freeroam.json","Sunrise/scripts/fleet_freeroam.json",
        "Sunrise/scripts/polaris_freeroam.json","Sunrise/scripts/planet_x_freeroam.json",
        "Sunrise/scripts/tangled_shore_freeroam.json","Sunrise/scripts/dreaming_city_freeroam.json",
    }};
    constexpr std::array<std::size_t,6> populations{{17,9,9,21,17,21}};
    constexpr std::array<std::size_t,6> npcs{{1,1,1,1,1,3}};
    constexpr std::array<std::size_t,6> placements{{3,3,2,2,1,3}};
    constexpr std::array<std::size_t,6> adventures{{3,3,2,2,6,3}};
    CHECK(profiles::kActivities.size()==scripts.size());
    for(std::size_t world=0;world<profiles::kActivities.size();++world) {
        const auto& definition=*profiles::kActivities[world];
        CHECK(definition.openWorld);CHECK(activity::open_world::valid(*definition.openWorld));
        const auto& destination=*definition.openWorld->authored;
        CHECK(definition.activity==destination.activity);CHECK(definition.bubble==destination.primaryBubble);
        CHECK(definition.registries.data()==destination.registries.data());
        CHECK(definition.registries.size()+5<=32);
        CHECK(definition.populations.size()==populations[world]);
        CHECK(destination.populations.size()==populations[world]);
        CHECK(definition.placements.size()==placements[world]);
        CHECK(definition.startRoutes.size()==adventures[world]);
        CHECK(destination.adventures.size()==adventures[world]);
        CHECK(definition.publicEventRallies.empty());CHECK(definition.publicEventInitials.empty());
        CHECK(definition.adventureOpenings.empty());
        std::size_t npcCount{};
        for(std::size_t i=0;i<definition.populations.size();++i) {
            CHECK(activity::population::valid(definition.populations[i]));
            const auto& binding=destination.populations[i];
            CHECK(binding.tacticalRows<=24);
            CHECK(definition.populations[i].taskMask==(binding.tacticalRows?(1U<<binding.tacticalRows)-1U:0U));
            if(destination.populations[i].kind==authored::PopulationKind::npc)++npcCount;
        }
        CHECK(npcCount==npcs[world]);
        for(const auto& registry:definition.registries) {
            CHECK(activity::registry::valid(registry));
            CHECK(authored::required(registry.scenario,registry.objectTag,registry.key,
                std::uint64_t{1}<<registry.bubble));
            CHECK(!authored::required(registry.scenario,registry.objectTag,registry.key,0));
        }
        activity::placement::wire::Batch allFlags{};
        CHECK(activity::open_world::append_all_placements(definition.placements,allFlags));
        CHECK(allFlags.count==definition.placements.size());
        for(const auto& route:definition.startRoutes) {
            CHECK(route.scenario==destination.scenario);CHECK(route.rootPackage==destination.activity);
            bool published{};
            for(std::size_t i=0;i<allFlags.count;++i) {
                const auto& flag=allFlags.entries[i];
                if(flag.registry==route.registry && flag.slot==route.slot && flag.bubble==route.bubble)published=true;
            }
            CHECK(published);
        }
        std::ifstream input(scripts[world],std::ios::binary);CHECK(input.good());
        const std::string text((std::istreambuf_iterator<char>(input)),{});std::string error;
        auto document=sunrise::state::activity::coo::script::MissionDocument::parse(text,*definition.profile,error);
        if(!document)std::fprintf(stderr,"%s: %s\n",scripts[world],error.c_str());
        CHECK(document);CHECK(activity::PersistentActivity::valid(definition,*document));
        const activity::population::Owner owner{0xD3570000U+world,{1}};constexpr std::uint64_t boot=77;
        activity::population::Service service;CHECK(service.begin(owner,definition.populations,boot));
        CHECK(service.project_retained().count==0);
        activity::open_world::Configuration configuration{};
        CHECK(activity::open_world::configure(*document,configuration));
        CHECK(configuration.patrolRequests==1 && configuration.npcRequests==1);
        activity::open_world::Director director;
        CHECK(director.begin(owner,boot,*definition.openWorld,definition.populations,configuration));
        std::array<sunrise::state::activity::coo::NativePopulationLedger<64>,32> ledgers{};
        std::array<std::uint8_t,32> pending{};std::array<bool,64> bubbles{};
        for(const auto& capability:definition.populations)bubbles[capability.registry->bubble]=true;
        std::uint64_t now=1000;std::size_t started{};
        for(std::size_t bubble=0;bubble<bubbles.size();++bubble)if(bubbles[bubble]) {
            CHECK(director.update(now++,static_cast<std::uint32_t>(bubble),true,service,
                std::span<const sunrise::state::activity::coo::NativePopulationLedger<64>>(ledgers),
                std::span<const std::uint8_t>(pending)));
            const auto projected=service.project(static_cast<std::uint32_t>(bubble));
            std::size_t expected{};
            for(const auto& capability:definition.populations)
                if(capability.registry->bubble==bubble)++expected;
            CHECK(projected.count==expected);
            started+=expected;
            const auto retained=service.project_retained();CHECK(retained.count==started);
            // Crossing into a zone with no new request must not clear the
            // objective, selected row, spawn rule, quota or source generation.
            CHECK(director.update(now++,63,true,service,
                std::span<const sunrise::state::activity::coo::NativePopulationLedger<64>>(ledgers),
                std::span<const std::uint8_t>(pending)));
            CHECK(service.project(63).count==0);
            const auto away=service.project_retained();CHECK(away.count==retained.count);
            for(std::size_t r=0;r<retained.count;++r) {
                const auto& before=retained.entries[r];const auto& after=away.entries[r];
                CHECK(after.bubble==before.bubble && after.slot==before.slot);
                CHECK(after.source.registry==before.source.registry);
                CHECK(after.source.generation==before.source.generation);
                CHECK(after.source.looseRequested==before.source.looseRequested);
                CHECK(after.source.ruleSlot==before.source.ruleSlot);
                CHECK(after.source.hasSpawnRule==before.source.hasSpawnRule);
                CHECK(after.source.tactical.registry==before.source.tactical.registry);
                CHECK(after.source.tactical.slot==before.source.tactical.slot);
                CHECK(after.source.tactical.row==before.source.tactical.row);
                CHECK(after.source.tactical.revision==before.source.tactical.revision);
            }
        }
        for(std::size_t i=0;i<definition.populations.size();++i)
            CHECK(service.target(i)==1);
        CHECK(service.project_retained().count==definition.populations.size());
        // Every individual bubble fits this target, but all resident sources do
        // not. The retained-policy budget must reject the aggregate overload.
        activity::open_world::Director overBudget;
        configuration.patrolRequests=21;
        CHECK(!overBudget.begin(owner,boot,*definition.openWorld,definition.populations,configuration));

        // The source's own cost reports may move its existing actors between
        // authored task rows without spawning, resetting, or changing quotas.
        std::size_t adaptiveIndex{};
        while(adaptiveIndex<destination.populations.size()
            && destination.populations[adaptiveIndex].tacticalRows<2)++adaptiveIndex;
        CHECK(adaptiveIndex<destination.populations.size());
        const std::array<activity::population::Capability,1> adaptiveCapabilities{{definition.populations[adaptiveIndex]}};
        activity::population::Service adaptive;CHECK(adaptive.begin(owner,adaptiveCapabilities,boot));
        const auto& first=adaptiveCapabilities.front();const auto bubble=first.registry->bubble;
        CHECK(adaptive.request({owner,1,1,first.registry->key,first.slot,1,boot},bubble)
            ==activity::population::Result::accepted);
        auto before=adaptive.project(bubble).entries[0].source;
        CHECK(before.tactical.revision==1 && before.tactical.row==0);
        sunrise::middleware::bap::activity_message::sense_update::SenseObject sense{};
        sense.registryKey=first.registry->key;sense.slotIndex=first.slot;sense.slotType=1;
        sense.hasNativeSchema=true;sense.nativeSchema=0x80807ECC;sense.nativeRevision=1;
        sense.hasRootDelta=true;sense.sourceDelta.present=1;sense.sourceDelta.scalar[0]=1;
        sense.hasSquadOutput=true;sense.squadOutput.initialized=true;
        sense.squadOutput.hasRevision=true;sense.squadOutput.revision=1;
        sense.squadOutput.costMask=3;sense.squadOutput.cost[0]=50;sense.squadOutput.cost[1]=10;
        CHECK(!adaptive.observe(63,sense));CHECK(adaptive.observe(bubble,sense));
        auto after=adaptive.project_retained().entries[0].source;
        CHECK(after.tactical.row==1 && after.tactical.revision==1);
        CHECK(after.generation==before.generation && after.looseRequested==before.looseRequested);
        CHECK(after.ruleSlot==before.ruleSlot && adaptive.last_request()==1);
        const auto selectedRevision=adaptive.revision();CHECK(!adaptive.observe(bubble,sense));
        CHECK(adaptive.revision()==selectedRevision);
        // Cost-only deltas keep the source and evaluator revisions established
        // by the prior report; an unreachable or foreign row cannot win.
        sense.nativeRevision=2;sense.sourceDelta={};sense.squadOutput.hasRevision=false;
        sense.squadOutput.costMask=1;sense.squadOutput.cost[0]=10;
        CHECK(adaptive.observe(bubble,sense));CHECK(adaptive.tactical(0).row==1); // Tie is stable.
        sense.nativeRevision=3;sense.squadOutput.costMask=2;sense.squadOutput.cost[1]=127;
        CHECK(adaptive.observe(bubble,sense));CHECK(adaptive.tactical(0).row==0);
        sense.nativeRevision=4;sense.squadOutput.hasRevision=true;sense.squadOutput.revision=2;
        sense.squadOutput.cost[1]=0;
        CHECK(adaptive.observe(bubble,sense));CHECK(adaptive.tactical(0).row==0); // Stale evaluator.
        sense.nativeRevision=5;sense.squadOutput.hasRevision=false;
        CHECK(adaptive.observe(bubble,sense));CHECK(adaptive.tactical(0).row==0); // Unconfirmed sparse revision.
        sense.nativeRevision=6;sense.squadOutput.hasRevision=true;sense.squadOutput.revision=1;sense.sourceDelta.present=1;
        sense.sourceDelta.scalar[0]=2;
        CHECK(!adaptive.observe(bubble,sense));CHECK(adaptive.tactical(0).row==0); // Other generation.
        sense.sourceDelta.scalar[0]=1;sense.squadOutput.costMask=3;
        sense.squadOutput.cost[0]=127;sense.squadOutput.cost[1]=127;
        CHECK(adaptive.observe(bubble,sense));CHECK(adaptive.tactical(0).row==0); // No reachable alternative.
        // Encoding matches the independently established mission source writer,
        // including the nonzero evaluator revision (all legacy widths unchanged).
        namespace native=sunrise::state::activity::coo::native_combatant;
        std::array<std::byte,128> actual{},expected{};
        sunrise::middleware::encoding::bits::Writer actualWriter(actual),expectedWriter(expected);
        const auto source=adaptive.project_retained().entries[0].source;
        const native::Source reference{source.registry,source.generation,source.ruleSlot,
            source.looseRequested,{source.tactical.registry,source.tactical.slot,source.tactical.row,source.tactical.revision}};
        CHECK(activity::population::codec::write_source(actualWriter,source));
        CHECK(native::write_source(expectedWriter,reference));
        CHECK(actualWriter.bit_count()==641 && actualWriter.bit_count()==expectedWriter.bit_count());
        CHECK(actual==expected);
        // Renewal starts a new evaluator lease and drops all old sparse costs.
        sense.nativeRevision=7;sense.sourceDelta={};sense.sourceDelta.consumedPresent=true;
        sense.sourceDelta.consumedCount=1;sense.sourceDelta.consumed[0]=1;sense.hasSquadOutput=false;
        CHECK(adaptive.observe(bubble,sense));
        CHECK(adaptive.renew({owner,adaptive.revision(),2,first.registry->key,first.slot,1,boot},bubble)
            ==activity::population::Result::accepted);
        CHECK(adaptive.commit_renewal(0));CHECK(adaptive.tactical(0).revision==2);
        CHECK(!adaptive.observe(bubble,sense));
    }
}
