#include "middleware/encoding/bit_writer.h"
#include "server/runtime/activity/mercury_public_event_music.h"
#include "state/activity/omega_music_authority.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace runtime = sunrise::server::runtime::activity::music;
namespace data = sunrise::server::runtime::activity::mercury::public_events;
namespace wire = runtime::wire;
namespace old = sunrise::state::activity::omega_music;
namespace bits = sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool value, const char *name) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL %u %s\n", checks, name);
        std::exit(1);
    }
}
struct Group {
    std::uint32_t key;
    std::span<const std::uint8_t> slotTypes, slotFlags;
    std::span<const std::uint16_t> slotIndices;
};
struct Block {
    std::uint32_t bubble;
    std::span<const std::uint32_t> keys;
    std::span<const std::uint8_t> presence, states;
};
struct Roster {
    std::array<Group, 2> groups{};
    std::size_t groupCount{}, topLevelGroupCount{};
    std::vector<Block> bubbleSubBlocks;
    std::span<const std::uint32_t> topLevelKeys;
    std::span<const std::uint8_t> topLevelPresence, topLevelStates;
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    check(runtime::Runtime::valid(data::kMusicDefinition), "exact native music registry and four candidate keys");
    auto bad = data::kMusicDefinition;
    bad.slot = 108;
    check(!runtime::Runtime::valid(bad), "dialogue cannot be selected as music");
    bad = data::kMusicDefinition;
    bad.candidates = {};
    check(!runtime::Runtime::valid(bad), "missing native candidates rejected");
    runtime::Context c{{123, {7}}, 11, 12, 13, 14, 29, 15, true, true};
    runtime::Runtime service;
    auto foreign = c;
    foreign.bubble = 16;
    check(!service.begin(data::kMusicDefinition, foreign), "wrong admission bubble rejected");
    check(service.begin(data::kMusicDefinition, c), "music lifetime begins");
    wire::Batch batch;
    check(service.append(batch) && !batch.count, "begin alone publishes nothing");
    foreign = c;
    foreign.owner.incarnation.value++;
    check(!service.request(data::kMusicCandidates[0], 1, foreign), "foreign incarnation cannot select music");
    check(!service.request(0x12345678, 1, c) && !service.request(data::kMusicCandidates[0], 2, c),
          "unknown keys and skipped epochs rejected");
    for (std::size_t i = 0; i < 4; ++i) {
        check(service.request(data::kMusicCandidates[i], i + 1, c), "UE accepts next explicit selector");
        check(service.request(data::kMusicCandidates[i], i + 1, c), "exact retry does not reset graph");
        check(!service.request(data::kMusicCandidates[(i + 1) % 4], i + 1, c),
              "same epoch cannot substitute candidate");
        check(service.update(c) && service.requested() && !service.failed(),
              "UE publishes requested authority without invented readiness");
        batch = {};
        check(service.append(batch) && batch.count == 1, "one retained type11 body");
        const auto &r = batch.entries[0];
        check(r.active[0] == (1U << i) && r.candidateCount == 4 && wire::valid(r), "single authored priority bit");
        std::array<std::byte, (wire::kBits + 7) / 8> bytes{}, reference{};
        bits::Writer w(bytes), other(reference);
        old::Authority legacy;
        check(old::select(legacy, i) && old::write(other, legacy), "established original codec comparison");
        check(wire::write(w, r) && w.bit_count() == wire::kBits && bytes == reference,
              "generic wire equals original codec for equivalent request");
        const auto name = "ordinal-" + std::to_string(i);
        std::ofstream file(output / (name + ".body"), std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        const auto decoded = wire::decoded(r);
        std::ofstream expected(output / (name + ".expected"), std::ios::binary);
        expected.write(reinterpret_cast<const char *>(decoded.data()), decoded.size());
        check(file.good() && expected.good(), "production native decoder fixtures exported");
    }
    foreign = c;
    foreign.bubble = 16;
    check(service.update(foreign), "region changes pause control updates");
    wire::Batch retained;
    check(service.append(retained) && retained.entries[0] == batch.entries[0], "region changes retain authority");
    check(!service.request(0, 5, foreign), "outside region cannot request retirement");
    check(service.request(0, 5, c) && service.update(c), "explicit clear command publishes withdrawal");
    batch = {};
    check(service.append(batch) && batch.entries[0].active == std::array<std::uint32_t, 4>{}, "clear has neutral mask");
    {
        const auto &r = batch.entries[0];
        std::array<std::byte, (wire::kBits + 7) / 8> bytes{};
        bits::Writer w(bytes);
        check(wire::write(w, r), "clear serializes");
        std::ofstream file(output / "clear.body", std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        const auto decoded = wire::decoded(r);
        std::ofstream expected(output / "clear.expected", std::ios::binary);
        expected.write(reinterpret_cast<const char *>(decoded.data()), decoded.size());
    }
    auto original = batch;
    check(!service.append(batch) && batch.count == original.count && batch.entries[0] == original.entries[0],
          "duplicate merge does not mutate batch");
    std::array<std::uint8_t, 2> types{11, 60}, flags{2, 1};
    std::array<std::uint16_t, 2> slots{109, 90};
    std::array<std::uint32_t, 1> keys{0xC8229B2B};
    std::array<std::uint8_t, 1> presence{1}, states{0x87};
    Roster roster;
    roster.groupCount = 1;
    roster.groups[0] = {keys[0], types, flags, slots};
    roster.bubbleSubBlocks.push_back({15, keys, presence, states});
    check(wire::valid(batch, roster, 120), "exact scoped writable music component admitted");
    check(wire::valid(batch, roster, 128), "retained full native roster authorizes old region body");
    roster.bubbleSubBlocks[0].states = {};
    check(!wire::valid(batch, roster, 128), "legacy roster cannot authorize cross-region retention");
    roster.bubbleSubBlocks[0].states = states;
    presence[0] = 0;
    check(!wire::valid(batch, roster, 120), "absent scope rejected");
    presence[0] = 1;
    flags[0] = 1;
    check(!wire::valid(batch, roster, 120), "read-only type11 rejected");
    flags[0] = 2;
    roster.topLevelGroupCount = 1;
    check(!wire::valid(batch, roster, 120), "bubble request cannot impersonate global scope");
    roster.topLevelGroupCount = 0;
    auto r = batch.entries[0];
    r.guards[0] = {keys[0], 60, 90};
    batch.entries[0] = r;
    check(wire::valid(batch, roster, 120), "native supported guard requires exact readable slot");
    flags[1] = 2;
    check(!wire::valid(batch, roster, 120), "unreadable guard rejected");
    flags[1] = 1;
    r.guards[0].type = 70;
    check(!wire::valid(r), "type70 is not a native music guard");
    r = batch.entries[0];
    r.active[0] = 16;
    check(!wire::valid(r), "mask outside native candidate count rejected");
    r = batch.entries[0];
    r.guards[4] = {keys[0], 60, 90};
    check(!wire::valid(r), "guard outside native candidate count rejected");
    r = batch.entries[0];
    r.guards[0].registry++;
    check(!wire::valid(r), "foreign registry reference rejected");
    check(wire::find(batch, keys[0], 11, 109) && !wire::find(batch, keys[0], 53, 109), "routing exact type and slot");
    roster.groups[0].slotFlags = {};
    check(!wire::valid(batch, roster, 120), "malformed roster vectors fail closed");
    batch.count = 99;
    check(!wire::find(batch, keys[0], 11, 109), "overflow batch cannot be routed");
    std::printf("PASS %u native music codec and UE lifecycle checks; audible playback remains live acceptance\n",
                checks);
}
