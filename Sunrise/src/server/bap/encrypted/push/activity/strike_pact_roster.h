#pragma once
#include <algorithm>
#include "../../../../../state/activity/strike_pact/catalog.h"
#include "../../../../../state/build_data/scenarios/definition.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
namespace sunrise::server::bap::encrypted::push::activity::strike_pact_roster {
namespace native=state::activity::strike_pact;
namespace layouts=state::build_data::scenarios;
namespace wire=middleware::bap::activity_message::sensor_auth_update;

// Known mission identities validate the cached authored roster. The complete roster below also
// retains authored groups which this mission does not issue commands to.
struct Group final {
    std::uint32_t key, tag;
    std::uint8_t bubble;
    bool topLevel;
    std::string_view name;
};
// Bubble ownership is the region each object is driven from in the proven mission policy: the
// Forest holds region 72 (bubble 9), the Chase 16 (bubble 2), the ledge and boss 0 (bubble 0), and
// the opening 120 (bubble 15). The presentation root is global, like every other mission's.
inline constexpr Group kGroups[]{
    {native::kRoot,       0x80F55229U, 0, true,  "presentation root"},
    {native::kOpening,    0x80F551DEU,15, false, "lighthouse opening"},
    {native::kTeleport,   0x80F551EBU,15, false, "lighthouse teleporter"},
    {native::kLighthouse, 0x80F55205U,15, false, "lighthouse area and tunnel"},
    {0x2763EC91U,         0x80F550B8U, 9, false, "infinite forest b"},
    {0xA9350228U,         0x80F550C9U, 9, false, "forest portal triggers"},
    {0x7E558786U,         0x80F550DAU, 9, false, "forest exit"},
    {0x588E5FB9U,         0x80F54FFEU, 2, false, "chase"},
    {0xFBD01A06U,         0x80F55018U, 2, false, "chase triggers"},
    {0x8E64DB66U,         0x80F55029U, 2, false, "chase entry"},
    {0xA5F083B5U,         0x80F54E07U, 0, false, "ledge, boss rooms and ending"},
    {0x73CBF939U,         0x80F54E12U, 0, false, "boss approach"},
    {0xC6F46FAFU,         0x80F54E1DU, 0, false, "boss room points"},
};
/** A bubble owns eight slice-set states, so its regions run 8*bubble to 8*bubble+7. */
inline constexpr int kRegionStateCount=8;

/** The published group is this exact authored object. */
[[nodiscard]] inline bool matches(const layouts::RosterGroup& actual,const Group& expected) noexcept {
    return actual.registryKey==expected.key && actual.objectTag==expected.tag
        && actual.slotCount>0 && actual.slotCount<=actual.slotTypes.size();
}

/** One admission's outcome, for the log. Nothing here refuses the destination's whole roster. */
struct Report final { unsigned added{},present{},missing{},full{}; std::uint32_t lastMissing{}; };

/**
 * Publish the complete authored roster with stable key and bubble ordinals. Native streaming
 * creates and retires each bubble's objects. Replacing this list at a boundary loses the old
 * bubble's cleanup entries and changes the generation of unrelated live sources.
 */
template<class Storage,class FindGroup>
[[nodiscard]] bool admit(const layouts::Definition& layout,Storage& storage,wire::Roster& roster,int,
                        FindGroup find,Report& report) noexcept {
    report={};
    if(layout.nameLength>layout.name.size()
        || std::string_view(layout.name.data(),layout.nameLength)!="strike_pact"
        || layout.bubbleCount>layouts::kBubbleCapacity
        || layout.rosterGroupCount>layout.rosterGroups.size()
        || layout.bubbleGroupCount>layout.bubbleGroups.size()) { return false; }
    std::array<std::uint16_t,wire::kGroupCapacity> indices{};
    std::array<std::uint64_t,wire::kGroupCapacity> owners{};
    std::size_t count{},roots{};
    const auto add=[&](std::uint16_t index,std::uint64_t mask,bool root) {
        for(std::size_t i=0;i<count;++i) {
            if(indices[i]!=index) { continue; }
            if(root!=(i<roots)) { return false; }
            owners[i]|=mask;return true;
        }
        if(count==indices.size() || count==storage.rosterGroups.size()) {
            ++report.full;return false;
        }
        indices[count]=index;owners[count++]=mask;
        if(root) { roots=count; }
        return true;
    };
    for(std::size_t i=0;i<layout.rosterGroupCount;++i) {
        if(!add(layout.rosterGroups[i],0,true)) { return false; }
    }
    // The first authored row is the mission-wide presentation root, shared by its bubbles.
    for(std::size_t bubble=0;bubble<layout.bubbleCount;++bubble) {
        const auto n=layout.authoredGroupCounts[bubble];
        if(n>layout.authoredGroups[bubble].size()) { return false; }
        if(n && !add(layout.authoredGroups[bubble][0],0,true)) { return false; }
    }
    for(std::size_t i=0;i<layout.bubbleGroupCount;++i) {
        if(!add(layout.bubbleGroups[i],layout.bubbleGroupMasks[i],false)) { return false; }
    }
    for(std::size_t bubble=0;bubble<layout.bubbleCount;++bubble) {
        for(std::size_t i=1;i<layout.authoredGroupCounts[bubble];++i) {
            if(!add(layout.authoredGroups[bubble][i],std::uint64_t{1}<<bubble,false)) { return false; }
        }
    }
    wire::Roster candidate{};
    for(std::size_t i=0;i<count;++i) {
        auto& group=storage.rosterGroups[i];
        if(!find(indices[i],group) || !group.slotCount || group.slotCount>group.slotTypes.size()) {
            ++report.missing;return false;
        }
        for(std::size_t j=0;j<i;++j) {
            if(candidate.groups[j].key==group.registryKey) { return false; }
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
        if(!found) { ++report.missing;report.lastMissing=expected.key;return false; }
        ++report.present;
    }
    std::size_t blocks{};
    for(std::size_t bubble=0;bubble<layouts::kBubbleCapacity;++bubble) {
        std::size_t keys{};
        for(std::size_t i=roots;i<count;++i) {
            if((owners[i]&(std::uint64_t{1}<<bubble))==0) { continue; }
            if(blocks>=storage.rosterSubBlockKeys.size()
                || keys>=storage.rosterSubBlockKeys[blocks].size()) { return false; }
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
    if(!candidate.playerKeyGroup) { return false; }
    roster=candidate;return true;
}
}
