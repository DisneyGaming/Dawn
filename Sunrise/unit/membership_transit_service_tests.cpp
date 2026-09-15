#include "server/runtime/activity/membership_transit_service.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace transit = sunrise::server::runtime::activity::membership_transit;
namespace activity = sunrise::state::activity;
namespace membership = sunrise::state::activity::membership;
namespace destination = sunrise::state::activity::destination;

namespace {

unsigned checks{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "failed check %u: %s\n", checks, message);
        std::exit(1);
    }
}

constexpr activity::ActivityInstanceKey kOwner{0x1001, {7}};
constexpr std::uint64_t kBoot = 0x2002;
constexpr std::uint64_t kMemberOne = 0x3003;
constexpr std::uint64_t kMemberTwo = 0x4004;
constexpr transit::Destination kFirst{11, 120, 0x5726AC0A};
constexpr transit::Destination kSecond{22, 64, 0x2EA8FB98};

transit::Receipt idle(std::uint64_t member, std::uint8_t token) {
    return {kOwner, kBoot, 0, member, {0, token, membership::kAbsentSliceSetIndex, 0}, -1};
}

transit::Command command(std::uint64_t request, std::uint32_t destination,
    std::uint64_t member, std::uint64_t revision) {
    return {kOwner, kBoot, revision, request, destination, member};
}

transit::Receipt receipt(std::uint64_t request, std::uint64_t member,
    membership::TeleportState local, std::int32_t actualRegion) {
    return {kOwner, kBoot, request, member, local, actualRegion};
}

void handshake() {
    transit::Service service;
    const std::array destinations{kFirst, kSecond};
    check(service.begin(kOwner, kBoot, destinations), "begin immutable authored definition");
    check(!service.begin(kOwner, kBoot, destinations), "live service cannot rebind");

    auto first = command(1, kFirst.id, kMemberOne, service.revision());
    check(service.request(first) == transit::RequestResult::not_ready,
        "request waits for native idle receipt");
    check(service.observe(idle(kMemberOne, 255)) == transit::ObservationResult::observed,
        "idle receipt seeds member token");
    first.expectedRevision = service.revision();
    check(service.request(first) == transit::RequestResult::accepted,
        "request publishes after idle receipt");
    auto view = service.project(kOwner, kBoot, kMemberOne);
    check(view.present && view.host.state == transit::kRequestingState && view.host.token == 1
            && view.host.sliceSetIndex == kFirst.region && view.host.sliceSetHash == kFirst.spawn,
        "255 token wraps to nonzero token 1 and publishes target");
    auto changed = first;
    changed.expectedRevision = service.revision();
    changed.destinationId = kSecond.id;
    check(service.request(changed) == transit::RequestResult::stale
            && service.revision() == 2,
        "same request identity with a changed destination is stale");
    view = service.project(kOwner, kBoot, kMemberOne);
    check(view.requestId == first.requestId && view.destination.id == kFirst.id
            && view.phase == transit::Phase::requesting,
        "changed duplicate does not mutate the retained request");
    check(service.request(first) == transit::RequestResult::duplicate
            && service.revision() == 2,
        "identical retry does not advance revision or token");

    auto wrong = receipt(first.requestId, kMemberOne,
        {transit::kArrivedState, static_cast<std::uint8_t>(view.host.token + 1U), kFirst.region,
            kFirst.spawn},
        kFirst.region);
    check(service.observe(wrong) == transit::ObservationResult::ignored,
        "wrong token cannot arrive");
    wrong = receipt(first.requestId, kMemberOne,
        {transit::kArrivedState, view.host.token, kSecond.region, kSecond.spawn}, kSecond.region);
    check(service.observe(wrong) == transit::ObservationResult::ignored,
        "wrong target cannot arrive");
    wrong = receipt(first.requestId, kMemberTwo,
        {transit::kArrivedState, view.host.token, kFirst.region, kFirst.spawn}, kFirst.region);
    check(service.observe(wrong) == transit::ObservationResult::ignored,
        "wrong member cannot arrive");
    wrong = receipt(first.requestId, kMemberOne,
        {transit::kArrivedState, view.host.token, kFirst.region, kFirst.spawn}, kSecond.region);
    check(service.observe(wrong) == transit::ObservationResult::observed,
        "wrong actual region leaves request pending");
    check(service.observe(receipt(first.requestId, kMemberOne,
        {transit::kArrivedState, view.host.token, kFirst.region, kFirst.spawn}, kFirst.region))
            == transit::ObservationResult::arrived,
        "exact local3 and actual region produce one arrival edge");
    view = service.project(kOwner, kBoot, kMemberOne);
    check(view.host.state == transit::kArrivedState && view.arrived && !view.released,
        "host3 is projected after arrival");
    check(service.observe(receipt(first.requestId, kMemberOne,
        {transit::kArrivedState, view.host.token, kFirst.region, kFirst.spawn}, kFirst.region))
            == transit::ObservationResult::duplicate,
        "duplicate arrival receipt is one-shot");
    check(service.observe(receipt(first.requestId, kMemberOne,
        {0, view.host.token, kFirst.region, kFirst.spawn}, kFirst.region))
            == transit::ObservationResult::released,
        "matching local0 releases host");
    view = service.project(kOwner, kBoot, kMemberOne);
    check(view.host.state == 0 && view.released, "host0 release remains projected");
    check(service.observe(receipt(first.requestId, kMemberOne,
        {0, view.host.token, kFirst.region, kFirst.spawn}, kFirst.region))
            == transit::ObservationResult::duplicate,
        "duplicate release receipt is one-shot");
}

void independent_members_and_round() {
    transit::Service service;
    const std::array destinations{kFirst, kSecond};
    check(service.begin(kOwner, kBoot, destinations), "independent service begins");
    check(service.observe(idle(kMemberOne, 4)) == transit::ObservationResult::observed,
        "member one idle");
    check(service.observe(idle(kMemberTwo, 8)) == transit::ObservationResult::observed,
        "member two idle");
    auto one = command(10, kFirst.id, kMemberOne, service.revision());
    check(service.request(one) == transit::RequestResult::accepted, "member one request");
    auto two = command(20, kSecond.id, kMemberTwo, service.revision());
    check(service.request(two) == transit::RequestResult::accepted, "member two request");
    auto firstView = service.project(kOwner, kBoot, kMemberOne);
    auto secondView = service.project(kOwner, kBoot, kMemberTwo);
    check(firstView.host.token == 5 && secondView.host.token == 9,
        "members receive independent tokens");
    check(service.request(command(21, kFirst.id, kMemberTwo, service.revision()))
            == transit::RequestResult::busy,
        "active member cannot be rebound");

    check(service.observe(receipt(one.requestId, kMemberOne,
        {transit::kArrivedState, firstView.host.token, kFirst.region, kFirst.spawn}, kFirst.region))
            == transit::ObservationResult::arrived,
        "member one arrives independently");
    check(service.observe(receipt(one.requestId, kMemberOne,
        {0, firstView.host.token, kFirst.region, kFirst.spawn}, kFirst.region))
            == transit::ObservationResult::released,
        "member one releases independently");

    const auto oldAck = receipt(one.requestId, kMemberOne,
        {transit::kArrivedState, firstView.host.token, kFirst.region, kFirst.spawn}, kFirst.region);
    auto next = command(11, kSecond.id, kMemberOne, service.revision());
    check(service.request(next) == transit::RequestResult::accepted,
        "released member can begin subsequent round");
    auto nextView = service.project(kOwner, kBoot, kMemberOne);
    check(nextView.host.token == 6 && nextView.host.sliceSetIndex == kSecond.region,
        "subsequent round advances from latest observed token");
    check(service.observe(oldAck) == transit::ObservationResult::ignored,
        "delayed old request cannot arrive later round");
    check(service.observe(receipt(next.requestId, kMemberOne,
        {transit::kArrivedState, nextView.host.token, kSecond.region, kSecond.spawn}, kSecond.region))
            == transit::ObservationResult::arrived,
        "new request has independent arrival identity");
    check(service.observe(receipt(next.requestId, kMemberOne,
        {0, nextView.host.token, kSecond.region, kSecond.spawn}, kSecond.region))
            == transit::ObservationResult::released,
        "new request releases independently");
    const auto revisionAfterRelease = service.revision();
    const auto priorRound = command(one.requestId, kFirst.id, kMemberOne, revisionAfterRelease);
    check(service.request(priorRound) == transit::RequestResult::stale
            && service.revision() == revisionAfterRelease,
        "prior-round request is stale after later round release");
    const auto afterStale = service.project(kOwner, kBoot, kMemberOne);
    check(afterStale.requestId == next.requestId && afterStale.destination.id == kSecond.id
            && afterStale.phase == transit::Phase::released,
        "prior-round request does not mutate released state");
}

void validation_and_scope() {
    const std::array destinations{kFirst, kSecond};
    transit::Service service;
    check(!service.begin({}, kBoot, destinations), "absent owner rejected");
    check(!service.begin(kOwner, 0, destinations), "zero boot rejected");
    check(!service.begin(kOwner, kBoot, std::span<const transit::Destination>{}),
        "empty definition rejected");
    check(!transit::Service::valid(transit::Destination{0, 1, 1}), "zero id rejected");
    check(!transit::Service::valid(transit::Destination{1, -1, 1}), "absent region rejected");
    check(!transit::Service::valid(transit::Destination{1, membership::kMaximumRegionIndex + 1, 1}),
        "region above native range rejected");
    check(!transit::Service::valid(transit::Destination{1, 1, 0}), "zero spawn rejected");
    check(!transit::Service::valid(transit::Destination{1, 1, destination::kAbsentSpawnSetHash}),
        "native absent spawn sentinel rejected");
    check(!transit::Service::valid(transit::Destination{1, 1,
        (std::numeric_limits<std::uint32_t>::max)()}), "all-one spawn sentinel rejected");
    const std::array duplicate{kFirst, transit::Destination{kFirst.id, 1, 2}};
    check(!transit::Service::valid(duplicate), "duplicate destination id rejected");

    std::array<transit::Destination, transit::kDestinationCapacity> many{};
    for (std::size_t index = 0; index < many.size(); ++index) {
        many[index] = {static_cast<std::uint32_t>(index + 1), 1,
            static_cast<std::uint32_t>(index + 2)};
    }
    check(transit::Service::valid(many), "bounded destination capacity accepted");
    many[many.size() - 1].id = many[0].id;
    check(!transit::Service::valid(many), "duplicate at bounded capacity rejected");
    std::array<transit::Destination, transit::kDestinationCapacity + 1> tooMany{};
    for (std::size_t index = 0; index < tooMany.size(); ++index) {
        tooMany[index] = {static_cast<std::uint32_t>(index + 1), 1,
            static_cast<std::uint32_t>(index + 2)};
    }
    check(!transit::Service::valid(tooMany), "definition over capacity rejected");

    transit::Service memberCapacity;
    check(memberCapacity.begin(kOwner, kBoot, destinations), "member capacity service begins");
    for (std::size_t index = 0; index < transit::kMemberCapacity; ++index) {
        check(memberCapacity.observe(idle(0x5000 + index, static_cast<std::uint8_t>(index)))
                == transit::ObservationResult::observed,
            "member within capacity receives idle receipt");
    }
    check(memberCapacity.observe(idle(0x5000 + transit::kMemberCapacity, 0))
            == transit::ObservationResult::capacity,
        "member over capacity is rejected");

    check(service.begin(kOwner, kBoot, destinations), "scope test begins");
    check(service.observe(idle(kMemberOne, 1)) == transit::ObservationResult::observed,
        "scope test idle");
    auto request = command(31, kFirst.id, kMemberOne, service.revision());
    auto wrongOwner = request;
    wrongOwner.owner.sessionId++;
    check(service.request(wrongOwner) == transit::RequestResult::stale, "wrong owner rejected");
    auto wrongBoot = request;
    wrongBoot.boot++;
    check(service.request(wrongBoot) == transit::RequestResult::stale, "wrong boot rejected");
    auto wrongRevision = request;
    wrongRevision.expectedRevision++;
    check(service.request(wrongRevision) == transit::RequestResult::stale,
        "wrong revision rejected");
    check(!service.project(wrongOwner.owner, kBoot, kMemberOne).present,
        "foreign projection is absent");
    check(service.cancel(wrongOwner.owner, kBoot) == false, "foreign cancel rejected");
    check(service.cancel(kOwner, kBoot), "exact cancel clears owner");
    check(!service.owner() && service.revision() == 0, "cancel clears without arrival");
}

} // namespace

int main() {
    handshake();
    independent_members_and_round();
    validation_and_scope();
    std::printf("membership transit service: %u checks, zero failures\n", checks);
}
