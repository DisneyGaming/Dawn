#include "middleware/content/packages/tables/scenario_reader.h"
#include "middleware/content/packages/tables/activity_table.h"
#include "middleware/content/packages/tables/internal.h"
#include "server/bap/encrypted/push/activity/activity_arrival.h"
#include "server/runtime/activity/haunted_forest_launch_profile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace tables = sunrise::middleware::content::packages::tables;
namespace destination = sunrise::state::activity::destination;
namespace scenarios = sunrise::state::build_data::scenarios;
namespace arrival = sunrise::server::bap::encrypted::push::activity;
namespace policy = sunrise::server::runtime::activity::initial_arrival;
namespace haunted = sunrise::server::runtime::activity::haunted_forest;

namespace profile_fixture {
std::array<sunrise::state::build_data::activities::Definition, 4095> catalog{};
std::size_t catalogCount{};
scenarios::Definition layout{};
bool layoutAvailable{true};
}
namespace sunrise::state::build_data::activities {
std::span<const Definition> entries() noexcept {
    return {profile_fixture::catalog.data(), profile_fixture::catalogCount};
}
}
namespace sunrise::state::build_data {
bool find_scenario_layout(std::string_view name, scenarios::Definition& output) noexcept {
    output = {};
    if (!profile_fixture::layoutAvailable || name != "infinite_abyss") return false;
    output = profile_fixture::layout;
    return true;
}
}

unsigned checks{};
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); \
} } while (false)

std::vector<std::byte> fixture(const std::filesystem::path& root, const char* name) {
    std::ifstream stream(root / name, std::ios::binary | std::ios::ate);
    CHECK(stream.good());
    const auto length = stream.tellg();
    CHECK(length > 0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), length);
    CHECK(stream.good());
    return bytes;
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::filesystem::path root(argv[1]);
    const auto scenario = fixture(root, "81550015.bin");
    const auto publicIndex = fixture(root, "81327CF0.bin");
    CHECK(tables::activities::decode(publicIndex, profile_fixture::catalog,
                                     profile_fixture::catalogCount));
    CHECK(profile_fixture::catalogCount == 1170);
    CHECK(profile_fixture::catalog[78].hash == 0x56B7B6A5);
    CHECK(profile_fixture::catalog[79].hash == 0x2A30935A);
    CHECK(profile_fixture::catalog[80].name() == "mission_abyss_intro");
    tables::Array bubbles{};
    CHECK(tables::scenario_bubbles(scenario, bubbles));
    CHECK(bubbles.count == 20);
    scenarios::Definition layout{};
    layout.tag = 0x81550015;
    layout.bubbleCount = static_cast<std::uint8_t>(bubbles.count);
    for (std::uint8_t i = 0; i < layout.bubbleCount; ++i) {
        tables::Bubble bubble{};
        tables::SliceState state{};
        CHECK(tables::bubble_at(scenario, bubbles, i, bubble));
        CHECK(bubble.stateCount == 1);
        CHECK(tables::slice_state_at(scenario, bubble, 0, state));
        CHECK(state.enabled);
        layout.bubbleStates[i] = scenarios::kBubbleEnabledByte;
        layout.bubbleHashes[i] = bubble.nameHash;
        layout.bubbleMapIndices[i] = static_cast<std::uint16_t>(state.mapBubbleIndex);
        layout.bubbleStateCounts[i] = static_cast<std::uint8_t>(bubble.stateCount);
        if (i == 13) {
            CHECK(bubble.stateDataOffset + 28 == 0x7CC);
            CHECK(bubble.nameHash == 0x47EA4CE9);
            CHECK(state.mapBubbleIndex == 30);
            CHECK(state.entryTag == 0x8155000E);
            CHECK(state.stateHash == 0x3677239F);
            CHECK(!state.isPublic);
        }
    }
    // The package Hash64 map links row 13 to this wrapper. The inventory separately
    // scans every newest installed package and verifies that no other row resolves.
    const auto wrapper = fixture(root, "8150A0A8.bin");
    std::uint32_t child{}, hash{};
    CHECK(tables::read(wrapper, 8, child) && child == 0x8150A9FC);
    CHECK(tables::read(wrapper, 24, hash) && hash == layout.bubbleHashes[13]);
    const auto childData = fixture(root, "8150A9FC.bin");
    CHECK(tables::read(childData, 8, child) && child == 13);

    sunrise::state::activity::defaults::DefaultDestination defaults{};
    defaults.fallback.initialSliceSet = 120;
    defaults.fallback.spawnSetHash = 0x2EA8FB98;
    destination::DestinationSelection selection{};
    constexpr char name[] = "infinite_abyss";
    std::memcpy(selection.packageName.data(), name, sizeof name - 1);
    selection.packageNameLength = sizeof name - 1;
    selection.activityIndex = 78;
    selection.previousActivityIndex = 78;
    selection.hasArrivalBubbleHash = true;
    selection.arrivalBubbleHash = destination::kAbsentSpawnSetHash;
    selection.hasSpawnSetHash = true;
    selection.spawnSetHash = destination::kAbsentSpawnSetHash;
    selection.reason = 0;
    selection.hasElementIndex = true;
    selection.elementIndex = 17;
    selection.descriptorBits.fill(std::byte{0xAD});
    selection.descriptorBitLength = 372;
    selection.hasDescriptorName = true;
    selection.descriptorNameBit = 42;
    const auto baseline = arrival::arrival_slice_set(defaults, selection, name, layout);
    std::printf("Current unprofiled #78 arrival = %u (archived failure selected 0).\n",
                static_cast<unsigned>(baseline));

    // Prove the existing server arrival boundary can select the authored region while
    // preserving the wire selection. This is a proposed input, not a runtime fix.
    const auto wireSelection = selection;
    selection.arrivalBubbleOverride = 13;
    selection.hasArrivalBubbleOverride = true;
    CHECK(arrival::arrival_slice_set(defaults, selection, name, layout) == 104);
    CHECK(selection.activityIndex == wireSelection.activityIndex);
    CHECK(selection.previousActivityIndex == wireSelection.previousActivityIndex);
    CHECK(selection.arrivalBubbleHash == wireSelection.arrivalBubbleHash);
    CHECK(selection.spawnSetHash == wireSelection.spawnSetHash);
    CHECK(selection.descriptorBits == wireSelection.descriptorBits);
    CHECK(destination::resolve_spawn_set_hash(selection, defaults.fallback.spawnSetHash)
          == 0x2EA8FB98);
    // Explicit caller overrides still have their established priority.
    selection.hasSliceSetOverride = true;
    selection.sliceSetOverride = 120;
    CHECK(arrival::arrival_slice_set(defaults, selection, name, layout) == 120);

    constexpr std::string_view mapStem = "infinite_forest_live";
    std::memcpy(layout.spawnStem.data(), mapStem.data(), mapStem.size());
    layout.spawnStemLength = static_cast<std::uint8_t>(mapStem.size());
    profile_fixture::layout = layout;
    selection = wireSelection;
    CHECK(haunted::apply_initial_arrival(selection) == policy::Result::applied);
    CHECK(arrival::arrival_slice_set(defaults, selection, name, layout) == 104);
    CHECK(!selection.hasSliceSetOverride);
    CHECK(destination::resolve_spawn_set_hash(selection, defaults.fallback.spawnSetHash)
          == 0x79E3AB1F);
    CHECK(selection.packageName == wireSelection.packageName);
    CHECK(selection.packageNameLength == wireSelection.packageNameLength);
    CHECK(selection.activityIndex == 78 && selection.previousActivityIndex == 78);
    CHECK(selection.reason == wireSelection.reason);
    CHECK(selection.elementIndex == wireSelection.elementIndex && selection.hasElementIndex);
    CHECK(selection.descriptorBits == wireSelection.descriptorBits);
    CHECK(selection.descriptorBitLength == wireSelection.descriptorBitLength);
    CHECK(selection.descriptorNameBit == wireSelection.descriptorNameBit);
    CHECK(selection.hasDescriptorName == wireSelection.hasDescriptorName);
    CHECK(selection.hasArrivalBubbleHash == wireSelection.hasArrivalBubbleHash);
    CHECK(selection.arrivalBubbleHash == wireSelection.arrivalBubbleHash);
    CHECK(selection.hasSpawnSetHash == wireSelection.hasSpawnSetHash);
    CHECK(selection.spawnSetHash == wireSelection.spawnSetHash);

    const auto unchanged = [](destination::DestinationSelection value, policy::Result expected) {
        std::array<std::byte, sizeof value> before{}, after{};
        std::memcpy(before.data(), &value, sizeof value);
        CHECK(haunted::apply_initial_arrival(value) == expected);
        std::memcpy(after.data(), &value, sizeof value);
        CHECK(before == after);
    };
    auto explicitSelection = wireSelection;
    explicitSelection.arrivalBubbleHash = 0xA83A9175;
    unchanged(explicitSelection, policy::Result::explicit_arrival);
    explicitSelection = wireSelection;
    explicitSelection.hasArrivalBubbleOverride = true;
    explicitSelection.arrivalBubbleOverride = 15;
    unchanged(explicitSelection, policy::Result::explicit_arrival);
    explicitSelection = wireSelection;
    explicitSelection.hasSliceSetOverride = true;
    explicitSelection.sliceSetOverride = 120;
    unchanged(explicitSelection, policy::Result::explicit_arrival);
    explicitSelection = wireSelection;
    explicitSelection.hasSpawnSetOverride = true;
    explicitSelection.spawnSetOverride = 0x12345678;
    unchanged(explicitSelection, policy::Result::explicit_arrival);
    explicitSelection = wireSelection;
    explicitSelection.spawnSetHash = 0x8BC697B5;
    CHECK(haunted::apply_initial_arrival(explicitSelection) == policy::Result::applied);
    CHECK(!explicitSelection.hasSpawnSetOverride);
    CHECK(destination::resolve_spawn_set_hash(explicitSelection, 0) == 0x8BC697B5);
    CHECK(arrival::arrival_slice_set(defaults, explicitSelection, name, layout) == 104);
    for (const auto otherIndex : {79, 80, 1076, -1}) {
        auto other = wireSelection;
        other.activityIndex = static_cast<std::int16_t>(otherIndex);
        unchanged(other, policy::Result::not_applicable);
    }
    auto otherPackage = wireSelection;
    otherPackage.packageName[0] = 'x';
    unchanged(otherPackage, policy::Result::not_applicable);

    const auto originalActivity = profile_fixture::catalog[78];
    const auto badActivity = [&](auto mutate) {
        mutate(profile_fixture::catalog[78]);
        unchanged(wireSelection, policy::Result::content_mismatch);
        profile_fixture::catalog[78] = originalActivity;
    };
    badActivity([](auto& value) { value.hash ^= 1; });
    badActivity([](auto& value) { value.index = 79; });
    badActivity([](auto& value) { value.gameplaySettingsHash ^= 1; });
    badActivity([](auto& value) { value.nativeType = 1; });
    badActivity([](auto& value) { value.destination = 15; });
    badActivity([](auto& value) { value.package[0] = 'x'; });
    const auto badLayout = [&](auto mutate) {
        mutate(profile_fixture::layout);
        unchanged(wireSelection, policy::Result::content_mismatch);
        profile_fixture::layout = layout;
    };
    badLayout([](auto& value) { value.tag ^= 1; });
    badLayout([](auto& value) { value.truncated = 1; });
    badLayout([](auto& value) { value.bubbleCount = 19; });
    badLayout([](auto& value) { value.bubbleStates[13] = scenarios::kBubbleDisabledByte; });
    badLayout([](auto& value) { value.bubbleHashes[13] ^= 1; });
    badLayout([](auto& value) { value.bubbleMapIndices[13] = 0; });
    badLayout([](auto& value) { value.bubbleStateCounts[13] = 2; });
    badLayout([](auto& value) { value.spawnStem[0] = 'x'; });
    badLayout([](auto& value) { value.spawnStemLength = 255; });
    profile_fixture::layoutAvailable = false;
    unchanged(wireSelection, policy::Result::content_mismatch);
    profile_fixture::layoutAvailable = true;
    profile_fixture::catalogCount = 78;
    unchanged(wireSelection, policy::Result::content_mismatch);
    profile_fixture::catalogCount = 1170;
    std::printf("%u package/profile/production-arrival checks passed. "
                "Initial spawn semantics and live launch still require the labeled test.\n", checks);
}
