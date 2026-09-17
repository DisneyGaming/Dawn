#pragma once
#include "../src/server/bap/encrypted/push/activity/hijacked_roster.h"
#include "fixtures/hijacked_full_roster.h"
#include <memory>
#include <cstdio>
#include <algorithm>
namespace hijacked_roster_lookup_tests {
namespace helper=dawn::server::bap::encrypted::push::activity::hijacked_roster;
namespace native=helper::native;
namespace layouts=helper::layouts;
namespace wire=helper::wire;
struct Storage {
    std::array<layouts::RosterGroup,wire::kGroupCapacity> rosterGroups{};
    std::array<wire::BubbleSubBlock,64> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,96>,64> rosterSubBlockKeys{};
};
struct Fixture {
    std::array<layouts::RosterGroup,std::size(native::kGroups)> rows{};
    std::array<std::uint16_t,std::size(native::kGroups)> indices{};
    hijacked_fixture::Startup packet;
    Storage storage;
    layouts::Definition layout{};
    unsigned copies{},keyQueries{};
    Fixture() {
        layout.tag=native::kScenario;layout.nameLength=15;layout.bubbleCount=45;
        std::copy(native::kPackage.begin(),native::kPackage.end(),layout.name.begin());
        for(std::size_t i=0;i<rows.size();++i) {
            const auto& expected=native::kGroups[i];auto& row=rows[i];
            indices[i]=static_cast<std::uint16_t>(1700+(rows.size()-1-i)*3);
            row.registryKey=expected.key;row.objectTag=expected.tag;
            row.slotCount=static_cast<std::uint16_t>(expected.slots.size());
            for(std::size_t j=0;j<expected.slots.size();++j) {
                const auto& slot=expected.slots[j];row.slotTypes[j]=slot.type;row.slotFlags[j]=slot.flags;
                row.slotIndices[j]=slot.index;row.descriptorTags[j]=slot.tag;row.descriptorOffsets[j]=slot.offset;
                row.componentClasses[j]=slot.component;row.senseSchemas[j]=slot.sense;row.authSchemas[j]=slot.auth;
            }
        }
    }
    bool index(std::uint32_t key,std::uint16_t& out) {
        ++keyQueries;
        for(std::size_t i=0;i<rows.size();++i) if(rows[i].registryKey==key) {out=indices[i];return true;}
        return false;
    }
    bool by_index(std::size_t index,layouts::RosterGroup& out) {
        ++copies;
        for(std::size_t i=0;i<rows.size();++i) if(indices[i]==index) {out=rows[i];return true;}
        return false;
    }
    bool by_key(std::uint32_t key,layouts::RosterGroup& out) {
        ++copies;
        for(const auto& row:rows) if(row.registryKey==key) {out=row;return true;}
        return false;
    }
    bool prepare() {
        return helper::prepare_layout(layout,[&](std::uint32_t key,std::uint16_t& out) {return index(key,out);},
            [&](std::size_t i,layouts::RosterGroup& out) {return by_index(i,out);});
    }
    bool admit(std::uint32_t& failed) {
        return helper::admit(layout,storage,packet.snapshot.roster,
            [&](std::uint32_t key,layouts::RosterGroup& out) {return by_key(key,out);},&failed);
    }
};
#define HRC_CHECK(value) do {if(!(value)) {std::fprintf(stderr,"FAIL Hijacked roster lookup line %d: %s\n",__LINE__,#value);return false;}} while(false)
inline bool run() {
    auto f=std::make_unique<Fixture>();std::uint32_t failed{};std::uint16_t rootIndex{};
    HRC_CHECK(f->index(native::kRoot,rootIndex));
    for(unsigned pass=0;pass<3;++pass) {
        f->copies=f->keyQueries=0;
        HRC_CHECK(f->prepare() && f->admit(failed) && failed==0);
        HRC_CHECK(f->copies==1+std::size(native::kGroups) && f->keyQueries==1);
        for(std::size_t bubble=0;bubble<f->layout.bubbleCount;++bubble) {
            HRC_CHECK(f->layout.authoredGroupCounts[bubble]==1);
            HRC_CHECK(f->layout.authoredGroups[bubble][0]==rootIndex);
        }
        // A later catalog may reorder every row. No remembered ordinal may be reused.
        for(auto& index:f->indices) index=static_cast<std::uint16_t>(index-100);
        rootIndex=static_cast<std::uint16_t>(rootIndex-100);
    }
    const auto key=f->rows[2].registryKey;
    f->rows[2].authSchemas[0]^=1;
    HRC_CHECK(!f->admit(failed) && failed==key);
    f->rows[2].authSchemas[0]^=1;f->rows[2].objectTag^=1;
    HRC_CHECK(!f->admit(failed) && failed==key);
    f->rows[2].objectTag^=1;f->rows[2].registryKey=0;
    HRC_CHECK(!f->admit(failed) && failed==key);
    f->rows[2].registryKey=key;
    // Validate the row again if the catalog changed between key resolution and copy.
    HRC_CHECK(!helper::prepare_layout(f->layout,
        [](std::uint32_t,std::uint16_t& index) {index=7;return true;},
        [&](std::size_t,layouts::RosterGroup& row) {row=f->rows[2];return true;}));
    HRC_CHECK(!helper::prepare_layout(f->layout,
        [](std::uint32_t,std::uint16_t& index) {index=UINT16_MAX;return true;},
        [&](std::size_t,layouts::RosterGroup&) {return false;}));
    // Exercise first admission as well as the already-present roster path.
    f=std::make_unique<Fixture>();f->packet.snapshot.roster.groupCount=f->packet.snapshot.roster.topLevelGroupCount;
    f->packet.snapshot.roster.bubbleSubBlocks={};
    HRC_CHECK(f->prepare() && f->admit(failed) && failed==0);
    HRC_CHECK(f->packet.snapshot.roster.groupCount==std::size(native::kGroups)+1);
    return true;
}
#undef HRC_CHECK
}
