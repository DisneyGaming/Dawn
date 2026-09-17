#pragma once
#include "state/activity/hijacked/native_catalog.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
namespace hijacked_fixture {
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace native=dawn::state::activity::hijacked;
// Installed scenario80B4206A: ordinary groups0/1403, bubble group1 with mask4000290A,
// then mission root1457 and all twelve native local groups. The inactive packet
// reproduces the2736-byte live packet immediately before the host timeout.
struct Startup final {
    wire::Snapshot snapshot{};
    std::array<std::array<std::uint8_t,256>,16> types{},flags{};
    std::array<std::array<std::uint16_t,256>,16> slots{};
    std::array<std::array<std::uint32_t,16>,64> keys{};
    std::array<std::size_t,64> counts{};
    std::array<wire::BubbleSubBlock,64> blocks{};
    struct Slot {std::uint8_t type,flags;std::uint16_t index;};
    template<class Range> void add(std::uint32_t key,const Range& source) {
        const auto index=snapshot.roster.groupCount++;std::size_t count{};
        for(const auto& slot:source) {
            types[index][count]=slot.type;flags[index][count]=slot.flags;slots[index][count++]=slot.index;
        }
        snapshot.roster.groups[index]={key,std::span(types[index]).first(count),
            std::span(flags[index]).first(count),std::span(slots[index]).first(count)};
    }
    Startup() {
        snapshot.lifetime=3;snapshot.patchEpoch={1,2};snapshot.hasRegion=true;snapshot.region=104;
        snapshot.playerKey=0x9EAA300100200002ULL;
        add(native::kGroups[0].key,native::kGroups[0].slots);
        add(0x9FFBBD32U,std::array<Slot,1>{{{19,2,0}}});
        add(native::kGroups[1].key,native::kGroups[1].slots);
        snapshot.roster.topLevelGroupCount=3;snapshot.roster.playerKeyGroup=0x4786C0E0U;
        std::array<Slot,16> participation{};
        for(std::uint16_t i=0;i<16;++i) {participation[i]={13,3,i};}
        add(0xEAAF16E2U,participation);
        for(std::size_t i=2;i<std::size(native::kGroups);++i) {add(native::kGroups[i].key,native::kGroups[i].slots);}
        for(std::size_t i=0;i<64;++i) {if(0x4000290AULL&(1ULL<<i)) {keys[i][counts[i]++]=0xEAAF16E2U;}}
        for(std::size_t i=2;i<std::size(native::kGroups);++i) {
            const auto& group=native::kGroups[i];keys[group.bubble][counts[group.bubble]++]=group.key;
        }
        std::size_t blockCount{};
        for(std::uint32_t i=0;i<64;++i) {if(counts[i]) {blocks[blockCount++]={i,std::span(keys[i]).first(counts[i])};}}
        snapshot.roster.bubbleSubBlocks=std::span(blocks).first(blockCount);
    }
    Startup(const Startup&)=delete;
    Startup& operator=(const Startup&)=delete;
};
}
