#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include "client/activity/mission_launch.h"
#include "client/activity/mission_launch_options.h"
#include "client/activity/campaign_openings.h"
#include "client/activity/mission_launch_testing.h"
#include "client/hooks/bootflow/mission_prelaunch.h"
#include "state/activity/forced/activity_forced_destination.h"
#include "state/activity/destination/activity_destination_snapshot.h"
#include "state/runtime/storage/internal.h"
#include "core/logging/log.h"
#include "middleware/content/packages/tables/activity_table.h"

namespace {
namespace launch = sunrise::client::activity::mission_launch;
namespace forced = sunrise::state::activity::forced;
namespace prelaunch = forced::prelaunch;
namespace build = sunrise::state::build_data;
namespace destination = sunrise::state::activity::destination;
unsigned g_checks{}, g_selects{}, g_commits{}, g_prepareCalls{};
std::uint64_t g_now{1000}, g_session{1};
std::int32_t g_step{29};
bool g_hooksReady{}, g_wrongDestination{}, g_descriptorValid{true};
std::array<std::byte, 0x1C900> g_manager{};
std::array<std::byte, 0xA40> g_record{};
destination::DestinationSelection g_actual{}, g_submitted{};
void check(bool value, const char* message) {
    ++g_checks; if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template<class T> void put(std::byte* data, std::size_t offset, T value) {
    std::memcpy(data + offset, &value, sizeof(value));
}
destination::DestinationSelection descriptor(std::int16_t index, std::string_view name) {
    destination::DestinationSelection value{};
    value.previousActivityIndex = value.activityIndex = index; value.reason = 0;
    value.packageNameLength = static_cast<std::uint8_t>(name.size());
    std::memcpy(value.packageName.data(), name.data(), name.size());
    value.descriptorBitLength = 620; value.descriptorNameBit = 100; value.hasDescriptorName = true;
    return value;
}
std::uintptr_t __fastcall world() { return reinterpret_cast<std::uintptr_t>(g_manager.data()); }
bool __fastcall ready(std::uintptr_t) { return true; }
std::uintptr_t __fastcall record(std::uint32_t member) {
    check(member == 0, "native primary member"); return reinterpret_cast<std::uintptr_t>(g_record.data());
}
void* __fastcall construct(void* buffer, std::uint32_t slot, std::int16_t index) {
    check(slot == 0 && index == 282, "construct pinned donor");
    put(static_cast<std::byte*>(buffer), 2, index); put(static_cast<std::byte*>(buffer), 4, index); return buffer;
}
bool __fastcall valid(const void*) { return g_descriptorValid; }
const char* __fastcall name(std::int16_t index) { check(index == 282, "native donor lookup"); return "mission_reunion"; }
void __fastcall clear() {}
void __fastcall select(std::uint8_t slot, const void* buffer) {
    ++g_selects; check(slot == 0, "native select slot");
    std::int16_t source{}, target{};
    std::memcpy(&source, static_cast<const std::byte*>(buffer) + 2, sizeof(source));
    std::memcpy(&target, static_cast<const std::byte*>(buffer) + 4, sizeof(target));
    forced::ForcedDestination selected{}; forced::snapshot(selected);
    check(forced::active(selected), "opening enabled before native selection");
    g_submitted = descriptor(target, "mission_reunion");
    const auto* profile = prelaunch::configured(selected);
    if (profile) {
        check(g_hooksReady, "donor cannot reach selection before both hooks are ready");
        check(forced::commit_prelaunch_authored_selection(source, target, selected), "real prelaunch commit");
        g_submitted = descriptor(profile->activity, profile->package);
    }
    check(forced::apply(g_submitted), "real forced destination applies to the native contract");
    if (g_wrongDestination) { g_submitted = descriptor(282, "mission_reunion"); }
}
void __fastcall commit(std::int32_t enabled) { check(enabled == 1, "native commit enabled"); ++g_commits; }
std::int32_t __fastcall step() { return g_step; }
void orbit() { g_step = 29; launch::poll(); }
void arrive() {
    g_step = 33; launch::poll();
    check(launch::snapshot().busy && !launch::snapshot().inMission, "loading remains pending");
    g_actual = g_submitted; ++g_session; g_step = 38; launch::poll();
}
void unchanged(const forced::ForcedDestination& expected) {
    forced::ForcedDestination current{}; forced::snapshot(current);
    check(launch::destination_name(current) == launch::destination_name(expected)
        && current.spawnSetHash == expected.spawnSetHash, "waiting or rejected launch preserves existing override");
}
}
namespace sunrise::state::runtime::storage { State g_state{}; SRWLOCK g_stateLock = SRWLOCK_INIT; }
namespace sunrise::core::log { void write(Channel, Level, std::string_view) noexcept {} }
namespace sunrise::state::activity {
std::uint64_t newest_joined_session() noexcept { return g_session; }
std::uint64_t mission_run_generation() noexcept { return g_session; }
void reset_mission_authority_runtime_initialization() noexcept {}
}
namespace sunrise::state::activity::destination {
bool snapshot(std::uint64_t session, DestinationSelection& value) noexcept {
    value = g_actual; return session == g_session && value.packageNameLength != 0;
}
}
namespace sunrise::state::build_data {
bool find_scenario_layout(std::string_view name, scenarios::Definition& value) noexcept {
    value = {};
    if (name == "mission_reunion") { return true; }
    for (const auto& mission : launch::openings::kMissions) {
        const auto& opening = mission.destination;
        if (name != launch::destination_name(opening)) { continue; }
        std::copy(name.begin(), name.end(), value.name.begin()); value.nameLength = static_cast<std::uint8_t>(name.size());
        std::copy(name.begin(), name.end(), value.spawnStem.begin()); value.spawnStemLength = value.nameLength;
        value.bubbleCount = opening.bubble + 1; value.bubbleStateCounts[opening.bubble] = 1;
        value.bubbleMapIndices[opening.bubble] = opening.bubble; return true;
    }
    return false;
}
bool find_spawn_sets(std::string_view name, std::span<spawn_sets::NameHash> values, std::size_t& count) noexcept {
    count = 0;
    for (const auto& mission : launch::openings::kMissions) {
        const auto& opening = mission.destination;
        if (name != launch::destination_name(opening) || !opening.hasSpawnSetHash || values.empty()) { continue; }
        values[0] = {}; values[0].value = opening.spawnSetHash; values[0].pointCount = values[0].inMapPackage = 1;
        values[0].bubbleMask[opening.bubble / 8] = static_cast<std::uint8_t>(1U << (opening.bubble % 8));
        count = 1; return true;
    }
    return false;
}
}
namespace sunrise::client::hooks::bootflow {
bool prepare_mission_prelaunch(const forced::ForcedDestination& value) noexcept {
    ++g_prepareCalls; return prelaunch::configured(value) == nullptr || g_hooksReady;
}
}
namespace sunrise::client::activity::mission_launch::testing {
std::uint64_t now() noexcept { return g_now; }
std::uintptr_t native_entry(std::uintptr_t rva) noexcept {
    switch (rva) {
    case 0xC03430: return reinterpret_cast<std::uintptr_t>(&world);
    case 0x1788810: case 0x178D740: return reinterpret_cast<std::uintptr_t>(&ready);
    case 0xBFA030: return reinterpret_cast<std::uintptr_t>(&record);
    case 0xC061D0: return reinterpret_cast<std::uintptr_t>(&construct);
    case 0x4D5460: return reinterpret_cast<std::uintptr_t>(&valid);
    case 0xDDECA0: return reinterpret_cast<std::uintptr_t>(&name);
    case 0xBF95D0: return reinterpret_cast<std::uintptr_t>(&::clear);
    case 0xBFB1F0: return reinterpret_cast<std::uintptr_t>(&select);
    case 0xBF97D0: return reinterpret_cast<std::uintptr_t>(&commit);
    case 0xE2D510: return reinterpret_cast<std::uintptr_t>(&step);
    default: return 0;
    }
}
}
int main(int argc, char** argv) {
    check(argc == 2, "provide installed activity table");
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate); check(file.good(), "fixture opens");
    const auto size = file.tellg(); check(size > 0, "fixture populated");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size)); file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size); check(file.good(), "fixture read");
    static std::array<build::activities::Definition, build::activities::kCapacity> rows{}; std::size_t count{};
    check(sunrise::middleware::content::packages::tables::activities::decode(bytes, rows, count), "decode installed catalog");
    check(build::activities::publish(std::span(rows).first(count)), "publish catalog");
    put(g_manager.data(), 0x10, std::int32_t{0});
    put(g_manager.data(), 0x18 + 0x1AEF8, std::int32_t{4});
    put(g_manager.data(), 0x18 + 0xE93C, std::int32_t{0});
    check(forced::publish(launch::openings::kOmegaOpening), "previous operator override");

    check(launch::request_opening(1), "Gateway request"); launch::poll();
    check(launch::snapshot().status == launch::Status::preparing && launch::snapshot().busy
        && g_selects == 0 && g_commits == 0, "delayed hooks defer donor launch");
    unchanged(launch::openings::kOmegaOpening);
    check(!launch::request_opening(2), "preparing request immutable");
    g_now += 10001; launch::poll();
    check(launch::snapshot().status == launch::Status::prelaunchUnavailable && !launch::snapshot().busy
        && g_selects == 0 && g_commits == 0, "missing hooks time out without submitting Chosen");
    unchanged(launch::openings::kOmegaOpening);
    g_descriptorValid = false;
    check(launch::request_opening(1), "rejected descriptor request"); launch::poll();
    check(launch::snapshot().status == launch::Status::descriptorRejected && g_selects == 0,
        "invalid native descriptor never launches");
    unchanged(launch::openings::kOmegaOpening); g_descriptorValid = true;

    for (std::size_t i = 0; i < launch::openings::kMissions.size(); ++i) {
        orbit(); g_hooksReady = false;
        check(launch::request_opening(i), "each opening can queue");
        const auto before = g_selects;
        launch::poll();
        if (prelaunch::configured(launch::openings::kMissions[i].destination)) {
            check(launch::snapshot().status == launch::Status::preparing && g_selects == before,
                "each authored profile waits for hooks");
            g_hooksReady = true; launch::poll();
        }
        check(g_selects == before + 1 && g_commits == g_selects && launch::snapshot().status == launch::Status::queued,
            "ready opening submits once");
        launch::poll(); check(g_selects == before + 1 && !launch::snapshot().inMission, "orbit does not imply arrival");
        arrive();
        const auto state = launch::snapshot();
        check(state.status == launch::Status::arrived && !state.busy && state.inMission
            && state.current_name() == launch::destination_name(launch::openings::kMissions[i].destination),
            "actual mission arrival resolves selected opening");
        launch::poll(); check(launch::snapshot().inMission, "presence persists after request completion");
        check(!launch::request_opening(i), "in-mission launch blocked");
        orbit(); check(!launch::snapshot().inMission && launch::snapshot().status == launch::Status::idle,
            "return to orbit resets presence and arrival status");
    }
    g_hooksReady = true; g_wrongDestination = true;
    check(launch::request_opening(1), "wrong destination scenario"); launch::poll(); arrive();
    check(launch::snapshot().status == launch::Status::unexpectedDestination && !launch::snapshot().busy
        && launch::snapshot().inMission && launch::snapshot().current_name() == "mission_reunion",
        "Chosen arrival reports actual mission and ends pending Gateway immediately");
    orbit(); g_wrongDestination = false;
    check(launch::request_opening(1), "Gateway replay"); launch::poll(); arrive();
    check(launch::snapshot().status == launch::Status::arrived, "Gateway replay commits a fresh opening");
    orbit();
    // A Director launch has no Dawn request but must still publish presence.
    g_actual = descriptor(292, "mission_abs"); ++g_session; g_step = 38; launch::poll();
    check(launch::snapshot().inMission && launch::snapshot().current_name() == "mission_abs"
        && !launch::snapshot().busy, "Director launch tracks current mission");
    g_session = 0; launch::poll(); check(!launch::snapshot().inMission, "missing session cannot claim arrival");
    check(g_prepareCalls > 8, "readiness seam exercised");
    std::cout << "PASS: " << g_checks << " prelaunch ordering, real override, arrival, replay and presence checks\n";
}
