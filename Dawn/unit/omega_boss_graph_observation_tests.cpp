#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <span>

#include "client/hooks/bootflow/omega_boss_graph.h"
#include "client/hooks/bootflow/omega_boss_graph_observation.h"

namespace graph = dawn::client::hooks::bootflow::omega_boss_graph;
namespace observe = dawn::client::hooks::bootflow::omega_boss_graph_observation;
namespace {
unsigned checks{};
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
using Bank = std::array<std::byte, 0x440>;
using Raw = std::array<std::byte, observe::kGraphBytes>;
constexpr std::array rows{2, 5, 1, 1, 13};
constexpr std::array<std::uint32_t, 5> clips{
    0x80F4517D, 0x80F1FD8C, 0x80F4517C, 0x80F4517C, 0x80F45188};
constexpr std::array clipIndices{5, 4, 3, 3, 17};
constexpr std::array limits{1.F, 8.700000762939453F, 10.000000953674316F,
                            10.000000953674316F, 7.200000286102295F};
constexpr std::array loops{true, false, true, true, false};
constexpr std::array phases{observe::Phase::leadIn, observe::Phase::fly,
                            observe::Phase::idle, observe::Phase::hover,
                            observe::Phase::summon};

// Relevant unmodified bytes of extracted 80F45190 (see observation/asset-proof.json).
// Optional argv[1] substitutes the complete extracted resource for this fixture.
Bank asset_bank() {
    Bank result{};
    graph::write(result, 0x08, std::uint64_t{25});
    graph::write(result, 0x10, std::uint64_t{0x90});
    graph::write(result, 0x68, std::uint64_t{18});
    graph::write(result, 0x70, std::uint64_t{0x180});
    graph::write(result, 0x80, std::uint32_t{0x80F45178});
    for (std::size_t n = 0; n < rows.size(); ++n) {
        const auto row = 0x200 + static_cast<std::size_t>(rows[n]) * 0x20;
        graph::write(result, row + 0x14, limits[n]);
        graph::write(result, row + 0x18, static_cast<std::int16_t>(clipIndices[n]));
        graph::write(result, 0xB0 + static_cast<std::size_t>(clipIndices[n]) * 4, clips[n]);
    }
    return result;
}
graph::Owner owner() {
    return {7, 0x37F92001, 0x53F4200E, 41, 0, 0x14F9EA25,
            0x76F9EA22, 0x05F3A00E, 0x2DF9ECFD, 0, 0x63FAA2EA};
}
Raw native_snapshot(std::size_t node) {
    Raw raw{};
    const auto bound = owner();
    graph::write(raw, 0x00, bound.entity);
    graph::write(raw, 0x04, bound.character);
    graph::write(raw, 0x10, observe::kGraphAsset);
    graph::write(raw, 0x14, bound.biped);
    graph::write(raw, 0x18, bound.entity);
    graph::write(raw, 0x1C, bound.biped);
    raw[0x20] = loops[node] ? std::byte{1} : std::byte{0};
    raw[0x21] = std::byte{1};
    graph::write(raw, 0x28, observe::kInvalid);
    graph::write(raw, 0x2C, observe::kInvalid);
    graph::write(raw, 0x30, rows[node]);
    graph::write(raw, 0x38, limits[node]);
    graph::write(raw, 0x3C, 0.125F);
    graph::write(raw, 0xA4, static_cast<std::int32_t>(node));
    graph::write(raw, 0xA8, std::int32_t{0});
    graph::write(raw, 0xB0, static_cast<std::int32_t>(node));
    graph::write(raw, 0xB4, std::int32_t{0});
    return raw;
}
void exact_nodes(const Bank& bank) {
    for (std::size_t n = 0; n < phases.size(); ++n) {
        auto raw = native_snapshot(n);
        observe::Snapshot snapshot;
        check(observe::decode(raw, bank, true, snapshot), "actual graph node and bank mapping accepted");
        check(snapshot.phase == phases[n] && snapshot.clip == clips[n]
              && snapshot.bankRow == rows[n], "loaded node proves matching clip and phase");
        check(observe::belongs_to(snapshot, owner(), owner(), graph::kGroup, graph::kSequence),
              "exact full issued ownership accepted");
        check(!observe::decode(raw, bank, false, snapshot), "failed original does not acknowledge node");
        raw[0x21] = std::byte{0};
        check(!observe::decode(raw, bank, true, snapshot), "inactive playback cannot acknowledge node");
        raw[0x21] = std::byte{1};
        raw[0x20] = loops[n] ? std::byte{0} : std::byte{1};
        check(!observe::decode(raw, bank, true, snapshot), "wrong loop state rejects mismatched playback");
    }
    for (const auto pair : std::array{std::array{1, 3}, std::array{3, 4}, std::array{4, 2}}) {
        auto raw = native_snapshot(static_cast<std::size_t>(pair[0]));
        graph::write(raw, 0xA4, pair[1]);
        observe::Snapshot snapshot;
        check(observe::decode(raw, bank, true, snapshot), "next-node request may precede native load");
        check(snapshot.phase == phases[static_cast<std::size_t>(pair[0])],
              "request alone never acknowledges next phase");
        graph::write(raw, 0xB0, std::int32_t{-1});
        check(!observe::decode(raw, bank, true, snapshot), "requested node with no loaded node rejected");
    }
    auto raw = native_snapshot(3);
    graph::write(raw, 0x3C, 9.F);
    observe::Snapshot snapshot;
    check(observe::decode(raw, bank, true, snapshot) && snapshot.phase == observe::Phase::hover,
          "native playback time need not be normalized to one");
}
void stale_ownership(const Bank& bank) {
    observe::Snapshot snapshot;
    check(observe::decode(native_snapshot(1), bank, true, snapshot), "owned fly fixture loads");
    const auto issued = owner();
    for (unsigned field = 0; field < 10; ++field) {
        auto current = issued;
        switch (field) {
        case 0: ++current.run; break;
        case 1: current.member ^= 0x20000000; break;
        case 2: current.actor ^= 0x20000000; break;
        case 3: ++current.generation; break;
        case 4: ++current.revision; break;
        case 5: current.parent ^= 0x20000000; break;
        case 6: current.character ^= 0x20000000; break;
        case 7: current.animation ^= 0x20000000; break;
        case 8: current.biped ^= 0x20000000; break;
        case 9: current.entity ^= 0x20000000; break;
        }
        check(!observe::belongs_to(snapshot, current, issued, graph::kGroup, graph::kSequence),
              "stale full ownership or same-index recycled handles rejected");
    }
    check(!observe::belongs_to(snapshot, issued, issued, graph::kGroup ^ 1, graph::kSequence),
          "wrong named group rejected");
    check(!observe::belongs_to(snapshot, issued, issued, graph::kGroup, graph::kSequence ^ 1),
          "wrong named sequence rejected");
    auto wrong = snapshot;
    wrong.entity ^= 0x20000000;
    check(!observe::belongs_to(wrong, issued, issued, graph::kGroup, graph::kSequence),
          "other entity graph cannot claim current camera");
    wrong = snapshot; wrong.character ^= 0x20000000;
    check(!observe::belongs_to(wrong, issued, issued, graph::kGroup, graph::kSequence),
          "other character graph rejected");
    wrong = snapshot; wrong.biped ^= 0x20000000;
    check(!observe::belongs_to(wrong, issued, issued, graph::kGroup, graph::kSequence),
          "other biped graph rejected");
}
void invalid_data(const Bank& bank) {
    observe::Snapshot out;
    const auto base = native_snapshot(1);
    for (std::size_t length = 0; length < base.size(); ++length)
        check(!observe::decode(std::span{base}.first(length), bank, true, out), "short graph copy rejected");
    for (const std::size_t offset : {0x10U, 0x18U, 0x1CU, 0x28U, 0x2CU, 0x30U, 0xA8U, 0xB4U}) {
        auto raw = base;
        graph::write(raw, offset, observe::field<std::uint32_t>(raw, offset) ^ 1U);
        check(!observe::decode(raw, bank, true, out), "foreign graph, playback key or node rejected");
    }
    for (const auto invalid : {-1, 5, 2147483647}) {
        auto raw = base;
        graph::write(raw, 0xA4, invalid);
        check(!observe::decode(raw, bank, true, out), "requested node bounds checked");
        raw = base;
        graph::write(raw, 0xB0, invalid);
        check(!observe::decode(raw, bank, true, out), "loaded node bounds checked");
    }
    for (float invalid : {-1.F, 99.F, std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()}) {
        auto raw = base;
        graph::write(raw, 0x3C, invalid);
        check(!observe::decode(raw, bank, true, out), "invalid playback time rejected");
    }
    for (float invalid : {0.F, -1.F, std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()}) {
        auto raw = base;
        graph::write(raw, 0x38, invalid);
        check(!observe::decode(raw, bank, true, out), "invalid native playback limit rejected");
    }
    for (const std::size_t offset : {0x08U, 0x10U, 0x68U, 0x70U}) {
        auto changed = bank;
        graph::write(changed, offset, std::numeric_limits<std::uint64_t>::max());
        check(!observe::decode(base, changed, true, out), "bank count or relative overflow rejected");
    }
    auto changed = bank;
    graph::write(changed, 0xB0 + 4 * 4, std::uint32_t{0x80F1FD8D});
    check(!observe::decode(base, changed, true, out), "wrong actual clip rejected despite expected bank row");
    changed = bank;
    graph::write(changed, 0x200 + 5 * 0x20 + 0x18, std::int16_t{-1});
    check(!observe::decode(base, changed, true, out), "negative clip index rejected");
    graph::write(changed, 0x200 + 5 * 0x20 + 0x18, std::int16_t{25});
    check(!observe::decode(base, changed, true, out), "clip index over count rejected");
    for (std::size_t length = 0; length < 0x2C0; ++length)
        check(!observe::decode(base, std::span{bank}.first(length), true, out), "short bank copy rejected");
}
} // namespace

int main(int argc, char** argv) {
    auto bank = asset_bank();
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        check(file.good(), "actual bank fixture opens");
        file.read(reinterpret_cast<char*>(bank.data()), static_cast<std::streamsize>(bank.size()));
        check(file.gcount() == static_cast<std::streamsize>(bank.size()) && file.peek() == EOF,
              "actual bank fixture is exactly 440 hex bytes");
    }
    exact_nodes(bank);
    stale_ownership(bank);
    invalid_data(bank);
    std::printf("omega boss graph observation: %u checks passed\n", checks);
}
