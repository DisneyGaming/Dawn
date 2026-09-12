#pragma once
#include <algorithm>
#include "../../../../../state/activity/strike_bond/catalog.h"
#include "../../../../../state/build_data/scenarios/definition.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
namespace sunrise::server::bap::encrypted::push::activity::strike_bond_roster {
namespace native=state::activity::strike_bond;
namespace layouts=state::build_data::scenarios;
namespace wire=middleware::bap::activity_message::sensor_auth_update;

// The mission identities this activity actually publishes, used to validate the cached authored
// roster before it is handed to the client.
//
// A group only appears in the roster when it carries descriptor-backed slots; the SDK export lists
// every authored object in the scenario, including geometry containers that carry none and objects
// belonging to the campaign variant that shares this scenario. An earlier version of this table was
// taken from that export and required five objects the live `strike_bond` layout never publishes:
//
//   5938C7C6 spire_entry   0CEDD4AD forest_exit   3094B6DA forest_native
//   42B697DB tunnel        762AC3BA arena
//
// Admission is all-or-nothing, so one of those refused the whole roster and the run died at
// `roster_finalize no_groups` before anything else could start. They are not published because
// every capability the mission graph has on them is a type-47 navigation point or a type-60
// trigger volume: markers are published as targets inside the directive body, and trigger volumes
// are answered from the authored bounds in the mission's own kVolumes against the player's
// position. Neither needs a roster group, and 762AC3BA has no descriptor-backed slot at all.
// Dropping them does not weaken the check - the ten below are still all-or-nothing.
struct Group final {
    std::uint32_t key, tag;
    std::uint8_t bubble;
    bool topLevel;
    std::string_view name;
};
inline constexpr Group kGroups[]{
    // Roots. The first carries the type-13 player slots that name the roster's player key, so its
    // absence is what would otherwise surface much later as an unexplained playerKeyGroup refusal.
    {0xF29221F5U,0x80FD25E7U, 0,true, "activity players and globals"},
    {0xBC279389U,0x80F54AD1U, 0,true, "presentation: directive, music and dialogue sensors"},
    // Bubble 1, the Gardener's Spire interior and terrace.
    {0xC95ECB1AU,0x80F54563U, 1,false,"spire interior, terrace and cannon"},
    {0xE5ABAF3FU,0x80F54571U, 1,false,"spire dialogue and mancannon triggers"},
    // Bubble 10, the Infinite Forest entry.
    {0x2763EC90U,0x80F5460EU,10,false,"forest entry, fortresses and navpoints"},
    {0x45B69C3CU,0x80F54627U,10,false,"forest start trigger"},
    // Bubble 15, the Lighthouse.
    {0x40BFB1C0U,0x80F54659U,15,false,"lighthouse landing and anomaly"},
    {0x5F8099ABU,0x80F54666U,15,false,"lighthouse teleport and engagement sensor"},
    // Bubble 17, the boss arena.
    {0x2CB86C0FU,0x80F54A8EU,17,false,"boss platform, golems and mancannons"},
    {0xA1F8CD1CU,0x80F54AA4U,17,false,"boss machine"},
};

// Identity lives in the mission catalog; this table names the subset the activity publishes. The
// two cannot drift apart on a key, a tag or a bubble without failing to compile.
static_assert([]{
    for(const auto& row:kGroups) {
        bool found=false;
        for(const auto& known:native::kGroups) {
            if(known.registry!=row.key) { continue; }
            if(known.tag!=row.tag) { return false; }
            if(!row.topLevel && (known.bubbles&(std::uint64_t{1}<<row.bubble))==0) { return false; }
            found=true;break;
        }
        if(!found) { return false; }
    }
    return true;
}(),"a required roster group is not the identity the strike_bond catalog declares");

/** A bubble owns eight slice-set states, so its regions run 8*bubble to 8*bubble+7. */
inline constexpr int kRegionStateCount=8;

/** The published group is this exact authored object. */
[[nodiscard]] inline bool matches(const layouts::RosterGroup& actual,const Group& expected) noexcept {
    return actual.registryKey==expected.key && actual.objectTag==expected.tag
        && actual.slotCount>0 && actual.slotCount<=actual.slotTypes.size();
}

/**
 * One admission's outcome, for the log.
 *
 * Admission is all-or-nothing and has twelve separate refusal points, so a bare failure says only
 * that the stage failed, not which authored assumption was wrong. Everything below `lastMissing`
 * is diagnostic: it names the refusal and records what the layout actually contained, so the
 * authored table can be corrected from evidence rather than guessed at. None of it participates in
 * any decision.
 */
struct Report final {
    unsigned added{},present{},missing{},full{}; std::uint32_t lastMissing{};

    /** Which refusal fired. Empty means the walk completed. */
    std::string_view stage{};
    /** Stage-specific context: usually the authored group index or key the refusal was about. */
    std::uint32_t detail{},detail2{};
    /** How far the walk got before refusing. */
    std::size_t resolved{},roots{};
    /** What the layout declared, so a capacity refusal can be read against the wire's own bound. */
    std::size_t layoutRosterGroups{},layoutBubbleGroups{},layoutBubbles{},layoutAuthoredGroups{};

    /** The identity row that could not be admitted, and what was actually published for its key. */
    std::uint32_t rowKey{},rowExpectedTag{},rowActualTag{};
    std::uint64_t rowOwners{};
    std::uint8_t rowExpectedBubble{};
    bool rowExpectedRoot{},rowKeyFound{},rowTagMatched{},rowSlotsValid{},rowRoot{};

    /** Per-bubble slice-set state. A bubble with more than one state swaps its whole object
     * registry when the state changes, so a bubble sitting on the wrong state presents a
     * different set of authored objects entirely. */
    struct BubbleState final {
        std::uint8_t bubble{},state{},stateCount{},authored{}; std::uint32_t hash{};
    };
    std::array<BubbleState,24> bubbles{};
    std::size_t bubbleStates{};

    /** A bounded snapshot of the roster the layout actually resolved to. */
    static constexpr std::size_t kObservedCapacity=24;
    struct Observed final {
        std::uint32_t key{},tag{}; std::uint64_t owners{}; std::uint16_t slots{}; bool root{};
    };
    std::array<Observed,kObservedCapacity> observed{};
    std::size_t observedCount{};
};

/**
 * Publish the complete authored roster with stable key and bubble ordinals. Native streaming
 * creates and retires each bubble's objects. Replacing this list at a boundary loses the old
 * bubble's cleanup entries and changes the generation of unrelated live sources.
 */
template<class Storage,class FindGroup>
[[nodiscard]] bool admit(const layouts::Definition& layout,Storage& storage,wire::Roster& roster,int,
                        FindGroup find,Report& report) noexcept {
    report={};
    report.stage="layout";
    if(layout.nameLength>layout.name.size()
        || std::string_view(layout.name.data(),layout.nameLength)!="strike_bond"
        || layout.bubbleCount>layouts::kBubbleCapacity
        || layout.rosterGroupCount>layout.rosterGroups.size()
        || layout.bubbleGroupCount>layout.bubbleGroups.size()) { return false; }
    report.layoutRosterGroups=layout.rosterGroupCount;
    report.layoutBubbleGroups=layout.bubbleGroupCount;
    report.layoutBubbles=layout.bubbleCount;
    for(std::size_t bubble=0;bubble<layout.bubbleCount;++bubble) {
        report.layoutAuthoredGroups+=layout.authoredGroupCounts[bubble];
        if(layout.authoredGroupCounts[bubble] && report.bubbleStates<report.bubbles.size()) {
            report.bubbles[report.bubbleStates++]={static_cast<std::uint8_t>(bubble),
                layout.bubbleStates[bubble],layout.bubbleStateCounts[bubble],
                layout.authoredGroupCounts[bubble],layout.bubbleHashes[bubble]};
        }
    }
    std::array<std::uint16_t,wire::kGroupCapacity> indices{};
    std::array<std::uint64_t,wire::kGroupCapacity> owners{};
    std::size_t count{},roots{};
    const auto add=[&](std::uint16_t index,std::uint64_t mask,bool root) {
        for(std::size_t i=0;i<count;++i) {
            if(indices[i]!=index) { continue; }
            if(root!=(i<roots)) {
                report.stage="ownership_conflict";report.detail=index;
                report.detail2=static_cast<std::uint32_t>(i);report.resolved=count;report.roots=roots;
                return false;
            }
            owners[i]|=mask;return true;
        }
        if(count==indices.size() || count==storage.rosterGroups.size()) {
            ++report.full;
            report.stage="group_capacity";report.detail=index;
            report.detail2=static_cast<std::uint32_t>(indices.size());
            report.resolved=count;report.roots=roots;
            return false;
        }
        indices[count]=index;owners[count++]=mask;
        if(root) { roots=count; }
        return true;
    };
    for(std::size_t i=0;i<layout.rosterGroupCount;++i) {
        if(!add(layout.rosterGroups[i],0,true)) { report.detail2=static_cast<std::uint32_t>(i);
            report.lastMissing=1;return false; }
    }
    // The first authored row is the mission-wide presentation root, shared by its bubbles.
    for(std::size_t bubble=0;bubble<layout.bubbleCount;++bubble) {
        const auto n=layout.authoredGroupCounts[bubble];
        if(n>layout.authoredGroups[bubble].size()) {
            report.stage="authored_count";report.detail=static_cast<std::uint32_t>(bubble);
            report.detail2=n;return false;
        }
        if(n && !add(layout.authoredGroups[bubble][0],0,true)) {
            report.lastMissing=2;report.rowExpectedBubble=static_cast<std::uint8_t>(bubble);return false;
        }
    }
    for(std::size_t i=0;i<layout.bubbleGroupCount;++i) {
        if(!add(layout.bubbleGroups[i],layout.bubbleGroupMasks[i],false)) {
            report.lastMissing=3;return false;
        }
    }
    for(std::size_t bubble=0;bubble<layout.bubbleCount;++bubble) {
        for(std::size_t i=1;i<layout.authoredGroupCounts[bubble];++i) {
            if(!add(layout.authoredGroups[bubble][i],std::uint64_t{1}<<bubble,false)) {
                report.lastMissing=4;report.rowExpectedBubble=static_cast<std::uint8_t>(bubble);return false;
            }
        }
    }
    report.resolved=count;report.roots=roots;
    wire::Roster candidate{};
    for(std::size_t i=0;i<count;++i) {
        auto& group=storage.rosterGroups[i];
        if(!find(indices[i],group) || !group.slotCount || group.slotCount>group.slotTypes.size()) {
            ++report.missing;
            report.stage="unresolved_group";report.detail=indices[i];
            report.detail2=static_cast<std::uint32_t>(i);report.resolved=i;return false;
        }
        if(report.observedCount<report.observed.size()) {
            report.observed[report.observedCount++]={group.registryKey,group.objectTag,owners[i],
                group.slotCount,i<roots};
        }
        for(std::size_t j=0;j<i;++j) {
            if(candidate.groups[j].key==group.registryKey) {
                report.stage="duplicate_key";report.detail=group.registryKey;
                report.detail2=static_cast<std::uint32_t>(i);report.resolved=i;return false;
            }
        }
        candidate.groups[i]={group.registryKey,
            std::span(group.slotTypes).first(group.slotCount),
            std::span(group.slotFlags).first(group.slotCount),
            std::span(group.slotIndices).first(group.slotCount)};
        if(i<roots && !candidate.playerKeyGroup
            && std::find(candidate.groups[i].slotTypes.begin(),candidate.groups[i].slotTypes.end(),
                std::uint8_t{13})!=candidate.groups[i].slotTypes.end()) {
            candidate.playerKeyGroup=group.registryKey;
        }
    }
    // Validate the mission's known identities without removing the other authored objects.
    for(const auto& expected:kGroups) {
        bool found=false;
        for(std::size_t i=0;i<count;++i) {
            if(!matches(storage.rosterGroups[i],expected)) { continue; }
            found=expected.topLevel?i<roots:(owners[i]&(std::uint64_t{1}<<expected.bubble))!=0;
        }
        if(!found) {
            ++report.missing;report.lastMissing=expected.key;
            // Decompose the match so the log distinguishes "this key was never published" from
            // "it was published under a different tag" from "it is in the wrong bubble". Those
            // three want completely different corrections and the combined predicate hides which.
            report.stage="identity";
            report.rowKey=expected.key;report.rowExpectedTag=expected.tag;
            report.rowExpectedBubble=expected.bubble;report.rowExpectedRoot=expected.topLevel;
            for(std::size_t i=0;i<count;++i) {
                const auto& actual=storage.rosterGroups[i];
                if(actual.registryKey!=expected.key) { continue; }
                report.rowKeyFound=true;report.rowActualTag=actual.objectTag;
                report.rowTagMatched=actual.objectTag==expected.tag;
                report.rowSlotsValid=actual.slotCount>0 && actual.slotCount<=actual.slotTypes.size();
                report.rowOwners=owners[i];report.rowRoot=i<roots;
                break;
            }
            return false;
        }
        ++report.present;
    }
    std::size_t blocks{};
    for(std::size_t bubble=0;bubble<layouts::kBubbleCapacity;++bubble) {
        std::size_t keys{};
        for(std::size_t i=roots;i<count;++i) {
            if((owners[i]&(std::uint64_t{1}<<bubble))==0) { continue; }
            if(blocks>=storage.rosterSubBlockKeys.size()
                || keys>=storage.rosterSubBlockKeys[blocks].size()) {
                report.stage="subblock_capacity";report.detail=static_cast<std::uint32_t>(bubble);
                report.detail2=static_cast<std::uint32_t>(blocks);return false;
            }
            storage.rosterSubBlockKeys[blocks][keys++]=candidate.groups[i].key;
        }
        if(keys) {
            storage.rosterSubBlocks[blocks]={static_cast<std::uint32_t>(bubble),
                std::span(storage.rosterSubBlockKeys[blocks]).first(keys)};
            ++blocks;
        }
    }
    candidate.groupCount=count;candidate.topLevelGroupCount=roots;
    candidate.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(blocks);
    if(!candidate.playerKeyGroup) {
        // A root group carrying a type-13 slot is what names the player key; without one the
        // client has no team state to attach to.
        report.stage="player_key";report.detail=static_cast<std::uint32_t>(roots);return false;
    }
    report.stage={};report.added=static_cast<unsigned>(count);
    roster=candidate;return true;
}
}
