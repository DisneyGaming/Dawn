#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "state/activity/forced/activity_forced_destination.h"
#include "state/activity/forced/prelaunch_profile.h"
#include "state/runtime/storage/internal.h"
#include "core/logging/log.h"

// Link the real routing implementation with only process state/logging seams.
namespace dawn::state::runtime::storage { State g_state{}; SRWLOCK g_stateLock = SRWLOCK_INIT; }
namespace dawn::core::log { void write(Channel, Level, std::string_view) noexcept {} }
namespace dawn::state::activity {
std::uint64_t mission_run_generation() noexcept { return 17; }
void reset_mission_authority_runtime_initialization() noexcept {}
}
namespace f = dawn::state::activity::forced;
namespace d = dawn::state::activity::destination;
namespace p = f::prelaunch;
void check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); } }
d::DestinationSelection descriptor(std::int16_t activity, const char* name) {
    d::DestinationSelection s{};
    s.previousActivityIndex = activity; s.activityIndex = activity; s.reason = 0;
    s.packageNameLength = static_cast<std::uint8_t>(std::strlen(name));
    std::memcpy(s.packageName.data(), name, s.packageNameLength);
    s.descriptorBitLength = 620; s.descriptorNameBit = 100; s.hasDescriptorName = true;
    s.descriptorBits.fill(std::byte{0xA5}); return s;
}
unsigned bits(const d::DestinationSelection& s, unsigned start, unsigned count) {
    unsigned value = 0;
    for (unsigned i = 0; i < count; ++i) {
        value = value * 2 + ((static_cast<unsigned>(s.descriptorBits[(start+i)/8]) >> (7-(start+i)%8)) & 1);
    }
    return value;
}
int main() {
    f::clear(); f::ForcedDestination selected{};
    check(!f::commit_prelaunch_authored_selection(282,282,selected), "empty selection committed");
    check(f::publish(f::profiles::kGatewayOpening), "Gateway preset rejected");
    check(!f::override_active(), "Gateway activated before native selection");
    auto donor = descriptor(282,"mission_red_legion");
    check(!f::apply(donor) && donor.activityIndex == 282, "Gateway used late donor rewrite");
    check(!f::commit_prelaunch_authored_selection(20,282,selected), "social donor accepted");
    check(!f::commit_prelaunch_authored_selection(282,292,selected), "mixed donor accepted");
    check(f::commit_prelaunch_authored_selection(282,282,selected), "native donor rejected");
    check(p::configured(selected) == &p::kGateway && f::override_active(), "wrong committed identity");
    check(!f::apply(donor), "committed Gateway reused Chosen descriptor");
    auto route = descriptor(292,"mission_abs");
    auto wrong = route; wrong.previousActivityIndex=266;
    check(!f::apply(wrong), "foreign source accepted");
    wrong=route; wrong.hasDescriptorName=false;
    check(!f::apply(wrong), "missing captured name accepted");
    wrong=route; wrong.descriptorNameBit=610;
    check(!f::apply(wrong), "truncated captured name accepted");
    check(f::apply(route), "native Gateway route rejected");
    check(route.activityIndex==292 && route.previousActivityIndex==292, "Gateway indices drifted");
    check(route.arrivalBubbleOverride==15 && route.sliceSetOverride==120, "Gateway region wrong");
    check(route.spawnSetOverride==0x69F52B3EU && route.hasSpawnSetOverride, "Gateway opening spawn lost");
    check(route.descriptorBitLength==620 && bits(route,4,12)==293 && bits(route,16,12)==293,
        "captured wire indices drifted");
    check(route.descriptorBits[60]==std::byte{0xA5}, "opaque authored tail changed");
    check(!f::mission_host_reestablishment_enabled(), "Gateway inherited unverified authority");
    check(f::publish(f::profiles::kGatewayOpening) && f::override_active(), "unchanged publish cleared commit");
    selected = f::profiles::kGatewayOpening; selected.hasSpawnSetHash=true; selected.spawnSetHash=123;
    check(f::publish(selected) && !f::override_active(), "edited profile retained stale commit");
    check(f::publish(f::profiles::kTowerfallOpening) && !f::override_active(), "Towerfall staging changed");
    check(f::commit_homecoming_authored_selection(282,282), "Towerfall native commit regressed");
    auto tower = descriptor(266,"mission_towerfall");
    check(f::apply(tower) && tower.activityIndex==266 && tower.arrivalBubbleOverride==9
        && tower.sliceSetOverride==72, "Towerfall opening regressed");
    f::clear(); check(f::publish(f::profiles::kTowerfallOpening), "Towerfall preset rejected");
    auto legacy = descriptor(282,"mission_red_legion");
    check(f::apply(legacy) && legacy.previousActivityIndex==282 && legacy.activityIndex==266,
        "Towerfall legacy authored fallback regressed");
    auto omega=f::profiles::kGatewayOpening; omega.packageName={};
    std::memcpy(omega.packageName.data(),"mission_scot",12); omega.packageNameLength=12;
    check(f::publish(omega) && f::override_active(), "Omega incorrectly staged");
    check(p::configured(omega)==nullptr, "Omega incorrectly enrolled in donor bootstrap");
    auto omegaRoute=descriptor(299,"mission_scot");
    check(f::apply(omegaRoute) && omegaRoute.activityIndex==299, "Omega route regressed");
    check(f::suspend_omega_for_completed_run(17) && !f::override_active(), "Omega ending suspension regressed");
    f::clear(); check(!f::override_active(), "clear retained commit");
    std::puts("Mission prelaunch: Gateway staging/wire contract, Towerfall compatibility, Omega isolation passed.");
}
