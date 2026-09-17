#pragma once

#include "server/runtime/activity/open_world_definitions.h"
#include "server/runtime/activity/open_world_runtime.h"
#include "state/activity/coo/native_combatant_authority.h"
#include "middleware/encoding/bit_writer.h"
#include <array>
#include <fstream>
#include "state/activity/coo/edz_moon_lost_sector_group_catalog.h"

inline void open_world_cases() {
    namespace activity=sunrise::server::runtime::activity;
    namespace profiles=activity::open_world::profiles;
    namespace authored=sunrise::state::activity::coo::open_world;
    constexpr std::array<const char*,8> scripts{{
        "Sunrise/scripts/eden_freeroam.json","Sunrise/scripts/fleet_freeroam.json",
        "Sunrise/scripts/polaris_freeroam.json","Sunrise/scripts/planet_x_freeroam.json",
        "Sunrise/scripts/tangled_shore_freeroam.json","Sunrise/scripts/dreaming_city_freeroam.json",
        "Sunrise/scripts/edz_freeroam.json","Sunrise/scripts/luna_freeroam.json",
    }};
    constexpr std::array<std::size_t,8> populations{{136,92,104,233,140,20,1,1}};
    constexpr std::array<std::size_t,8> lostSectorPopulations{{70,84,52,131,111,50,205,203}};
    constexpr std::array<std::size_t,8> npcs{{0,0,0,0,1,2,0,0}};
    constexpr std::array<std::size_t,8> twoCategorySources{{16,17,6,30,4,0,0,0}};
    constexpr std::array<std::size_t,8> placements{{3,3,2,2,1,3,0,0}};
    constexpr std::array<std::size_t,8> adventures{{3,3,2,2,6,3,0,0}};
    CHECK(profiles::kActivities.size()==scripts.size());
    for(std::size_t world=0;world<profiles::kActivities.size();++world) {
        const auto& definition=*profiles::kActivities[world];
        CHECK(definition.openWorld);CHECK(activity::open_world::valid(*definition.openWorld));
        const auto& destination=*definition.openWorld->authored;
        CHECK(definition.activity==destination.activity);CHECK(definition.bubble==destination.primaryBubble);
        CHECK(definition.registries.data()==destination.registries.data());
        CHECK(definition.registries.size()+definition.lostSectorRegistries.size()+5
            <=sunrise::middleware::bap::activity_message::sensor_auth_update::kGroupCapacity);
        CHECK(definition.populations.size()<=activity::population::kSourceCapacity);
        CHECK(definition.populations.size()==populations[world]+lostSectorPopulations[world]);
        CHECK(destination.populations.size()==populations[world]);
        const auto ordinary=definition.populations.first(destination.populations.size());
        CHECK(!definition.lostSectorRegistries.empty());
        CHECK(definition.lostSectors!=nullptr);
        if(definition.lostSectors) {
            const auto base=definition.lostSectors->capabilityBase;
            CHECK(base==ordinary.size());
            CHECK(activity::lost_sector::valid(*definition.lostSectors,definition.populations.subspan(base)));
        }
        CHECK(definition.placements.size()==placements[world]);
        CHECK(definition.startRoutes.size()==adventures[world]);
        CHECK(destination.adventures.size()==adventures[world]);
        CHECK(definition.publicEventRallies.empty());CHECK(definition.publicEventInitials.empty());
        CHECK(definition.adventureOpenings.empty());
        std::size_t npcCount{},twoCategoryCount{};
        for(std::size_t i=0;i<ordinary.size();++i) {
            CHECK(activity::population::valid(ordinary[i]));
            const auto& binding=destination.populations[i];
            CHECK(binding.tacticalRows<=24);
            const bool solariumLeader=definition.activity=="fleet_freeroam"
                && ordinary[i].registry->key==0x1EE02F73U;
            const bool countedPatrol=world!=5 && binding.kind==authored::PopulationKind::patrol;
            CHECK(countedPatrol?(binding.requestOverride>=1 && binding.requestOverride<=9):binding.requestOverride==0);
            if(solariumLeader) {
                CHECK(binding.tactical==3 && binding.tacticalRows==8);
                CHECK(ordinary[i].registry->bubble==11);
                if(binding.source==0) {
                    CHECK(binding.rule==25 && binding.categories==2);
                    CHECK(binding.requestOverride==1 && binding.secondRequestOverride==0);
                } else if(binding.source==1) {
                    CHECK(binding.rule==24 && binding.categories==1);
                    CHECK(binding.requestOverride==1 && binding.secondRequestOverride==0);
                } else {
                    CHECK(binding.source==2 && binding.rule==6 && binding.categories==1);
                    CHECK(binding.requestOverride==1 && binding.secondRequestOverride==0);
                }
            }
            CHECK(binding.categories==2?binding.secondRequestOverride<=9:binding.secondRequestOverride==0);
            CHECK(binding.categories>=1 && binding.categories<=8);
            if(binding.categories==2)++twoCategoryCount;
            CHECK(ordinary[i].taskMask==(binding.tacticalRows?(1U<<binding.tacticalRows)-1U:0U));
            if(destination.populations[i].kind==authored::PopulationKind::npc)++npcCount;
        }
        CHECK(npcCount==npcs[world]);
        CHECK(twoCategoryCount==twoCategorySources[world]);
        for(const auto& registry:definition.registries) {
            CHECK(activity::registry::valid(registry));
            CHECK(authored::required(registry.scenario,registry.objectTag,registry.key,
                std::uint64_t{1}<<registry.bubble)
                || sunrise::state::activity::coo::edz_moon_lost_sector_groups::required(
                    registry.scenario,registry.objectTag,registry.key,std::uint64_t{1}<<registry.bubble));
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
        CHECK(director.begin(owner,boot,*definition.openWorld,ordinary,configuration));
        std::array<sunrise::state::activity::coo::NativePopulationLedger<64>,activity::population::kSourceCapacity> ledgers{};
        std::array<std::uint8_t,activity::population::kSourceCapacity> pending{};std::array<bool,64> bubbles{};
        for(const auto& capability:ordinary)bubbles[capability.registry->bubble]=true;
        std::uint64_t now=1000;std::size_t started{};
        for(std::size_t bubble=0;bubble<bubbles.size();++bubble)if(bubbles[bubble]) {
            CHECK(director.update(now++,static_cast<std::uint32_t>(bubble),true,service,
                std::span<const sunrise::state::activity::coo::NativePopulationLedger<64>>(ledgers),
                std::span<const std::uint8_t>(pending)));
            const auto projected=service.project(static_cast<std::uint32_t>(bubble));
            std::size_t expected{};
            for(const auto& capability:ordinary)
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
        for(std::size_t i=0;i<ordinary.size();++i) {
            CHECK(service.target(i)==(destination.populations[i].requestOverride?destination.populations[i].requestOverride:1U));
            CHECK(service.second_target(i)==destination.populations[i].secondRequestOverride);
        }
        for(std::size_t i=ordinary.size();i<definition.populations.size();++i)CHECK(service.target(i)==0);
        CHECK(service.project_retained().count==ordinary.size());

        if(world==1) {
            // Solarium's complete reviewed source set is one alternative
            // Wizard/Knight leader and two unresolved primary Trooper sources.
            // The leader's unresolved secondary remains dormant.
            struct Ledger final {
                sunrise::state::activity::coo::PopulationCounts value{};
                [[nodiscard]] sunrise::state::activity::coo::PopulationCounts counts() const noexcept {return value;}
            };
            activity::population::Service solariumService;CHECK(solariumService.begin(owner,definition.populations,boot));
            activity::open_world::Director solariumDirector;
            CHECK(solariumDirector.begin(owner,boot,*definition.openWorld,ordinary,configuration));
            std::array<Ledger,activity::population::kSourceCapacity> solariumLedgers{};
            std::array<std::uint8_t,activity::population::kSourceCapacity> solariumPending{};
            const auto tick=[&](std::uint64_t time,std::uint32_t bubble) {
                return solariumDirector.update(time,bubble,true,solariumService,
                    std::span<const Ledger>(solariumLedgers),std::span<const std::uint8_t>(solariumPending));
            };
            CHECK(tick(100,11));const auto before=solariumService.project_retained();CHECK(before.count==9);
            CHECK(tick(101,63));const auto away=solariumService.project_retained();CHECK(away.count==9);
            std::array<std::size_t,3> members{};std::size_t memberCount{};
            for(std::size_t i=0;i<ordinary.size();++i) {
                const auto& cap=ordinary[i];
                if(cap.registry->key!=0x1EE02F73U)continue;
                CHECK(memberCount<members.size());members[memberCount++]=i;
                sunrise::middleware::bap::activity_message::sense_update::SenseObject consumed{};
                consumed.registryKey=cap.registry->key;consumed.slotIndex=cap.slot;consumed.slotType=1;
                consumed.hasNativeSchema=true;consumed.nativeSchema=0x80807ECC;consumed.nativeRevision=1;
                consumed.hasRootDelta=true;consumed.sourceDelta.present=1;
                consumed.sourceDelta.scalar[0]=solariumService.generation(i);
                consumed.sourceDelta.consumedPresent=true;consumed.sourceDelta.consumedCount=cap.categories;
                consumed.sourceDelta.consumed[0]=solariumService.target(i);
                consumed.sourceDelta.consumed[1]=solariumService.second_target(i);
                CHECK(solariumService.observe(11,consumed));
                solariumLedgers[i].value={1,0,1,0,false,false};
            }
            CHECK(memberCount==3);CHECK(tick(200,11));
            CHECK(tick(201+configuration.respawnMilliseconds,63));
            // Started bubble11 sources continue their lifecycle while the player
            // is elsewhere; bubble63 is not permission to start a new source.
            for(const auto i:members) {
                const auto& renewal=solariumService.renewal(i);
                CHECK(renewal.pending && renewal.nextTarget==1
                    && renewal.nextSecondTarget==0);
            }
            CHECK(tick(202+configuration.respawnMilliseconds,11));
            for(const auto i:members) {
                const auto& renewal=solariumService.renewal(i);
                CHECK(renewal.pending && renewal.nextTarget==1 && renewal.nextSecondTarget==0);
                const auto generation=solariumService.generation(i);
                CHECK(solariumService.commit_renewal(i));
                CHECK(solariumService.generation(i)==generation+1);
                CHECK(solariumService.target(i)==1 && solariumService.second_target(i)==0);
                solariumLedgers[i].value={};
            }
            CHECK(tick(203+configuration.respawnMilliseconds,63));
            CHECK(tick(204+configuration.respawnMilliseconds,11));
            const auto after=solariumService.project_retained();CHECK(after.count==9);
            for(std::size_t i=0;i<after.count;++i) {
                CHECK(after.entries[i].source.registry==before.entries[i].source.registry);
                CHECK(after.entries[i].slot==before.entries[i].slot);
                CHECK(after.entries[i].source.looseRequested==before.entries[i].source.looseRequested);
                CHECK(after.entries[i].source.secondRequested==before.entries[i].source.secondRequested);
            }
        }

        if(world==2) {
            // Reviewed Mars cohorts share species budgets across exact sibling
            // slots. The two hilltop categories are simultaneous, not weights.
            struct Expected final {std::uint32_t key;std::uint16_t slot,rule;std::uint8_t first,second;};
            constexpr std::array<Expected,22> expected{{
                {0xD503E412,0,15,1,0},{0xD503E412,1,15,1,0},
                {0xD503E412,2,15,6,0},{0xD503E412,3,15,1,0},
                {0x7D7A97EC,0,11,1,0},{0x7D7A97EC,1,11,1,0},{0x7D7A97EC,2,11,1,0},
                {0x9A197FDF,0,9,1,0},{0x9A197FDF,1,9,1,0},
                {0x8CF02C29,0,11,1,0},{0x8CF02C29,1,11,1,0},{0x8CF02C29,2,11,1,0},
                {0x8717837E,0,11,2,0},{0x8717837E,1,11,2,1},{0x8717837E,2,11,2,1},
                {0x14810D98,0,12,2,0},{0x14810D98,1,12,4,0},{0x14810D98,2,13,1,1},
                {0x79632BFE,0,7,2,0},{0x79632BFE,1,7,1,0},
                {0x3D44B1DA,0,9,5,0},{0x3D44B1DA,1,9,1,0},
            }};
            std::uint32_t total{};
            for(std::size_t i=0;i<destination.populations.size();++i)
                total+=service.target(i)+service.second_target(i);
            CHECK(total==159 && total*3<=activity::open_world::kAdmissionBurstCapacity
                && total*9<=activity::open_world::kEventBurstCapacity);
            for(const auto& row:expected) {
                std::size_t matches{};
                for(std::size_t i=0;i<ordinary.size();++i) {
                    const auto& cap=ordinary[i];
                    if(cap.registry->key!=row.key || cap.slot!=row.slot)continue;
                    ++matches;CHECK(destination.populations[i].rule==row.rule);
                    CHECK(service.target(i)==row.first && service.second_target(i)==row.second);
                    CHECK(cap.categories==(row.second?2:1));
                }
                CHECK(matches==1);
            }
            struct Ledger final {
                sunrise::state::activity::coo::PopulationCounts value{};
                [[nodiscard]] sunrise::state::activity::coo::PopulationCounts counts() const noexcept {return value;}
            };
            activity::population::Service cohortService;CHECK(cohortService.begin(owner,ordinary,boot));
            activity::open_world::Director cohortDirector;
            CHECK(cohortDirector.begin(owner,boot,*definition.openWorld,ordinary,configuration));
            std::array<Ledger,activity::population::kSourceCapacity> cohortLedgers{};
            std::array<std::uint8_t,activity::population::kSourceCapacity> cohortPending{};
            const auto tick=[&](std::uint64_t time,std::uint32_t bubble) {
                return cohortDirector.update(time,bubble,true,cohortService,
                    std::span<const Ledger>(cohortLedgers),std::span<const std::uint8_t>(cohortPending));
            };
            constexpr std::array<std::uint32_t,6> marsBubbles{{0,1,5,7,9,10}};
            for(const auto bubble:marsBubbles)CHECK(tick(100+bubble,bubble));
            const auto before=cohortService.project_retained();CHECK(before.count==104);
            for(std::size_t i=0;i<ordinary.size();++i) {
                if(destination.populations[i].kind==authored::PopulationKind::npc)continue;
                const auto& cap=ordinary[i];
                sunrise::middleware::bap::activity_message::sense_update::SenseObject consumed{};
                consumed.registryKey=cap.registry->key;consumed.slotIndex=cap.slot;consumed.slotType=1;
                consumed.hasNativeSchema=true;consumed.nativeSchema=0x80807ECC;consumed.nativeRevision=1;
                consumed.hasRootDelta=true;consumed.sourceDelta.present=1;
                consumed.sourceDelta.scalar[0]=cohortService.generation(i);
                consumed.sourceDelta.consumedPresent=true;consumed.sourceDelta.consumedCount=cap.categories;
                consumed.sourceDelta.consumed[0]=cohortService.target(i);
                consumed.sourceDelta.consumed[1]=cohortService.second_target(i);
                CHECK(cohortService.observe(cap.registry->bubble,consumed));
                // One confirmed admitted/dead actor is enough for this synthetic
                // lifecycle test; it does not equate requests with admissions.
                cohortLedgers[i].value={1,0,1,0,false,false};
            }
            CHECK(tick(200,1));CHECK(tick(201,5));
            CHECK(tick(202+configuration.respawnMilliseconds,63));
            // All previously started ordinary Mars patrols mature while away;
            // NPCs remain outside the patrol-renewal lifecycle.
            for(std::size_t i=0;i<ordinary.size();++i) {
                const auto& binding=destination.populations[i];
                const auto renewal=cohortService.renewal(i);
                if(binding.kind==authored::PopulationKind::npc)CHECK(!renewal.pending);
                else CHECK(renewal.pending && renewal.nextTarget==binding.requestOverride
                    && renewal.nextSecondTarget==binding.secondRequestOverride);
            }
            CHECK(tick(203+configuration.respawnMilliseconds,1));
            CHECK(tick(204+configuration.respawnMilliseconds,5));
            std::size_t renewals{};
            for(std::size_t i=0;i<ordinary.size();++i) {
                const auto& binding=destination.populations[i];
                if(binding.kind==authored::PopulationKind::npc) {CHECK(!cohortService.renewal(i).pending);continue;}
                CHECK(cohortService.renewal(i).pending);
                CHECK(cohortService.renewal(i).nextTarget==binding.requestOverride);
                CHECK(cohortService.renewal(i).nextSecondTarget==binding.secondRequestOverride);
                const auto generation=cohortService.generation(i);
                CHECK(cohortService.commit_renewal(i));++renewals;
                CHECK(cohortService.generation(i)==generation+1);
                CHECK(cohortService.target(i)==binding.requestOverride);
                CHECK(cohortService.second_target(i)==binding.secondRequestOverride);
                cohortLedgers[i].value={};
            }
            CHECK(renewals==104);
            CHECK(tick(205+configuration.respawnMilliseconds,63));
            CHECK(tick(206+configuration.respawnMilliseconds,1));
            CHECK(tick(207+configuration.respawnMilliseconds,5));
            const auto after=cohortService.project_retained();CHECK(after.count==before.count);
            for(std::size_t i=0;i<after.count;++i) {
                CHECK(after.entries[i].source.registry==before.entries[i].source.registry);
                CHECK(after.entries[i].slot==before.entries[i].slot);
                CHECK(after.entries[i].source.looseRequested==before.entries[i].source.looseRequested);
                CHECK(after.entries[i].source.secondRequested==before.entries[i].source.secondRequested);
                CHECK(after.entries[i].source.ruleSlot==before.entries[i].source.ruleSlot);
                CHECK(!cohortService.renewal(i).pending);
            }
        }

        if(world==4) {
            // Exercise request policy independently of generated destination rows.
            // A request remains a native source request; this test does not infer
            // an actor count from it.
            std::array<authored::PopulationBinding,activity::population::kSourceCapacity> bindings{};
            for(std::size_t i=0;i<destination.populations.size();++i) {
                bindings[i]=destination.populations[i];
                bindings[i].requestOverride=bindings[i].secondRequestOverride=0;
            }
            std::size_t first=SIZE_MAX,second=SIZE_MAX,unchanged=SIZE_MAX,npc=SIZE_MAX;
            for(std::size_t i=0;i<bindings.size() && i<destination.populations.size();++i) {
                if(bindings[i].kind==authored::PopulationKind::npc || bindings[i].categories!=2)continue;
                for(std::size_t j=i+1;j<destination.populations.size();++j)
                    if(bindings[j].kind==authored::PopulationKind::patrol
                        && ordinary[i].registry->bubble==ordinary[j].registry->bubble) {
                        first=i;second=j;break;
                    }
                if(first!=SIZE_MAX)break;
            }
            for(std::size_t i=0;i<destination.populations.size();++i)
                if(bindings[i].kind==authored::PopulationKind::patrol && bindings[i].categories==1
                    && i!=first && i!=second) {unchanged=i;break;}
            for(std::size_t i=0;i<destination.populations.size();++i)
                if(bindings[i].kind==authored::PopulationKind::npc) {npc=i;break;}
            CHECK(first!=SIZE_MAX && second!=SIZE_MAX && unchanged!=SIZE_MAX && npc!=SIZE_MAX);
            bindings[first].requestOverride=2;bindings[first].secondRequestOverride=3;
            bindings[second].requestOverride=3;
            auto overriddenDestination=destination;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(bindings.data(),destination.populations.size());
            const activity::open_world::Definition overriddenDefinition{&overriddenDestination};
            const activity::population::Owner overriddenOwner{0xD3571000U,{1}};
            // Keep the default quota within the expanded profile's aggregate
            // budget while exercising distinct per-source overrides.
            auto overrideConfiguration=configuration;overrideConfiguration.patrolRequests=1;
            activity::population::Service overriddenService;
            CHECK(overriddenService.begin(overriddenOwner,ordinary,boot));
            activity::open_world::Director overriddenDirector;
            CHECK(overriddenDirector.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration));
            struct Ledger final {
                sunrise::state::activity::coo::PopulationCounts value{};
                [[nodiscard]] sunrise::state::activity::coo::PopulationCounts counts() const noexcept {return value;}
            };
            std::array<Ledger,activity::population::kSourceCapacity> overrideLedgers{};
            std::array<std::uint8_t,activity::population::kSourceCapacity> overridePending{};
            const auto overriddenBubble=ordinary[first].registry->bubble;
            CHECK(overriddenDirector.update(100,overriddenBubble,true,overriddenService,
                std::span<const Ledger>(overrideLedgers),std::span<const std::uint8_t>(overridePending)));
            CHECK(overriddenService.target(first)==2 && overriddenService.second_target(first)==3
                && overriddenService.target(second)==3 && overriddenService.second_target(second)==0);
            for(std::size_t i=0;i<ordinary.size();++i)
                if(ordinary[i].registry->bubble==overriddenBubble && i!=first && i!=second)
                    CHECK(overriddenService.target(i)==1U);
            CHECK(overriddenDirector.update(101,ordinary[unchanged].registry->bubble,true,overriddenService,
                std::span<const Ledger>(overrideLedgers),std::span<const std::uint8_t>(overridePending)));
            CHECK(overriddenService.target(unchanged)==1);
            const auto beforeCrossing=overriddenService.project_retained();
            CHECK(overriddenDirector.update(102,63,true,overriddenService,
                std::span<const Ledger>(overrideLedgers),std::span<const std::uint8_t>(overridePending)));
            const auto afterCrossing=overriddenService.project_retained();
            CHECK(afterCrossing.count==beforeCrossing.count);
            for(std::size_t i=0;i<afterCrossing.count;++i) {
                CHECK(afterCrossing.entries[i].source.looseRequested==beforeCrossing.entries[i].source.looseRequested);
                CHECK(afterCrossing.entries[i].source.secondRequested==beforeCrossing.entries[i].source.secondRequested);
                CHECK(afterCrossing.entries[i].source.hasSecondCategory==beforeCrossing.entries[i].source.hasSecondCategory);
            }

            sunrise::middleware::bap::activity_message::sense_update::SenseObject consumed{};
            consumed.registryKey=ordinary[first].registry->key;
            consumed.slotIndex=ordinary[first].slot;consumed.slotType=1;
            consumed.hasNativeSchema=true;consumed.nativeSchema=0x80807ECC;consumed.nativeRevision=1;
            consumed.hasRootDelta=true;consumed.sourceDelta.present=1;
            std::uint32_t firstGeneration{};
            const auto retainedSources=overriddenService.project_retained();
            for(std::size_t i=0;i<retainedSources.count;++i)
                if(retainedSources.entries[i].source.registry==consumed.registryKey
                    && retainedSources.entries[i].slot==consumed.slotIndex)
                    firstGeneration=retainedSources.entries[i].source.generation;
            CHECK(firstGeneration);consumed.sourceDelta.scalar[0]=firstGeneration;
            consumed.sourceDelta.consumedPresent=true;
            consumed.sourceDelta.consumedCount=ordinary[first].categories;
            consumed.sourceDelta.consumed[0]=2;consumed.sourceDelta.consumed[1]=3;
            CHECK(overriddenService.observe(overriddenBubble,consumed));
            overrideLedgers[first].value={1,0,1,0,false,false};
            CHECK(overriddenDirector.update(103,overriddenBubble,true,overriddenService,
                std::span<const Ledger>(overrideLedgers),std::span<const std::uint8_t>(overridePending)));
            CHECK(overriddenDirector.update(103+configuration.respawnMilliseconds,overriddenBubble,true,overriddenService,
                std::span<const Ledger>(overrideLedgers),std::span<const std::uint8_t>(overridePending)));
            CHECK(overriddenService.renewal(first).pending && overriddenService.renewal(first).nextTarget==2
                && overriddenService.renewal(first).nextSecondTarget==3);
            CHECK(overriddenService.commit_renewal(first));
            const auto renewedSources=overriddenService.project_retained();
            bool foundRenewed{},foundSecond{};
            for(std::size_t i=0;i<renewedSources.count;++i) {
                const auto& row=renewedSources.entries[i];
                if(row.source.registry==ordinary[first].registry->key
                    && row.slot==ordinary[first].slot) {
                    CHECK(row.source.looseRequested==2 && row.source.secondRequested==3
                        && row.source.hasSecondCategory && row.source.generation==firstGeneration+1);foundRenewed=true;
                }
                if(row.source.registry==ordinary[second].registry->key
                    && row.slot==ordinary[second].slot) {
                    CHECK(row.source.looseRequested==3);foundSecond=true;
                }
            }
            CHECK(foundRenewed && foundSecond && overriddenService.target(second)==3);

            auto invalidBindings=bindings;
            invalidBindings[first].requestOverride=22;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(invalidBindings.data(),destination.populations.size());
            activity::open_world::Director invalidOverride;
            CHECK(!invalidOverride.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration));
            invalidBindings=bindings;invalidBindings[npc].requestOverride=2;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(invalidBindings.data(),destination.populations.size());
            activity::open_world::Director invalidNpc;
            CHECK(!invalidNpc.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration));
            invalidBindings=bindings;invalidBindings[unchanged].secondRequestOverride=1;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(invalidBindings.data(),destination.populations.size());
            activity::open_world::Director invalidSecondCategory;
            CHECK(!invalidSecondCategory.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration));
            invalidBindings=bindings;
            for(std::size_t i=0;i<destination.populations.size();++i)
                if(invalidBindings[i].categories==2)invalidBindings[i].secondRequestOverride=21;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(invalidBindings.data(),destination.populations.size());
            std::size_t secondCategoryRequests{};
            for(std::size_t i=0;i<destination.populations.size();++i)
                secondCategoryRequests+=(invalidBindings[i].requestOverride?invalidBindings[i].requestOverride:1U)
                    +invalidBindings[i].secondRequestOverride;
            activity::open_world::Director secondCategoryBudget;
            CHECK(secondCategoryBudget.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration)
                ==(secondCategoryRequests<=activity::open_world::kRetainedRequestCapacity));
            invalidBindings=bindings;invalidBindings[npc].requestOverride=1;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(invalidBindings.data(),destination.populations.size());
            activity::open_world::Director unchangedNpc;
            CHECK(unchangedNpc.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration));
            activity::population::Service npcService;CHECK(npcService.begin(overriddenOwner,ordinary,boot));
            CHECK(unchangedNpc.update(200,ordinary[npc].registry->bubble,true,npcService,
                std::span<const Ledger>(overrideLedgers),std::span<const std::uint8_t>(overridePending)));
            CHECK(npcService.target(npc)==1);
            invalidBindings=bindings;
            for(std::size_t i=0;i<destination.populations.size();++i)
                if(invalidBindings[i].kind==authored::PopulationKind::patrol)invalidBindings[i].requestOverride=21;
            overriddenDestination.populations=std::span<const authored::PopulationBinding>(invalidBindings.data(),destination.populations.size());
            activity::open_world::Director retainedOverBudget;
            CHECK(!retainedOverBudget.begin(overriddenOwner,boot,overriddenDefinition,ordinary,overrideConfiguration));
        }
        // The retained-policy budget rejects this aggregate overload, whether
        // or not the currently occupied bubble alone would fit.
        activity::open_world::Director overBudget;
        configuration.patrolRequests=21;
        std::array<authored::PopulationBinding,activity::population::kSourceCapacity> defaultBindings{};
        for(std::size_t i=0;i<destination.populations.size();++i) {
            defaultBindings[i]=destination.populations[i];
            defaultBindings[i].requestOverride=defaultBindings[i].secondRequestOverride=0;
        }
        auto defaultDestination=destination;
        defaultDestination.populations=std::span<const authored::PopulationBinding>(defaultBindings.data(),destination.populations.size());
        auto defaultDefinition=*definition.openWorld;defaultDefinition.authored=&defaultDestination;
        const auto worstRequests=(ordinary.size()-npcCount)*21+npcCount;
        CHECK(overBudget.begin(owner,boot,defaultDefinition,ordinary,configuration)
            ==(worstRequests<=activity::open_world::kRetainedRequestCapacity));

        // The source's own cost reports may move its existing actors between
        // authored task rows without spawning, resetting, or changing quotas.
        std::size_t adaptiveIndex{};
        while(adaptiveIndex<destination.populations.size()
            && destination.populations[adaptiveIndex].tacticalRows<2)++adaptiveIndex;
        CHECK(adaptiveIndex<destination.populations.size());
        const std::array<activity::population::Capability,1> adaptiveCapabilities{{ordinary[adaptiveIndex]}};
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
        native::Source reference{source.registry,source.generation,source.ruleSlot,
            static_cast<std::uint8_t>(source.looseRequested),{source.tactical.registry,source.tactical.slot,source.tactical.row,source.tactical.revision}};
        std::array<std::uint8_t,8> categories{};
        categories[0]=static_cast<std::uint8_t>(source.looseRequested);
        categories[1]=static_cast<std::uint8_t>(source.secondRequested);
        for(std::size_t i=2;i<first.categories;++i)
            categories[i]=static_cast<std::uint8_t>(source.additionalRequested[i-2]);
        reference.categories=std::span(categories).first(first.categories);reference.hasRule=source.hasSpawnRule;
        reference.looseRequested=0;
        CHECK(activity::population::codec::write_source(actualWriter,source));
        CHECK(native::write_source(expectedWriter,reference));
        CHECK(actualWriter.bit_count()==641+32U*(first.categories-1U) && actualWriter.bit_count()==expectedWriter.bit_count());
        CHECK(actual==expected);
        // Renewal starts a new evaluator lease and drops all old sparse costs.
        sense.nativeRevision=7;sense.sourceDelta={};sense.sourceDelta.consumedPresent=true;
        sense.sourceDelta.consumedCount=first.categories;sense.sourceDelta.consumed[0]=1;sense.hasSquadOutput=false;
        CHECK(adaptive.observe(bubble,sense));
        CHECK(adaptive.renew({owner,adaptive.revision(),2,first.registry->key,first.slot,1,boot},bubble)
            ==activity::population::Result::accepted);
        CHECK(adaptive.commit_renewal(0));CHECK(adaptive.tactical(0).revision==2);
        CHECK(!adaptive.observe(bubble,sense));
    }
}
