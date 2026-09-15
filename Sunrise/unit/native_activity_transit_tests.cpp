#include "server/runtime/activity/native_activity_transit.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace transit = sunrise::server::runtime::activity::native_activity_transit;
namespace service = sunrise::server::runtime::activity::membership_transit;
namespace membership = sunrise::state::activity::membership;
namespace activity = sunrise::state::activity;

namespace {

unsigned checks{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "failed check %u: %s\n", checks, message);
        std::exit(1);
    }
}

constexpr activity::ActivityInstanceKey kOwner{0x7101, {41}};
constexpr std::uint64_t kBoot = 0x8101;
constexpr std::uint64_t kMemberOne = 0x9101;
constexpr std::uint64_t kMemberTwo = 0x9102;
constexpr std::uint64_t kLateMember = 0x9103;
constexpr service::Destination kFirst{101, 120, 0x5726AC0A};
constexpr service::Destination kSecond{202, 64, 0x2EA8FB98};

service::Receipt idle(std::uint64_t member, std::uint8_t token) {
    return {kOwner, kBoot, 0, member,
            {0, token, membership::kAbsentSliceSetIndex, 0},
            membership::kAbsentRegionIndex};
}

service::Projection observe(std::uint64_t member, bool receipt,
                            service::Destination target, std::uint8_t token,
                            std::int8_t state, std::int32_t region) {
    return transit::project(kOwner, member, receipt,
                            {state, token, target.region, target.spawn}, region);
}

void release_member(std::uint64_t member, const service::Destination& target,
                    std::uint8_t token) {
    const auto result = observe(member, true, target, token, 0, target.region);
    check(result.released, "matching native zero releases member");
}

void test_bridge_contract() {
    const std::array destinations{kFirst, kSecond};
    check(transit::bind(kOwner, kBoot, destinations), "bind copies authored destinations");
    check(transit::bind(kOwner, kBoot, destinations), "exact bind is idempotent");
    const std::array conflicting{kSecond, kFirst};
    check(!transit::bind(kOwner, kBoot, conflicting), "conflicting destination order fails");
    check(!transit::bind(kOwner, kBoot + 1, destinations), "conflicting boot fails");
    check(!transit::request_all(kOwner, kBoot + 1, 1, kFirst.id), "wrong boot request fails");
    check(!transit::request_all(kOwner, kBoot, 1, kFirst.id), "empty cohort fails");
    check(transit::membership_due(kOwner, 0), "bound service discovers initial membership");
    transit::note_membership_published(kOwner, 1000);
    check(!transit::membership_due(kOwner, 1249), "bootstrap refresh respects cadence");
    check(transit::membership_due(kOwner, 1250), "bootstrap retries without a cohort");

    check(!transit::project(kOwner, kMemberOne, false,
                            {0, 7, membership::kAbsentSliceSetIndex, 0},
                            kFirst.region).present,
        "absent receipt does not seed a member");
    check(!transit::project(kOwner, kMemberOne, true,
                            {0, 7, membership::kAbsentSliceSetIndex, 0},
                            membership::kAbsentRegionIndex).present,
        "absent region does not seed a member");
    check(!transit::request_all(kOwner, kBoot, 1, kFirst.id),
        "absent receipt remains outside the participant set");

    check(!transit::project(kOwner, kMemberOne, true,
                            {3, 7, kFirst.region, kFirst.spawn}, kFirst.region).present,
        "invalid new local three is ignored");
    check(transit::project(kOwner, kMemberOne, true,
                           {0, 7, membership::kAbsentSliceSetIndex, 0}, kFirst.region).phase
            == service::Phase::idle,
        "valid idle receipt records a participant");
    check(!transit::membership_due(kOwner, 1500), "known idle member ends bootstrap refresh");
    check(transit::request_all(kOwner, kBoot, 1, kFirst.id), "one-member cohort requests");
    check(transit::request_all(kOwner, kBoot, 1, kFirst.id),
        "exact cohort/destination retry is idempotent");
    check(!transit::request_all(kOwner, kBoot, 1, kSecond.id),
        "same cohort with different destination is stale");
    check(!transit::request_all(kOwner, kBoot, 0, kFirst.id), "zero cohort id fails");

    auto status = transit::snapshot(kOwner, kBoot, 1);
    check(status.bound && status.requested && status.members == 1
            && !status.arrived && !status.released,
        "requested status freezes one member");
    check(transit::membership_due(kOwner, 0), "request immediately prompts membership");
    transit::note_membership_published(kOwner, 1000);
    check(!transit::membership_due(kOwner, 1000), "publication cancels immediate due");
    check(!transit::membership_due(kOwner, 1249), "active cohort waits for transport cadence");
    check(transit::membership_due(kOwner, 1250), "active cohort repeats at 250ms cadence");

    const auto requesting = transit::project(kOwner, kMemberOne, true,
        {service::kRequestingState, 8, kFirst.region, kFirst.spawn}, kFirst.region);
    check(requesting.host.state == service::kRequestingState && requesting.requestId == 1,
        "request projects cohort id and next token");
    const auto wrongRegion = transit::project(kOwner, kMemberOne, true,
        {service::kArrivedState, 8, kFirst.region, kFirst.spawn}, kSecond.region);
    check(wrongRegion.phase == service::Phase::requesting,
        "wrong region cannot arrive");
    const auto arrived = transit::project(kOwner, kMemberOne, true,
        {service::kArrivedState, 8, kFirst.region, kFirst.spawn}, kFirst.region);
    check(arrived.phase == service::Phase::arrived && arrived.host.state == service::kArrivedState,
        "exact local three and region arrive");
    status = transit::snapshot(kOwner, kBoot, 1);
    check(status.arrived && !status.released, "snapshot reports all frozen members arrived");
    transit::note_membership_published(kOwner, 2000);
    check(!transit::membership_due(kOwner, 2000), "arrival publication cancels immediate due");
    check(transit::membership_due(kOwner, 2250), "arrived host repeats on cadence");

    release_member(kMemberOne, kFirst, 8);
    status = transit::snapshot(kOwner, kBoot, 1);
    check(!status.arrived && status.released && status.members == 1,
        "snapshot retains last cohort after native zero");
    check(transit::membership_due(kOwner, 2300), "release prompts final zero publication");
    transit::note_membership_published(kOwner, 2300);
    check(!transit::membership_due(kOwner, 999999), "final zero publication cancels forever-repeat");
    const auto retained = transit::project(kOwner, kMemberOne, false, {}, -1);
    check(retained.present && retained.released && retained.host.state == 0,
        "released projection is retained without a new receipt");
}

void test_cohort_freeze_and_atomicity() {
    transit::release(kOwner, kBoot);
    check(transit::bind(kOwner, kBoot, std::array{ kFirst, kSecond }), "rebind after release");
    check(transit::project(kOwner, kMemberOne, true,
                           {0, 9, membership::kAbsentSliceSetIndex, 0}, kFirst.region).phase
            == service::Phase::idle,
        "first member seeds second run");
    check(transit::project(kOwner, kMemberTwo, true,
                           {0, 4, membership::kAbsentSliceSetIndex, 0}, kFirst.region).phase
            == service::Phase::idle,
        "second member seeds second run");
    check(transit::request_all(kOwner, kBoot, 10, kSecond.id), "two-member cohort requests");
    auto status = transit::snapshot(kOwner, kBoot, 10);
    check(status.members == 2 && !status.arrived, "two members are frozen in cohort");

    auto first = transit::project(kOwner, kMemberOne, true,
        {service::kArrivedState, 10, kSecond.region, kSecond.spawn}, kSecond.region);
    check(first.phase == service::Phase::arrived, "first member arrives");
    check(!transit::snapshot(kOwner, kBoot, 10).arrived,
        "one of two members cannot satisfy all-arrived");
    auto second = transit::project(kOwner, kMemberTwo, true,
        {service::kArrivedState, 5, kSecond.region, kSecond.spawn}, kSecond.region);
    check(second.phase == service::Phase::arrived,
        "second member arrives with its own token");
    check(transit::snapshot(kOwner, kBoot, 10).arrived,
        "both exact members satisfy all-arrived");

    check(transit::project(kOwner, kLateMember, true,
                           {0, 3, membership::kAbsentSliceSetIndex, 0}, kFirst.region).phase
            == service::Phase::idle,
        "late member is observed after cohort freeze");
    status = transit::snapshot(kOwner, kBoot, 10);
    check(status.members == 2 && status.arrived,
        "late member does not falsely satisfy existing cohort");

    check(!transit::request_all(kOwner, kBoot, 11, kFirst.id),
        "active existing members block next cohort");
    release_member(kMemberOne, kSecond, 10);
    release_member(kMemberTwo, kSecond, 5);
    check(transit::request_all(kOwner, kBoot, 12, kFirst.id),
        "released members plus late member form next cohort");
    status = transit::snapshot(kOwner, kBoot, 12);
    check(status.members == 3 && status.requested, "next cohort includes late member");
    check(!transit::request_all(kOwner, kBoot, 11, kFirst.id), "stale cohort id rejected");
    check(!transit::request_all(kOwner, kBoot, 12, kSecond.id), "changed duplicate is rejected");

    transit::release(kOwner, kBoot);
}

void test_initial_optional_tuple() {
    check(transit::bind(kOwner, kBoot, std::array{kFirst,kSecond}), "bind optional-tuple run");
    transit::Destination spawn{};
    check(!transit::spawn_destination(kOwner,spawn), "opening spawn retained before any transit request");
    static_cast<void>(transit::project(kOwner, kMemberOne, false, {}, -1));
    check(!transit::request_all(kOwner, kBoot, 1, kFirst.id),
        "unreported region cannot enroll an idle default");
    static_cast<void>(transit::project(kOwner, kMemberOne, false,
        {0,0,kFirst.region,kFirst.spawn}, kFirst.region));
    check(!transit::request_all(kOwner, kBoot, 1, kFirst.id),
        "unreported nondefault destination cannot enroll a member");
    static_cast<void>(transit::project(kOwner, kMemberOne, false, {}, kFirst.region));
    check(transit::request_all(kOwner, kBoot, 1, kFirst.id),
        "native untouched idle default can initiate first host command");
    check(transit::spawn_destination(kOwner,spawn) && spawn==kFirst,
        "accepted teleport also selects native spawn override");
    auto projection = transit::project(kOwner, kMemberOne, false,
        {3,1,kFirst.region,kFirst.spawn}, kFirst.region);
    check(projection.host.token == 1 && projection.phase == service::Phase::requesting,
        "missing receipt cannot confirm arrival");
    projection = transit::project(kOwner, kMemberOne, true,
        {3,1,kFirst.region,kFirst.spawn}, kFirst.region);
    check(projection.arrived, "matching explicit receipt confirms arrival");
    projection = transit::project(kOwner, kMemberOne, false, {}, kFirst.region);
    check(projection.arrived && !projection.released,
        "later absent default cannot release a transaction");
    release_member(kMemberOne, kFirst, 1);
    check(transit::spawn_destination(kOwner,spawn) && spawn==kFirst,
        "completed teleport retains destination for respawns");
    check(transit::request_all(kOwner,kBoot,2,kSecond.id)
        && transit::spawn_destination(kOwner,spawn) && spawn==kSecond,
        "return transit replaces prior encounter spawn");
    transit::release(kOwner, kBoot);
    check(!transit::spawn_destination(kOwner,spawn), "released owner cannot retain spawn override");
}

// The respawn latch is now written by a qualified arrival and cleared on the next travel-prepare
// edge; destination_bound is the read-only validation the transit pulse keeps in its place.
void test_respawn_latch_lifetime() {
    constexpr activity::ActivityInstanceKey owner{0x7102, {42}};
    constexpr std::uint64_t boot = 0x8102;
    const std::array destinations{kFirst, kSecond};
    check(transit::bind(owner, boot, destinations), "latch owner binds");
    check(transit::destination_bound(owner, boot, kFirst.id), "authored id is bound");
    check(!transit::destination_bound(owner, boot, 999U), "unknown id is not bound");
    check(!transit::destination_bound(owner, boot + 1, kFirst.id), "wrong boot is not bound");
    check(!transit::destination_bound(owner, boot, 0U), "absent id is not bound");
    check(!transit::respawn_latched(owner), "nothing is latched before arrival");
    check(transit::set_respawn_destination(owner, boot, kSecond.id), "arrival latches");
    check(transit::respawn_latched(owner), "the latch is visible");
    service::Destination spawn{};
    check(transit::spawn_destination(owner, spawn) && spawn == kSecond,
          "the latched set is retained for respawns");
    transit::clear_respawn_destination(owner, boot);
    check(!transit::respawn_latched(owner), "the prepare edge clears the latch");
    check(!transit::spawn_destination(owner, spawn),
          "a cleared latch retains no spawn override");
    transit::release(owner, boot);
}

} // namespace

int main() {
    test_bridge_contract();
    test_cohort_freeze_and_atomicity();
    test_initial_optional_tuple();
    test_respawn_latch_lifetime();
    std::printf("native activity transit: %u checks, zero failures\n", checks);
}
