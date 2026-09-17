#include "server/bap/encrypted/push/activity/eater_of_worlds_roster.h"
#include "server/bap/encrypted/activity_message/membership/activity_membership_route.h"
#include "state/activity/membership/transactions/internal.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace roster=dawn::server::bap::encrypted::push::activity::eater_of_worlds_roster;
namespace native=dawn::state::activity::eater_of_worlds;
namespace layouts=dawn::state::build_data::scenarios;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)

struct Storage {
    std::array<wire::BubbleSubBlock,64> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,wire::kGroupCapacity>,64> rosterSubBlockKeys{};
};
struct NoKeyStorage {
    std::array<wire::BubbleSubBlock,64> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,wire::kGroupCapacity>,0> rosterSubBlockKeys{};
};
struct NoBlockStorage {
    std::array<wire::BubbleSubBlock,0> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,wire::kGroupCapacity>,64> rosterSubBlockKeys{};
};

int main() {
    namespace route=dawn::server::bap::encrypted::activity_message::membership;
    namespace client=dawn::middleware::bap::activity_message::client_authoritative_data;
    // Live run1 after the reactor transition: the held/current leg remained region56 while the
    // outgoing second leg named entrance16. Dropping currentRegion rewound publication to16.
    client::ClientAuthoritativeData parsed{};
    parsed.hasCurrentRegion=true;parsed.currentRegion={56,0x45A97FF8U,true};
    parsed.hasRegion=true;parsed.region={16,0x8BA80878U,true};
    const auto update=route::make_authoritative(parsed,"raid_envy_v310");
    CHECK(route::retains_held_region("raid_envy_v310"));
    CHECK(update.hasCurrentRegion && update.currentRegion.index==56
        && update.currentRegion.hash==0x45A97FF8U);
    CHECK(update.hasRegion && update.region.index==16 && update.region.hash==0x8BA80878U);
    const auto merged=dawn::state::activity::membership::transactions::merge({},update);
    CHECK(merged.region.index==56 && merged.region.hash==0x45A97FF8U
        && merged.currentRegion.index==56);
    CHECK(!route::retains_held_region("raid_envy_v310_extra"));
    CHECK(!route::make_authoritative(parsed,"raid_envy_v310_extra").hasCurrentRegion);

    std::size_t descriptors{},publishedGroups{};
    for(std::size_t groupIndex=0;groupIndex<std::size(native::kGroups);++groupIndex) {
        const auto& source=native::kGroups[groupIndex];
        const auto& published=roster::kRosterGroups[groupIndex];
        std::size_t descriptorIndex{};
        for(const auto& slot:source.slots) {
            const auto expectedFlags=static_cast<std::uint8_t>(
                (slot.sense!=UINT32_MAX?layouts::kSlotSenseFlag:0U)
                |(slot.auth!=UINT32_MAX?layouts::kSlotAuthFlag:0U));
            CHECK(slot.flags==expectedFlags);
            if(slot.tag==UINT32_MAX) continue;
            CHECK(descriptorIndex<published.slotCount);
            CHECK(published.slotIndices[descriptorIndex]==slot.index);
            CHECK(published.slotTypes[descriptorIndex]==slot.type);
            CHECK(published.slotFlags[descriptorIndex]==slot.flags);
            CHECK(published.descriptorTags[descriptorIndex]==slot.tag);
            CHECK(published.descriptorOffsets[descriptorIndex]==slot.offset);
            CHECK(published.componentClasses[descriptorIndex]==slot.component);
            CHECK(published.senseSchemas[descriptorIndex]==slot.sense);
            CHECK(published.authSchemas[descriptorIndex]==slot.auth);
            ++descriptorIndex;++descriptors;
        }
        CHECK(descriptorIndex==published.slotCount);
        publishedGroups+=published.slotCount!=0?1U:0U;
    }
    CHECK(descriptors==891);
    CHECK(publishedGroups==17);

    layouts::Definition layout{};
    constexpr std::string_view name="raid_envy_v310";
    std::copy(name.begin(),name.end(),layout.name.begin());
    layout.nameLength=static_cast<std::uint8_t>(name.size());
    layout.tag=native::kScenario;layout.bubbleCount=8;
    Storage storage{};wire::Roster output{};
    CHECK(roster::admit(layout,storage,output));
    CHECK(output.groupCount==17);CHECK(output.topLevelGroupCount==2);
    CHECK(output.playerKeyGroup==0x24C67333U);
    CHECK(output.groups[0].key==0xD6E30062U);
    CHECK(output.groups[1].key==0x24C67333U);
    std::size_t publishedDescriptors{};
    for(std::size_t i=0;i<output.groupCount;++i) {
        const auto& group=output.groups[i];
        CHECK(group.key!=0x9FFBBD32U);CHECK(group.key!=0x96E0A5E5U);
        const layouts::RosterGroup* expected{};
        for(const auto& candidate:roster::kRosterGroups)
            if(candidate.slotCount && candidate.registryKey==group.key) {expected=&candidate;break;}
        CHECK(expected);CHECK(group.slotTypes.size()==expected->slotCount);
        CHECK(group.slotFlags.size()==expected->slotCount);
        CHECK(group.slotIndices.size()==expected->slotCount);
        for(std::size_t slot=0;slot<expected->slotCount;++slot) {
            CHECK(group.slotTypes[slot]==expected->slotTypes[slot]);
            CHECK(group.slotFlags[slot]==expected->slotFlags[slot]);
            CHECK(group.slotIndices[slot]==expected->slotIndices[slot]);
        }
        publishedDescriptors+=group.slotTypes.size();
    }
    CHECK(publishedDescriptors==891);
    for(std::uint8_t bubble=0;bubble<8;++bubble) {
        const wire::BubbleSubBlock* block{};
        for(const auto& candidate:output.bubbleSubBlocks)
            if(candidate.bubble==bubble) {CHECK(!block);block=&candidate;}
        std::size_t expectedCount{};
        for(std::size_t i=0;i<std::size(native::kGroups);++i) {
            const auto& group=native::kGroups[i];
            if(!roster::top_level(group) && roster::kRosterGroups[i].slotCount
                && (group.bubblesMask&(1U<<bubble))) {
                CHECK(block);CHECK(expectedCount<block->keys.size());
                CHECK(block->keys[expectedCount++]==group.key);
            }
        }
        CHECK((block?block->keys.size():0U)==expectedCount);
    }
    const auto retained=output;
    NoKeyStorage noKeys{};CHECK(!roster::admit(layout,noKeys,output));CHECK(output.groupCount==retained.groupCount);
    NoBlockStorage noBlocks{};CHECK(!roster::admit(layout,noBlocks,output));CHECK(output.groupCount==retained.groupCount);
    layout.tag=0;
    CHECK(!roster::admit(layout,storage,output));
    CHECK(output.groupCount==retained.groupCount);
    std::printf("PASS: %u checks; 891 descriptors, authored indices, two globals, empty-key collision exclusion and bubble masks\n",checks);
}
