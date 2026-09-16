#include "server/runtime/activity/haunted_forest_definition.h"
#include "server/runtime/activity/persistent_activity.h"
#include "state/activity/coo/mission_script.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace hf = sunrise::server::runtime::activity::haunted_forest::mode;
namespace activity = sunrise::server::runtime::activity;
namespace coo = sunrise::state::activity::coo;
namespace hud = sunrise::server::runtime::activity::haunted_forest_hud;
namespace env = sunrise::server::runtime::activity::haunted_forest::environment;
namespace music = sunrise::server::runtime::activity::music;
namespace round_music = sunrise::server::runtime::activity::round_music;

namespace {
bool check(bool value, const char* expression) noexcept {
    if (!value) std::fprintf(stderr, "FAIL: %s\n", expression);
    return value;
}

struct CountingWriter final {
    std::size_t bits{};
    bool write(std::uint64_t, std::uint8_t width) noexcept { bits += width; return true; }
    std::size_t bit_count() const noexcept { return bits; }
};

activity::ambient_population::sense::SenseObject qualified_occupancy(
    std::uint32_t revision, std::int32_t count = 1, std::int32_t token = 0) {
    const auto countCode = static_cast<std::uint32_t>(static_cast<std::int64_t>(count)
        + 2147483648LL);
    const auto tokenCode = static_cast<std::uint32_t>(static_cast<std::int64_t>(token)
        + 2147483648LL);
    activity::ambient_population::sense::SenseObject value{};
    value.registryKey = hf::kOwnedRegistries[0].key;
    value.slotType = 30; value.slotIndex = 109;
    value.hasNativeSchema = true; value.nativeSchema = 0x80809531;
    value.nativeRevision = revision; value.hasRootDelta = true; value.bodyBits = 99;
    value.bodyFirst = (std::uint64_t{1} << 63) | (std::uint64_t(count != 0) << 62)
        | (std::uint64_t(countCode) << 29) | (tokenCode >> 3);
    value.bodySecond = (std::uint64_t(tokenCode & 7U) << 32) | revision;
    return value;
}
}

#define CHECK(value) if (!check(static_cast<bool>(value), #value)) return 1

int main(int argc, char** argv) {
    const std::filesystem::path path = argc > 1
        ? std::filesystem::path(argv[1])
        : std::filesystem::path("Sunrise/scripts/infinite_abyss.json");

    std::string error;
    auto document = coo::script::MissionDocument::read_native_policy(
        path, hf::kProfile, error);
    if (!document) {
        std::fprintf(stderr, "MissionDocument rejected %ls: %s\n",
            path.c_str(), error.c_str());
        return 1;
    }
    // These assertions exercise the checked-in JSON through the production
    // parser and its native profile authorization, not a text/regex fixture.
    CHECK(document->views().valid);
    CHECK(document->views().missionId == "infinite_abyss");
    CHECK(document->views().profileId == hf::kProfile.id);
    CHECK(coo::script::authorized(document->views(), hf::kProfile));
    {
        auto actions=hud::actions();
        std::uint64_t ticks{};
        CHECK(activity::round_activity::duration_ticks(hf::kRounds,*document,ticks));
        CHECK(activity::round_activity::bind_timer(hf::kRounds,*document,actions));
        const auto duration=document->views().parameter("forest.duration_ms")->value;
        CHECK(ticks==duration*673200ULL/1000ULL);
        activity::cue_presentation::Service configured;
        const activity::population::Owner testOwner{31,{4}};
        CHECK(configured.begin(testOwner,15,actions));
        CHECK(configured.request({testOwner,15,1,1,hud::kStart,hud::kQualifiedEntryEvidence,0})
            ==activity::cue_presentation::Result::accepted);
        CHECK(configured.presentation()->timer.remaining==ticks);
    }
    {
        // Exercise the actual Haunted recipe through shared request/cycle
        // admission. The arena exclusion must survive disable and reseeding.
        namespace generator = activity::forest_generator;
        generator::Service service;
        const generator::Owner generatorOwner{91,{9}};
        std::array<generator::AnchorConfiguration,1> coordinates{};
        coordinates[0][3]={2,2};
        const std::array<std::uint32_t,1> seeds{12345};
        CHECK(service.begin(generatorOwner,1,hf::kGenerators,seeds,coordinates));
        std::uint32_t previousSeed{};
        // All four authored approach centers occupy coarse column 2. North
        // is at generator height 0, east/west at 1, and south at 2. Exercise
        // a full platform rotation plus return to the initial platform.
        constexpr std::array<generator::wire::Anchor,4> platformAnchors{{
            {2,1,0,true},{2,1,0,true},{2,0,0,true},{2,2,0,true}}};
        for(std::uint64_t cycle=1;cycle<=5;++cycle) {
            const auto result=cycle==1
                ? service.request({generatorOwner,1,service.revision(),service.last_request()+1,
                    hf::kOwnedRegistries[0].key,1,98},13)
                : service.begin_cycle({generatorOwner,1,service.revision(),service.last_request()+1,
                    hf::kOwnedRegistries[0].key,98,1,cycle},13);
            CHECK(result==generator::Result::accepted);
            const auto state=service.project(13).entries[0].state;
            CHECK(state.primary.enabled && state.primary.seed!=previousSeed);
            CHECK(state.primary.blockedCount==1);
            CHECK((state.primary.blockedCells[0]==generator::wire::Cell{2,2,0}));
            CHECK(state.primary.anchors==platformAnchors);
            CountingWriter writer;
            CHECK(generator::wire::write_payload(writer,state));
            CHECK(writer.bits==generator::wire::body_bits(state)
                && writer.bits==generator::wire::kPayloadBaseBits+48);
            previousSeed=state.primary.seed;
            CHECK(service.request({generatorOwner,1,service.revision(),service.last_request()+1,
                hf::kOwnedRegistries[0].key,2,98},13)==generator::Result::accepted);
            const auto disabled=service.project(13).entries[0].state.primary;
            CHECK(!disabled.enabled && disabled.seed==previousSeed
                && disabled.blockedCount==1 && disabled.blockedCells==state.primary.blockedCells);
        }
    }
    CHECK(document->views().role("round.entry"));
    CHECK(document->views().role("round.traversal"));
    CHECK(document->views().role("round.toEncounter"));
    CHECK(document->views().role("round.encounter"));
    CHECK(document->views().role("round.returning"));
    CHECK(document->views().role("round.rewards"));

    CHECK(hf::kOwnedRegistries.size() == 4);
    CHECK(hf::kOwnedRegistries[0].key == hf::kRegistries[0].key);
    CHECK(hf::kOwnedRegistries[1].key == hf::kArenaRegistry[0].key);
    CHECK(hf::kOwnedRegistries[2].key == hf::kAuxiliaryRegistry[0].key);
    CHECK(hf::kOwnedRegistries[3].key == 0x1EB557ADU);
    CHECK(hf::kOwnedRegistries[3].objectTag == 0x81550360U);
    CHECK(hf::kOwnedRegistries[3].scenario == 0x81550015U);
    CHECK(hf::kRounds.music == &hf::kRoundMusic);
    CHECK(hf::kRounds.music->definition == &hf::kMusicDefinition);
    CHECK(hf::kMusicDefinition.registry == &hf::kOwnedRegistries[3]);
    CHECK(hf::kOwnedRegistries[3].topLevel);
    CHECK(!coo::registry::required(hf::kOwnedRegistries[3],
        hf::kOwnedRegistries[3].scenario, hf::kOwnedRegistries[3].objectTag,
        hf::kOwnedRegistries[3].key,
        std::uint64_t{1} << hf::kOwnedRegistries[3].bubble));
    CHECK(hf::kMusicDefinition.candidates.size() == 8);
    CHECK(hf::kMusicDefinition.candidates[0] == 0x896FF320U);
    CHECK(hf::kMusicDefinition.candidates[1] == 0x5CFE4C22U);
    CHECK(hf::kMusicDefinition.candidates[2] == 0xAC8DD943U);
    CHECK(hf::kMusicDefinition.candidates[4] == 0xA1ED3A41U);
    for (const auto& registry : hf::kOwnedRegistries) CHECK(activity::registry::valid(registry));

    // 12 platform rows, the chest, its block, then o_coffer_1..5 on registry slots 63-67.
    CHECK(hf::kPlacements.size() == 19);
    CHECK(hf::kPlacements[12].slot == 55 && hf::kPlacements[13].slot == 56);
    CHECK(hf::kPlacements[14].slot == 63 && hf::kPlacements[18].slot == 67);
    CHECK(hf::kRewardPlacementIndices.size() == 2);
    CHECK(hf::kRewardPlacementIndices[0] == 12 && hf::kRewardPlacementIndices[1] == 13);
    // The coffers are published only while the host switch admits them.
    CHECK(hf::kCofferPlacementIndices.size() == 5);
    for (std::size_t i = 0; i < hf::kCofferPlacementIndices.size(); ++i)
        CHECK(hf::kCofferPlacementIndices[i] == static_cast<std::uint16_t>(14U + i));
    CHECK(hf::kRounds.experimentalRewardPlacementIndices.size() == 5);
    CHECK(hf::kCaptures.size() == 4);
    CHECK(hf::kOccupancy.size() == 5);
    CHECK(hf::kDevices.size() == 5);
    CHECK(hf::kOwnedPopulationCapabilities.size() == 110);
    CHECK(hf::kGeneratedPalettes.size() == 19);
    CHECK(hf::kRankedPrefabs.size() == 163);
    for (const auto& placement : hf::kPlacements)
        CHECK(placement.registry == &hf::kOwnedRegistries[0]);
    for (std::size_t i=0;i<hf::kPopulationCapabilities.size();++i) {
        const auto& capability=hf::kOwnedPopulationCapabilities[i];
        CHECK(capability.registry == &hf::kOwnedRegistries[1]);
        CHECK(capability.tactical.registry == hf::kOwnedRegistries[1].key);
    }
    const auto& pit=hf::kOwnedPopulationCapabilities.back();
    CHECK(activity::population::valid(pit));
    CHECK(pit.registry==&hf::kOwnedRegistries[0] && pit.slot==59 && pit.rule==160);
    CHECK(pit.tactical.slot==61 && pit.tactical.row==0 && pit.namedMember==60);
    CHECK(hf::kRounds.rewardPopulationIndices.size()==1 && hf::kRounds.rewardPopulationIndices[0]==109);
    CHECK(hf::kRounds.trapOccupancyIndex==4);
    // Unarmed native occupancy is useful to optional music, but must never
    // manufacture a gameplay receipt. Foreign epochs/revisions cannot clear it.
    activity::occupancy_wait::Service pitMonitor;
    const activity::population::Owner pitOwner{77,{3}};
    CHECK(pitMonitor.begin(pitOwner,19,hf::kOccupancy));
    auto pitSense=qualified_occupancy(1);
    pitSense.registryKey=hf::kOwnedRegistries[0].key;pitSense.slotIndex=103;
    unsigned submitted{};
    auto submit=[&](coo::Event){++submitted;return true;};
    CHECK(!pitMonitor.occupied(4));
    CHECK(!pitMonitor.observe(pitOwner,19,13,pitSense,submit));
    CHECK(pitMonitor.occupied(4) && submitted==0);
    pitSense=qualified_occupancy(2,0);
    pitSense.registryKey=hf::kOwnedRegistries[0].key;pitSense.slotIndex=103;
    CHECK(!pitMonitor.observe(pitOwner,20,13,pitSense,submit));
    CHECK(pitMonitor.occupied(4));
    CHECK(!pitMonitor.observe(pitOwner,19,13,pitSense,submit));
    CHECK(!pitMonitor.occupied(4) && submitted==0);
    CHECK(!pitMonitor.occupied(65535));

    CHECK(hf::score_weight(0x80F44C55U) == 2);
    CHECK(hf::score_weight(0x80F44C57U) == 4);
    CHECK(hf::score_weight(0x80F44C67U) == 6);
    CHECK(hf::score_weight(0x80C5964EU) == 0);
    CHECK(hf::score_weight(0xFFFFFFFFU) == 0);
    CHECK(hf::kScores.size() == hf::kPositiveScoreCount);
    for (const auto& prefab : hf::kRankedPrefabs)
        CHECK(hf::score_weight(prefab.tag) == hf::score_weight(prefab.rank));
    CHECK(hf::kRounds.encounters.size() == 13);
    CHECK(hf::kRounds.encounterDestinationId == 5);
    CHECK(hf::kRounds.rewardDestinationId == 6);
    CHECK(hf::kRounds.transitDestinations.size() == 6);
    CHECK(hf::kRounds.platforms.size() == 1);
    CHECK(hf::kRounds.platforms[0].transitDestinationId == 1);
    CHECK(hf::kTransitEffectRoutes[0].effect == 4);
    for(std::size_t i=0;i<3;++i) CHECK(hf::kTransitTarget1[i].slot == 44+i);
    CHECK(hf::kTransitEffectRoutes[0].arrival.slot == 110);
    CHECK(hf::kTransitLanding[0][0].placement.slot == 30
        && hf::kTransitLanding[0][1].placement.slot == 31);
    CHECK(hf::kRounds.completionTimerAsset == hud::kCompletionTimerAsset);
    CHECK(hf::kRounds.environment != nullptr);
    CHECK(hf::kEnvironmentData.effects.size() == 4);
    CHECK(hf::kEnvironmentData.forestSwitches.size() == 6);
    CHECK(hf::kPlacements[12].interactionMode
        == activity::placement::interaction::Mode::enabled);
    // Coffers use their authored interaction controller for cipher redemption.
    for (std::size_t i = 14; i < hf::kPlacements.size(); ++i)
        CHECK(hf::kPlacements[i].interactionMode
            == activity::placement::interaction::Mode::enabled);

    CHECK(activity::native_capture::Runtime::valid(hf::kActivity, *document));

    CHECK(activity::PersistentActivity::valid(hf::kActivity, *document));
    CHECK(hf::kRounds.completionTimerAsset.type == 18);
    CHECK(!activity::round_activity::valid_content_asset(hf::kRounds.completionTimerAsset));

    // Exercise the real persistent startup path with the parsed profile and a
    // valid synthetic clock, including the first no-timer HUD action.
    auto persistentStorage = std::make_unique<activity::PersistentActivity>();
    auto& persistent = *persistentStorage;
    const activity::population::Owner owner{77,{3}};
    auto sharedDocument=std::shared_ptr<const coo::script::MissionDocument>(std::move(document));
    CHECK(persistent.begin(owner,hf::kActivity,sharedDocument,19));
    const activity::activity_clock::Publication clock{
        {owner,19,1,hf::kRegistries[0].scenario,hf::kActivity.bubble},
        {false,1000.0F/30.0F},0};
    const auto frame=persistent.update(hf::kActivity.bubble,true,{},true,clock);
    CHECK(frame.clock.domain == clock.domain);
    CHECK(frame.music.count == 1);
    CHECK(frame.music.entries[0].registry == hf::kOwnedRegistries[3].key
        && frame.music.entries[0].slot == 1
        && frame.music.entries[0].candidateCount == 8
        && frame.music.entries[0].active[0] == 1U);
    CHECK(!frame.optionalPresentationFailure);
    {
        const auto& request = frame.music.entries[0];
        CountingWriter writer;
        CHECK(music::wire::write(writer, request));
        CHECK(writer.bit_count() == music::wire::kBits);
    }

    // The real activity topology has three native roots followed by the
    // Forest-local group. Music owns the exact four-slot root schema, but its
    // bubble remains only the arrival context for this admission.
    {
        namespace catalog = activity::registry::catalog;
        namespace roster_wire = activity::registry::wire;
        struct Storage final {
            std::array<catalog::RosterGroup, 8> rosterGroups{};
            std::array<roster_wire::BubbleSubBlock, 4> rosterSubBlocks{};
            std::array<std::array<std::uint32_t, 8>, 4> rosterSubBlockKeys{};
        };
        auto fill = [](catalog::RosterGroup& row, const coo::registry::Definition& definition) {
            row = {};
            row.registryKey = definition.key;
            row.objectTag = definition.objectTag;
            row.slotCount = static_cast<std::uint16_t>(definition.slots.size());
            for (std::size_t i = 0; i < definition.slots.size(); ++i) {
                const auto& slot = definition.slots[i];
                row.slotIndices[i] = slot.index;
                row.slotTypes[i] = slot.type;
                row.slotFlags[i] = slot.flags();
                row.componentClasses[i] = slot.componentClass;
                row.senseSchemas[i] = slot.senseSchema;
                row.authSchemas[i] = slot.authSchema;
                row.descriptorTags[i] = slot.descriptorTag;
            }
        };
        auto fillRoot = [](catalog::RosterGroup& row, std::uint32_t key, std::uint32_t tag) {
            row = {};
            row.registryKey = key;
            row.objectTag = tag;
            row.slotCount = 1;
            row.slotIndices[0] = 0;
            row.slotTypes[0] = 13;
            row.slotFlags[0] = 2;
            row.componentClasses[0] = 1;
            row.senseSchemas[0] = UINT32_MAX;
            row.authSchemas[0] = 1;
            row.descriptorTags[0] = 1;
        };
        auto resolved = std::make_unique<std::array<catalog::RosterGroup, 4>>();
        auto& resolvedRows = *resolved;
        fillRoot(resolvedRows[0], 0x4786C0E0U, 1);
        fill(resolvedRows[1], hf::kOwnedRegistries[3]);
        fillRoot(resolvedRows[2], 0x29D7B029U, 3);
        fill(resolvedRows[3], hf::kOwnedRegistries[0]);

        std::array<std::uint8_t, 1> localPresence{1};
        std::array<std::uint8_t, 1> localStates{0x87};
        std::array<std::uint32_t, 1> localKeys{hf::kOwnedRegistries[0].key};
        std::array<roster_wire::BubbleSubBlock, 1> localBlocks{{
            {13, localKeys, localPresence, localStates},
        }};
        roster_wire::Roster roster{};
        for (std::size_t i = 0; i < resolvedRows.size(); ++i) {
            const auto& row = resolvedRows[i];
            roster.groups[i] = {row.registryKey,
                std::span(row.slotTypes).first(row.slotCount),
                std::span(row.slotFlags).first(row.slotCount),
                std::span(row.slotIndices).first(row.slotCount)};
        }
        roster.groupCount = 4;
        roster.topLevelGroupCount = 3;
        roster.playerKeyGroup = resolvedRows[0].registryKey;
        roster.bubbleSubBlocks = localBlocks;

        catalog::Definition layout{};
        constexpr std::string_view activityName{"infinite_abyss"};
        std::copy(activityName.begin(), activityName.end(), layout.name.begin());
        layout.nameLength = static_cast<std::uint8_t>(activityName.size());
        layout.tag = hf::kOwnedRegistries[3].scenario;
        layout.bubbleCount = 14;
        layout.bubbleHashes[hf::kOwnedRegistries[3].bubble] = hf::kOwnedRegistries[3].bubbleHash;
        auto storage = std::make_unique<Storage>();
        const auto find = [&](std::uint32_t key, catalog::RosterGroup& row) noexcept {
            for (const auto& candidate : resolvedRows) {
                if (candidate.registryKey == key) {
                    row = candidate;
                    return true;
                }
            }
            return false;
        };
        const auto admit = [&](const coo::registry::Definition& definition,
            roster_wire::Roster& target) {
            return activity::registry::admit(layout, *storage, target, definition, find);
        };
        const auto sameShape = [](const roster_wire::Roster& a, const roster_wire::Roster& b) {
            if (a.groupCount != b.groupCount || a.topLevelGroupCount != b.topLevelGroupCount
                || a.playerKeyGroup != b.playerKeyGroup
                || a.bubbleSubBlocks.data() != b.bubbleSubBlocks.data()
                || a.bubbleSubBlocks.size() != b.bubbleSubBlocks.size()) return false;
            for (std::size_t i = 0; i < a.groupCount; ++i) {
                const auto& x = a.groups[i];
                const auto& y = b.groups[i];
                if (x.key != y.key || x.slotTypes.data() != y.slotTypes.data()
                    || x.slotTypes.size() != y.slotTypes.size()
                    || x.slotFlags.data() != y.slotFlags.data()
                    || x.slotIndices.data() != y.slotIndices.data()) return false;
            }
            return true;
        };

        const auto base = roster;
        CHECK(admit(hf::kOwnedRegistries[3], roster) == activity::registry::Admission::present);
        CHECK(sameShape(roster, base));

        auto malformedCount = roster;
        malformedCount.topLevelGroupCount = malformedCount.groupCount + 1;
        CHECK(admit(hf::kOwnedRegistries[3], malformedCount)
            == activity::registry::Admission::invalid);

        auto localDeclaration = hf::kOwnedRegistries[3];
        localDeclaration.topLevel = false;
        CHECK(admit(localDeclaration, roster) == activity::registry::Admission::conflict);

        auto absent = roster;
        absent.groups[0] = roster.groups[0];
        absent.groups[1] = roster.groups[2];
        absent.groups[2] = roster.groups[3];
        absent.groupCount = 3;
        absent.topLevelGroupCount = 2;
        CHECK(admit(hf::kOwnedRegistries[3], absent) == activity::registry::Admission::missingGroup);

        auto duplicate = roster;
        duplicate.groups[4] = roster.groups[1];
        duplicate.groupCount = 5;
        CHECK(admit(hf::kOwnedRegistries[3], duplicate) == activity::registry::Admission::conflict);

        std::array<std::uint32_t, 1> localMusicKeys{hf::kOwnedRegistries[3].key};
        std::array<roster_wire::BubbleSubBlock, 2> localConflictBlocks{{
            localBlocks[0], {13, localMusicKeys, localPresence, localStates},
        }};
        auto localConflict = roster;
        localConflict.bubbleSubBlocks = localConflictBlocks;
        CHECK(admit(hf::kOwnedRegistries[3], localConflict) == activity::registry::Admission::conflict);

        std::array<std::uint32_t, 3> topKeys{
            resolvedRows[0].registryKey, hf::kOwnedRegistries[3].key, resolvedRows[2].registryKey};
        std::array<std::uint8_t, 3> topPresence{1, 1, 1};
        std::array<std::uint8_t, 3> topStates{0x83, 0x84, 0x85};
        roster.topLevelKeys = topKeys;
        roster.topLevelPresence = topPresence;
        roster.topLevelStates = topStates;
        CHECK(admit(hf::kOwnedRegistries[3], roster) == activity::registry::Admission::present);
        topPresence[1] = 0;
        CHECK(admit(hf::kOwnedRegistries[3], roster) == activity::registry::Admission::conflict);
        topPresence[1] = 1;

        const auto savedSchema = resolvedRows[1].authSchemas[1];
        resolvedRows[1].authSchemas[1] ^= 1;
        CHECK(admit(hf::kOwnedRegistries[3], roster) == activity::registry::Admission::schemaMismatch);
        resolvedRows[1].authSchemas[1] = savedSchema;
        roster.topLevelKeys = {};
        roster.topLevelPresence = {};
        roster.topLevelStates = {};

        auto topBatch = frame.music;
        CHECK(topBatch.entries[0].scope == UINT32_MAX);
        CHECK(music::wire::valid(topBatch, roster, 104));
        std::array<std::uint32_t, 2> mixedLocalKeys{localKeys[0], hf::kOwnedRegistries[3].key};
        std::array<std::uint8_t, 2> rootLocalPresence{1, 1};
        std::array<std::uint8_t, 2> rootLocalStates{0x87, 0x88};
        std::array<roster_wire::BubbleSubBlock, 1> rootLocalReferenceBlocks{{
            {13, mixedLocalKeys, rootLocalPresence, rootLocalStates},
        }};
        auto rootWithLocalReference = roster;
        rootWithLocalReference.bubbleSubBlocks = rootLocalReferenceBlocks;
        CHECK(!music::wire::valid(topBatch, rootWithLocalReference, 104));
        auto localScopeBatch = topBatch;
        localScopeBatch.entries[0].scope = 13;
        CHECK(!music::wire::valid(localScopeBatch, roster, 104));
        auto localAsRootBatch = topBatch;
        localAsRootBatch.entries[0].registry = hf::kOwnedRegistries[0].key;
        CHECK(!music::wire::valid(localAsRootBatch, roster, 104));
        roster.topLevelKeys = topKeys;
        topPresence[1] = 0;
        roster.topLevelPresence = topPresence;
        roster.topLevelStates = topStates;
        CHECK(!music::wire::valid(topBatch, roster, 104));
        roster.topLevelKeys = {};
        roster.topLevelPresence = {};
        roster.topLevelStates = {};
        auto duplicateWire = roster;
        duplicateWire.groups[4] = roster.groups[1];
        duplicateWire.groupCount = 5;
        CHECK(!music::wire::valid(topBatch, duplicateWire, 104));
    }
    CHECK(frame.statusEffects.count == 4);
    for (std::size_t i=0; i<frame.statusEffects.count; ++i) {
        CHECK(!frame.statusEffects.entries[i].enabled);
        CHECK(frame.statusEffects.entries[i].registry == hf::kOwnedRegistries[0].key);
        CHECK(frame.statusEffects.entries[i].slot == env::kEffectSlots[i]);
    }
    CHECK(frame.nativeForestSwitches.count == 6);
    for (std::size_t i=0; i<frame.nativeForestSwitches.count; ++i) {
        CHECK(frame.nativeForestSwitches.entries[i].key == env::kForestSwitches[i].key);
        CHECK(frame.nativeForestSwitches.entries[i].value == env::kForestSwitches[i].value);
    }
    CHECK(frame.nativeForestSwitches.entries[4].key == 0x045DC993U);
    CHECK(frame.nativeForestSwitches.entries[4].value == 0xFE1640D2U);
    CHECK(frame.nativeForestSwitches.entries[5].key == 0x98CBCE18U);
    CHECK(frame.nativeForestSwitches.entries[5].value == 0xB955F8A7U);
    CHECK(frame.cues.count == 1);
    CHECK(frame.cues.entries[0].event == hud::actions()[7].presentation.event);
    CHECK(!frame.cues.entries[0].hasTimer);
    CHECK(persistent.round().round_snapshot().phase == activity::timed_round::Phase::entry);

    // The shared selector policy preserves all authored ordinals while only
    // selecting the four reconstructed host transitions. Repeated same-phase
    // updates do not replay the graph or advance its request sequence.
    round_music::Service musicService;
    CHECK(musicService.configure(hf::kRounds.music));
    const auto musicContext = music::Context{{77,{3}},19,123,77,3,78,13,true,true};
    auto checkSelection = [&](activity::timed_round::Phase phase, bool expired,
        std::uint32_t mask, std::uint64_t sequence) -> bool {
        if (!check(musicService.update(musicContext,phase,expired), "music selection update")) return false;
        music::wire::Batch output{};
        if (!check(musicService.append(output) && output.count == 1, "music selection retained")) return false;
        if (!check(output.entries[0].active[0] == mask, "music selection mask")) return false;
        return check(musicService.sequence() == sequence, "music selection sequence");
    };
    CHECK(checkSelection(activity::timed_round::Phase::entry,false,1U,1));
    CHECK(checkSelection(activity::timed_round::Phase::entry,false,1U,1));
    CHECK(checkSelection(activity::timed_round::Phase::traversal,false,1U,1));
    CHECK(checkSelection(activity::timed_round::Phase::entry,false,1U,1));
    CHECK(checkSelection(activity::timed_round::Phase::encounter,false,2U,2));
    CHECK(checkSelection(activity::timed_round::Phase::encounter,true,4U,3));
    CHECK(checkSelection(activity::timed_round::Phase::returning,false,16U,4));
    CHECK(checkSelection(activity::timed_round::Phase::rewards,false,16U,4));
    CHECK(checkSelection(activity::timed_round::Phase::complete,false,0U,5));
    CHECK(checkSelection(activity::timed_round::Phase::complete,false,0U,5));
    round_music::Service trapMusic;
    CHECK(trapMusic.configure(hf::kRounds.music));
    CHECK(trapMusic.update(musicContext,activity::timed_round::Phase::rewards,false,false,true));
    CHECK(trapMusic.selection()==hf::kRoundMusic.trap && trapMusic.sequence()==1);
    CHECK(trapMusic.update(musicContext,activity::timed_round::Phase::rewards,false,false,true));
    CHECK(trapMusic.sequence()==1);
    CHECK(trapMusic.update(musicContext,activity::timed_round::Phase::rewards,false,false,false));
    CHECK(trapMusic.selection()==hf::kRoundMusic.victory && trapMusic.sequence()==2);
    CHECK(trapMusic.update(musicContext,activity::timed_round::Phase::encounter,false,false,true));
    CHECK(trapMusic.selection()==hf::kRoundMusic.boss);
    CHECK(trapMusic.update(musicContext,activity::timed_round::Phase::rewards,false,true,true));
    CHECK(trapMusic.selection()==0);
    round_music::Service lifecycleMusic;
    CHECK(lifecycleMusic.configure(hf::kRounds.music));
    CHECK(lifecycleMusic.update(musicContext,activity::timed_round::Phase::entry,false));
    CHECK(lifecycleMusic.update(musicContext,activity::timed_round::Phase::rewards,false));
    CHECK(lifecycleMusic.update(musicContext,activity::timed_round::Phase::rewards,false,true));
    music::wire::Batch lifecycleOutput{};
    CHECK(lifecycleMusic.append(lifecycleOutput) && lifecycleOutput.count == 1
        && lifecycleOutput.entries[0].active[0] == 0U && lifecycleMusic.sequence() == 3);
    auto foreignContext = musicContext;
    foreignContext.owner.incarnation.value = 4;
    CHECK(!musicService.update(foreignContext,activity::timed_round::Phase::entry,false));
    CHECK(musicService.failed());

    // Missing admission disables only optional music publication. It does not
    // create a body and does not become a gameplay failure.
    round_music::Service unavailable;
    CHECK(unavailable.configure(hf::kRounds.music));
    auto unavailableContext = musicContext;
    unavailableContext.admitted = false;
    CHECK(unavailable.update(unavailableContext,activity::timed_round::Phase::entry,false));
    music::wire::Batch noMusic{};
    CHECK(unavailable.append(noMusic) && noMusic.count == 0 && !unavailable.failed());

    // The parsed profile uses the activity session namespace for capture
    // commands, while entry remains visibly unarmed at position 0.2.
    auto integrationStorage = std::make_unique<activity::PersistentActivity>();
    auto& integration = *integrationStorage;
    const activity::population::Owner integrationOwner{0x9EAA300100200001ULL, {7}};
    CHECK(integration.begin(integrationOwner, hf::kActivity, sharedDocument, 20));
    const activity::activity_clock::Publication integrationClock{
        {integrationOwner, 20, 1, hf::kRegistries[0].scenario, hf::kActivity.bubble},
        {false, 1000.0F / 30.0F}, 0};
    auto entry = integration.update(hf::kActivity.bubble, true, {}, true, integrationClock);
    CHECK(entry.devices.count == 1 && entry.devices.entries[0].registry == hf::kOwnedRegistries[0].key
        && entry.devices.entries[0].slot == hf::kPlatforms[0].device.slot
        && entry.devices.entries[0].state.position.value == 0.2F
        && entry.devices.entries[0].state.position.revision == 1);
    CHECK(!integration.capture().state(0).requested);
    CHECK(integration.generator().project(hf::kActivity.bubble).count == 0);

    CHECK(integration.observe_occupancy(integrationOwner, 20, hf::kActivity.bubble,
        qualified_occupancy(1)));
    for (int i = 0; i < 4; ++i)
        entry = integration.update(hf::kActivity.bubble, true, {}, true, integrationClock);
    const auto& capture = integration.capture().state(0);
    CHECK(capture.requested && capture.ticket.runIdentity
        == activity::capture_feedback::RunIdentity::activitySession
        && capture.ticket.token.run == integrationOwner.sessionId
        && capture.ticket.requested.clock.running
        && activity::capture_feedback::valid(capture.ticket));
    CHECK(entry.devices.count == 1 && entry.devices.entries[0].state.position.value == 0.1F
        && entry.devices.entries[0].state.position.revision == 2);
    CHECK(entry.generators.count == 0
        && integration.generator().project(hf::kActivity.bubble).count == 0);
    activity::capture_bridge::release(integrationOwner);

    // Exercise the authored branch/terror/results requests through the real
    // cue service, including its timer-preserving variant update path.
    activity::cue_presentation::Service cues;
    CHECK(cues.begin(owner,19,hud::actions()));
    CHECK(cues.request({owner,19,1,1,hud::kInitialEnter,0,UINT64_MAX})
        == activity::cue_presentation::Result::accepted);
    CHECK(cues.request({owner,19,2,2,hud::kStart,hud::kQualifiedEntryEvidence,10})
        == activity::cue_presentation::Result::accepted);
    const auto running=cues.presentation()->timer;
    CHECK(cues.update({owner,19,3,3,hud::kStart,2,{}})
        == activity::cue_presentation::Result::accepted);
    CHECK(cues.presentation()->timer == running && cues.presentation()->variant == 2);
    CHECK(cues.request({owner,19,4,4,hud::kTerror,0,UINT64_MAX})
        == activity::cue_presentation::Result::accepted);
    CHECK(cues.presentation()->timer == running);
    CHECK(cues.request({owner,19,5,5,hud::kResults,0,20})
        == activity::cue_presentation::Result::accepted);
    CHECK(!cues.presentation()->timer.advancing);
    CHECK(cues.update({owner,19,6,6,hud::kResults,3,{}})
        == activity::cue_presentation::Result::accepted);
    CHECK(cues.presentation()->variant == 3);

    // Expiry retains the current branch and accepts its final kill progress.
    activity::cue_presentation::Service collapse;
    CHECK(collapse.begin(owner,19,hud::actions()));
    CHECK(collapse.request({owner,19,1,1,hud::kStart,hud::kQualifiedEntryEvidence,10})
        == activity::cue_presentation::Result::accepted);
    CHECK(collapse.request({owner,19,2,2,hud::kCollapse,0,20})
        == activity::cue_presentation::Result::accepted);
    CHECK(collapse.update({owner,19,3,3,hud::kCollapse,9,
        activity::cue_presentation::wire::Progress{98,100}})
        == activity::cue_presentation::Result::accepted);
    CHECK(collapse.update({owner,19,4,4,hud::kCollapse,{},
        activity::cue_presentation::wire::Progress{100,100}})
        == activity::cue_presentation::Result::accepted);
    CHECK(collapse.presentation()->variant==9 && collapse.presentation()->hasProgress
        && collapse.presentation()->progress.current==100 && collapse.presentation()->timer.advancing);
    CHECK(collapse.request({owner,19,5,5,hud::kCollapse,0,30})
        == activity::cue_presentation::Result::accepted);
    CHECK(collapse.update({owner,19,6,6,hud::kCollapse,0,{}})
        == activity::cue_presentation::Result::accepted);
    CHECK(collapse.presentation()->variant==0 && !collapse.presentation()->hasProgress);

    activity::round_sequence::Service sequences;
    activity::public_event::sequence::Context sequenceContext{owner,19,1,1,1,1000,78,13,true,true};
    using Phase=activity::timed_round::Phase;
    CHECK(sequences.update(hf::kRoundSequences,sequenceContext,1,Phase::traversal));
    activity::public_event::sequence::wire::Batch sequenceBatch{};
    CHECK(sequences.append(sequenceBatch) && sequenceBatch.count==0);
    CHECK(sequences.update(hf::kRoundSequences,sequenceContext,1,Phase::toEncounter));
    CHECK(sequences.append(sequenceBatch) && sequenceBatch.count==1
        && sequenceBatch.entries[0].registry==0x34D23982 && sequenceBatch.entries[0].slot==89
        && sequenceBatch.entries[0].generation==1 && sequenceBatch.entries[0].startTicks==1000);
    sequenceContext.clockTicks=2000;
    CHECK(sequences.update(hf::kRoundSequences,sequenceContext,1,Phase::toEncounter));
    sequenceBatch={};CHECK(sequences.append(sequenceBatch) && sequenceBatch.entries[0].startTicks==1000);
    CHECK(sequences.update(hf::kRoundSequences,sequenceContext,1,Phase::returning));
    sequenceBatch={};CHECK(sequences.append(sequenceBatch) && sequenceBatch.count==2
        && sequenceBatch.entries[1].registry==0x41D79D07 && sequenceBatch.entries[1].slot==129);
    CHECK(sequences.update(hf::kRoundSequences,sequenceContext,1,Phase::rewards));
    sequenceContext.clockTicks=3000;
    CHECK(sequences.update(hf::kRoundSequences,sequenceContext,2,Phase::toEncounter));
    sequenceBatch={};CHECK(sequences.append(sequenceBatch) && sequenceBatch.count==2
        && sequenceBatch.entries[0].generation==2 && sequenceBatch.entries[0].startTicks==3000
        && sequenceBatch.entries[1].generation==1 && sequenceBatch.entries[1].startTicks==2000);
    ++sequenceContext.owner.incarnation.value;
    CHECK(!sequences.update(hf::kRoundSequences,sequenceContext,3,Phase::toEncounter));

    return 0;
}
