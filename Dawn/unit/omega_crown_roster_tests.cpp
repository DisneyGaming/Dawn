#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "server/bap/encrypted/push/activity/omega_route_roster.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/bap/activity_message/sense_update.h"
#include "server/bap/encrypted/activity_message/omega_opening_ack.h"
#include "fixtures/omega_reveal_acknowledgement.h"
namespace route=dawn::server::bap::encrypted::push::activity::omega_route_roster;
namespace layouts=dawn::state::build_data::scenarios;
namespace wire=route::message;
namespace mission=dawn::state::activity::omega::mission;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};
void check(bool ok,const char* label) {++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
struct Scratch {
    std::array<layouts::RosterGroup,wire::kGroupCapacity> rosterGroups{};
    std::array<wire::BubbleSubBlock,3> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,wire::kGroupCapacity>,3> rosterSubBlockKeys{};
};
static Scratch scratch;
static std::array<layouts::RosterGroup,5> fixtures;
template<class T> T take(std::ifstream& file) {T value{};file.read(reinterpret_cast<char*>(&value),sizeof value);check(bool(file),"fixture field");return value;}
bool load(std::uint32_t key,layouts::RosterGroup& out) {
    for(const auto& group:fixtures) if(group.registryKey==key){out=group;return true;}
    out={};out.registryKey=key;out.slotCount=1;out.slotTypes[0]=47;out.slotFlags[0]=2;return true;
}
void base(wire::Roster& roster) {
    roster={};roster.groupCount=7;roster.topLevelGroupCount=3;roster.playerKeyGroup=route::base[0];
    for(std::size_t i=0;i<route::base.size();++i) {
        auto& group=scratch.rosterGroups[i];load(route::base[i],group);
        if(i==2) {group.slotCount=2;group.slotTypes[0]=68;group.slotTypes[1]=53;group.slotFlags[1]=2;group.slotIndices[1]=2;}
        roster.groups[i]={group.registryKey,std::span(group.slotTypes).first(group.slotCount),std::span(group.slotFlags).first(group.slotCount),std::span(group.slotIndices).first(group.slotCount)};
    }
}
std::uint64_t field(bits::Reader& reader,std::uint8_t width) {std::uint64_t value{};check(reader.read(width,value),"packet field");return value;}
void sense_capacity() {
    namespace sense=dawn::middleware::bap::activity_message::sense_update;
    static std::array<std::byte,65536> bytes{};
    static sense::SenseUpdate result;
    for(unsigned count:{400U,513U}) {
        bits::Writer writer(bytes);
        check(writer.write(0,64)&&writer.write(0,64)&&writer.write(0,1)&&writer.write(0,1),"sense epoch and absent roster");
        check(writer.write(1,1)&&writer.write(0x99BD2FEBU,32)&&writer.write(count*89U+1U,32),"sense group");
        for(unsigned i=0;i<count;++i)
            check(writer.write(1,1)&&writer.write(0x99BD2FEBU,32)&&writer.write(44,7)&&writer.write(0x8000U+i,16)
                &&writer.write(0,1)&&writer.write(i,32),"native unchanged scene sense");
        check(writer.write(0,1)&&writer.write(0,1)&&writer.write(0,1),"sense terminators");
        std::size_t consumed{},written{};check(writer.finish(written),"sense packet fits");
        const bool parsed=sense::parse_sense_update(std::span(bytes).first(written),result,consumed);
        check(parsed==(count==400),"full Crown sense fits; bounded overflow still rejected");
        if(parsed) check(result.objectCount==400 && result.objects[399].slotIndex==399,"last sense object retained");
    }
}
void opening_ack() {
    namespace sense=dawn::middleware::bap::activity_message::sense_update;
    namespace ack=dawn::server::bap::encrypted::activity_message::omega_ack;
    static sense::SenseUpdate result;
    std::size_t consumed{};
    check(sense::parse_sense_update(dawn::unit::fixtures::kOmegaRevealAcknowledgement,result,consumed),"captured opening parses");
    check(ack::exact_omega_initial_report(result),"legacy eleven-group opening preserved");
    const auto existing=result.rosterEntryCount;
    for(const auto& group:route::catalog::groups) {
        auto& entry=result.rosterEntries[result.rosterEntryCount++];
        entry.registryKey=group.key;entry.bubble=14;entry.active=true;entry.state=0x83;
    }
    check(ack::exact_omega_initial_report(result),"complete sixteen-group opening still releases Ikora and portal");
    for(unsigned i=existing;i<result.rosterEntryCount;++i) {
        auto& entry=result.rosterEntries[i];entry.bubble=15;
        check(!ack::exact_omega_initial_report(result),"wrong Crown area rejected");entry.bubble=14;
        entry.active=false;check(!ack::exact_omega_initial_report(result),"inactive Crown registry rejected");entry.active=true;
        const auto key=entry.registryKey;entry.registryKey=0xDEADBEEFU;
        check(!ack::exact_omega_initial_report(result),"unrelated replacement group rejected");entry.registryKey=key;
    }
    --result.rosterEntryCount;check(!ack::exact_omega_initial_report(result),"partial Crown set cannot start opening");
}
int main() {
    sense_capacity();opening_ack();
    std::ifstream file(std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_crown_roster.bin",std::ios::binary);
    check(bool(file),"open independently extracted cache fixture");
    for(auto& group:fixtures) {
        group.registryKey=take<std::uint32_t>(file);group.objectTag=take<std::uint32_t>(file);group.slotCount=take<std::uint16_t>(file);
        for(std::size_t i=0;i<group.slotCount;++i) {
            group.slotTypes[i]=take<std::uint8_t>(file);group.slotFlags[i]=take<std::uint8_t>(file);group.slotIndices[i]=take<std::uint16_t>(file);
            group.descriptorTags[i]=take<std::uint32_t>(file);group.descriptorOffsets[i]=take<std::uint32_t>(file);group.componentClasses[i]=take<std::uint32_t>(file);
            group.senseSchemas[i]=take<std::uint32_t>(file);group.authSchemas[i]=take<std::uint32_t>(file);
        }
        check(route::catalog::valid(group,group.registryKey),"complete native catalog accepted");
        auto bad=group;--bad.slotCount;check(!route::catalog::valid(bad,group.registryKey),"truncated descriptor list rejected");
        for(std::size_t i=0;i<group.slotCount;++i) {
            bad=group;bad.slotIndices[i]^=1;check(!route::catalog::valid(bad,group.registryKey),"wrong ordinal rejected");
            bad=group;bad.authSchemas[i]^=1;check(!route::catalog::valid(bad,group.registryKey),"wrong schema rejected");
            bad=group;bad.slotFlags[i]^=1;check(!route::catalog::valid(bad,group.registryKey),"wrong flags rejected");
        }
    }
    wire::Snapshot snapshot{};
    base(snapshot.roster);
    check(!route::append(scratch,snapshot.roster,[](auto key,auto& out){return key!=0x0040BF06U && load(key,out);}),"missing Crown registry fails instead of silent omission");
    base(snapshot.roster);check(route::append(scratch,snapshot.roster,load),"production appender admits all sixteen groups");
    check(snapshot.roster.groupCount==16 && snapshot.roster.topLevelGroupCount==3,"base and group counts preserved");
    check(snapshot.roster.bubbleSubBlocks[1].keys.size()==8,"all eight lair groups in bubble14");
    for(const auto& source:mission::kSources) {
        bool found=false;
        for(const auto& group:snapshot.roster.groups) if(group.key==source.registry) {
            // The unchanged F4 fixture is outside this catalog-specific test.
            if(source.registry==0xF4D0E0B2U){found=true;break;}
            for(std::size_t i=0;i<group.slotTypes.size();++i)
                found|=group.slotTypes[i]==1 && group.slotIndices[i]==source.slot;
        }
        check(found,"every configured encounter source is registered");
    }
    snapshot.lifetime=3;snapshot.omegaSceneAuthority=snapshot.omegaDialogueArm=true;
    snapshot.omegaWaypointRegistry=0xA3928C71U;snapshot.omegaWaypointIndex=13;
    snapshot.playerKey=0x1080123456789ABULL;
    static std::array<std::byte,256*1024> buffer{};
    std::size_t maximum{};
    for(unsigned wave=0;wave<15;++wave) for(unsigned mode=0;mode<3;++mode) {
        snapshot.preserveMissionAuthorityState=mode!=0;
        snapshot.omegaOpeningStage=mode==2?wire::kOmegaOpeningStageSettled:0;
        snapshot.omegaMission.generation=2;
        for(std::size_t i=0;i<mission::kSources.size();++i)
            snapshot.omegaMission.requested[i]=mission::kSources[i].wave<=wave?mission::kSources[i].requested:std::array<std::uint8_t,2>{};
        std::size_t written{};const bool encoded=wire::encode_sensor_auth_update(snapshot,buffer,written);
        if(!encoded) std::fprintf(stderr,"packet wave=%u mode=%u\n",wave,mode);
        check(encoded,"full roster and wave packet fits transport");
        maximum=std::max(maximum,written);
        bits::Reader reader(std::span(buffer).first(written));
        check(reader.skip(wire::kLatchBitWithoutGrant+1+wire::delta_bits(3,snapshot.roster.bubbleSubBlocks)),"skip phase1");
        std::array<unsigned,5> seen{};
        while(field(reader,1)) {
            const auto key=static_cast<std::uint32_t>(field(reader,32));check(field(reader,32)==0,"group zero field");
            while(field(reader,1)) {
                check(field(reader,32)==key,"object registry retained");
                check(reader.skip(7+16),"native type and ordinal");
                for(std::size_t i=0;i<fixtures.size();++i) if(key==fixtures[i].registryKey) ++seen[i];
                const auto length=field(reader,32);check(reader.skip(static_cast<std::size_t>(length)),"native object block length");
            }
        }
        check(field(reader,1)==0 && reader.remaining_bits()<8,"no truncated or trailing packet data");
        for(std::size_t i=0;i<fixtures.size();++i)
            check(seen[i]==fixtures[i].slotCount,"every Crown/rescue descriptor emitted after opening too");
    }
    std::printf("%u checks passed: complete Crown roster, corrupted descriptors, all 15 wave packets in 3 publication modes; max packet=%zu bytes\n",checks,maximum);
}
