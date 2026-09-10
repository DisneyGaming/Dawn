#include "../src/server/bap/encrypted/push/activity/native_roster_lifetime_projection.h"
#include "../src/server/bap/internal.h"
#include "../src/middleware/encoding/bit_reader.h"
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits = sunrise::middleware::encoding::bits;
namespace life = sunrise::server::bap::encrypted::push::activity::roster_lifetime;
namespace {
unsigned checks{};
void check(bool value, int line) { ++checks; if (!value) { std::printf("FAIL %d\n", line); std::exit(1); } }
#define CHECK(value) check((value), __LINE__)
constexpr life::Identity identity{0x9EAA300100200001ULL,1,UINT64_MAX,UINT64_MAX,0x80F4696A};
struct Encoded { std::array<std::byte,8192> bytes{}; std::size_t bits{}; };
Encoded encode(const wire::Roster& roster, bool legacy) {
    Encoded result; bits::Writer writer(result.bytes);
    CHECK(legacy ? wire::legacy_write_roster_delta(writer,roster,3)
                 : wire::write_roster_delta(writer,roster,3));
    result.bits=writer.bit_count(); return result;
}
std::uint64_t take(bits::Reader& reader,std::uint8_t width) {
    std::uint64_t result{}; CHECK(reader.read(width,result)); return result;
}
void top_values(const Encoded& value,std::span<const std::uint32_t> keys,
    std::span<const std::uint8_t> presence,std::span<const std::uint8_t> states) {
    bits::Reader reader(value.bytes);
    CHECK(take(reader,3)==7); CHECK(take(reader,9)==keys.size());
    for(auto key:keys) CHECK(take(reader,32)==key);
    CHECK(take(reader,1)==1);
    for(std::size_t word=0;word<8;++word) {
        const auto actual=take(reader,32);
        for(std::size_t bit=0;bit<32;++bit) {
            const auto i=word*32+bit;
            CHECK(((actual>>bit)&1U)==(i<keys.size()?presence[i]:0));
        }
    }
    CHECK(take(reader,1)==1); CHECK(take(reader,9)==keys.size());
    for(auto state:states) CHECK(take(reader,8)==state);
}
}

// This target exercises only phase-1 roster encoding. Any accidental authority
// body call is a test failure, rather than a simulated native implementation.
namespace sunrise::middleware::bap::activity_message::sensor_auth_update {
std::size_t auth_body_bits(const Snapshot&,std::uint32_t,std::uint8_t,std::uint16_t,bool) noexcept {std::abort();}
bool write_auth_body(encoding::bits::Writer&,const Snapshot&,std::uint32_t,std::uint8_t,std::uint16_t,bool) noexcept {std::abort();}
std::size_t legacy_auth_body_bits(const Snapshot&,std::uint32_t,std::uint8_t,std::uint16_t,bool) noexcept {std::abort();}
bool legacy_write_auth_body(encoding::bits::Writer&,const Snapshot&,std::uint32_t,std::uint8_t,std::uint16_t,bool) noexcept {std::abort();}
}

int main() {
    constexpr std::array<std::uint32_t,8> mercury{0x74337EDD,0x564C6ECE,0xF25B938B,0x2749BAAE,
        0xEB1E8934,0x85C38F77,0x4A3E4900,0x2571C34D};
    std::array<wire::BubbleSubBlock,64> storage{};
    storage[0]={15,mercury,{},{}};
    wire::Roster initial{}; initial.groups[0].key=0x4786C0E0;
    initial.groupCount=initial.topLevelGroupCount=1;
    initial.bubbleSubBlocks=std::span(storage).first(1);
    auto prior=std::make_unique<life::State>();
    CHECK(life::prepare({},identity,15,0x83,true,initial,*prior)==life::Result::ready);
    const auto oldBytes=encode(initial,true);
    wire::Roster projected=initial;
    CHECK(life::project(*prior,projected,storage));
    for(bool legacy:{false,true}) {
        const auto encoded=encode(projected,legacy);
        CHECK(encoded.bits==oldBytes.bits); CHECK(encoded.bytes==oldBytes.bytes);
        top_values(encoded,life::view(prior->top).keys,life::view(prior->top).presence,life::view(prior->top).states);
    }
    // A different regional root and local group append without moving Mercury's
    // acknowledged block or its Vance/Cabal entry ordinals.
    constexpr std::array<std::uint32_t,1> dungeon{0xABCDEF12};
    std::array<wire::BubbleSubBlock,1> incoming{{{16,dungeon,{},{}}}};
    wire::Roster next{}; next.groups[0].key=0x98765432; next.groups[1].key=0x4786C0E0;
    next.groupCount=next.topLevelGroupCount=2;next.bubbleSubBlocks=incoming;
    auto candidate=std::make_unique<life::State>();
    CHECK(life::prepare(*prior,identity,16,0x83,false,next,*candidate)==life::Result::ready);
    CHECK(candidate->top.keys[0]==0x4786C0E0 && candidate->top.keys[1]==0x98765432);
    CHECK(candidate->blocks[0].bubble==15 && candidate->blocks[1].bubble==16);
    CHECK(candidate->blocks[0].entries.keys[1]==0x564C6ECE);
    CHECK(candidate->blocks[0].entries.keys[7]==0x2571C34D);
    for(std::size_t i=0;i<mercury.size();++i) {
        CHECK(candidate->blocks[0].entries.states[i]==0x83);
        CHECK(candidate->blocks[0].entries.presence[i]==1);
    }
    CHECK(prior->top.count==1 && prior->blockCount==1); // Discard has spent no ordinal.
    auto committed=std::make_unique<life::State>(*candidate);
    CHECK(life::project(*committed,next,storage));
    candidate.reset(); // Every projected span belongs to the delivered after-image.
    CHECK(next.topLevelKeys.data()==committed->top.keys.data());
    CHECK(next.bubbleSubBlocks[0].keys.data()==committed->blocks[0].entries.keys.data());
    CHECK(encode(next,false).bytes==encode(next,true).bytes);

    // Tombstones and distinct state bytes span every word of the native mask.
    std::array<std::uint32_t,256> keys{};
    std::array<std::uint8_t,256> presence{},states{};
    for(std::size_t i=0;i<keys.size();++i) {
        keys[i]=static_cast<std::uint32_t>(0x1000+i);
        presence[i]=i%3?1:0; states[i]=static_cast<std::uint8_t>(0x80+(i%128));
    }
    wire::Roster retained{}; retained.topLevelKeys=keys;
    retained.topLevelPresence=presence;retained.topLevelStates=states;
    for(bool legacy:{false,true}) top_values(encode(retained,legacy),keys,presence,states);
    retained.topLevelStates=std::span(states).first(255);
    std::array<std::byte,8192> invalid{};bits::Writer writer(invalid);
    CHECK(!wire::legacy_write_roster_delta(writer,retained,3));
    CHECK(writer.bit_count()==0);
    std::printf("%u checks passed; Session=%zu ActivityBinding=%zu Lifetime=%zu RegionSnapshot=%zu\n",checks,
        sizeof(sunrise::server::bap::Session),sizeof(sunrise::server::bap::ActivityBindingState),
        sizeof(life::State),sizeof(sunrise::server::bap::RegionTransitionSnapshot));
}
