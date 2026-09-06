#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <sstream>
#include <vector>
#include "core/logging/log.h"
#include "middleware/encoding/bit_reader.h"
#include "client/hooks/bootflow/omega_lair_delivery.h"
#include "state/activity/omega/omega_lair_start.h"
#include "state/activity/omega/omega_lair_authority.h"
#include "state/activity/omega/omega_mission_authority.h"
#include "state/activity/omega/omega_mission_devices.h"
#include "client/hooks/bootflow/omega_mission_delivery.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"

namespace sunrise::core::log {
std::atomic<unsigned> firstWaveReceipts{};
void write(Channel channel, Level, std::string_view event) noexcept {
    if (channel == Channel::state
        && event.starts_with("ev=omega_lair_start stage=left_started "))
        firstWaveReceipts.fetch_add(1, std::memory_order_relaxed);
}
}

namespace {
namespace start=sunrise::state::activity::omega::lair_start;
namespace authority=sunrise::state::activity::omega::lair_authority;
namespace wire=sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{},failures{};
void check(bool value,const char* text) { ++checks; if (!value) { ++failures; std::cerr<<text<<'\n'; } }
void native_delivery(const std::filesystem::path& capture) {
    namespace delivery=sunrise::client::hooks::bootflow::omega_lair_delivery;
    const auto load=[&](const std::string& name,std::size_t size) {
        std::ifstream stream(capture/name,std::ios::binary);
        std::vector<std::byte> result(size);
        stream.read(reinterpret_cast<char*>(result.data()),size);
        check(stream.gcount()==static_cast<std::streamsize>(size),"live capture available");
        return result;
    };
    for (const auto& source:authority::kSources) {
        std::ostringstream name; name<<"component-13100-"<<std::uppercase<<std::hex<<source.definition<<".bin";
        auto component=load(name.str(),0x700);
        auto object=load("source-"+std::to_string(source.slot)+"-object.bin",0x70);
        auto body=load("source-"+std::to_string(source.slot)+"-auth.bin",0xC4);
        check(delivery::source(component)==&source,"captured source identity and authored slot agree");
        check(delivery::pending(source,2,true,object,body),"captured pending first-wave body admitted");
        check(!delivery::adopted(component,body),"regression: decoded live request never reached component");
        check(!delivery::pending(source,2,false,object,body),"no delivery before arm summon");
        check(!delivery::pending(source,3,true,object,body),"stale generation rejected");
        for (const std::size_t offset:{0U,4U,6U,0xCU,0x18U,0x68U,0x6EU}) {
            auto wrong=object; wrong[offset]^=std::byte{1};
            check(!delivery::pending(source,2,true,wrong,body),"foreign or unready sync object rejected");
        }
        for (const std::size_t offset:{0U,4U,6U,0x2CU,0x30U,0x7CU,0xA0U,0xA4U,0xA6U,0xB4U,0xBCU,0xBDU}) {
            auto wrong=body; wrong[offset]^=std::byte{1};
            check(!delivery::pending(source,2,true,object,wrong),"foreign count, placement or generation rejected");
        }
        std::memcpy(component.data()+0x180,body.data(),body.size());
        check(delivery::adopted(component,body),"native copy completion prevents duplicate delivery");
        component[0]^=std::byte{1};
        check(!delivery::source(component),"unrelated component cannot receive request");
        check(!delivery::pending(source,2,true,object,std::span<const std::byte>(body).first(0xC3)),"short body rejected");
    }
}
void ownership() {
    start::State state;
    check(!state.prepare(1,0).leftStarted,"dormant initial state");
    check(!state.left_started({1,2,0x11,0x22,0x33}),"unprepared member cannot start fight");
    check(state.prepare(1,2).generation==2,"doorway prepares generation");
    check(!state.left_started({2,2,0x11,0x22,0x33}),"foreign run rejected");
    check(!state.left_started({1,3,0x11,0x22,0x33}),"foreign generation rejected");
    check(!state.left_started({1,2,0x11,0x22,0x22}),"aliased character and biped rejected");
    check(!state.left_started({1,2,0xFFFFFFFF,0x22,0x33}),"invalid actor rejected");
    check(state.left_started({1,2,0x11,0x22,0x33}),"owned native left starts fight");
    check(!state.left_started({1,2,0x11,0x22,0x33}),"duplicate native receipt cannot repeat batch");
    check(state.prepare(1,2).leftStarted,"keepalive retains cumulative request");
    check(!state.prepare(1,3).leftStarted,"changed generation fails closed");
    check(!state.prepare(2,3).leftStarted,"new mission resets batch");
    check(!state.left_started({1,2,0x11,0x22,0x33}),"old callback cannot reset new run");
}
void source_bodies(const std::filesystem::path& fixtures) {
    for (const auto& source:authority::kSources) for (unsigned mode=0;mode<3;++mode) {
        const auto generation=mode==0 ? 0U : 17U;
        const bool active=mode==2;
        wire::Snapshot snapshot{};
        snapshot.omegaLairAuthority=true;
        snapshot.omegaLairGeneration=generation;
        snapshot.omegaLairLeftStarted=active;
        std::array<std::byte,81> output{};
        bits::Writer writer(output);
        check(wire::write_auth_body(writer,snapshot,authority::kRegistry,1,source.slot,false),"production body writes");
        check(writer.bit_count()==641,"production body width641");
        const auto name="lair-source-"+std::to_string(source.slot)+"-"+std::to_string(generation)+"-"+std::to_string(active ? 1 : 0)+".bin";
        std::ifstream file(fixtures/name,std::ios::binary);
        std::array<std::byte,81> expected{};
        file.read(reinterpret_cast<char*>(expected.data()),expected.size());
        check(file.gcount()==81 && output==expected,"production bytes match independent native reflection fixture");
        std::array<std::byte,80> shortBytes{};
        bits::Writer shortWriter(shortBytes);
        check(!wire::write_auth_body(shortWriter,snapshot,authority::kRegistry,1,source.slot,false),"truncated packet rejected");
    }
    wire::Snapshot snapshot{};
    snapshot.omegaLairAuthority=true;
    for (std::uint16_t slot=0;slot<130;++slot) {
        check(wire::auth_body_bits(snapshot,authority::kRegistry,1,slot,false)==(slot>=3 && slot<=8 ? 641U : 0U),"only initial six sources gain authority");
        check(wire::auth_body_bits(snapshot,authority::kRegistry,2,slot,false)==0,"no proxy member admitted at startup");
        check(wire::auth_body_bits(snapshot,0x0040BF06,1,slot,false)==0,"Crown excluded from startup");
    }
    for (const auto generation:{0U,0x80000000U,0xFFFFFFFFU}) {
        std::array<std::byte,81> bytes{};
        bits::Writer writer(bytes);
        check(!authority::write_source(writer,authority::kSources[0],generation,true) && writer.bit_count()==0,"invalid activation rejected before write");
    }
}
void mission_packets() {
    namespace mission=sunrise::state::activity::omega::mission;
    namespace auth=sunrise::state::activity::omega::mission_authority;
    namespace devices=sunrise::state::activity::omega::mission_devices;
    for(const auto key:{0xF4D0E0B2U,0x0040BF06U,0x0040BF05U,0x0040BF04U,0x0040BF03U,devices::kRegistry})
    for(const bool active:{false,true}) {
        wire::Snapshot snapshot{}; snapshot.lifetime=3; snapshot.preserveMissionAuthorityState=true;
        snapshot.omegaMission.generation=17; snapshot.omegaMission.cannons=active?15:0;
        std::vector<std::uint8_t> types,flags;std::vector<std::uint16_t> slots;
        for(const auto& source:mission::kSources) if(source.registry==key) {
            types.push_back(1);slots.push_back(source.slot);flags.push_back(3);
            if(active) snapshot.omegaMission.requested[mission::source_index(key,source.slot)]=source.requested;
            if(source.member) {types.push_back(2);slots.push_back(source.member);flags.push_back(3);}
        }
        if(key==devices::kRegistry) for(const auto& cannon:devices::kCannons)
            for(const auto slot:{cannon.core,cannon.fx,cannon.gate}) {
                types.push_back(slot==cannon.gate?23:4);slots.push_back(slot);flags.push_back(3);
            }
        snapshot.roster.groupCount=snapshot.roster.topLevelGroupCount=1;
        snapshot.roster.groups[0]={key,types,flags,slots};
        std::array<std::byte,8192> packet{};std::size_t written{};
        check(wire::encode_sensor_auth_update(snapshot,packet,written),"full mission registry packet encodes");
        bits::Reader reader(std::span(packet).first(written));
        const auto get=[&](std::uint8_t width) {std::uint64_t value{};check(reader.read(width,value),"mission packet field");return value;};
        check(reader.skip(wire::kLatchBitWithoutGrant+1+wire::delta_bits(1,{})),"mission delta boundary");
        check(get(1)==1 && get(32)==key && get(32)==0,"each mission registry survives settled publication");
        for(std::size_t i=0;i<slots.size();++i) {
            const auto source=auth::find(key,types[i],slots[i]);
            const auto width=source?auth::bits(*source):types[i]==2?42U:types[i]==4?252U:147U;
            check(get(1)==1 && get(32)==key && get(7)==types[i]+1U && get(16)==0x8000U+slots[i],"native mission object identity");
            check(get(32)==width+3,"complete schema remainder includes sense tail");
            check(get(1)==1 && get(1)==1,"mission authority present");
            std::array<std::byte,128> expected{};bits::Writer writer(expected);
            check(wire::write_auth_body(writer,snapshot,key,types[i],slots[i],false) && writer.bit_count()==width,"mission schema width");
            bits::Reader body(expected);
            for(std::size_t bit=0;bit<width;++bit) {std::uint64_t value{};check(body.read(1,value)&&get(1)==value,"complete packet retains mission body");}
            check(get(1)==0,"mission absent sense tail");
            if(source) {
                bits::Reader decoded(expected);check(decoded.skip(117),"source category offset");
                std::uint64_t categories{};check(decoded.read(4,categories)&&categories==source->categories,"native category count");
                for(unsigned c=0;c<source->categories;++c) {
                    std::uint64_t count{};check(decoded.read(32,count),"source count readable");
                    check(count==0x80000000ULL+(active&&!source->member?source->requested[c]:0),"proxy activation cannot duplicate a loose actor");
                }
            }
        }
        check(get(1)==0&&get(1)==0&&get(1)==0,"mission packet complete without duplicate groups");
    }
}
void complete_packets(const std::filesystem::path& fixtures) {
    // Match the settled mission's restricted publication path. The original
    // body tests cannot detect a source dropped by the enclosing roster writer.
    const std::array<std::uint8_t,6> types{1,1,1,1,1,1};
    const std::array<std::uint16_t,6> slots{3,4,5,6,7,8};
    for (const std::uint8_t flagsValue : {std::uint8_t{0},std::uint8_t{3}})
    for (const bool active : {false,true}) {
        std::array<std::uint8_t,6> flags{}; flags.fill(flagsValue);
        wire::Snapshot snapshot{}; snapshot.lifetime=3;
        snapshot.preserveMissionAuthorityState=true;
        snapshot.omegaLairAuthority=true; snapshot.omegaLairGeneration=17;
        snapshot.omegaLairLeftStarted=active;
        snapshot.roster.groupCount=snapshot.roster.topLevelGroupCount=1;
        snapshot.roster.groups[0]={authority::kRegistry,types,flags,slots};
        std::array<std::byte,2048> packet{}; std::size_t written{};
        check(wire::encode_sensor_auth_update(snapshot,packet,written),"complete settled Lair packet encodes");
        bits::Reader reader(std::span(packet).first(written));
        const auto get=[&](std::uint8_t width) {std::uint64_t value{}; check(reader.read(width,value),"packet field available");return value;};
        check(reader.skip(wire::kLatchBitWithoutGrant+1+wire::delta_bits(1,{})),"full packet phase2 boundary");
        check(get(1)==1 && get(32)==authority::kRegistry && get(32)==0,"Lair group survives restricted publication");
        for (const auto slot:slots) {
            check(get(1)==1 && get(32)==authority::kRegistry && get(7)==2 && get(16)==0x8000U+slot,"exact ordered source record");
            check(get(32)==643U+static_cast<unsigned>((flagsValue&wire::kSlotSenseFlag)!=0),"source remainder includes native descriptor tail");
            check(get(1)==1 && get(1)==1,"source reset and authority present");
            const auto name="lair-source-"+std::to_string(slot)+"-17-"+std::to_string(active?1:0)+".bin";
            std::array<std::byte,81> expected{};
            std::ifstream file(fixtures/name,std::ios::binary);file.read(reinterpret_cast<char*>(expected.data()),expected.size());
            check(file.gcount()==81,"native fixture loaded");
            bits::Reader native(expected);
            for(unsigned bit=0;bit<641;++bit) {std::uint64_t value{};check(native.read(1,value) && get(1)==value,"full packet retains native source body");}
            if (flagsValue&wire::kSlotSenseFlag) check(get(1)==0,"absent sense tail");
        }
        check(get(1)==0 && get(1)==0 && get(1)==0,"exact six sources and complete packet terminators");
    }
}
void real_receipt_publication(const std::filesystem::path& fixtures) {
    // Link the production mutex-backed latch rather than the runtime fixture's
    // receipt counter. The runtime test separately proves that only native left
    // playback calls this API; this test checks its retained authority output.
    constexpr std::uint64_t run = 0xA988;
    constexpr std::uint32_t generation = 17;
    constexpr std::uint32_t actor = 0x74F42006, character = 0x5AF9EA20, biped = 0x1BF9EA1F;
    auto& receipts = sunrise::core::log::firstWaveReceipts;
    const auto before = receipts.load();
    auto publish = [&](std::uint64_t currentRun, bool active) {
        const auto retained = start::prepare(currentRun, generation);
        check(retained.generation == generation && retained.leftStarted == active,
              "real receipt projects the prepared generation and activation");
        wire::Snapshot snapshot{};
        snapshot.omegaLairAuthority = true;
        snapshot.omegaLairGeneration = retained.generation;
        snapshot.omegaLairLeftStarted = retained.leftStarted;
        for (const auto& source : authority::kSources) {
            std::array<std::byte, 81> output{}, expected{};
            bits::Writer writer(output);
            check(wire::write_auth_body(writer, snapshot, authority::kRegistry, 1, source.slot, false)
                      && writer.bit_count() == 641,
                  "real receipt reaches the exact native source body");
            const auto name = "lair-source-" + std::to_string(source.slot) + "-17-"
                + std::to_string(active ? 1 : 0) + ".bin";
            std::ifstream file(fixtures / name, std::ios::binary);
            file.read(reinterpret_cast<char*>(expected.data()), expected.size());
            check(file.gcount() == 81 && output == expected,
                  "real latch emits independent native fixture for each of six sources");
        }
    };
    publish(run, false);
    start::note_left_started(run + 1, generation, actor, character, biped);
    start::note_left_started(run, generation + 1, actor, character, biped);
    start::note_left_started(run, generation, actor, character, character);
    check(receipts.load() == before, "invalid real callbacks do not emit a wave receipt");
    publish(run, false);
    std::array<std::thread, 8> callbacks;
    for (auto& callback : callbacks)
        callback = std::thread([=] { start::note_left_started(run, generation, actor, character, biped); });
    for (auto& callback : callbacks) callback.join();
    check(receipts.load() == before + 1, "concurrent qualified callbacks accept exactly one first wave");
    publish(run, true);
    publish(run, true);
    start::note_left_started(run, generation, actor, character, biped);
    check(receipts.load() == before + 1, "duplicate callback keeps the cumulative request unchanged");
    publish(run + 1, false);
    start::note_left_started(run, generation, actor, character, biped);
    publish(run + 1, false);
    check(receipts.load() == before + 1, "stale real callback cannot reactivate a new mission");
}
}
int main(int argc,char** argv) {
    mission_packets();
    if(argc!=3) { std::cerr<<"Provide native fixture and live capture directories\n"; return 2; }
    native_delivery(argv[2]); ownership(); source_bodies(argv[1]); complete_packets(argv[1]); real_receipt_publication(argv[1]);
    std::cout<<(failures ? "FAIL: " : "PASS: ")<<checks<<" Lair startup ownership/source checks; "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
