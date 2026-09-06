#include <array>
#include <cstdio>
#include <cstdlib>

#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"

namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits = sunrise::middleware::encoding::bits;

void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main() {
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
    std::puts("PASS: Tower Watch dialogue, directive, Scene and Omega isolation");
}
