#include "../src/server/runtime/activity/lost_sector_runtime.h"
#include "../src/server/runtime/activity/lost_sector_catalog.h"
#include "../src/server/runtime/activity/lost_sector_reward_policy.h"
#include "../src/server/runtime/activity/open_world_definitions.h"
#include "../src/server/runtime/activity/mercury_populations.h"
#include "../src/server/runtime/activity/native_activity_runtime.h"
#include "../src/server/runtime/activity/native_activity_profiles.h"
#include "../src/server/runtime/activity/moon_lost_sector_runtime.h"
#include "../src/server/runtime/activity/lost_sector_destructible_definitions.h"
#include "../src/server/bap/encrypted/activity_message/lost_sector_reward_claim_slots.h"
#include "../src/middleware/bap/activity_message/sense_update.h"
#include "../src/state/activity/native_population_events.h"
#include "../src/state/activity/coo/open_world_member_catalog.h"
#include <array>
#include <cstdio>

namespace activity=dawn::server::runtime::activity;
namespace ls=activity::lost_sector;
namespace registry=dawn::state::activity::coo::registry;

namespace {
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)
inline constexpr std::array<registry::Slot,4> kSlots{{
    {0,1,0x80809A3BU,0x80807ECCU,0x80807EC9U,0x81000001U},
    {1,1,0x80809A3BU,0x80807ECCU,0x80807EC9U,0x81000002U},
    {2,1,0x80809A3BU,0x80807ECCU,0x80807EC9U,0x81000003U},
    {3,4,0x80809927U,0x8080992EU,0x8080992FU,0x81000004U},
}};
inline constexpr registry::Definition kRegistry{"sector",0x80F00001U,0x10000001U,
    0x82000001U,0x20000001U,7,kSlots};
inline constexpr std::array<activity::population::Capability,3> kCapabilities{{
    {&kRegistry,0,0,{},false},{&kRegistry,1,0,{},false},
    {&kRegistry,2,0,{},false},
}};
inline constexpr std::array<ls::Stage,2> kStages{{{0,2},{2,1}}};
inline constexpr std::array<ls::Sector,1> kSectors{{{"proof sector",7,0,2,&kRegistry,3}}};
inline constexpr std::array<ls::SourcePolicy,3> kPolicies{{{2,0,false},{1,0,false},{1,0,true}}};
inline constexpr ls::Definition kDefinition{kSectors,kStages,kPolicies,0};
}

int main() {
    CHECK(activity::lost_sector_destructible::kBindings.size()==4
        && activity::lost_sector_destructible::supported(0x815700D5U)
        && activity::lost_sector_destructible::supported(0x815700DEU)
        && !activity::lost_sector_destructible::supported(0x815700D2U));
    CHECK(activity::moon_lost_sector::current_gate_death(false,7,7,true));
    CHECK(!activity::moon_lost_sector::current_gate_death(true,7,7,true));
    CHECK(!activity::moon_lost_sector::current_gate_death(false,7,8,true));
    CHECK(!activity::moon_lost_sector::current_gate_death(false,8,8,false));
    struct LookupEntry final { activity::population::Owner owner{}; };
    std::array<LookupEntry,3> lookup{{{{0x11,1}},{{}},{{0x22,2}}}};
    const activity::population::Owner laterOwner{0x22,2};
    const auto* later=activity::native_activity::detail::find_owned_entry(lookup,laterOwner,
        [](const LookupEntry& entry) noexcept {return entry.owner;});
    CHECK(later==&lookup[2]);
    CHECK(!activity::native_activity::detail::find_owned_entry(lookup,
        activity::population::Owner{0x33,3},[](const LookupEntry& entry) noexcept {return entry.owner;}));
    struct ClaimSlot final { bool occupied{},delivered{};std::uint32_t generation{}; };
    std::array<ClaimSlot,32> claimSlots{};
    std::uint32_t currentGeneration{};
    for(std::uint32_t generation=1;generation<=40;++generation) {
        currentGeneration=generation;
        const auto slot=dawn::server::bap::encrypted::lost_sector_rewards::detail::select_claim_slot(
            claimSlots,[](const ClaimSlot& claim) {return claim.occupied;},
            [currentGeneration](const ClaimSlot& claim) {
                return claim.delivered && claim.generation!=currentGeneration;
            },
            [generation](const ClaimSlot& claim) {return claim.generation==generation;});
        CHECK(slot<claimSlots.size());claimSlots[slot]={true,true,generation};
    }
    std::array<ClaimSlot,1> interleaved{{{true,true,7}}};
    CHECK(dawn::server::bap::encrypted::lost_sector_rewards::detail::select_claim_slot(
        interleaved,[](const ClaimSlot& claim) {return claim.occupied;},
        [](const ClaimSlot&) {return false;},[](const ClaimSlot&) {return false;})
        ==dawn::server::bap::encrypted::lost_sector_rewards::detail::kNoClaimSlot);
    CHECK(dawn::server::bap::encrypted::lost_sector_rewards::detail::select_claim_slot(
        interleaved,[](const ClaimSlot& claim) {return claim.occupied;},
        [](const ClaimSlot&) {return false;},
        [](const ClaimSlot& claim) {return claim.generation==7;})
        ==dawn::server::bap::encrypted::lost_sector_rewards::detail::kDuplicateClaimSlot);
    for(auto& claim:claimSlots)claim.delivered=false;
    CHECK(dawn::server::bap::encrypted::lost_sector_rewards::detail::select_claim_slot(
        claimSlots,[](const ClaimSlot& claim) {return claim.occupied;},
        [](const ClaimSlot& claim) {return claim.delivered;},
        [](const ClaimSlot&) {return false;})
        ==dawn::server::bap::encrypted::lost_sector_rewards::detail::kNoClaimSlot);
    CHECK(ls::valid(kDefinition,kCapabilities));
    // A failed all-source activation must not publish partial expected-run
    // generations. Pre-activate the service so the Director's dormant request
    // is rejected after it has staged nonzero generations.
    {
        const activity::population::Owner rollbackOwner{0x524F4C4CU,{3}};
        activity::population::Service rollbackService;
        CHECK(rollbackService.begin(rollbackOwner,kCapabilities,77));
        for(std::size_t i=0;i<kCapabilities.size();++i) {
            const auto& cap=kCapabilities[i];
            CHECK(rollbackService.request({rollbackOwner,rollbackService.revision(),i+1,
                cap.registry->key,cap.slot,1,77},kRegistry.bubble)
                ==activity::population::Result::accepted);
        }
        ls::Director rollback;
        CHECK(rollback.begin(rollbackOwner,77,kDefinition,kCapabilities));
        CHECK(rollback.expected_generation(0,2)==0);
        CHECK(!rollback.update(kRegistry.bubble,true,rollbackService,[](auto...){return false;}));
        CHECK(rollback.expected_generation(0,2)==0);
        CHECK(rollback.diagnostics(0).phase==ls::Phase::dormant);
    }
    {
        auto capabilities=kCapabilities;capabilities[0].categories=3;
        auto policies=kPolicies;policies[0].first=2;policies[0].second=1;
        policies[0].additional[0]=2;policies[0].categoryCount=3;
        const ls::Definition multigroup{kSectors,kStages,policies,0};
        CHECK(ls::valid(multigroup,capabilities));
        policies[0].additional[1]=1;CHECK(!ls::valid(multigroup,capabilities));
        policies[0].additional[1]=0;policies[0].categoryCount=9;
        CHECK(!ls::valid(multigroup,capabilities));
    }
    // Native death receipts can leave corpse objects resident for many
    // seconds. They must not block the next room after the complete source is
    // dead, while live actors and queued/provisional births still do.
    CHECK(ls::source_defeated(false,true,true,false,2,2,0));
    CHECK(!ls::source_defeated(true,true,true,false,2,2,0));
    CHECK(!ls::source_defeated(false,false,true,false,2,2,0));
    CHECK(!ls::source_defeated(false,true,false,false,2,2,0));
    CHECK(!ls::source_defeated(false,true,true,true,2,2,0));
    CHECK(!ls::source_defeated(false,true,true,false,0,0,0));
    CHECK(!ls::source_defeated(false,true,true,false,2,1,0));
    CHECK(!ls::source_defeated(false,true,true,false,2,2,1));
    {
        namespace reward=activity::lost_sector::reward;
        dawn::middleware::bap::activity_message::object_sense::Output use{};
        use.generation=2;use.alive=true;use.present=true;use.hasUse=true;use.used=true;
        use.useRevision=7;
        const ls::RewardTicket ticket{{0x1234U,{5}},91,3,0x44556677U,2,8,12};
        CHECK(reward::accepted_use(ticket,12,0x44556677U,4,8,use));
        auto changed=use;changed.generation=1;
        CHECK(!reward::accepted_use(ticket,12,0x44556677U,4,8,changed));
        changed=use;changed.used=false;
        CHECK(!reward::accepted_use(ticket,12,0x44556677U,4,8,changed));
        changed=use;changed.useRevision=0;
        CHECK(reward::accepted_use(ticket,12,0x44556677U,4,8,changed));
        changed=use;changed.useRevision=-1;
        CHECK(!reward::accepted_use(ticket,12,0x44556677U,4,8,changed));
        changed=use;changed.present=false;
        CHECK(!reward::accepted_use(ticket,12,0x44556677U,4,8,changed));
        CHECK(!reward::accepted_use(ticket,11,0x44556677U,4,8,use));
        CHECK(!reward::accepted_use(ticket,12,0x44556676U,4,8,use));
        CHECK(!reward::accepted_use(ticket,12,0x44556677U,3,8,use));
        CHECK(!reward::accepted_use(ticket,12,0x44556677U,4,9,use));
    }
    // Reproduce the production callback with a real native ledger and mailbox:
    // both actors are dead but their corpse identities remain resident.
    {
        namespace coo=dawn::state::activity::coo;
        namespace native=dawn::state::activity::native_population;
        const activity::population::Owner activityOwner{0xABCDEFU,{1}};
        const coo::PopulationOwner sourceOwner{activityOwner.sessionId,88,1,
            {0x10000001U,0x81000001U,1,0},1};
        const native::Lease lease{activityOwner,sourceOwner,7,true};
        coo::NativePopulationLedger<4> ledger;
        native::Mailbox mailbox;
        CHECK(ledger.begin(sourceOwner) && mailbox.bind(lease));
        const coo::PopulationActor first{sourceOwner,11,21,101};
        const coo::PopulationActor second{sourceOwner,12,22,102};
        CHECK(ledger.admitted(first)==coo::PopulationIntake::accepted);
        CHECK(ledger.admitted(second)==coo::PopulationIntake::accepted);
        CHECK(ledger.died(first)==coo::PopulationIntake::accepted);
        CHECK(ledger.died(second)==coo::PopulationIntake::accepted);
        const auto counts=ledger.counts();
        CHECK(counts.admitted==2 && counts.dead==2 && counts.alive==0 && counts.resident==2);
        CHECK(ls::source_defeated(mailbox.pending_lease(lease),true,sourceOwner.valid(),
            counts.failed,counts.admitted,counts.dead,counts.alive));
        CHECK(mailbox.quiescent(std::span(&lease,1)));
        // A provisional birth on the same lease closes both native gates even
        // though the already admitted ledger remains fully dead.
        const auto creation=mailbox.begin_creation();native::Receipt receipt{};
        const native::Event pending{lease,{sourceOwner,13,23,creation.nonce},31,
            native::Kind::admitted};
        CHECK(mailbox.stage(creation,pending,receipt)==native::StageResult::staged);
        CHECK(mailbox.pending_lease(lease));
        CHECK(!mailbox.quiescent(std::span(&lease,1)));
        CHECK(!ls::source_defeated(mailbox.pending_lease(lease),true,sourceOwner.valid(),
            counts.failed,counts.admitted,counts.dead,counts.alive));
    }
    // The production source-recreation boundary replaces only authenticated
    // live actors removed by streaming. One-shot cohorts subtract real deaths;
    // recurring patrols retain casualty replacements while both remain capped
    // by the source's intended occupancy.
    {
        namespace coo=dawn::state::activity::coo;
        auto capabilities=kCapabilities;capabilities[0].categories=3;
        activity::population::Service streamedService;
        const activity::population::Owner activityOwner{0x5354524DU,{4}};
        CHECK(streamedService.begin(activityOwner,capabilities,404));
        activity::population::Command request{activityOwner,streamedService.revision(),1,
            kRegistry.key,0,2,404,1};
        request.categoryCount=3;request.additionalRequested[0]=1;
        CHECK(streamedService.request(request,kRegistry.bubble)==activity::population::Result::accepted);
        const coo::PopulationOwner sourceOwner{activityOwner.sessionId,1,
            activityOwner.incarnation.value,{kRegistry.key,kSlots[0].descriptorTag,1,0},
            streamedService.generation(0)};
        coo::NativePopulationLedger<8> ledger;CHECK(ledger.begin(sourceOwner));
        const coo::PopulationActor dead{sourceOwner,101,201,1};
        const coo::PopulationActor survivor0{sourceOwner,102,202,2};
        const coo::PopulationActor survivor1{sourceOwner,103,203,3};
        const coo::PopulationActor survivor2a{sourceOwner,104,204,4};
        const coo::PopulationActor survivor2b{sourceOwner,105,205,5};
        CHECK(ledger.admitted(dead,0)==coo::PopulationIntake::accepted);
        CHECK(ledger.admitted(survivor0,0)==coo::PopulationIntake::accepted);
        CHECK(ledger.admitted(survivor1,1)==coo::PopulationIntake::accepted);
        CHECK(ledger.admitted(survivor2a,2)==coo::PopulationIntake::accepted);
        CHECK(ledger.admitted(survivor2b,2)==coo::PopulationIntake::accepted);
        CHECK(ledger.died(dead)==coo::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(dead)==coo::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(survivor0)==coo::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(survivor1)==coo::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(survivor2a)==coo::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(survivor2b)==coo::PopulationIntake::accepted);
        std::array<std::uint8_t,8> survivors{};CHECK(ledger.source_recreated(sourceOwner,survivors));
        CHECK(survivors[0]==1 && survivors[1]==1 && survivors[2]==2);
        const auto oneShot=activity::native_activity::detail::streamed_replacements(
            streamedService,0,ledger,survivors,3,false);
        CHECK(oneShot[0]==1 && oneShot[1]==1 && oneShot[2]==1);
        const auto recurring=activity::native_activity::detail::streamed_replacements(
            streamedService,0,ledger,survivors,3,true);
        CHECK(recurring[0]==1 && recurring[1]==1 && recurring[2]==1);
        activity::population::Command refill{activityOwner,streamedService.revision(),2,
            kRegistry.key,0,oneShot[0],404,oneShot[1]};
        refill.categoryCount=3;refill.additionalRequested[0]=oneShot[2];
        CHECK(streamedService.rehydrate(refill,kRegistry.bubble)==activity::population::Result::accepted);
        CHECK(streamedService.target(0,0)==3 && streamedService.target(0,1)==2
            && streamedService.target(0,2)==2);
        CHECK(streamedService.occupancy_target(0,0)==2 && streamedService.occupancy_target(0,1)==1
            && streamedService.occupancy_target(0,2)==1);
        // Three prior recurring casualties do not erase three live replacement
        // actors when that source later streams out.
        coo::NativePopulationLedger<8> recurringLedger;CHECK(recurringLedger.begin(sourceOwner));
        for(unsigned i=0;i<3;++i) {
            const coo::PopulationActor casualty{sourceOwner,200+i,300+i,10+i};
            CHECK(recurringLedger.admitted(casualty,0)==coo::PopulationIntake::accepted);
            CHECK(recurringLedger.died(casualty)==coo::PopulationIntake::accepted);
            CHECK(recurringLedger.actor_retired(casualty)==coo::PopulationIntake::accepted);
        }
        for(unsigned i=0;i<3;++i) {
            const coo::PopulationActor live{sourceOwner,210+i,310+i,20+i};
            CHECK(recurringLedger.admitted(live,0)==coo::PopulationIntake::accepted);
            CHECK(recurringLedger.actor_retired(live)==coo::PopulationIntake::accepted);
        }
        survivors={};CHECK(recurringLedger.source_recreated(sourceOwner,survivors));
        CHECK(survivors[0]==3);
        const auto patrol=activity::native_activity::detail::streamed_replacements(
            streamedService,0,recurringLedger,survivors,3,true);
        CHECK(patrol[0]==2); // capped by this source's intended occupancy
    }
    auto never=[](std::size_t,std::uint16_t,const ls::Stage&) {return false;};

    namespace catalog=activity::lost_sector::catalog;
    std::size_t catalogSectors{},catalogBosses{};
    const auto validateCatalog=[&](const auto& capabilities,const auto& sectors,
        const auto& stages,const auto& policies) {
        const ls::Definition definition{sectors,stages,policies,0};
        if(!ls::valid(definition,capabilities))return false;
        catalogSectors+=sectors.size();
        for(const auto& sector:sectors) {
            std::size_t bosses{};
            for(std::size_t stageIndex=sector.firstStage;
                stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
                const auto& stage=stages[stageIndex];
                for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source)
                    bosses+=policies[source].boss?1U:0U;
            }
            if(bosses!=1)return false;
            catalogBosses+=bosses;
        }
        return true;
    };
    CHECK(validateCatalog(catalog::mercury::kCapabilities,catalog::mercury::kSectors,
        catalog::mercury::kStages,catalog::mercury::kPolicies));
    CHECK(validateCatalog(catalog::mars::kCapabilities,catalog::mars::kSectors,
        catalog::mars::kStages,catalog::mars::kPolicies));
    CHECK(validateCatalog(catalog::io::kCapabilities,catalog::io::kSectors,
        catalog::io::kStages,catalog::io::kPolicies));
    CHECK(validateCatalog(catalog::titan::kCapabilities,catalog::titan::kSectors,
        catalog::titan::kStages,catalog::titan::kPolicies));
    CHECK(validateCatalog(catalog::nessus::kCapabilities,catalog::nessus::kSectors,
        catalog::nessus::kStages,catalog::nessus::kPolicies));
    CHECK(validateCatalog(catalog::tangled_shore::kCapabilities,catalog::tangled_shore::kSectors,
        catalog::tangled_shore::kStages,catalog::tangled_shore::kPolicies));
    CHECK(validateCatalog(catalog::dreaming_city::kCapabilities,catalog::dreaming_city::kSectors,
        catalog::dreaming_city::kStages,catalog::dreaming_city::kPolicies));
    CHECK(validateCatalog(catalog::edz::kCapabilities,catalog::edz::kSectors,
        catalog::edz::kStages,catalog::edz::kPolicies));
    CHECK(validateCatalog(catalog::moon::kCapabilities,catalog::moon::kSectors,
        catalog::moon::kStages,catalog::moon::kPolicies));
    CHECK(catalogSectors==42 && catalogBosses==42);
    CHECK(activity::open_world::profiles::nessus::Storage::populations[0].recurringRehydration);
    CHECK(!catalog::nessus::kCapabilities[0].recurringRehydration);
    CHECK(activity::mercury::kPopulations[0].recurringRehydration
        && activity::mercury::kPopulations[31].recurringRehydration
        && !activity::mercury::kPopulations[2].recurringRehydration);
    const auto membersCover=[](const auto& capabilities) {
        for(const auto& capability:capabilities) {
            dawn::state::activity::coo::Asset asset{capability.registry->key,0,1,capability.slot};
            for(const auto& slot:capability.registry->slots)
                if(slot.index==capability.slot && slot.type==1)asset.definition=slot.descriptorTag;
            if(!dawn::state::activity::open_world_members::registered(
                asset.definition,asset.registry,asset.slot))return false;
        }
        return true;
    };
    CHECK(membersCover(activity::mercury::kPopulations));
    for(const auto* profile:activity::open_world::profiles::kActivities)
        CHECK(profile && membersCover(profile->populations));
    CHECK(activity::native_activity_profile("edz_freeroam",8)
        ==activity::open_world::profiles::kActivities[6]);
    CHECK(activity::native_activity_profile("luna_freeroam",169)
        ==activity::open_world::profiles::kActivities[7]);
    CHECK(!activity::native_activity_profile("edz_freeroam",169)
        && !activity::native_activity_profile("luna_freeroam",8));
    for(std::size_t profileIndex=0;profileIndex<activity::open_world::profiles::kActivities.size();++profileIndex) {
        const auto* profile=activity::open_world::profiles::kActivities[profileIndex];
        CHECK(profile && !profile->activity.empty());
        for(const auto ordinal:profile->activityOrdinals)
            CHECK(activity::native_activity_profile(profile->activity,ordinal)==profile);
        for(std::size_t peer=profileIndex+1;peer<activity::open_world::profiles::kActivities.size();++peer) {
            const auto* other=activity::open_world::profiles::kActivities[peer];
            CHECK(other && profile!=other && profile->activity!=other->activity);
        }
    }
    CHECK(catalog::mercury::kCapabilities.size()==22 && catalog::mars::kCapabilities.size()==52
        && catalog::io::kCapabilities.size()==70 && catalog::titan::kCapabilities.size()==84
        && catalog::nessus::kCapabilities.size()==131 && catalog::tangled_shore::kCapabilities.size()==111
        && catalog::dreaming_city::kCapabilities.size()==50
        && catalog::edz::kCapabilities.size()==205 && catalog::moon::kCapabilities.size()==203);

    // Moon protection is independent of all-at-entry admission. Exact
    // Nightmare/guardian deaths remove the authored boss shield; Revelation
    // additionally requires four live health-qualified crystal destructions.
    {
        const activity::population::Owner moonOwner{0x4D4F4F4EU,{12}};constexpr std::uint64_t boot=404;
        activity::population::Service service;
        CHECK(service.begin(moonOwner,catalog::moon::kCapabilities,boot));
        ls::Director sectors;
        const auto definition=catalog::moon::definition(0);
        CHECK(sectors.begin(moonOwner,boot,definition,catalog::moon::kCapabilities));
        CHECK(sectors.update(4,true,service,[](auto...){return false;}));
        activity::moon_lost_sector::Director mechanics;
        CHECK(mechanics.begin(moonOwner,boot,0));
        std::array<bool,activity::population::kSourceCapacity> defeated{};
        const auto gate=[&](std::size_t source,std::uint32_t) {return defeated[source];};
        auto sourceIndex=[&](std::uint16_t slot) {
            const auto stage=catalog::moon::kStages[1];
            for(std::size_t i=stage.first;i<std::size_t(stage.first)+stage.count;++i)
                if(catalog::moon::kCapabilities[i].slot==slot)return i;
            return std::size_t(activity::population::kSourceCapacity);
        };
        activity::placement::wire::Batch objects{};activity::moon_lost_sector::ShieldBatch shields{};
        dawn::middleware::bap::activity_message::native::world_device::Batch devices{};
        CHECK(mechanics.update(4,true,sectors,service,objects,shields,devices,gate));
        CHECK(objects.count==0 && devices.count==1 && devices.entries[0].slot==16
            && devices.entries[0].state.position.value==1.F
            && devices.entries[0].state.position.revision==1
            && shields.count==1 && shields.entries[0].enabled
            && shields.entries[0].effectSlot==76 && shields.entries[0].filterSlot==95
            && shields.entries[0].protectedSource==64);
        constexpr std::array<std::uint16_t,3> guardianSlots{{34,41,48}};
        constexpr std::array<std::uint32_t,4> crystalDefinitions{{
            0x815700D5U,0x815700D8U,0x815700DBU,0x815700DEU}};
        const auto consumeMoonSource=[&](std::size_t source,std::uint32_t revision) {
            dawn::middleware::bap::activity_message::sense_update::SenseObject value{};
            value.registryKey=catalog::moon::kCapabilities[source].registry->key;
            value.slotIndex=catalog::moon::kCapabilities[source].slot;
            value.slotType=1;value.hasNativeSchema=true;value.nativeSchema=0x80807ECC;
            value.nativeRevision=revision;value.hasRootDelta=true;value.sourceDelta.present=1;
            value.sourceDelta.scalar[0]=service.generation(source);
            value.sourceDelta.consumedPresent=true;
            value.sourceDelta.consumedCount=catalog::moon::kCapabilities[source].categories;
            for(std::size_t category=0;category<value.sourceDelta.consumedCount;++category)
                value.sourceDelta.consumed[category]=service.target(source,category);
            return service.observe(4,value)!=nullptr;
        };
        for(std::size_t crystal=0;crystal<3;++crystal) {
            const auto guardian=sourceIndex(guardianSlots[crystal]);
            defeated[guardian]=true;CHECK(consumeMoonSource(guardian,static_cast<std::uint32_t>(crystal+1)));
            objects={};shields={};devices={};
            CHECK(mechanics.update(4,true,sectors,service,objects,shields,devices,gate));
            const auto request=mechanics.object_request(crystalDefinitions[crystal]);CHECK(request.valid());
            activity::moon_lost_sector::ObjectReceipt receipt{moonOwner,boot,request.owner,request.source,
                0x10000U+crystal,static_cast<std::uint32_t>(10+crystal),
                static_cast<std::uint32_t>(20+crystal),static_cast<std::uint32_t>(30+crystal)};
            CHECK(mechanics.observe_object(receipt,false));
            CHECK(mechanics.observe_object(receipt,true));
        }
        objects={};shields={};devices={};CHECK(mechanics.update(4,true,sectors,service,objects,shields,devices,gate));
        const auto bossCrystal=mechanics.object_request(crystalDefinitions[3]);CHECK(bossCrystal.valid());
        activity::moon_lost_sector::ObjectReceipt bossReceipt{moonOwner,boot,bossCrystal.owner,bossCrystal.source,
            0x10010U,30,40,50};
        CHECK(mechanics.observe_object(bossReceipt,false) && mechanics.observe_object(bossReceipt,true));
        objects={};shields={};devices={};CHECK(mechanics.update(4,true,sectors,service,objects,shields,devices,gate));
        CHECK(shields.count==1 && !shields.entries[0].enabled);
        const auto boss=sourceIndex(64);CHECK(boss<activity::population::kSourceCapacity);
        CHECK(consumeMoonSource(boss,4));
        CHECK(sectors.update(4,true,service,[](auto...){return true;}));
        CHECK(sectors.diagnostics(1).phase==ls::Phase::cleared);
        CHECK(sectors.update(5,true,service,[](auto...){return false;}));
        CHECK(sectors.update(4,true,service,[](auto...){return false;}));
        CHECK(sectors.diagnostics(1).runs==2 && service.renewal(boss).pending);
        objects={};shields={};devices={};
        CHECK(mechanics.update(4,true,sectors,service,objects,shields,devices,gate));
        CHECK(objects.count==0 && shields.count==1 && shields.entries[0].enabled);

        // Logistics publishes every exact route/loopback device open and
        // withdraws its paired physical blockers at the second run revision.
        activity::population::Service logisticsService;
        CHECK(logisticsService.begin(moonOwner,catalog::moon::kCapabilities,boot));
        ls::Director logisticsSectors;CHECK(logisticsSectors.begin(moonOwner,boot,
            catalog::moon::definition(0),catalog::moon::kCapabilities));
        CHECK(logisticsSectors.update(1,true,logisticsService,[](auto...){return false;}));
        activity::moon_lost_sector::Director logistics;
        CHECK(logistics.begin(moonOwner,boot,0));objects={};shields={};devices={};
        CHECK(logistics.update(1,true,logisticsSectors,logisticsService,objects,shields,devices,
            [](auto...){return false;}));
        CHECK(devices.count==5 && objects.count==5);
        for(std::size_t i=0;i<devices.count;++i)
            CHECK(devices.entries[i].state.position.value==1.F
                && devices.entries[i].state.position.revision==1
                && devices.entries[i].state.position.snap);
        for(std::size_t i=0;i<objects.count;++i)
            CHECK(!objects.entries[i].active && objects.entries[i].generation==2);
    }

    // Nessus is the largest retained open-world profile. Keep every ordinary
    // patrol source published while visiting each Lost Sector, then prove the
    // complete 360-source batch survives the wire capacity boundary.
    {
        namespace nessus=activity::open_world::profiles::nessus;
        constexpr auto ordinary=nessus::Storage::populations.size();
        static_assert(nessus::kPopulations.size()<=activity::population::kSourceCapacity);
        const activity::population::Owner capacityOwner{0x4E455353U,{4}};
        activity::population::Service capacityService;
        CHECK(capacityService.begin(capacityOwner,nessus::kPopulations,303));
        std::uint64_t request=1;
        for(std::size_t source=0;source<ordinary;++source) {
            const auto& capability=nessus::kPopulations[source];
            const activity::population::Command command{capacityOwner,capacityService.revision(),
                request++,capability.registry->key,capability.slot,1,303,
                static_cast<std::uint8_t>(capability.categories==2?1:0)};
            CHECK(capacityService.request(command,capability.registry->bubble)
                ==activity::population::Result::accepted);
        }
        ls::Director capacityDirector;
        CHECK(capacityDirector.begin(capacityOwner,303,nessus::kLostSectorDefinition,
            catalog::nessus::kCapabilities));
        for(const auto& sector:catalog::nessus::kSectors)
            CHECK(capacityDirector.update(sector.bubble,true,capacityService,never));
        const auto retained=capacityService.project_retained();
        CHECK(retained.count==nessus::kPopulations.size());
        CHECK(retained.count<retained.entries.size());
        const auto retainedRevision=capacityService.revision();
        for(const auto& sector:catalog::nessus::kSectors)
            CHECK(capacityDirector.update(sector.bubble,true,capacityService,never));
        CHECK(capacityService.revision()==retainedRevision);
        CHECK(capacityService.project_retained().count==retained.count);
    }

    // Every installed sector requests all authored rooms and its boss on entry.
    // Only the singleton boss clears the sector; optional survivors do not
    // prevent the chest/completion gate and are not duplicated on reentry.
    const auto exerciseCatalog=[&](const auto& capabilities,const auto& sectors,
        const auto& stages,const auto& policies,std::uint64_t session) {
        for(std::size_t sectorIndex=0;sectorIndex<sectors.size();++sectorIndex) {
            const activity::population::Owner catalogOwner{session+sectorIndex,{11}};
            activity::population::Service catalogService;
            if(!catalogService.begin(catalogOwner,capabilities,101))return false;
            const ls::Definition catalogDefinition{sectors,stages,policies,0};
            ls::Director catalogDirector;
            if(!catalogDirector.begin(catalogOwner,101,catalogDefinition,capabilities))return false;
            const auto consumeSource=[&](std::size_t source,std::uint32_t revision) {
                dawn::middleware::bap::activity_message::sense_update::SenseObject value{};
                value.registryKey=capabilities[source].registry->key;
                value.slotIndex=capabilities[source].slot;value.slotType=1;
                value.hasNativeSchema=true;value.nativeSchema=0x80807ECC;
                value.nativeRevision=revision;value.hasRootDelta=true;
                value.sourceDelta.present=1;
                value.sourceDelta.scalar[0]=catalogService.generation(source);
                value.sourceDelta.consumedPresent=true;
                value.sourceDelta.consumedCount=capabilities[source].categories;
                value.sourceDelta.consumed[0]=catalogService.target(source);
                if(capabilities[source].categories==2)
                    value.sourceDelta.consumed[1]=catalogService.second_target(source);
                return catalogService.observe(sectors[sectorIndex].bubble,value)!=nullptr;
            };
            const auto findBoss=[&] {
                const auto& sector=sectors[sectorIndex];std::size_t boss=capabilities.size();
                for(std::size_t relative=0;relative<sector.stageCount;++relative) {
                    const auto& stage=stages[sector.firstStage+relative];
                    for(std::size_t source=stage.first;
                        source<std::size_t(stage.first)+stage.count;++source)
                        if(policies[source].boss)boss=source;
                }
                return boss;
            };
            const auto& sector=sectors[sectorIndex];
            if(!catalogDirector.update(sector.bubble,true,catalogService,never))return false;
            activity::placement::wire::Batch reward{};
            if(!catalogDirector.append_rewards(sector.bubble,true,reward) || reward.count)return false;
            for(std::size_t relative=0;relative<sector.stageCount;++relative) {
                const auto& stage=stages[sector.firstStage+relative];
                for(std::size_t source=stage.first;
                    source<std::size_t(stage.first)+stage.count;++source)
                    if(!catalogService.target(source))return false;
            }
            const auto entryRevision=catalogService.revision();
            if(!catalogDirector.update(sector.bubble,true,catalogService,never)
                || catalogService.revision()!=entryRevision)return false;
            const auto boss=findBoss();
            const auto cleared=[](std::size_t,std::uint16_t,const ls::Stage&) {return true;};
            if(boss==capabilities.size() || !consumeSource(boss,1)
                || !catalogDirector.update(sector.bubble,true,catalogService,cleared)
                || catalogDirector.diagnostics(sectorIndex).phase!=ls::Phase::cleared
                || !catalogDirector.append_rewards(sector.bubble,true,reward) || reward.count!=1
                || reward.entries[0].registry!=sector.rewardRegistry->key
                || reward.entries[0].slot!=sector.rewardSlot || reward.entries[0].generation!=1
                || reward.entries[0].interactionMode!=activity::placement::interaction::Mode::enabled
                || !catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,1)
                || catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,2))return false;
            activity::placement::wire::Batch loadingReward{};
            if(!catalogDirector.append_rewards(sector.bubble,false,loadingReward) || loadingReward.count
                || !catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,1)
                || !catalogDirector.update((sector.bubble+1U)%64U,true,catalogService,never)
                || catalogDirector.diagnostics(sectorIndex).phase!=ls::Phase::resetReady
                || !catalogDirector.update(sector.bubble,true,catalogService,never))return false;
            if(!catalogService.renewal(boss).pending
                || !catalogService.commit_renewal(boss))return false;
            std::size_t optional=capabilities.size();
            for(std::size_t relative=0;relative<sector.stageCount && optional==capabilities.size();++relative) {
                const auto& stage=stages[sector.firstStage+relative];
                for(std::size_t source=stage.first;
                    source<std::size_t(stage.first)+stage.count;++source)
                    if(!policies[source].boss) {optional=source;break;}
            }
            if(optional==capabilities.size() || !consumeSource(optional,2)
                || !catalogDirector.update(sector.bubble,true,catalogService,cleared)
                || catalogService.renewal(optional).pending)return false;
            if(!catalogDirector.update(sector.bubble,true,catalogService,cleared)
                || catalogDirector.diagnostics(sectorIndex).phase!=ls::Phase::active
                || !consumeSource(boss,2)
                || !catalogDirector.update(sector.bubble,true,catalogService,cleared))return false;
            if(catalogDirector.diagnostics(sectorIndex).phase!=ls::Phase::cleared
                || catalogDirector.diagnostics(sectorIndex).runs!=2)return false;
            reward={};
            if(!catalogDirector.append_rewards(sector.bubble,true,reward) || reward.count!=1
                || reward.entries[0].generation!=2
                || catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,1)
                || !catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,2))return false;
            if(!catalogDirector.update((sector.bubble+1U)%64U,true,catalogService,never)
                || !catalogDirector.update(sector.bubble,true,catalogService,never)
                || !catalogService.renewal(optional).pending
                || !catalogService.renewal(boss).pending
                || !catalogService.commit_renewal(boss)
                || !catalogDirector.update(sector.bubble,true,catalogService,cleared)
                || catalogDirector.diagnostics(sectorIndex).phase!=ls::Phase::active
                || !consumeSource(boss,3)
                || !catalogDirector.update(sector.bubble,true,catalogService,cleared)
                || catalogDirector.diagnostics(sectorIndex).phase!=ls::Phase::cleared
                || catalogDirector.diagnostics(sectorIndex).runs!=3)return false;
            reward={};
            if(!catalogDirector.append_rewards(sector.bubble,true,reward) || reward.count!=1
                || reward.entries[0].generation!=3
                || catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,2)
                || !catalogDirector.reward_ticket(reward.entries[0].registry,reward.entries[0].slot,3))return false;
        }
        return true;
    };
    CHECK(exerciseCatalog(catalog::mercury::kCapabilities,catalog::mercury::kSectors,
        catalog::mercury::kStages,catalog::mercury::kPolicies,0x10000U));
    CHECK(exerciseCatalog(catalog::mars::kCapabilities,catalog::mars::kSectors,
        catalog::mars::kStages,catalog::mars::kPolicies,0x20000U));
    CHECK(exerciseCatalog(catalog::io::kCapabilities,catalog::io::kSectors,
        catalog::io::kStages,catalog::io::kPolicies,0x30000U));
    CHECK(exerciseCatalog(catalog::titan::kCapabilities,catalog::titan::kSectors,
        catalog::titan::kStages,catalog::titan::kPolicies,0x40000U));
    CHECK(exerciseCatalog(catalog::nessus::kCapabilities,catalog::nessus::kSectors,
        catalog::nessus::kStages,catalog::nessus::kPolicies,0x50000U));
    CHECK(exerciseCatalog(catalog::tangled_shore::kCapabilities,catalog::tangled_shore::kSectors,
        catalog::tangled_shore::kStages,catalog::tangled_shore::kPolicies,0x60000U));
    CHECK(exerciseCatalog(catalog::dreaming_city::kCapabilities,catalog::dreaming_city::kSectors,
        catalog::dreaming_city::kStages,catalog::dreaming_city::kPolicies,0x70000U));
    CHECK(exerciseCatalog(catalog::edz::kCapabilities,catalog::edz::kSectors,
        catalog::edz::kStages,catalog::edz::kPolicies,0x80000U));
    CHECK(exerciseCatalog(catalog::moon::kCapabilities,catalog::moon::kSectors,
        catalog::moon::kStages,catalog::moon::kPolicies,0x90000U));

    // Mercury's ordinary surface is bubble15; a qualified bubble16 preload
    // requests the complete Pariah's Refuge population without main-bubble arrival.
    {
        const activity::population::Owner mercuryOwner{0x4D455243U,{1}};
        activity::population::Service mercuryService;
        CHECK(mercuryService.begin(mercuryOwner,catalog::mercury::kCapabilities,91));
        auto definition=catalog::mercury::definition(0);
        ls::Director mercuryDirector;
        CHECK(mercuryDirector.begin(mercuryOwner,91,definition,catalog::mercury::kCapabilities));
        CHECK(mercuryDirector.update(15,true,mercuryService,never,16));
        CHECK(mercuryService.project(16).count==catalog::mercury::kCapabilities.size());
        CHECK(mercuryDirector.diagnostics(0).stage==0 && mercuryDirector.diagnostics(0).runs==1);
    }

    // A qualified incoming region queues the complete sector exactly once. It
    // is not an arrival or completion signal, and publication is idempotent.
    {
        const activity::population::Owner preloadOwner{0x4321U,{3}};
        activity::population::Service preload;CHECK(preload.begin(preloadOwner,kCapabilities,55));
        ls::Director prewarm;CHECK(prewarm.begin(preloadOwner,55,kDefinition,kCapabilities));
        CHECK(prewarm.update(6,true,preload,never,7));
        CHECK(preload.target(0)==2 && preload.target(1)==1 && preload.target(2)==1);
        const auto revision=preload.revision();
        CHECK(prewarm.update(6,true,preload,never,7));
        CHECK(preload.revision()==revision);
        auto remoteClear=[](std::size_t,std::uint16_t,const ls::Stage&) {return true;};
        CHECK(prewarm.update(6,true,preload,remoteClear,UINT32_MAX));
        CHECK(preload.target(2)==1 && prewarm.diagnostics(0).stage==0);
    }

    const activity::population::Owner owner{0x1234U,{9}};
    activity::population::Service service;CHECK(service.begin(owner,kCapabilities,77));
    ls::Director director;CHECK(director.begin(owner,77,kDefinition,kCapabilities));
    CHECK(director.update(7,true,service,never));
    CHECK(service.target(0)==2 && service.target(1)==1 && service.target(2)==1);
    const auto firstRevision=service.revision();
    CHECK(director.update(7,true,service,never));
    CHECK(service.revision()==firstRevision); // boundary noise cannot duplicate a resident boss/stage
    CHECK(director.update(6,true,service,never));
    CHECK(director.update(7,true,service,never));
    CHECK(service.revision()==firstRevision); // uncleared exit/re-entry is not a reset

    const auto consume=[&](std::size_t index,std::uint32_t nativeRevision) {
        dawn::middleware::bap::activity_message::sense_update::SenseObject value{};
        value.registryKey=kCapabilities[index].registry->key;value.slotIndex=kCapabilities[index].slot;
        value.slotType=1;value.hasNativeSchema=true;value.nativeSchema=0x80807ECC;
        value.nativeRevision=nativeRevision;value.hasRootDelta=true;value.sourceDelta.present=1;
        value.sourceDelta.scalar[0]=service.generation(index);value.sourceDelta.consumedPresent=true;
        value.sourceDelta.consumedCount=1;value.sourceDelta.consumed[0]=service.target(index);
        return service.observe(7,value)!=nullptr;
    };
    auto clear=[](std::size_t,std::uint16_t,const ls::Stage&) {return true;};
    CHECK(consume(0,1) && consume(1,1));
    CHECK(director.update(7,true,service,clear));
    CHECK(director.diagnostics(0).phase==ls::Phase::active); // optional deaths do not clear the boss gate
    CHECK(consume(2,1));
    CHECK(director.update(7,true,service,clear));
    CHECK(director.diagnostics(0).phase==ls::Phase::cleared);
    const auto clearRevision=service.revision();
    CHECK(director.update(7,true,service,clear));
    CHECK(service.revision()==clearRevision); // standing in a cleared sector stays cleared
    CHECK(director.update(7,false,service,never));
    CHECK(director.diagnostics(0).phase==ls::Phase::cleared); // loading is not an exit
    CHECK(director.update(6,true,service,never,7));
    CHECK(director.diagnostics(0).phase==ls::Phase::resetReady);
    CHECK(service.revision()==clearRevision); // outgoing/stale prefetch cannot rearm on the exit tick

    unsigned staleClearCalls{};
    const auto staleClear=[&](std::size_t,std::uint16_t,const ls::Stage&) {++staleClearCalls;return true;};
    CHECK(director.update(7,true,service,staleClear));
    CHECK(director.diagnostics(0).phase==ls::Phase::active);
    CHECK(director.diagnostics(0).runs==2);
    CHECK(service.renewal(0).pending && service.renewal(1).pending && service.renewal(2).pending);
    CHECK(staleClearCalls==0); // old-generation consumed/quiescent state cannot clear the renewal
    CHECK(service.revision()==clearRevision);
    service.cancel_renewal(0);
    CHECK(service.commit_renewal(1));
    CHECK(service.commit_renewal(2));
    CHECK(director.update(7,true,service,staleClear));
    CHECK(service.renewal(0).pending && !service.renewal(1).pending);
    CHECK(staleClearCalls==0); // partial commits retry only the cancelled corpse-held source
    CHECK(service.commit_renewal(0));
    CHECK(director.update(7,true,service,staleClear));
    CHECK(staleClearCalls==0); // the new generation has not been consumed/admitted

    std::printf("PASS %u lost-sector runtime checks\n",checks);
}
