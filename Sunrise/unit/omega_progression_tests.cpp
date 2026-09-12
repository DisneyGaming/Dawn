#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "state/activity/omega/omega_progression.h"
#include "state/build_data/scenarios/omega_route_groups.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include "client/hooks/bootflow/omega_presentation.h"
#include "client/hooks/bootflow/omega_boss_spawn.h"
#include "client/hooks/bootflow/omega_animation_request.h"
#include "state/activity/omega/omega_forest_encounters.h"
#include "state/activity/omega/omega_portal_entry.h"
#include "state/activity/omega/omega_boss_authority.h"
#include "client/hooks/bootflow/omega_enemy_forest_receipts.h"
#include "server/bap/encrypted/activity_message/omega_opening_ack.h"
#include "fixtures/omega_reveal_acknowledgement.h"
#include "fixtures/omega_reveal_runtime_headers.h"

namespace omega = sunrise::state::activity::omega;
namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits = sunrise::middleware::encoding::bits;

void check(bool passed, const char* message) {
    if (!passed) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
std::uint64_t field(bits::Reader& reader, std::uint8_t width) {
    std::uint64_t value = 0;
    check(reader.read(width, value), "field fits");
    return value;
}

void progression() {
    check(omega::dialogue_source(0x80F47BDA, 0x1408), "live Omega dialogue source is accepted");
    check(!omega::dialogue_source(0x80F47BDA, 0x1300)
          && !omega::dialogue_source(0x80F47BD9, 0x1408), "unrelated components cannot acknowledge cues");
    omega::Progression tracker;
    auto step = [&](std::uint64_t now, bool gate, omega::Point point,
                    bool arrived = true, bool present = true) {
        return tracker.update(1, now, gate, arrived, present, point);
    };
    check(step(1, false, omega::kForestEntrance).route == omega::Route::opening,
          "position alone cannot start the mission");
    check(step(2, true, {}).route == omega::Route::forestEntrance, "gate targets entrance");
    auto p = step(3, true, omega::kForestEntrance);
    check(p.route == omega::Route::forestExit && !p.vistaDialogue, "checkpoint waits for tunnel dispatch");
    check(!step(100000, true, omega::kForestExit).vistaDialogue, "elapsed time cannot fake dispatch");
    tracker.dispatched(1, 6, 100000);
    check(!step(109099, true, omega::kForestExit).vistaDialogue, "tunnel cue gets full duration");
    tracker.dispatched(1, 6, 108000);
    p = step(109100, true, omega::kForestExit);
    check(p.vistaDialogue && !p.exitDialogue && p.route == omega::Route::crownEntrance,
          "fast traversal queues vista first; duplicate dispatch does not postpone it");
    tracker.dispatched(1, 7, 109200);
    check(!step(121039, true, omega::kCrownEntrance).exitDialogue, "vista gets full two-clip duration");
    p = step(121040, true, omega::kCrownEntrance);
    check(p.route == omega::Route::reveal && p.exitDialogue, "route stops at reveal with exit cue");
    p = step(130000, false, omega::kForestEntrance, false);
    check(p.route == omega::Route::reveal && p.vistaDialogue && p.exitDialogue,
          "transition and backtracking retain progression");
    p = tracker.update(2, 140000, false, true, true, omega::kCrownEntrance);
    check(p.route == omega::Route::opening && !p.vistaDialogue && !p.exitDialogue, "new run resets everything");
    p = tracker.update(2, 140001, true, true, false, omega::kForestEntrance);
    check(p.route == omega::Route::forestEntrance, "missing position cannot cross checkpoint");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    p = tracker.update(2, 140002, true, true, true, {nan, 0, 0});
    check(p.route == omega::Route::forestEntrance, "nonfinite position is ignored");
    p = tracker.update(2, 140003, true, false, true, omega::kForestEntrance);
    check(p.route == omega::Route::forestEntrance, "loading does not consume stale position");
}

void dialogue_wire() {
    for (unsigned mask = 0; mask < 8; ++mask) {
      for (unsigned lair = 0; lair < 6; ++lair) {
        wire::Snapshot snapshot{};
        snapshot.omegaSceneAuthority = snapshot.omegaDialogueArm = true;
        snapshot.omegaTunnelDialogue = (mask & 1) != 0;
        snapshot.omegaVistaDialogue = (mask & 2) != 0;
        snapshot.omegaExitDialogue = (mask & 4) != 0;
        snapshot.omegaLairDialogueRequestedMask = lair == 0 ? 0U
            : lair < 3 ? 1U << 12 : (1U << 12) | (1U << 13);
        snapshot.omegaLairDialoguePendingRow = lair == 1 ? 12U : lair == 3 ? 13U : 255U;
        // An inconsistent pending row must not create an unrequested generation or length mismatch.
        if (lair == 5) {
            snapshot.omegaLairDialogueRequestedMask = 0;
            snapshot.omegaLairDialoguePendingRow = 12;
        }
        std::array<std::byte, 4096> buffer{};
        bits::Writer writer(buffer);
        check(wire::write_auth_body(writer, snapshot, 0x82FB58B7, 53, 2, false), "type 53 encodes");
        check(writer.bit_count() == wire::auth_body_bits(snapshot, 0x82FB58B7, 53, 2, false), "type 53 measured length");
        bits::Reader reader(buffer);
        check(reader.skip(55), "root reference");
        unsigned activeCount = 0;
        for (unsigned row = 0; row < 128; ++row) {
            const bool active = row == 0 || (row == 6 && (mask & 1))
                || (row == 7 && (mask & 2)) || (row == 9 && (mask & 4))
                || (row == 12 && lair == 1) || (row == 13 && lair == 3);
            const bool requested = active || (row == 12 && lair >= 1 && lair <= 4)
                || (row == 13 && lair >= 3 && lair <= 4);
            check(field(reader, 64) == UINT64_MAX, "default dialogue time");
            check(field(reader, 1) == (active ? 1U : 0U), "optional time matches active row");
            if (active) { check(field(reader, 64) == 1, "active time"); ++activeCount; }
            check(reader.skip(55), "dialogue row reference");
            check(field(reader, 32) == 0x80000000ULL + (requested ? 1U : 0U), "generation is stable one-shot, including retired Lair cues");
            check(field(reader, 2) == (active ? 3U : 1U), "only intended rows play");
        }
        check(writer.bit_count() == 19767 + activeCount * 64, "optional fields determine exact body size");
        check(buffer.size()*8 - reader.remaining_bits() == writer.bit_count(), "all dialogue bits consumed");
      }
    }
}

void directive_wire() {
    for (const auto route : {omega::Route::opening, omega::Route::forestEntrance,
                            omega::Route::forestExit, omega::Route::crownEntrance, omega::Route::reveal}) {
        wire::Snapshot snapshot{};
        snapshot.omegaSceneAuthority = snapshot.omegaDialogueArm = snapshot.omegaForestBanner = true;
        const auto target = omega::waypoint(route);
        snapshot.omegaWaypointRegistry = target.registry;
        snapshot.omegaWaypointIndex = target.index;
        snapshot.omegaWaypointDestination = 0x12345678;
        std::array<std::byte, 1024> buffer{};
        bits::Writer writer(buffer);
        check(wire::write_auth_body(writer, snapshot, 0x82FB58B7, 68, 0, false), "type 68 encodes");
        check(writer.bit_count() == 4802, "waypoint preserves native 4802-bit body");
        bits::Reader reader(buffer);
        check(reader.skip(110), "directive root references");
        check(field(reader, 32) == 0x1EBF4621, "forest objective event");
        check(reader.skip(575), "remaining pre-target fields");
        for (unsigned index = 0; index < 4; ++index) {
            const bool selected = target.registry != 0 && index == 0;
            check(field(reader, 32) == (selected ? target.registry : 0x811C9DC5), "authored waypoint registry");
            check(field(reader, 7) == (selected ? 48U : 0U), "biased type decodes to 47");
            check(field(reader, 16) == (selected ? target.index + 0x8000U : 0x7FFFU), "biased authored slot");
            check(reader.skip(55), "no position override reference");
            const std::array<std::uint32_t, 4> locator{0x12345678, target.bubbleName, target.registry, target.pointName};
            for (const auto hash : locator)
                check(field(reader, 32) == (selected ? hash : 0x811C9DC5), "native destination/bubble/registry/point locator");
            check(field(reader, 1) == 0, "default target flag");
        }
        check(reader.skip(2*1563), "two absent directive entries");
        check(field(reader, 3) == 1, "ring selects first entry");
        check(buffer.size()*8 - reader.remaining_bits() == 4802, "native directive boundaries");
    }
    // Tower Watch's authored-cue path must never inherit an Omega target or extra dialogue row.
    wire::Snapshot snapshot{};
    snapshot.publishAuthoredCueTransition = true;
    snapshot.authoredCueRegistry = 0x12345678;
    snapshot.authoredDialogueRecord = 4;
    snapshot.authoredDirectiveEvent = 0xAABBCCDD;
    snapshot.omegaTunnelDialogue = snapshot.omegaVistaDialogue = snapshot.omegaExitDialogue = true;
    snapshot.omegaLairDialogueRequestedMask = (1U << 12) | (1U << 13);
    snapshot.omegaLairDialoguePendingRow = 13;
    snapshot.omegaWaypointRegistry = omega::kCrownGroup;
    snapshot.omegaWaypointIndex = 13;
    std::array<std::byte, 4096> buffer{};
    bits::Writer writer(buffer);
    check(wire::write_auth_body(writer, snapshot, snapshot.authoredCueRegistry, 53, 2, false), "authored cue encodes");
    check(writer.bit_count() == 19831, "authored cue keeps one active row");
    bits::Writer directive(buffer);
    check(wire::write_auth_body(directive, snapshot, snapshot.authoredCueRegistry, 68, 0, false), "authored directive encodes");
    bits::Reader reader(buffer);
    check(reader.skip(717), "authored directive target offset");
    check(field(reader, 32) == 0x811C9DC5, "other mission has no Omega waypoint");
}

void lair_directive_wire() {
    for (const auto event : {0x3517D4D5U, 0x31A51CEBU}) {
        wire::Snapshot snapshot{};
        snapshot.omegaSceneAuthority = snapshot.omegaDialogueArm = snapshot.omegaForestBanner = true;
        snapshot.omegaLairObjectiveEvent = event;
        // A delayed route target cannot restore the retired Forest/Crown entrance waypoint.
        snapshot.omegaWaypointRegistry = omega::kCrownGroup;
        snapshot.omegaWaypointIndex = 13;
        snapshot.omegaWaypointDestination = 0x12345678;
        std::array<std::byte,1024> buffer{};
        bits::Writer writer(buffer);
        check(wire::write_auth_body(writer,snapshot,0x82FB58B7,68,0,false), "Lair objective encodes");
        check(writer.bit_count()==4802, "Lair directive retains native record width");
        bits::Reader reader(buffer);
        check(reader.skip(110) && field(reader,32)==event, "exact authored Lair objective selected");
        check(reader.skip(575), "Lair target offset");
        for (unsigned target=0;target<4;++target) {
            check(field(reader,32)==0x811C9DC5 && field(reader,7)==0 && field(reader,16)==0x7FFF,
                  "retired entrance target absent from Lair objective");
            check(reader.skip(55), "absent position override");
            for (unsigned name=0;name<4;++name)
                check(field(reader,32)==0x811C9DC5, "retired cross-area locator absent");
            check(field(reader,1)==0,"default target flag");
        }
        check(reader.skip(2*1563) && field(reader,3)==1,"unchanged native directive ring");
    }
    omega::Progression tracker;
    tracker.loaded(70,14,0x12345678);
    auto progress=tracker.update(70,100,true,true,true,{-1491.0F,481.0F,-15.0F});
    check(progress.lairObjectiveEvent==0x3517D4D5U,"authored Lair010 feeds progression objective");
    progress=tracker.update(70,101,true,true,true,{-1480.0F,210.0F,-20.0F});
    check(progress.lairObjectiveEvent==0x31A51CEBU,"authored Lair050 advances defense objective");
    tracker.cinematic_completed(70,1000);
    progress=tracker.update(70,1249,true,true,false,{});
    check(progress.lairDialoguePendingRow==13,"already offered arena cue is not reordered by late camera receipt");
    progress=tracker.update(71,2000,true,true,false,{});
    check(progress.lairObjectiveEvent==0 && progress.lairDialogueRequestedMask==0,
          "new run clears Lair objectives and dialogue receipts");
}

// Slot lists captured from the installed mission_scot cache; no synthetic type-47 sync slots.
constexpr std::array<std::uint8_t, 51> bossTypes{1,2,26,26,4,4,4,4,4,4,4,4,4,4,4,4,4,4,23,23,23,23,4,4,23,4,23,4,4,23,4,4,4,26,26,4,4,4,4,23,23,23,23,70,34,34,34,34,66,66,66};
constexpr std::array<std::uint16_t, 51> bossIndices{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,54,56,57};
constexpr std::array<std::uint8_t, 51> bossFlags{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2};
constexpr std::array<std::uint8_t, 53> introTypes{3,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,6,70,31,31,31,31,31,31,31,31,31,31,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66};
constexpr std::array<std::uint16_t, 53> introIndices{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,36,38,40,42,44,46,48,50,52,54,70,71,72,73,74,75,105,107};
constexpr std::array<std::uint8_t, 53> introFlags{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};

void complete_packet() {
    constexpr std::array<std::uint32_t, 11> keys{
        0x4786C0E0, 0x29D7B029, 0x82FB58B7, 0xBA5F26EF, 0xD00142CF,
        0xF7A6CE7F, 0x2763EC97, omega::kHandoffGroup, omega::kCrownGroup, omega::kBossGroup, omega::kRevealGroup};
    constexpr std::array<std::uint8_t, 1> passiveType{47}, runtimeType{18};
    constexpr std::array<std::uint8_t, 3> lifetimeTypes{17, 18, 13}, lifetimeFlags{2, 2, 2};
    constexpr std::array<std::uint16_t, 3> lifetimeIndices{17, 18, 5};
    constexpr std::array<std::uint16_t, 1> passiveIndex{0};
    constexpr std::array<std::uint8_t, 1> passiveFlags{2};
    constexpr std::array<std::uint8_t, 1> portalTypes{4}, portalFlags{3};
    constexpr std::array<std::uint8_t, 2> cueTypes{68, 53}, cueFlags{2, 2};
    constexpr std::array<std::uint16_t, 2> cueIndices{0, 2};
    const std::array<std::uint32_t, 2> forestKeys{keys[6], keys[7]};
    const std::array<std::uint32_t, 3> crownKeys{keys[8], keys[9], keys[10]};
    const std::array<std::uint32_t, 3> openingKeys{keys[3], keys[4], keys[5]};
    const std::array<wire::BubbleSubBlock, 3> bubbles{{{11, forestKeys}, {14, crownKeys}, {15, openingKeys}}};
    for (bool portalEnabled : {false, true}) for (bool bossActive : {false,true})
    for (bool leftStarted : {false, true})
    for (unsigned armStage=0;armStage<3;++armStage)
    for (unsigned mode = 0; mode < 3; ++mode) {
        wire::Snapshot snapshot{};
        snapshot.lifetime = 3;
        snapshot.omegaForestVexEncounters = true;
        snapshot.omegaPortalEntry = portalEnabled;
        snapshot.omegaBossAuthority = true;
        snapshot.omegaBossGeneration = bossActive ? 17U : 0U;
        if(armStage) snapshot.omegaMission.arm={17,armStage,false,armStage==1};
        snapshot.omegaLairAuthority = true;
        snapshot.omegaLairGeneration = snapshot.omegaBossGeneration;
        snapshot.omegaLairLeftStarted = bossActive && leftStarted;
        // The required hash must seed before carrier activation, and must not cause
        // participation to be resent when the native mission owns its inputs.
        snapshot.omegaPortalPlayerHash = true;
        snapshot.roster.playerKeyGroup = keys[0];
        snapshot.playerKey = 0x1080123456789ABULL;
        snapshot.omegaSceneAuthority = snapshot.omegaDialogueArm = true;
        snapshot.omegaTunnelDialogue = snapshot.omegaVistaDialogue = snapshot.omegaExitDialogue = true;
        snapshot.omegaWaypointRegistry = omega::kCrownGroup;
        snapshot.omegaWaypointIndex = 13;
        snapshot.preserveMissionAuthorityState = mode != 0;
        if (mode == 2) snapshot.omegaOpeningStage = wire::kOmegaOpeningStageSettled;
        snapshot.roster.groupCount = keys.size();
        snapshot.roster.topLevelGroupCount = 3;
        snapshot.roster.bubbleSubBlocks = bubbles;
        for (std::size_t i = 0; i < keys.size(); ++i)
            snapshot.roster.groups[i] = {keys[i], i == 0 ? runtimeType : passiveType, passiveFlags, passiveIndex};
        snapshot.roster.groups[2] = {keys[2], cueTypes, cueFlags, cueIndices};
        snapshot.roster.groups[0] = {keys[0], lifetimeTypes, lifetimeFlags, lifetimeIndices};
        snapshot.roster.groups[3] = {keys[3], portalTypes, portalFlags, passiveIndex};
        snapshot.roster.groups[9] = {keys[9], bossTypes, bossFlags, bossIndices};
        snapshot.roster.groups[10] = {keys[10], introTypes, introFlags, introIndices};
        std::array<std::byte, 8192> buffer{};
        std::size_t written = 0;
        check(wire::encode_sensor_auth_update(snapshot, buffer, written), "eleven-group packet encodes");
        check(written < 8192, "full reveal seed packet stays below 8 KiB");
        bits::Reader reader(std::span(buffer).first(written));
        check(reader.skip(wire::kLatchBitWithoutGrant + 1 + wire::delta_bits(3, bubbles)), "phase-2 boundary");
        unsigned cueCount = 0, groupCount = 0, portalCount = 0, participationCount = 0;
        unsigned bossParentCount=0,bossMemberCount=0,lairSourceCount=0;
        while (field(reader, 1) != 0) {
            const auto group = field(reader, 32);
            check(reader.skip(32), "group filler");
            ++groupCount;
            if (mode != 0) check(group == keys[2] || (portalEnabled && group == keys[3]) || group == keys[6] || group == keys[7] || group == keys[8] || group == keys[9] || group == keys[10],
                                 "native-owned runtime stays untouched after ownership transfer");
            while (field(reader, 1) != 0) {
                check(field(reader, 32) == group, "object belongs to group");
                const auto type = field(reader, 7) - 1;
                const auto index = field(reader,16)-0x8000U;
                const auto size = field(reader, 32);
                if (type == 13) {
                    check(mode == 0 && group == keys[0] && size == 226,
                          "player hash seeds a keyed 224-bit body only before native ownership");
                    ++participationCount;
                }
                if (group == keys[3]) {
                    check(type == 4 && size == (portalEnabled ? 255U : 3U)
                          && (portalEnabled || mode == 0), "native contact includes its descriptor-required sense tail");
                    if (portalEnabled) ++portalCount;
                }
                if(group==keys[3]) {
                    check(field(reader,1)==1 && field(reader,1)==static_cast<unsigned>(portalEnabled)
                          && reader.skip(portalEnabled ? 252 : 0),
                          "contact native decoder consumes reset, authority presence and exact payload");
                    check(field(reader,1)==0,"contact native sense absence fits inside its declared block");
                } else if(group==keys[9]&&type==1&&index==0) {
                    check(size==644,"parent641bits plus auth reset/presence and sense absence");
                    check(field(reader,1)==1&&field(reader,1)==1,"parent authority resets to explicit body");
                    check(reader.skip(173),"parent native fields through generation presence");
                    check(field(reader,31)==snapshot.omegaBossGeneration,"retained packet carries exact parent generation");
                    check(reader.skip(437)&&field(reader,1)==0,"parent remainder ends at absent native sense");
                    ++bossParentCount;
                } else if(group==keys[9]&&type==2&&index==1) {
                    const bool issued=bossActive && armStage;
                    check(size==(issued?311U:183U),"explicit reset or arm body includes exact outer framing");
                    check(field(reader,1)==1&&field(reader,1)==1&&field(reader,1)==1,"member body and generation are present");
                    check(field(reader,31)==snapshot.omegaBossGeneration,"retained packet shares exact member generation");
                    check(field(reader,2)==1&&field(reader,3)==1,"member native retirement/location defaults");
                    check(field(reader,1)==static_cast<unsigned>(bossActive),"retained packet activates only intended member");
                    check(field(reader,1)==0 && field(reader,1)==1,"control present while auxiliary member record remains absent");
                    check(field(reader,31)==(issued?armStage:0U),"full packet retains independent native control revision");
                    check(field(reader,6)==0 && field(reader,6)==0 && field(reader,3)==0,"full packet control preserves non-scalar flags and hash defaults");
                    check(field(reader,32)==0x811C9DC5U && field(reader,7)==0 && field(reader,16)==0x7FFFU && field(reader,32)==0,"full packet control keeps absent target and index");
                    check(field(reader,5)==(issued?2U:0U),"only active owned arms carry scalar rows");
                    if(issued) {
                        check(field(reader,32)==0xA2AE120FU && field(reader,32)==(armStage==1?0x3F800000U:0U),"full packet left scalar follows high and release commands");
                        check(field(reader,32)==0x8496ABD2U && field(reader,32)==0,"full packet keeps the other arm low");
                    }
                    check(field(reader,3)==0,"animation queue, last nested record and native sense remain absent");
                    ++bossMemberCount;
                } else if(group==keys[10] && type==1 && index>=3 && index<=8) {
                    check(size==644,"initial combat source has reflected641-bit body");
                    check(field(reader,1)==1 && field(reader,1)==1,"source body reset and payload present");
                    check(reader.skip(121),"source category boundary");
                    check(field(reader,32)==0x80000000ULL+(snapshot.omegaLairLeftStarted ? 2U : 0U),
                          "combat remains dormant until native arm receipt");
                    check(reader.skip(20) && field(reader,31)==snapshot.omegaLairGeneration,
                          "first wave carries matching run generation");
                    check(reader.skip(437) && field(reader,1)==0,"source tail preserves following record");
                    ++lairSourceCount;
                } else {
                    if(group==keys[9])check(size==2||size==3,"all unrelated boss-group siblings remain seed-only");
                    check(reader.skip(static_cast<std::size_t>(size)), "object fits declared length");
                }
                if (group == keys[2] && (type == 53 || type == 68)) ++cueCount;
            }
        }
        check(cueCount == 2, "dialogue and waypoint survive native script ownership transfer");
        check(portalCount == (portalEnabled ? 1U : 0U), "carrier publishes exactly once per packet, including initial and native-owned paths");
        check(participationCount == (mode == 0 ? 1U : 0U),
              "player hash never reintroduces destructive participation resends");
        check(bossParentCount==1&&bossMemberCount==1,"native boss authority survives full and retained forest-group publication");
        check(lairSourceCount==6,"all six initial sources survive native-owned keepalives");
        check(groupCount == (mode == 0 ? 11U : 6U + (portalEnabled ? 1U : 0U)), "only intended groups publish");
        check(field(reader, 1) == 0 && reader.remaining_bits() < 8, "packet ends with padding only");
    }
}

void uncached_route_group() {
    namespace layouts = sunrise::state::build_data::scenarios;
    layouts::RosterGroup group;
    check(layouts::pinned_omega_route_group(omega::kHandoffGroup, group), "missing cached handoff has a pinned definition");
    check(group.objectTag == 0x80F47547 && group.slotCount == 2
          && group.slotTypes[0] == 30 && group.slotIndices[0] == 0
          && group.slotTypes[1] == 57 && group.slotIndices[1] == 3, "handoff uses package descriptor indices");
    check(group.descriptorTags[0] == 0x80F47541 && group.descriptorOffsets[0] == 0x218
          && group.authSchemas[0] == 0x80809532, "handoff descriptor metadata matches source");
    check(layouts::pinned_omega_route_group(omega::kCrownGroup, group)
          && group.objectTag == 0x80F47B4B && group.slotIndices[1] == 7, "Crown fallback has its own selector index");
    check(!layouts::pinned_omega_route_group(0x12345678, group) && group.slotCount == 0,
          "fallback cannot fabricate an unrelated group");
}

void stalled_presentation() {
    namespace presentation = sunrise::client::hooks::bootflow::omega_presentation;
    // Reproduce the observed run: the host crosses the entrance at 135156, while no
    // new authority packet reaches either component until the Crown tunnel at 246734.
    omega::Progression tracker;
    (void)tracker.update(1, 111906, true, true, true, {});
    tracker.dispatched(1, 6, 111922);
    const auto entered = tracker.update(1, 135156, true, true, true, omega::kForestEntrance);
    check(entered.vistaDialogue && !entered.exitDialogue, "entrance checkpoint has the vista intent");

    std::array<std::byte, 0x1400> dialogue;
    dialogue.fill(std::byte{0xA5});
    presentation::write<std::uint32_t>(dialogue, 0, 0x80F47BDA);
    presentation::write<std::int64_t>(dialogue, 8, 0x1408);
    constexpr std::size_t vista = 0x188 + 7*32, exit = 0x188 + 9*32;
    constexpr std::size_t processedVista = 0x1188 + 7*4;
    presentation::write<std::int32_t>(dialogue, processedVista, 0);
    const auto before = dialogue;
    check(presentation::sync_dialogue(dialogue, entered) == (1U << 7),
          "native scan receives vista without a packet or region change");
    check(presentation::read<std::uint64_t>(dialogue, vista) == UINT64_MAX
          && presentation::read<std::uint64_t>(dialogue, vista+8) == 1
          && presentation::read<std::uint64_t>(dialogue, vista+16) == 0xFFFF00FF811C9DC5ULL
          && presentation::read<std::int32_t>(dialogue, vista+24) == 1
          && presentation::read<std::uint8_t>(dialogue, vista+28) == 2,
          "native cue uses the decoded wire values");
    for (std::size_t i = 0; i < dialogue.size(); ++i)
        if (i < vista || i >= vista+29)
            check(dialogue[i] == before[i], "all other cues, padding and processed generations stay native-owned");
    check(presentation::sync_dialogue(dialogue, entered) == 0, "repeated scan does not rearm cue");

    // Native dispatch records generation 1. A late stale packet can restore generation 0,
    // but the repair must keep generation 1 (already processed), rather than request replay.
    presentation::write<std::int32_t>(dialogue, processedVista, 1);
    presentation::write<std::int32_t>(dialogue, vista+24, 0);
    presentation::write<std::uint8_t>(dialogue, vista+28, 0);
    presentation::write<std::uint64_t>(dialogue, vista+8, 0);
    check(presentation::sync_dialogue(dialogue, entered) == (1U << 7), "stale packet is repaired before scan");
    check(presentation::read<std::int32_t>(dialogue, vista+24)
          == presentation::read<std::int32_t>(dialogue, processedVista), "repair does not request a new native dispatch");
    tracker.dispatched(1, 7, 135172);
    const auto left = tracker.update(1, 267719, true, true, true, omega::kForestExit);
    const auto beforeExit = dialogue;
    check(presentation::sync_dialogue(dialogue, left) == (1U << 9), "exit checkpoint adds only the exit cue");
    for (std::size_t i = 0; i < dialogue.size(); ++i)
        if (i < exit || i >= exit+29)
            check(dialogue[i] == beforeExit[i], "exit leaves vista, later boss dialogue and processed generations alone");

    std::array<std::byte, 0xB08> directive;
    directive.fill(std::byte{0xA5});
    presentation::write<std::uint32_t>(directive, 0, 0x80F47BD4);
    presentation::write<std::int64_t>(directive, 8, 0xB88);
    presentation::write<std::uint32_t>(directive, 0x190, 0x1EBF4621);
    presentation::write<std::int8_t>(directive, 0x198, 0);
    presentation::write<std::uint32_t>(directive, 0x1F8, 0xF7A6CE7F);
    presentation::write<std::uint8_t>(directive, 0x1FC, 47);
    presentation::write<std::uint16_t>(directive, 0x1FE, 10);
    const auto oldDirective = directive;
    check(presentation::sync_directive(directive, entered), "marker leaves entrance without a packet or region change");
    check(presentation::read<std::uint32_t>(directive, 0x1F8) == omega::kHandoffGroup
          && presentation::read<std::uint8_t>(directive, 0x1FC) == 47
          && presentation::read<std::uint16_t>(directive, 0x1FE) == 8, "marker points to the forest exit");
    for (std::size_t i = 0; i < directive.size(); ++i)
        if (i < 0x1F8 || i == 0x1FD || (i >= 0x200 && i < 0x208) || i >= 0x218)
            check(directive[i] == oldDirective[i], "text, lifecycle, ring, other targets and marker runtime stay intact");
    check(!presentation::sync_directive(directive, entered), "marker update is idempotent");
    // Walking away from an already crossed checkpoint must not select the earlier target.
    // Also cover a delayed authority packet restoring the old entrance reference.
    const auto away = tracker.update(1, 268000, true, true, true, {});
    auto staleDirective = oldDirective;
    check(presentation::sync_directive(staleDirective, away)
          && presentation::read<std::uint32_t>(staleDirective, 0x1F8) == omega::kCrownGroup,
          "walking away and a stale packet cannot restore the previous waypoint");
    const auto back = tracker.update(1, 268100, true, true, true, omega::kForestEntrance);
    check(back.route == omega::Route::crownEntrance
          && !presentation::sync_directive(staleDirective, back),
          "returning to an earlier checkpoint keeps the advanced waypoint");
    check(presentation::sync_directive(directive, left)
          && presentation::read<std::uint32_t>(directive, 0x1F8) == omega::kCrownGroup
          && presentation::read<std::uint16_t>(directive, 0x1FE) == 13, "next checkpoint targets Crown entrance");

    auto rejectedDialogue = before;
    auto rejectedDirective = oldDirective;
    check(presentation::sync_dialogue(rejectedDialogue, {}) == 0
          && !presentation::sync_directive(rejectedDirective, {}), "no intent from an old run or opening");
    check(rejectedDialogue == before && rejectedDirective == oldDirective, "no-intent calls leave memory unchanged");
    check(presentation::sync_dialogue(std::span(rejectedDialogue).first(presentation::kDialogueBytes-1), left) == 0
          && !presentation::sync_directive(std::span(rejectedDirective).first(presentation::kDirectiveBytes-1), left),
          "short component spans are rejected");
    check(rejectedDialogue == before && rejectedDirective == oldDirective, "short spans leave memory unchanged");
    for (const std::size_t offset : {std::size_t{0}, std::size_t{8}}) {
        rejectedDialogue = before;
        rejectedDirective = oldDirective;
        rejectedDialogue[offset] ^= std::byte{1};
        rejectedDirective[offset] ^= std::byte{1};
        const auto otherDialogue = rejectedDialogue;
        const auto otherDirective = rejectedDirective;
        check(presentation::sync_dialogue(rejectedDialogue, left) == 0
              && !presentation::sync_directive(rejectedDirective, left), "both source identity fields scope the repair");
        check(rejectedDialogue == otherDialogue && rejectedDirective == otherDirective, "other mission components stay unchanged");
    }
    for (const std::size_t offset : {std::size_t{0x190}, std::size_t{0x198}}) {
        rejectedDirective = oldDirective;
        rejectedDirective[offset] ^= std::byte{1};
        const auto inactive = rejectedDirective;
        check(!presentation::sync_directive(rejectedDirective, entered) && rejectedDirective == inactive,
              "an unrelated or retired objective is not revived");
    }
}

void native_area_retirement() {
    omega::Progression tracker;
    tracker.loaded(1, 11, 0x12345678);
    check(tracker.current().route == omega::Route::opening, "area alone cannot start a mission");
    const auto forest = tracker.update(1, 1, true, true, false, {});
    check(forest.route == omega::Route::forestExit, "forest load retires entrance even without a proximity sample");
    tracker.loaded(1, 15, 0x12345678);
    check(tracker.current().route == omega::Route::forestExit, "stale lighthouse region cannot revive entrance");
    tracker.loaded(1, 14, 0x12345678);
    check(tracker.current().route == omega::Route::crownEntrance, "Crown load retires forest exit");
    tracker.loaded(1, 11, 0x811C9DC5);
    check(tracker.current().route == omega::Route::crownEntrance
        && tracker.current().destination == 0x12345678, "backtracking and unset destination preserve checkpoint");
    tracker.loaded(2, 15, 0x12345678);
    check(tracker.current().route == omega::Route::opening, "new mission resets area retirement");
    namespace presentation = sunrise::client::hooks::bootflow::omega_presentation;
    std::array<std::byte, presentation::kDirectiveBytes> bytes{};
    presentation::write<std::uint32_t>(bytes, 0, 0x80F47BD4);
    presentation::write<std::int64_t>(bytes, 8, 0xB88);
    presentation::write<std::uint32_t>(bytes, 0x190, 0x1EBF4621);
    check(presentation::sync_directive(bytes, forest), "loaded forest repairs locator");
    check(presentation::read<std::uint32_t>(bytes, 0x208) == 0x12345678
        && presentation::read<std::uint32_t>(bytes, 0x20C) == 0x47EA4CEA
        && presentation::read<std::uint32_t>(bytes, 0x210) == omega::kHandoffGroup
        && presentation::read<std::uint32_t>(bytes, 0x214) == 0xBF60FA2A, "complete named locator reaches native memory");
    check(!presentation::sync_directive(bytes, forest), "unchanged locator is stable");
    presentation::write<std::uint32_t>(bytes, 0x208, 0x811C9DC5);
    check(presentation::sync_directive(bytes, forest), "stale authority locator is repaired");
}

void opening_acknowledgement() {
    namespace sense = sunrise::middleware::bap::activity_message::sense_update;
    namespace ack = sunrise::server::bap::encrypted::activity_message::omega_ack;
    sense::SenseUpdate captured{};
    std::size_t consumed = 0;
    check(sense::parse_sense_update(sunrise::unit::fixtures::kOmegaRevealAcknowledgement, captured, consumed),
          "failed teleport acknowledgement parses through the production decoder");
    check(consumed == 2369 && captured.rosterEntryCount == 11 && captured.objectCount == 6,
          "replay reproduces the eleven-entry readiness regression");
    check(ack::exact_omega_initial_report(captured), "reveal roster now arms the opening and teleport");

    // The extra groups must be exact and active; accepting arbitrary extra keys would
    // incorrectly preserve progression after a genuinely different roster is loaded.
    for (std::size_t i = 0; i < captured.rosterEntryCount; ++i) {
        auto invalid = captured;
        invalid.rosterEntries[i].active = false;
        check(!ack::exact_omega_initial_report(invalid), "inactive roster member blocks readiness");
        invalid = captured;
        invalid.rosterEntries[i].bubble = 3;
        check(!ack::exact_omega_initial_report(invalid), "wrong-area roster member blocks readiness");
    }
    auto invalid = captured;
    invalid.rosterEntries[7] = invalid.rosterEntries[6];
    check(!ack::exact_omega_initial_report(invalid), "duplicate boss cannot replace intro group");
    invalid = captured;
    invalid.rosterEntries[7].registryKey = 0x12345678;
    check(!ack::exact_omega_initial_report(invalid), "unknown reveal group blocks readiness");
    invalid = captured;
    invalid.objects[2].bodyFirst ^= 1;
    check(!ack::exact_omega_initial_report(invalid), "opening sensor evidence remains mandatory");
    invalid = captured;
    invalid.rosterEntryCount = 10;
    check(!ack::exact_omega_initial_report(invalid), "incomplete reveal roster blocks readiness");

    for (const unsigned count : {6U, 7U, 9U}) {
        auto legacy = captured;
        legacy.rosterEntryCount = 0;
        legacy.bubbleBlockCount = count == 6 ? 1 : (count == 7 ? 2 : 3);
        for (std::size_t i = 0; i < captured.rosterEntryCount; ++i) {
            const auto entry = captured.rosterEntries[i];
            if (entry.registryKey == omega::kBossGroup || entry.registryKey == omega::kRevealGroup) continue;
            if (count < 9 && (entry.registryKey == omega::kHandoffGroup || entry.registryKey == omega::kCrownGroup)) continue;
            if (count == 6 && entry.registryKey == 0x2763EC97) continue;
            legacy.rosterEntries[legacy.rosterEntryCount++] = entry;
        }
        check(legacy.rosterEntryCount == count && ack::exact_omega_initial_report(legacy),
              "legacy opening and forest rosters remain accepted");
    }
}

namespace {
unsigned bossRequestCalls{};
std::byte* requestedBoss{};
std::uint64_t bossRequestReason{};
std::array<std::int32_t, 10> bossRequestCounts{};
void capture_boss_request(std::byte* component, std::uint64_t reason,
                          const std::int32_t* counts, std::int32_t* result) noexcept {
    ++bossRequestCalls;
    requestedBoss = component;
    bossRequestReason = reason;
    std::memcpy(bossRequestCounts.data(), counts, sizeof bossRequestCounts);
    result[0] = 1;
    reinterpret_cast<unsigned char*>(result)[4] = 2;
}
}

void boss_spawn_gateway() {
    namespace boss = sunrise::client::hooks::bootflow::omega_boss_spawn;
    namespace presentation = sunrise::client::hooks::bootflow::omega_presentation;
    std::array<std::byte, boss::kComponentBytes> component{};
    // The fixture comes from the real callback, independently correlated to the
    // exact sq_boss roster pointer. It does not assume the on-disk class at +4.
    std::memcpy(component.data(), omega_reveal_runtime_fixture::boss.data(), 16);
    const auto before = component;
    omega::Progress progress{};
    progress.route = omega::Route::reveal;
    progress.loadedBubble = 14;
    progress.bossDoorReached = true;
    std::array<std::int32_t, 9> result{};
    check(boss::matches(component), "captured live boss header accepted");
    check(boss::submit(true, progress, component, result, &capture_boss_request),
          "boss reaches native request without installing the retired debug probe");
    check(bossRequestCalls == 1 && requestedBoss == component.data() && bossRequestReason == 2
          && bossRequestCounts == std::array<std::int32_t, 10>{1, 1}
          && result[0] == 1 && reinterpret_cast<unsigned char*>(result.data())[4] == 2,
          "native callback receives one authored category and preserves queue result");
    check(component == before, "request gateway does not edit authority or actor counts");
    presentation::write<std::uint32_t>(component, 4, 0x80809A3B);
    check(!boss::submit(true, progress, component, result, &capture_boss_request),
          "asset-definition class is not accepted as the live component class");
    std::memcpy(component.data(), omega_reveal_runtime_fixture::anchor.data(), 16);
    check(!boss::submit(true, progress, component, result, &capture_boss_request),
          "captured neighboring reveal spawner cannot request Panoptes");
    component = before;
    check(!boss::submit(false, progress, component, result, &capture_boss_request), "other missions cannot request boss");
    progress.route = omega::Route::crownEntrance;
    progress.bossDoorReached = false;
    check(!boss::submit(true, progress, component, result, &capture_boss_request), "Lair loading alone cannot request boss");
    progress.bossDoorReached = true;
    check(boss::submit(true, progress, component, result, &capture_boss_request), "door exit requests boss before close waypoint");
    progress.route = omega::Route::forestExit;
    check(!boss::submit(true, progress, component, result, &capture_boss_request), "pre-Lair route cannot request boss");
    progress.route = omega::Route::reveal;
    progress.loadedBubble = 11;
    check(!boss::submit(true, progress, component, result, &capture_boss_request), "unloaded Lair cannot request boss");
    progress.loadedBubble = 14;
    for (const std::size_t offset : {std::size_t{0}, std::size_t{4}, std::size_t{8}}) {
        component[offset] ^= std::byte{1};
        check(!boss::submit(true, progress, component, result, &capture_boss_request),
              "unrelated resource, class and relative offset are all rejected");
        component[offset] ^= std::byte{1};
    }
    check(!boss::submit(true, progress, std::span{component}.first(8), result, &capture_boss_request)
          && !boss::submit(true, progress, component, std::span{result}.first(8), &capture_boss_request)
          && !boss::submit(true, progress, component, result, nullptr), "invalid buffers and absent entry cannot dispatch");
    check(bossRequestCalls == 2, "rejected requests do not reach native code");
}

void cinematic_source_identity() {
    namespace source = sunrise::client::hooks::bootflow::omega_reveal_source;
    namespace presentation = sunrise::client::hooks::bootflow::omega_presentation;
    const auto captured = omega_reveal_runtime_fixture::intro;
    check(source::matches(captured, source::kIntro), "captured live cinematic header accepted");
    check(!source::matches(captured, source::kBoss)
          && !source::matches(omega_reveal_runtime_fixture::boss, source::kIntro),
          "boss and cinematic sources cannot be interchanged");
    auto component = captured;
    presentation::write<std::uint32_t>(component, 4, 0x80804F06);
    check(!source::matches(component, source::kIntro), "cinematic definition class is not its runtime class");
    for (std::size_t bytes = 0; bytes < captured.size(); ++bytes)
        check(!source::matches(std::span{captured}.first(bytes), source::kIntro), "truncated source header rejected");
    for (const std::size_t offset : {std::size_t{0}, std::size_t{4}, std::size_t{8}, std::size_t{15}}) {
        component = captured;
        component[offset] ^= std::byte{1};
        check(!source::matches(component, source::kIntro), "wrong cinematic resource, class or full 64-bit offset rejected");
    }
}

void forest_lifetime(const char* exportPath) {
    namespace encounters = omega::forest_encounters;
    check(encounters::selected("mission_scot") && !encounters::selected("mission_towerfall")
          && !encounters::selected("mission_scot_extra") && !encounters::selected(""), "Vex selection is exact mission scope");
    auto bit = [](const auto& buffer, std::size_t n) {
        return (std::to_integer<unsigned>(buffer[n / 8]) >> (7 - n % 8)) & 1U;
    };
    for (bool overrideSpawn : {false, true}) {
        wire::Snapshot snapshot{};
        snapshot.lifetime = 3;
        snapshot.hasSpawnOverride = overrideSpawn;
        snapshot.spawnSetHash = 0x1234ABCD;
        snapshot.spawnSliceSet = 0x1FF;
        std::array<std::byte, 78> old{}, revised{};
        sunrise::middleware::encoding::bits::Writer oldWriter(old), newWriter(revised);
        check(wire::write_auth_body(oldWriter, snapshot, 0x4786C0E0, 17, 17, false)
              && oldWriter.bit_count() == 520, "other missions retain the 520-bit lifetime body");
        snapshot.omegaForestVexEncounters = true;
        snapshot.omegaSceneAuthority = false; // Portal quiescence must not remove the switch.
        check(wire::write_auth_body(newWriter, snapshot, 0x4786C0E0, 17, 17, false)
              && newWriter.bit_count() == 617
              && wire::auth_body_bits(snapshot, 0x4786C0E0, 17, 17, false) == 617,
              "Omega writes and declares exactly 617 bits even while presentation is quiesced");
        bits::Reader decoded(revised);
        check(decoded.skip(104) && field(decoded, 6) == 2, "typed switch list has two entries");
        check(field(decoded, 32) == 0xB3C1251B && field(decoded, 1) == 1
              && field(decoded, 32) == 0x80800007 && field(decoded, 32) == 0x80000000,
              "waiting selector remains signed integer zero");
        check(field(decoded, 32) == 0x67AF9045 && field(decoded, 1) == 1
              && field(decoded, 32) == 0x80800070 && field(decoded, 32) == 0x050C5D2E,
              "Vex selector is a present raw hash, without integer bias or wildcard value");
        for (std::size_t i = 0; i < 520; ++i) {
            if (i >= 104 && i < 110) continue; // Only the list count and appended row change.
            check(bit(old, i) == bit(revised, i >= 207 ? i + 97 : i),
                  "all original lifetime fields and spawn/quarantine tail remain bit-identical");
        }
        std::array<std::byte, 77> truncated{};
        sunrise::middleware::encoding::bits::Writer shortWriter(truncated);
        check(!wire::write_auth_body(shortWriter, snapshot, 0x4786C0E0, 17, 17, false),
              "undersized 616-bit buffer fails without accepting a truncated body");
        if (exportPath && !overrideSpawn) {
            std::FILE* file{};
            check(fopen_s(&file, exportPath, "wb") == 0 && file, "open independently decoded lifetime artifact");
            check(std::fwrite(revised.data(), 1, revised.size(), file) == revised.size(), "export exact encoded lifetime body");
            check(std::fclose(file) == 0, "close lifetime artifact");
        }
    }
}

void forest_population_receipts() {
    namespace receipt = sunrise::client::hooks::bootflow::omega_enemy_forest;
    namespace presentation = sunrise::client::hooks::bootflow::omega_presentation;
    std::array<std::byte, receipt::kEntryBytes> bytes{};
    presentation::write<std::uint32_t>(bytes, 0x34, 0x12345678U);
    presentation::write<std::uint8_t>(bytes, 0x18, 0);
    presentation::write<std::uint8_t>(bytes, 0x19, 3);
    presentation::write<std::uint8_t>(bytes, 0x22, 25);
    presentation::write<std::uint8_t>(bytes, 0x23, 7);
    presentation::write<std::uint8_t>(bytes, 0x24, 9);
    presentation::write<std::uint8_t>(bytes, 0x27, 2);
    const auto entry = receipt::entry(bytes);
    check(entry.encounter == 0x12345678U && entry.kind == 0 && entry.state == 3
          && entry.palette == 25 && entry.area == 7 && entry.gateway == 9 && entry.pending == 2,
          "current forest entry offsets preserve native identity and request state");
    const auto digest = receipt::hash(bytes);
    presentation::write<std::uint32_t>(bytes, 0x34, 0x12347678U); // Same low slot, different salt.
    check(receipt::entry(bytes).encounter != entry.encounter && receipt::hash(bytes) != digest,
          "recycled encounter salt remains distinguishable in decoded entry and change receipt");
    presentation::write<std::uint32_t>(bytes, 0x34, UINT32_MAX);
    check(receipt::entry(bytes).encounter == UINT32_MAX,
          "invalid native encounter sentinel is retained for caller qualification");
    check(receipt::read<std::uint32_t>(std::span(bytes).first(0x37), 0x34) == 0
          && receipt::read<std::uint32_t>(bytes, SIZE_MAX) == 0,
          "bounded field reader never assembles a partial or overflowed identity");

    std::array<std::byte, 3 * receipt::kActorRowBytes> rows{};
    presentation::write<std::int32_t>(rows, 0x20, 3);
    presentation::write<std::int32_t>(rows, 0x24, 2);
    presentation::write<std::uint8_t>(rows, 0x2C, 1);
    presentation::write<std::int32_t>(rows, 0x50, INT32_MAX); // Disabled row does not contribute.
    presentation::write<std::int32_t>(rows, 0x54, INT32_MAX);
    presentation::write<std::int32_t>(rows, 0x80, 7);
    presentation::write<std::int32_t>(rows, 0x84, 4);
    presentation::write<std::uint8_t>(rows, 0x8C, 2); // Any nonzero native enable flag counts.
    receipt::Totals totals{};
    check(receipt::actor_totals(rows, 3, totals) && totals.rows == 3 && totals.enabled == 2
          && totals.remaining == 10 && totals.queued == 6,
          "only enabled native request rows contribute to accounting");
    presentation::write<std::int32_t>(rows, 0x20, INT32_MAX);
    presentation::write<std::int32_t>(rows, 0x80, INT32_MAX);
    presentation::write<std::int32_t>(rows, 0x24, -2);
    presentation::write<std::int32_t>(rows, 0x84, -4);
    check(receipt::actor_totals(rows, 3, totals)
          && totals.remaining == 2LL * INT32_MAX && totals.queued == -6,
          "native signed request counters sum in64bits without being interpreted as kills");
    const auto reject = [&](std::span<const std::byte> data, std::int64_t count) {
        totals = {99, 88, 77, 66};
        return !receipt::actor_totals(data, count, totals) && totals.rows == 0
               && totals.enabled == 0 && totals.remaining == 0 && totals.queued == 0;
    };
    check(reject(rows, -1) && reject(rows, INT64_MIN), "negative native row count rejected and prior totals cleared");
    check(reject(rows, 4) && reject(std::span(rows).first(rows.size() - 1), 3),
          "missing or partially copied actor rows return failure instead of successful empty accounting");
    std::array<std::byte, receipt::kMaximumActorRows * receipt::kActorRowBytes> maximum{};
    check(receipt::actor_totals(maximum, receipt::kMaximumActorRows, totals)
          && totals.rows == receipt::kMaximumActorRows && totals.enabled == 0,
          "full512row forest palette capacity is accepted");
    check(reject(maximum, receipt::kMaximumActorRows + 1) && reject(maximum, INT64_MAX),
          "oversized native palette count rejected before row indexing");
    check(receipt::actor_totals({}, 0, totals) && totals.rows == 0 && totals.enabled == 0
          && totals.remaining == 0 && totals.queued == 0,
          "explicit zero row request remains distinct from rejected accounting");

    std::uintptr_t address{};
    check(receipt::relative(0x20000, 0x30, 0xD8, address) && address == 0x20108
          && receipt::relative(0x20000, -0x30, 0xD8, address) && address == 0x200A8,
          "native relative rows support both displacement signs and field suffix");
    check(!receipt::relative(UINTPTR_MAX - 3, 0, 4, address)
          && !receipt::relative(UINTPTR_MAX - 3, 4, 0, address),
          "suffix and positive displacement overflow rejected independently");
    check(!receipt::relative(0x10000, INT64_MIN, 0, address)
          && !receipt::relative(0x10000, -1, 0, address),
          "negative displacement underflow and low invalid address rejected");
    auto oldReceipt = std::uint64_t{1469598103934665603ULL};
    auto recycledReceipt = oldReceipt;
    receipt::fold(oldReceipt, entry.encounter);
    receipt::fold(recycledReceipt, std::uint32_t{0x12347678U});
    check(oldReceipt != recycledReceipt, "population receipt digest retains full salted encounter identity");
}

void portal_player_hash() {
    for (const bool enabled : {false, true}) for (const bool region : {false, true}) {
        wire::Snapshot snapshot{};
        snapshot.omegaPortalPlayerHash = enabled;
        snapshot.hasRegion = region;
        snapshot.region = 120;
        snapshot.playerKey = 0x1080123456789ABULL;
        snapshot.awaitClientSync = true;
        std::array<std::byte, 40> bytes{};
        bits::Writer writer(bytes);
        const auto expected = 192U + (region ? 32U : 0U) + (enabled ? 32U : 0U);
        check(wire::write_auth_body(writer, snapshot, 0x4786C0E0, 13, 5, true)
              && writer.bit_count() == expected
              && wire::auth_body_bits(snapshot, 0x4786C0E0, 13, 5, true) == expected,
              "participation body accounts for the nonempty hash list");
        check(writer.write(0xABCDEFU, 24), "following record sentinel fits");
        bits::Reader reader(bytes);
        check(field(reader, 1) == static_cast<unsigned>(region), "optional region presence");
        if (region) check(field(reader, 32) == 0x80000078, "region retains native bias");
        // Reflected 808094E4 and 80804F35 fields precede the +48/808094DD record.
        check(reader.skip(54), "skip earlier participation fields");
        check(field(reader, 1) == 1 && field(reader, 64) == snapshot.playerKey,
              "keyed player identity survives the change");
        check(field(reader, 5) == 0 && field(reader, 6) == 3, "record enums retain neutral encodings");
        check(field(reader, 6) == static_cast<unsigned>(enabled), "808094E1 count is zero or one");
        if (enabled) check(field(reader, 32) == 0x52B968BAU, "exact authored predicate hash on the wire");
        check(field(reader, 6) == 0, "second variable array stays empty and aligned");
        check(field(reader, 1) == 1 && field(reader, 4) == 2U,
              "arrival hold and respawn state remain aligned");
        check(field(reader, 3) == 0 && field(reader, 1) == 0 && field(reader, 8) == 128
              && field(reader, 32) == 0x80000000 && field(reader, 24) == 0xABCDEFU,
              "participation tail and following record are not displaced");
        check(wire::auth_body_bits(snapshot, 0x4786C0E0, 13, 6, false) == 0,
              "unkeyed participation slots do not gain a hash body");
        bits::Writer truncated(std::span(bytes).first(expected / 8U - 1U));
        check(!wire::write_auth_body(truncated, snapshot, 0x4786C0E0, 13, 5, true),
              "undersized participation packet is rejected");
    }
}

void portal_entry_contact() {
    namespace portal = omega::portal_entry;
    wire::Snapshot snapshot{};
    snapshot.omegaPortalEntry = true;
    std::array<std::byte, 32> bytes{};
    bits::Writer writer(bytes);
    check(wire::write_auth_body(writer, snapshot, portal::kRegistry, 4, 0, false)
          && writer.bit_count() == 252, "contact carrier body matches reflected width");
    std::array<std::byte, 31> shortBytes{};
    bits::Writer shortWriter(shortBytes);
    check(!wire::write_auth_body(shortWriter, snapshot, portal::kRegistry, 4, 0, false), "truncated carrier body is rejected");
    bits::Reader reader(bytes);
    check(field(reader, 32) == 0x80000001 && field(reader, 32) == 0x80000000,
          "carrier generation one and explicit index zero use signed biases");
    check(field(reader, 1) == 1 && field(reader, 1) == 0 && field(reader, 32) == 0x80000000,
          "native carrier activation preserves authored placement");
    check(field(reader, 32) == 0x811C9DC5 && field(reader, 7) == 0 && field(reader, 16) == 0x7FFF,
          "carrier has canonical absent actor reference");
    check(reader.skip(96) && field(reader, 1) == 0 && field(reader, 2) == 0,
          "neutral transform and optional-state tail remain aligned");
    check(wire::auth_body_bits(snapshot, portal::kRegistry, 23, 1, false) == 0
          && wire::auth_body_bits(snapshot, 0xD00142CF, 4, 2, false) == 0
          && wire::auth_body_bits(snapshot, portal::kRegistry, 4, 1, false) == 0,
          "contact publication excludes gate, Ikora beams and neighboring slots");
    snapshot.omegaPortalEntry = false;
    check(wire::auth_body_bits(snapshot, portal::kRegistry, 4, 0, false) == 0,
          "inactive snapshot does not activate the contact carrier");

}

void reveal_door_and_animation() {
    check(!omega::reveal_door::reached(-1492.F,476.F,-20.F), "doorway notch rejects inside threshold despite AABB overlap");
    check(omega::reveal_door::reached(-1492.F,472.F,-20.F), "center path beyond door enters spawn polygon");
    check(omega::reveal_door::reached(-1492.F,460.F,-25.F), "sampling past thin spawn volume is caught inside intro area");
    check(!omega::reveal_door::reached(-1492.F,480.F,-20.F)
        && !omega::reveal_door::reached(-1600.F,472.F,-20.F)
        && !omega::reveal_door::reached(-1492.F,472.F,-60.F), "outside, side and underground points cannot trigger");
    omega::Progression tracker;
    (void)tracker.update(77,0,true,true,true,{});
    tracker.loaded(77,14,0);
    check(!tracker.current().bossDoorReached, "native area load does not acknowledge doorway");
    check(!tracker.update(77,1,true,true,false,{-1492,472,-20}).bossDoorReached, "missing player cannot acknowledge doorway");
    check(tracker.update(77,2,true,true,true,{-1492,472,-20}).bossDoorReached, "door crossing latches");
    check(tracker.update(77,3,true,true,true,{-1492,490,-20}).bossDoorReached, "backtracking cannot replay door");
    check(!tracker.update(78,4,true,true,true,{-1492,472,-20}).bossDoorReached, "new run resets doorway latch");
    namespace anim=sunrise::client::hooks::bootflow::omega_animation;
    anim::Sequence sequence;
    auto request=sequence.request(true);
    check(request.bankRow==5 && request.clipHandle==-1 && request.animationHandle==-1
        && request.elapsed==0 && request.duration==8.7F && request.weight==1 && request.loop==0,
        "intro request uses bank row, with native handle sentinels and full weight");
    sequence.advance(std::numeric_limits<float>::quiet_NaN());
    check(sequence.request(false).elapsed==0, "nonfinite frame time does not corrupt playback");
    for (unsigned i=0;i<9;++i) sequence.advance(1.F);
    request=sequence.request(false);
    check(sequence.phase()==anim::Phase::hoverPose && request.rate==0 && request.elapsed>8.6F,
        "intro completion retains raised pose while player stays distant");
    request=sequence.request(true);
    check(sequence.phase()==anim::Phase::summon && request.bankRow==13 && request.elapsed==0,
        "approach switches to summon row after intro finishes");
    for (unsigned i=0;i<8;++i) sequence.advance(1.F);
    request=sequence.request(true);
    check(sequence.phase()==anim::Phase::summonPose && request.rate==0 && request.bankRow==13,
        "remaining nearby does not replay summon");
}

void boss_authority_scope_and_generation() {
    namespace authority=omega::boss_authority;
    wire::Snapshot snapshot{};
    for(bool enabled:{false,true}) {
        snapshot.omegaBossAuthority=enabled;
        check(wire::auth_body_bits(snapshot,authority::kRegistry,1,0,false)==(enabled?641U:0U),"parent body only when boss authority is scoped");
        check(wire::auth_body_bits(snapshot,authority::kRegistry,2,1,false)==(enabled?180U:0U),"explicit control reset only when boss authority is scoped");
        for(auto type:std::array<std::uint8_t,3>{1,2,66})
        for(auto index:std::array<std::uint16_t,3>{0,1,57}) {
            const bool owned=(type==1&&index==0)||(type==2&&index==1);
            if(!owned)check(wire::auth_body_bits(snapshot,authority::kRegistry,type,index,false)==0,"unrelated boss slot has no fabricated authority");
            check(wire::auth_body_bits(snapshot,0x95FB2E00U,type,index,false)==0,"another registry has no boss authority");
        }
    }
    snapshot.omegaBossAuthority=true;
    omega::Progression tracker;
    (void)tracker.update(33,0,true,true,true,{});tracker.loaded(33,14,0);
    check(omega::boss_generation(33,tracker.current())==0,"native area loading cannot activate boss generation");
    auto progress=tracker.update(33,1,true,true,true,{-1492,472,-20});
    const auto first=omega::boss_generation(33,progress);
    check(first==34,"doorway starts stable nonzero run generation");
    progress=tracker.update(33,2,true,true,true,{-1492,490,-20});
    check(omega::boss_generation(33,progress)==first,"backtracking retains boss generation");
    tracker.loaded(33,11,0);check(omega::boss_generation(33,tracker.current())==first,"region reload cannot recreate boss generation");
    progress=tracker.update(34,3,true,true,true,{-1492,472,-20});
    check(omega::boss_generation(34,progress)==0,"fresh run becomes dormant until a new doorway crossing");
    tracker.loaded(34,14,0);progress=tracker.update(34,4,true,true,true,{-1492,472,-20});
    const auto second=omega::boss_generation(34,progress);
    check(second==35&&second!=first,"fresh door crossing uses a new run generation");
    for(auto generation:std::array<std::uint32_t,4>{0,first,0,second}) {
        snapshot.omegaBossGeneration=generation;
        std::array<std::byte,23> member{};bits::Writer writer(member);
        check(wire::write_auth_body(writer,snapshot,authority::kRegistry,2,1,false)&&writer.bit_count()==180,"scoped member reset body survives active/dormant transitions");
        bits::Reader reader(member);
        check(field(reader,1)==1&&field(reader,31)==generation,"serializer encodes exact run generation");
        check(reader.skip(5)&&field(reader,1)==static_cast<unsigned>(generation!=0),"serializer keeps enablement consistent with generation");
        check(field(reader,1)==0 && field(reader,1)==1 && field(reader,31)==0,"startup and dormancy explicitly clear independent control revision");
        check(field(reader,6)==0 && field(reader,6)==0 && field(reader,3)==0,"reset leaves actor flags and hash domain empty");
        check(field(reader,32)==0x811C9DC5U && field(reader,7)==0 && field(reader,16)==0x7FFFU && field(reader,32)==0,"reset retains absent target and index");
        check(field(reader,5)==0 && field(reader,1)==0 && field(reader,1)==0,"reset contains no scalar actions or replacement animation queue");
    }
}

int main(int argc, char** argv) {
    boss_authority_scope_and_generation();
    reveal_door_and_animation();
    portal_player_hash();
    portal_entry_contact();
    forest_population_receipts();
    forest_lifetime(argc == 2 ? argv[1] : nullptr);
    boss_spawn_gateway();
    cinematic_source_identity();
    opening_acknowledgement();
    native_area_retirement();
    progression(); dialogue_wire(); directive_wire(); lair_directive_wire(); complete_packet(); uncached_route_group();
    stalled_presentation();
    std::puts("PASS: Omega Vex lifetime encoding/scope/tail/bounds, boss request, teleport acknowledgement, progression and wire regression checks");
}
