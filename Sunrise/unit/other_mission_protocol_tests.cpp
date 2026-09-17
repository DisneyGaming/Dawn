#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include "state/activity/strike_bond/authority.h"
#include "state/activity/eater_of_worlds/authority.h"
#include "state/activity/eater_of_worlds/doors.h"
#include "server/bap/encrypted/push/activity/eater_of_worlds_roster.h"

namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits = sunrise::middleware::encoding::bits;

void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

#ifdef OMEGA_PORT_LOCAL
// Native 3CA310 calls 351070 with an eight-byte destination. That reader copies
// the raw MSB-first bit stream into bytes, then the caller loads a little-endian
// qword. Decode those bytes explicitly instead of mirroring Writer::write(64).
std::uint64_t native_raw_clock(std::span<const std::byte> packet, std::size_t firstBit) {
    std::uint64_t ticks{};
    for (std::size_t byte = 0; byte < 8; ++byte) {
        std::uint8_t raw{};
        for (std::size_t bit = 0; bit < 8; ++bit) {
            const auto position = firstBit + byte * 8 + bit;
            const auto source = std::to_integer<unsigned>(packet[position / 8]);
            raw = static_cast<std::uint8_t>((raw << 1) | ((source >> (7 - position % 8)) & 1U));
        }
        ticks |= std::uint64_t{raw} << (byte * 8);
    }
    return ticks;
}
void check_gameplay_clock_transport() {
    for (const bool grant : {false, true}) {
        wire::Snapshot snapshot{};
        snapshot.lifetime = 3;
        snapshot.patchEpoch = {0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
        snapshot.hasGrant = grant;
        snapshot.grant = {11, 7};
        std::array<std::byte, 4096> baseline{}, packet{};
        std::size_t baselineSize{}, size{};
        check(wire::encode_sensor_auth_update(snapshot, baseline, baselineSize),
            "default clock packet encodes");
        const std::size_t clockBit = wire::kLatchBitWithoutGrant - wire::kActivityTokenWidth
            + (grant ? wire::kBubbleBlockBits : 0U);
        check(native_raw_clock(baseline, clockBit) == 0, "default native clock remains zero");
        for (const auto ticks : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{4712400},
                                std::uint64_t{0x0123456789ABCDEFULL},
                                std::uint64_t{0x8000000000000000ULL}, UINT64_MAX}) {
            snapshot.gameplayClockTicks = ticks;
            check(wire::encode_sensor_auth_update(snapshot, packet, size) && size == baselineSize,
                "native clock does not change packet width");
            check(native_raw_clock(packet, clockBit) == ticks,
                "native raw clock decodes little-endian after optional grant");
            bits::Reader decoded(packet);
            std::uint64_t value{};
            check(decoded.skip(clockBit + 64) && decoded.read(1, value) && value == 1,
                "clock preserves following enable latch");
            bits::Reader before(baseline), after(packet);
            for (std::size_t bit = 0; bit < size * 8; ++bit) {
                std::uint64_t a{}, b{};
                check(before.read(1, a) && after.read(1, b), "compare complete clock packet");
                if (bit < clockBit || bit >= clockBit + 64) {
                    check(a == b, "clock changes no epoch, grant, roster or padding bit");
                }
            }
        }
        snapshot.archiveOmega = true;
        snapshot.gameplayClockTicks = 0;
        check(wire::encode_sensor_auth_update(snapshot, baseline, baselineSize),
            "archive baseline clock packet encodes");
        snapshot.gameplayClockTicks = UINT64_MAX;
        check(wire::encode_sensor_auth_update(snapshot, packet, size)
            && size == baselineSize && packet == baseline,
            "archive Omega ignores non-archive gameplay clock field");
    }
}
void check_eater_outer_wire() {
    namespace eater=sunrise::state::activity::eater_of_worlds;
    namespace roster=sunrise::server::bap::encrypted::push::activity::eater_of_worlds_roster;
    namespace layouts=sunrise::state::build_data::scenarios;
    struct Storage {
        std::array<wire::BubbleSubBlock,64> rosterSubBlocks{};
        std::array<std::array<std::uint32_t,wire::kGroupCapacity>,64> rosterSubBlockKeys{};
    } storage;
    layouts::Definition layout{};constexpr std::string_view package="raid_envy_v310";
    std::copy(package.begin(),package.end(),layout.name.begin());layout.nameLength=static_cast<std::uint8_t>(package.size());
    layout.tag=eater::kScenario;layout.bubbleCount=8;
    wire::Snapshot snapshot{};snapshot.lifetime=3;snapshot.hasRegion=true;snapshot.phaseOneOnly=false;
    check(roster::admit(layout,storage,snapshot.roster) && snapshot.roster.groupCount==17,
        "Eater supplemental roster admits all 17 client groups");
    check(eater::kTraversalDoors.size()==2
        && eater::traversal_door({0x5654D7FDU,0x80C43A65U,23,0})==&eater::kTraversalDoors[0]
        && eater::traversal_door({0x5654D7FDU,0x80C43A68U,23,1})==&eater::kTraversalDoors[1]
        && eater::traversal_door({0xA9E6185FU,0x80B49E83U,23,1})==nullptr,
        "Eater traversal doors are exact and do not include the outer entrance door");
    for(const auto& door:eater::kTraversalDoors) {
        const auto* device=eater::find(door.device.registry,door.device.type,door.device.slot);
        const auto* monitor=eater::find(door.monitor.registry,door.monitor.type,door.monitor.slot);
        const auto volume=std::find_if(std::begin(eater::kVolumes),std::end(eater::kVolumes),
            [&](const auto& candidate) {return candidate.asset==door.volume;});
        check(device && device->asset.definition==door.device.definition
            && monitor && monitor->asset.definition==door.monitor.definition,
            "Eater traversal door and monitor identities match the recovered catalog");
        check(volume!=std::end(eater::kVolumes),
            "Eater traversal volume identity matches the recovered catalog");
    }
    std::array<std::byte,65536> packet{};std::size_t written{};
    const auto lifetimeOrdinal=[&](std::uint32_t key,std::uint16_t slot) {
        bits::Writer body(packet);
        check(wire::write_auth_body(body,snapshot,key,17,slot,false)
            && body.bit_count()==520,"lifetime body writes its exact native width");
        bits::Reader fields(packet);std::uint64_t ordinal{};
        check(fields.skip(72) && fields.read(32,ordinal),"lifetime authority+C ordinal decodes");
        return static_cast<std::uint32_t>(ordinal-0x80000000ULL);
    };
    // Before exact Eater admission, the legacy generic lifetime fallback resolves region zero.
    snapshot.lifetimeScenarioOrdinal=2;
    check(lifetimeOrdinal(0x24C67333U,3)==0,
        "unselected Eater-shaped lifetime does not consume scoped scenario state");
    snapshot.hasSpawnOverride=true;snapshot.spawnSliceSet=16;snapshot.spawnSetHash=0x8BA80878U;
    std::array<std::byte,65> genericBody{},eaterBody{};
    {
        bits::Writer before(genericBody);
        check(wire::write_auth_body(before,snapshot,0x24C67333U,17,3,false),
            "generic Eater-shaped lifetime fixture writes");
    }
    snapshot.eaterOfWorldsLifetime=true;
    {
        bits::Writer after(eaterBody);
        check(wire::write_auth_body(after,snapshot,0x24C67333U,17,3,false),
            "selected Eater lifetime fixture writes");
        bits::Reader before(genericBody),afterReader(eaterBody);
        for(std::size_t bit=0;bit<520;++bit) {
            std::uint64_t oldBit{},newBit{};
            check(before.read(1,oldBit) && afterReader.read(1,newBit),
                "Eater lifetime fixture comparison covers all bits");
            if(bit<72 || bit>=104) check(oldBit==newBit,
                "Eater ordinal fix preserves lifecycle, switches and spawn fields");
        }
    }
    for(std::int32_t region=0;region<=56;region+=8) {
        const auto projected=roster::lifetime_scenario(region);
        check(projected && *projected==static_cast<std::uint32_t>(region/8),
            "every authored Eater packed region projects to its bubble ordinal");
        snapshot.lifetimeScenarioOrdinal=projected;
        check(lifetimeOrdinal(0x24C67333U,3)==static_cast<std::uint32_t>(region/8),
            "selected Eater lifetime publishes the current bubble before or after arrival");
    }
    for(std::int32_t region=0;region<=63;++region) {
        check(static_cast<bool>(roster::lifetime_scenario(region))==(region%8==0),
            "Eater lifetime accepts only the eight aligned packed regions");
    }
    for(const auto invalid:{-1,std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::max(),64,65})
        check(!roster::lifetime_scenario(invalid),"Eater lifetime rejects out-of-range regions");
    snapshot.lifetimeScenarioOrdinal.reset();
    bits::Writer missing(packet);
    check(!wire::write_auth_body(missing,snapshot,0x24C67333U,17,3,false),
        "selected Eater lifetime rejects an absent scenario ordinal");
    for(const std::uint32_t invalid:{8U,63U,64U}) {
        snapshot.lifetimeScenarioOrdinal=invalid;
        bits::Writer oversized(packet);
        check(!wire::write_auth_body(oversized,snapshot,0x24C67333U,17,3,false),
            "selected Eater lifetime rejects an out-of-range scenario ordinal");
    }
    snapshot.lifetimeScenarioOrdinal=7;
    check(lifetimeOrdinal(0x24C67332U,3)==0 && lifetimeOrdinal(0x24C67333U,2)==0,
        "Eater lifetime projection rejects foreign keys and slots");
    check(lifetimeOrdinal(0x4786C0E0U,3)==7,
        "existing admitted shared lifetime still consumes its scenario ordinal");
    snapshot.lifetimeScenarioOrdinal=2;
    snapshot.region=16;
    check(wire::encode_sensor_auth_update(snapshot,packet,written) && written,
        "pre-arrival Eater packet encodes with lifetime while its mission frame is disabled");
    eater::Frame frame{};frame.enabled=frame.checked=true;frame.spawnGeneration=7;
    for(auto& state:frame.native) {
        state.generation=7;state.position=1.F;
        state.managed=state.desired=state.prepared=state.active=state.acknowledged=true;
    }
    for(const int region:{16,56,48}) {
        frame.region=region;snapshot.region=region;snapshot.eater_of_worlds=frame;
        snapshot.lifetimeScenarioOrdinal=roster::lifetime_scenario(region);
        check(wire::encode_sensor_auth_update(snapshot,packet,written) && written,
            "enabled Eater packet encodes across entrance, reactor and Argos regions");
        std::vector<std::byte> tooSmall(written-1,std::byte{0xA5});std::size_t rejected=99;
        check(!wire::encode_sensor_auth_update(snapshot,tooSmall,rejected) && rejected==0
            && tooSmall.front()==std::byte{0xA5},"Eater outer packet rejects short storage before writes");
    }
    for(const auto& asset:eater::kAssets) {
        const auto width=wire::auth_body_bits(snapshot,asset.asset.registry,
            static_cast<std::uint8_t>(asset.asset.type),asset.asset.slot,false);
        if(!width) continue;
        bits::Writer body(packet);
        check(wire::write_auth_body(body,snapshot,asset.asset.registry,
            static_cast<std::uint8_t>(asset.asset.type),asset.asset.slot,false)
            && body.bit_count()==width,"Eater offered body width matches the shared outer writer");
    }
}
#endif

int main() {
#ifdef OMEGA_PORT_LOCAL
    check_gameplay_clock_transport();
    check_eater_outer_wire();
#endif
    // A Tower Watch publication must retain its original single dialogue record
    // and target-free directive even if unrelated Omega fields are populated.
    wire::Snapshot snapshot{};
    snapshot.publishAuthoredCueTransition = true;
    snapshot.authoredCueRegistry = 0x12345678U;
    snapshot.authoredDialogueRecord = 4;
    snapshot.authoredDirectiveEvent = 0xAABBCCDDU;
    snapshot.omegaTunnelDialogue = snapshot.omegaVistaDialogue = snapshot.omegaExitDialogue = true;
    snapshot.omegaLairDialogueRequestedMask = (1U << 12) | (1U << 13);
    snapshot.omegaLairDialoguePendingRow = 13;
    snapshot.omegaWaypointRegistry = 0x95FB2E01U;
    snapshot.omegaWaypointIndex = 13;
    std::array<std::byte, 4096> buffer{};
    bits::Writer dialogue(buffer);
    check(wire::write_auth_body(dialogue, snapshot, snapshot.authoredCueRegistry, 53, 2, false),
        "Tower Watch dialogue must encode");
    check(dialogue.bit_count() == 19831, "Tower Watch must retain one active dialogue row");
    bits::Writer directive(buffer);
    check(wire::write_auth_body(directive, snapshot, snapshot.authoredCueRegistry, 68, 0, false),
        "Tower Watch directive must encode");
    check(directive.bit_count() == 4802, "Tower Watch directive width must be unchanged");
    bits::Reader reader(buffer);
    std::uint64_t value{};
    check(reader.skip(717) && reader.read(32, value) && value == 0x811C9DC5U,
        "Omega waypoints must not enter Tower Watch's directive");

    snapshot = {};
    snapshot.publishAuthoredSceneSelector = true;
    snapshot.authoredSceneRegistry = 0x9D8076E4U;
    snapshot.authoredSceneType = 43;
    snapshot.authoredSceneIndex = 5;
    snapshot.authoredSceneSelector = 0x80B82771U;
    snapshot.authoredSceneEntryRegistry = 0x9D8076E4U;
    snapshot.authoredSceneEntryType = 2;
    snapshot.authoredSceneEntryIndex = 6;
    bits::Writer scene(buffer);
    check(wire::write_auth_body(scene, snapshot, snapshot.authoredSceneRegistry, 43, 5, false),
        "Tower Watch authored Scene must encode");
    bits::Reader sceneReader(buffer);
    check(sceneReader.read(32, value) && value == 0x00B82771U,
        "The existing signed Scene selector must keep its wire bias");
    // Exercise the complete shared writer, not just the mission-local serializer.
    namespace garden=sunrise::state::activity::strike_bond;
    snapshot={};snapshot.strike_bond.enabled=true;snapshot.strike_bond.spawnGeneration=128;
    for(const bool restricted:{true,false}) {
        snapshot.strike_bond.restricted=restricted;
        bits::Writer director(buffer);
        check(wire::write_auth_body(director,snapshot,0x4786C0E0U,35,1,false) && director.bit_count()==359,
            "Garden respawn director size and write paths agree");
        bits::Reader state(buffer);check(state.read(1,value) && value==(restricted?1U:0U),"Garden restriction writes explicit on/off");
        bits::Writer lifetime(buffer);
        check(wire::write_auth_body(lifetime,snapshot,0x4786C0E0U,17,3,false),"Garden lifetime filter writes");
        bits::Reader filter(buffer);check(filter.skip(72) && filter.read(32,value) && value==(restricted?0x80000011U:0x80000000U),
            "Garden restriction targets authored Spire bubble 17");
    }
    for(const auto& golem:garden::kGolems) {
        const auto lens=garden::kLenses[golem.lens].source;
        auto& native=snapshot.strike_bond.native[garden::asset_index(lens)];native.managed=native.active=true;
        bits::Writer effect(buffer);
        check(wire::write_auth_body(effect,snapshot,golem.registry,26,golem.tether,false) && effect.bit_count()==186,
            "native Garden shield body is routed through shared codec");
        bits::Reader armed(buffer);check(armed.skip(1) && armed.read(1,value) && value==0,"live cube holds Minotaur shield");
        snapshot.strike_bond.lensDestroyed.set(golem.lens);
        bits::Writer off(buffer);
        check(wire::write_auth_body(off,snapshot,golem.registry,26,golem.tether,false),"destroyed Garden cube updates effect");
        bits::Reader disabled(buffer);check(disabled.skip(1) && disabled.read(1,value) && value==1,"real cube death disables only linked shield");
        bits::Writer collection(buffer);
        check(wire::write_auth_body(collection,snapshot,golem.registry,34,golem.collection,false) && collection.bit_count()==94,
            "Garden collection selector is a separate native body");
    }
    std::puts("PASS: native clock transport, Tower Watch isolation, Garden shields and respawn protocol");
}
