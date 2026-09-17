#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include <cstdio>
#include <array>
#include <limits>
namespace auth=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace native=dawn::middleware::bap::activity_message::native;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{},failures{};
#define CHECK(x) do {++checks;if(!(x)){++failures;std::printf("FAIL %d: %s\n",__LINE__,#x);}}while(false)
void check_world_devices() {
    // These fixtures passed the original 4BEE90 decoder and 10699C0 staging
    // routines offline. Keep the goldens independent of the production codec.
    struct Fixture final {native::world_device::State state;const char* hex;};
    const std::array<Fixture,3> fixtures{{
        {{{.2F,1,false},{1,0,false},{0,0,false}},"3e4ccccd80011fc00000400000000000200000"},
        {{{.1F,2,false},{1,0,false},{0,0,false}},"3dcccccd80021fc00000400000000000200000"},
        {{{.3F,32767,true},{.7F,-32768,false},{.8F,-1,true}},"3e99999affff9f99999980000fd333335fffe0"}
    }};
    const auto nibble=[](char value){return static_cast<unsigned>(value<='9'?value-'0':value-'a'+10);};
    for(const auto& fixture:fixtures)for(const bool archive:{false,true}) {
        auth::Snapshot s{};s.archiveOmega=archive;s.devices.count=1;
        s.devices.entries[0]={0x34D23982,33,13,fixture.state};
        for(const bool legacy:{false,true}) {
            std::array<std::byte,19> actual{};bits::Writer writer(actual);
            const auto count=legacy?auth::legacy_auth_body_bits(s,0x34D23982,23,33,false):auth::auth_body_bits(s,0x34D23982,23,33,false);
            CHECK(count==147);
            CHECK(legacy?auth::legacy_write_auth_body(writer,s,0x34D23982,23,33,false):auth::write_auth_body(writer,s,0x34D23982,23,33,false));
            CHECK(writer.bit_count()==147);
            for(std::size_t i=0;i<actual.size();++i)
                CHECK(std::to_integer<unsigned>(actual[i])==(nibble(fixture.hex[i*2])<<4|nibble(fixture.hex[i*2+1])));
        }
    }

    // Complete packet admission checks the registry/slot's native owner scope;
    // activity owner/incarnation/boot checks live in world_device_service_tests.
    for(const bool archive:{false,true}) {
        std::array<std::uint8_t,1> types{23},flags{3},presence{1};
        std::array<std::uint16_t,1> slots{33};
        std::array<std::uint32_t,1> keys{0x34D23982};
        auth::BubbleSubBlock local{13,keys,presence};
        auth::Snapshot base{};base.archiveOmega=archive;base.region=104;base.lifetime=3;
        base.devices.count=1;base.devices.entries[0]={0x34D23982,33,13,fixtures[0].state};
        base.roster.groups[0]={0x34D23982,types,flags,slots};base.roster.groupCount=1;
        base.roster.bubbleSubBlocks={&local,1};
        std::array<std::byte,4096> encoded{};std::size_t bytes{};
        CHECK(auth::encode_sensor_auth_update(base,encoded,bytes) && bytes>0);
        auto empty=base;empty.devices={};
        CHECK(auth::encode_sensor_auth_update(empty,encoded,bytes) && bytes>0);
        for(unsigned field=0;field<19;++field) {
            auto bad=base;
            switch(field) {
            case 0:bad.region=96;break;
            case 1:bad.devices.entries[0].bubble=12;break;
            case 2:bad.devices.entries[0].registry=0x34D23983;break;
            case 3:bad.devices.entries[0].slot=32;break;
            case 4:types[0]=4;break;
            case 5:flags[0]=1;break;
            case 6:presence[0]=0;break;
            case 7:local.bubble=12;break;
            case 8:keys[0]=0x34D23983;break;
            case 9:bad.roster.bubbleSubBlocks={};break;
            case 10:bad.roster.groupCount=0;break;
            case 11:bad.roster.groups[1]=bad.roster.groups[0];bad.roster.groupCount=2;break;
            case 12:bad.devices.entries[1]=bad.devices.entries[0];bad.devices.count=2;break;
            case 13:bad.devices.count=native::world_device::kCapacity+1;break;
            case 14:bad.devices.entries[0].registry=0;break;
            case 15:bad.devices.entries[0].registry=UINT32_MAX;break;
            case 16:bad.devices.entries[0].registry=0x811C9DC5;break;
            case 17:bad.devices.entries[0].slot=32768;break;
            case 18:bad.devices.entries[0].bubble=64;break;
            }
            encoded.fill(std::byte{0xA5});const auto before=encoded;bytes=77;
            CHECK(!auth::encode_sensor_auth_update(bad,encoded,bytes) && bytes==0);
            CHECK(encoded==before);
            types[0]=23;flags[0]=3;presence[0]=1;local.bubble=13;keys[0]=0x34D23982;
        }
        for(const auto invalid:{-0.01F,1.01F,std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
            for(unsigned channel=0;channel<3;++channel) {
                auto bad=base;auto& state=bad.devices.entries[0].state;
                if(channel==0)state.position.value=invalid;
                if(channel==1)state.power.value=invalid;
                if(channel==2)state.lock.value=invalid;
                encoded.fill(std::byte{0xA5});const auto before=encoded;bytes=77;
                CHECK(!auth::encode_sensor_auth_update(bad,encoded,bytes) && bytes==0);
                CHECK(encoded==before);
                for(const bool legacy:{false,true}) {
                    auto writer=bits::Writer::measuring();
                    const auto count=legacy?auth::legacy_auth_body_bits(bad,0x34D23982,23,33,false):auth::auth_body_bits(bad,0x34D23982,23,33,false);
                    CHECK(count==0);
                    CHECK(!(legacy?auth::legacy_write_auth_body(writer,bad,0x34D23982,23,33,false):auth::write_auth_body(writer,bad,0x34D23982,23,33,false)));
                    CHECK(writer.bit_count()==0);
                }
            }
        }
    }
}
void check_mission_and_native_clock_selection() {
    for(const bool grant:{false,true}) {
        auth::Snapshot snapshot{};snapshot.lifetime=3;snapshot.phaseOneOnly=true;
        snapshot.hasGrant=grant;snapshot.grant={13,1};
        snapshot.gameplayClockTicks=0x0102030405060708ULL;
        std::array<std::byte,4096> encoded{};std::size_t size{};
        const auto start=auth::kLatchBitWithoutGrant+(grant?auth::kBubbleBlockBits:0)-64;
        const auto expectTicks=[&](std::uint64_t expected) {
            CHECK(auth::encode_sensor_auth_update(snapshot,encoded,size));
            bits::Reader reader(std::span(encoded).first(size));
            CHECK(reader.skip(start));std::uint64_t ticks{},byte{};
            for(unsigned shift=0;shift<64;shift+=8) {
                CHECK(reader.read(8,byte));ticks|=byte<<shift;
            }
            CHECK(ticks==expected);
        };
        // Authored missions keep their gameplay timestamp without a native service.
        expectTicks(snapshot.gameplayClockTicks);
        snapshot.activityClock=native::activity_clock::Configuration{false,1000.0F/30.0F};
        // An explicitly selected native clock owns the field even at time zero.
        expectTicks(0);
        snapshot.activityElapsedTicks=5385600;expectTicks(5385600);
        snapshot.activityClock.reset();snapshot.activityElapsedTicks=0;
        expectTicks(snapshot.gameplayClockTicks);
    }
}
int main() {
    check_mission_and_native_clock_selection();
    check_world_devices();
    for(const bool archive:{false,true})for(const bool grant:{false,true}) {
        auth::Snapshot s{};s.archiveOmega=archive;s.hasGrant=grant;s.grant={13,1};
        s.lifetime=3;s.phaseOneOnly=true;
        std::array<std::byte,4096> legacy{},configured{},updated{};std::size_t l{},c{},u{};
        CHECK(auth::encode_sensor_auth_update(s,legacy,l));
        s.activityClock=native::activity_clock::Configuration{false,1000.0F/30.0F};
        CHECK(auth::encode_sensor_auth_update(s,configured,c));
        CHECK(c==l && configured==legacy); // The separate type2 frame owns config.
        s.activityElapsedTicks=5385600;
        CHECK(auth::encode_sensor_auth_update(s,updated,u) && u==l);
        const auto start=auth::kLatchBitWithoutGrant+(grant?auth::kBubbleBlockBits:0)-64;
        bits::Reader reader(std::span(updated).first(u));std::uint64_t value{};
        for(std::size_t i=0;i<start;++i)CHECK(reader.read(1,value));
        // Native351070 copies these bytes directly into a little-endian u64.
        // A numeric read(64) would mirror the old encoder's endian mistake.
        std::uint64_t nativeTicks{};
        for(unsigned shift=0;shift<64;shift+=8) {CHECK(reader.read(8,value));nativeTicks|=value<<shift;}
        CHECK(nativeTicks==5385600);CHECK(reader.read(1,value) && value==1);
        // Only the recovered clock field may differ from the legacy packet.
        bits::Reader a(std::span(legacy).first(l)),b(std::span(updated).first(u));
        for(std::size_t i=0;i<l*8;++i){std::uint64_t av{},bv{};CHECK(a.read(1,av)&&b.read(1,bv));
            if(i<start || i>=start+64)CHECK(av==bv);}
        s.activityClock.reset();u=77;CHECK(!auth::encode_sensor_auth_update(s,updated,u) && u==0);
        s.activityClock=native::activity_clock::Configuration{false,std::numeric_limits<float>::quiet_NaN()};
        CHECK(!auth::encode_sensor_auth_update(s,updated,u));
    }
    // Both production body dispatchers preserve simultaneous typed overrides.
    const native::capture_controller::State captured{true,{true,0,5385600,0,5385600,123456,1.0F},false};
    for(const auto interaction:{native::interaction::Mode::unchanged,native::interaction::Mode::enabled}) {
        native::placement::Request r{0x34D23982,32,13,interaction,1,captured};
        std::array<std::byte,128> expected{};bits::Writer direct(expected);
        CHECK(native::placement::write(direct,r));
        CHECK(direct.bit_count()==640+(interaction==native::interaction::Mode::unchanged?0:123));
        auth::Snapshot s{};s.placements.count=1;s.placements.entries[0]=r;
        for(const bool legacy:{false,true}) {
            std::array<std::byte,128> actual{};bits::Writer writer(actual);
            const auto n=legacy?auth::legacy_auth_body_bits(s,r.registry,4,r.slot,false):auth::auth_body_bits(s,r.registry,4,r.slot,false);
            CHECK(n==direct.bit_count());
            CHECK(legacy?auth::legacy_write_auth_body(writer,s,r.registry,4,r.slot,false):auth::write_auth_body(writer,s,r.registry,4,r.slot,false));
            CHECK(actual==expected && writer.bit_count()==n);
        }
        r.generation=0;auto invalid=bits::Writer::measuring();
        CHECK(!native::placement::write(invalid,r) && invalid.bit_count()==0);
    }
    // The generator's reflected lists are count-driven, including in both
    // production dispatchers and their full packet admission paths.
    for(const auto count:{0U,1U,100U}) {
        native::forest_generator::Request r{0x34D23982,98,13,{}};
        r.state.primary.overrides=native::forest_generator::Enabled|native::forest_generator::Seed;
        r.state.primary.enabled=true;r.state.primary.seed=0xFEDCBA98;
        r.state.primary.blockedCount=static_cast<std::uint8_t>(count);
        r.state.secondary.blockedCount=static_cast<std::uint8_t>(count);
        std::array<std::byte,2048> expected{};bits::Writer direct(expected);
        CHECK(native::forest_generator::write_payload(direct,r.state));
        CHECK(direct.bit_count()==1750+96*count);
        auth::Snapshot s{};s.generators.count=1;s.generators.entries[0]=r;
        for(const bool legacy:{false,true}) {
            std::array<std::byte,2048> actual{};bits::Writer writer(actual);
            const auto n=legacy?auth::legacy_auth_body_bits(s,r.registry,37,r.slot,false):auth::auth_body_bits(s,r.registry,37,r.slot,false);
            CHECK(n==direct.bit_count());
            CHECK(legacy?auth::legacy_write_auth_body(writer,s,r.registry,37,r.slot,false):auth::write_auth_body(writer,s,r.registry,37,r.slot,false));
            CHECK(actual==expected && writer.bit_count()==n);
        }
        const std::array<std::uint8_t,1> types{37},flags{3},presence{1};
        const std::array<std::uint16_t,1> slots{98};
        const std::array<std::uint32_t,1> keys{r.registry};
        const auth::BubbleSubBlock local{13,keys,presence};
        s.roster.groups[0]={r.registry,types,flags,slots};s.roster.groupCount=1;
        s.roster.bubbleSubBlocks={&local,1};s.region=104;s.lifetime=3;
        for(const bool archive:{false,true}) {
            s.archiveOmega=archive;std::array<std::byte,4096> encoded{};std::size_t bytes{};
            CHECK(auth::encode_sensor_auth_update(s,encoded,bytes) && bytes>0);
            s.region=96;CHECK(!auth::encode_sensor_auth_update(s,encoded,bytes) && bytes==0);s.region=104;
            s.generators.entries[0].slot=97;
            CHECK(!auth::encode_sensor_auth_update(s,encoded,bytes) && bytes==0);
            s.generators.entries[0].slot=98;
        }
    }
    std::printf("native clock protocol: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
