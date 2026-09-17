// Exercise the real type-6 handler, including admission and connection-local observation.
// These stubs replace only external settings, destination lookup, and diagnostic sinks.
// The production parser, latch, body writer, and handler are compiled unchanged.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef OMEGA_TEST_ROUTE_SOURCE
#define OMEGA_TEST_ROUTE_SOURCE "../src/server/bap/encrypted/activity_message/activity_message_route.cpp"
#endif
#include OMEGA_TEST_ROUTE_SOURCE
#include "middleware/encoding/bit_reader.h"
#include "fixtures/omega_native_sense_captures.h"

namespace route_fixture {
namespace bap = dawn::server::bap;
namespace message = dawn::middleware::bap::activity_message;
namespace wire = message::sensor_auth_update;
namespace bits = dawn::middleware::encoding::bits;
constexpr dawn::state::activity::ActivityInstanceKey kInstance{0x9EAA300100200001ULL, {1}};
dawn::core::settings::Settings settings{};
std::string_view destination = "mission_scot";
bool destinationPresent = true;
std::vector<std::string> logs;
std::string lastValidation;
bool lastParsed{};
bool lastSceneDecoded{};
std::uint32_t lastSceneRevision{};
unsigned checks{};

void check(bool pass, const char* why) {
    ++checks;
    if (!pass) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
std::vector<std::byte> hex(std::string_view value) {
    check((value.size() & 1U) == 0, "captured packet has whole bytes");
    const auto digit = [](char c) { return static_cast<unsigned>(c <= '9' ? c-'0' : c-'A'+10); };
    std::vector<std::byte> result(value.size()/2);
    for (std::size_t i=0; i<result.size(); ++i)
        result[i]=static_cast<std::byte>(16U*digit(value[2*i])+digit(value[2*i+1]));
    return result;
}
// Unmodified packets 1, 11, 13 and 14 from the 756BC95B live run.
const auto roster = hex(
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF701A3C3607014EBD814C17DAC5BC0000001C000000000000000000000000000000000000000000000000000000020383838383C0000005E089D8FB25FA0C68D9E0000000600000000000000010507078000000EC1D1C94638CAFD9700FA68705940000001C00000000000000020E0E0E0F0000001F83BA5F26EFD00142CFF7A6CE7F80000003800000000000000041C1C1C1EE97C9BBC0000053B74BE4DDE620006BFFFFFFFCFE000002FFFFFFFEBFFFFFFF80000000EE97C9BBE3C00141000200000002E800A1678000012F740050B3C1400040498C0000000000000007A002859EB20007000000012000000000F40050B3CC400857FFFFFFF9FC000005FFFFFFFD7FFFFFFF00000001E800A167C780118200040000000400");
const auto opening = hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3A002859E00000139D00142CF3F0029E00000006000000000000000400");
const auto release = hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3A002859E00000149D00142CF59000380EC0F96205E4AAA9400000000C00");
const auto retained = hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3A002859E00000189D00142CF59000380EC0F96209E4AAA941566321AC0000001000");
const auto latestRoster = hex(dawn::unit::fixtures::omega_native_sense_capture(1));
const auto latestOpening = hex(dawn::unit::fixtures::omega_native_sense_capture(9));
const auto latestRelease = hex(dawn::unit::fixtures::omega_native_sense_capture(13));
const auto latestRetained = hex(dawn::unit::fixtures::omega_native_sense_capture(29));

void deliver(bap::Session& session, std::span<const std::byte> payload,
             std::uint64_t account = kInstance.sessionId) {
    const message::Request request{account, 6, 0, payload};
    bap::encrypted::activity_message::report_sense_update(session, request);
}
std::unique_ptr<bap::Session> connection(bool acknowledge=true, bool enter=true, bool latest=false) {
    destination = "mission_scot";
    destinationPresent = true;
    auto session = std::make_unique<bap::Session>();
    session->authenticated = true;
    session->activity.instance = kInstance;
    session->activityPatchEpoch = {UINT64_MAX, UINT64_MAX};
    session->activityPatchEpochSeen = true;
    if (acknowledge) deliver(*session, latest ? latestRoster : roster);
    if (enter) deliver(*session, latest ? latestOpening : opening);
    check(session->activity.sensorObservation.omegaRosterReady == acknowledge,
          "actual roster handler establishes exact native readiness");
    check(session->activity.sensorObservation.omegaOpeningTriggered == (acknowledge && enter),
          "actual opening handler enforces roster-before-opening ordering");
    session->activity.keepaliveDueTick = 999;
    logs.clear();
    return session;
}
unsigned releases() {
    unsigned count = 0;
    for (const auto& line : logs)
        if (line.find("stage=host_release") != std::string::npos) ++count;
    return count;
}
void unchanged(const bap::Session& session, const char* why) {
    check(!session.activity.sensorObservation.omegaIkoraLattice.released, why);
    check(session.activity.keepaliveDueTick == 999, "rejected receipt cannot wake authority publisher");
    check(releases() == 0, "rejected receipt cannot announce host release");
}
void native_wire(const bap::Session& session, bool released) {
    // Publisher projection is independently audited/tested elsewhere. Here use its observation
    // inputs with the real writer to prove the accepted receipt yields native gate/carrier state.
    wire::Snapshot snapshot{};
    snapshot.seedAuthoredSensors = snapshot.omegaSceneAuthority = true;
    snapshot.omegaIkoraPortalRequested = session.activity.sensorObservation.omegaOpeningTriggered;
    snapshot.omegaIkoraLatticeReleased = session.activity.sensorObservation.omegaIkoraLattice.released;
    snapshot.omegaPortalEntry = snapshot.omegaIkoraLatticeReleased;
    std::array<std::byte, 20> gate{};
    bits::Writer writer(gate);
    check(wire::write_auth_body(writer, snapshot, 0xD00142CF, 23, 16, false), "native lattice gate body encodes");
    bits::Reader reader(gate);
    std::uint64_t position{}, revision{}, snap{};
    check(reader.read(32, position) && reader.read(16, revision) && reader.read(1, snap),
          "native lattice gate prefix independently decodes");
    check(position == (released ? 0U : 0x3F800000U), "release changes native lattice position from one to zero");
    check(revision == (released ? 0x8002U : 0x8001U), "release advances native gate revision");
    check(snap == static_cast<unsigned>(!released), "native dissolve remains a smooth position update");
    check(wire::auth_body_bits(snapshot, 0xBA5F26EF, 4, 0, false) == (released ? 252U : 0U),
          "native contact carrier is published only after accepted lattice release");
}
void live_route(bool latest=false) {
    check(!settings.omegaExperiments.syntheticStageMachine && !settings.omegaExperiments.sceneAuthority,
          "regression reproduces both disabled live experimental settings");
    auto session = connection(true, true, latest);
    native_wire(*session, false);
    deliver(*session, latest ? latestRelease : release);
    check(lastParsed && lastValidation == "ok" && lastSceneDecoded && lastSceneRevision == 3,
          "actual handler receives decoded native Scene output without projection loss");
    check(session->activity.sensorObservation.omegaIkoraLattice.released,
          "actual production route releases lattice with both legacy settings disabled");
    check(session->activity.keepaliveDueTick == 0, "accepted receipt wakes normal authority publication");
    check(releases() == 1, "accepted native release is announced exactly once");
    native_wire(*session, true);
    session->activity.keepaliveDueTick = 999;
    deliver(*session, latest ? latestRelease : release);
    deliver(*session, latest ? latestRetained : retained);
    check(lastSceneDecoded && lastSceneRevision == 4, "retained native event list is decoded by production route");
    check(session->activity.sensorObservation.omegaIkoraLattice.released,
          "retained native report preserves released state");
    check(session->activity.keepaliveDueTick == 999 && releases() == 1,
          "duplicate and retained events neither re-release nor force duplicate publication");
}
void admission() {
    auto session = connection();
    deliver(*session, release, kInstance.sessionId+1);
    unchanged(*session, "foreign activity handle cannot release native lattice");
    session = connection();
    session->activity.instance.incarnation.value = 2;
    deliver(*session, release);
    unchanged(*session, "stale incarnation cannot select destination or release lattice");
    session = connection();
    session->activityPatchEpochSeen = false;
    deliver(*session, release);
    unchanged(*session, "missing patch epoch cannot release native lattice");
    session = connection();
    session->activityPatchEpoch.first = 7;
    deliver(*session, release);
    unchanged(*session, "wrong patch epoch cannot release native lattice");
    session = connection();
    destination = "mission_other";
    deliver(*session, release);
    unchanged(*session, "other destination cannot release native lattice");
    session = connection();
    destinationPresent = false;
    deliver(*session, release);
    unchanged(*session, "missing destination cannot release native lattice");
    session = connection(false, true);
    deliver(*session, release);
    unchanged(*session, "native release before exact roster acknowledgement is rejected");
    session = connection(true, false);
    deliver(*session, release);
    unchanged(*session, "native release before opening volume entry is rejected");
    for (std::size_t bytes=0; bytes<release.size(); ++bytes) {
        session = connection();
        deliver(*session, std::span(release).first(bytes));
        unchanged(*session, "every truncated captured release is rejected atomically");
    }
}
} // namespace route_fixture

namespace dawn::core::settings {
const Settings& get() noexcept { return route_fixture::settings; }
}
namespace dawn::core::log {
bool accepts(Channel, Level) noexcept { return true; }
void write(Channel, Level, std::string_view event) noexcept { route_fixture::logs.emplace_back(event); }
}
namespace dawn::state::activity::destination {
bool snapshot(ActivityInstanceKey key, DestinationSelection& output) noexcept {
    output = {};
    if (!route_fixture::destinationPresent || key != route_fixture::kInstance) return false;
    const auto name = route_fixture::destination;
    std::memcpy(output.packageName.data(), name.data(), name.size());
    output.packageNameLength = static_cast<std::uint8_t>(name.size());
    return true;
}
}
namespace dawn::state::build_data::scenarios {
void export_cue_observation_mapping(std::string_view,
    const middleware::bap::activity_message::sense_update::SenseUpdate&) noexcept {}
}
namespace dawn::server::bap::encrypted::diagnostics::omega_trace {
void record_sense(std::uint64_t, std::uint64_t, std::uint64_t, std::string_view validation,
    std::string_view, std::string_view, bool, bool, bool parsed,
    const message::sense_update::SenseUpdate& update, std::span<const std::byte>) noexcept {
    route_fixture::lastValidation = validation;
    route_fixture::lastParsed = parsed;
    route_fixture::lastSceneDecoded = false;
    route_fixture::lastSceneRevision = 0;
    for (std::size_t i=0; i<update.objectCount; ++i) {
        const auto& object = update.objects[i];
        if (object.registryKey == 0xD00142CF && object.slotType == 43 && object.slotIndex == 1) {
            route_fixture::lastSceneDecoded = object.hasSceneOutput;
            route_fixture::lastSceneRevision = object.sceneOutput.revision;
        }
    }
}
}
int main() {
    route_fixture::live_route();
    route_fixture::live_route(true);
    route_fixture::admission();
    std::printf("PASS: %u production lattice route checks\n", route_fixture::checks);
}
