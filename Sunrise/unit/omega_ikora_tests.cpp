#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <vector>

#include "middleware/bap/activity_message/sense_update.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include "state/activity/omega/omega_ikora_lattice.h"
#include "state/activity/omega/omega_lair_dialogue.h"
#include "client/hooks/bootflow/omega_presentation.h"

namespace bits = sunrise::middleware::encoding::bits;
namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace sense = sunrise::middleware::bap::activity_message::sense_update;
namespace ikora = sunrise::state::activity::omega::ikora;
namespace {
unsigned checks{};
void check(bool value, const char* description) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
std::uint64_t get(bits::Reader& reader, std::uint8_t width) {
    std::uint64_t value{}; check(reader.read(width, value), "complete independent field"); return value;
}
void hex_matches(std::span<const std::byte> bytes, std::string_view expected) {
    check(bytes.size() * 2 == expected.size(), "fixture length");
    const auto nibble = [](char c) { return static_cast<unsigned>(c <= '9' ? c - '0' : c - 'a' + 10); };
    for (std::size_t i = 0; i < bytes.size(); ++i)
        check(std::to_integer<unsigned>(bytes[i]) == 16 * nibble(expected[2*i]) + nibble(expected[2*i+1]),
              "production wire matches independent supplied native fixture");
}
void authority() {
    // Independent source fixture hashes to FE64E853...; retained C7 to C27D34C... .
    constexpr std::string_view source =
        "c08e4ee2807fffc08e4ee2807fff88c0000000c2529800000018000000060472771702393b8a01ffff"
        "02393b8a01ffff02393b8a01ffff02393b8a01fffe00000002000000020c300000000ac08e4ee280";
    for (bool requested : {false, true}) for (bool released : {false, true}) {
        wire::Snapshot snapshot{};
        snapshot.omegaSceneAuthority = snapshot.seedAuthoredSensors = true;
        snapshot.omegaIkoraPortalRequested = requested;
        snapshot.omegaIkoraLatticeReleased = released;
        std::array<std::byte, 81> body{}; bits::Writer sourceWriter(body);
        check(wire::write_auth_body(sourceWriter, snapshot, ikora::kRegistry, 1, 0, false)
            && sourceWriter.bit_count() == 641, "production source body641");
        hex_matches(body, source);
        std::array<std::byte, 21> scene{}; bits::Writer sceneWriter(scene);
        check(wire::write_auth_body(sceneWriter, snapshot, ikora::kRegistry, 43, 1, false)
            && sceneWriter.bit_count() == (requested ? 161U : 129U), "Scene generation/list/event framing");
        if (requested) hex_matches(scene, "80ec0f960e800a167828000000000020e3f6553b80");
        std::array<std::byte, 20> gate{}; bits::Writer gateWriter(gate);
        check(wire::write_auth_body(gateWriter, snapshot, ikora::kRegistry, 23, 16, false)
            && gateWriter.bit_count() == 147, "gate body147");
        check(gateWriter.write(0x1D, 5), "following gate sentinel");
        bits::Reader r(gate);
        check(get(r,32) == (released ? 0U : 0x3F800000U), "closed1 released0 position");
        check(get(r,16) == (released ? 0x8002U : 0x8001U), "position revision monotonically advances");
        check(get(r,1) == static_cast<unsigned>(!released), "initial snap then native smooth dissolve");
        check(get(r,32)==0x3F800000U && get(r,16)==0x7FFFU && get(r,1)==0, "power untouched");
        check(get(r,32)==0 && get(r,16)==0x7FFFU && get(r,1)==0 && get(r,5)==0x1D, "lock untouched and next record aligned");
        for (std::uint16_t panel = 7; panel <= 15; ++panel)
            check(wire::auth_body_bits(snapshot, ikora::kRegistry, 4, panel, false)==0, "unplaced fake walls stay absent");
        check(wire::auth_body_bits(snapshot, 0xBA5F26EF, 23, 1, false)==0, "interior portal gate is separate");
        snapshot.seedAuthoredSensors = false;
        check(wire::auth_body_bits(snapshot, ikora::kRegistry, 1, 0, false)==0, "source omitted outside scope");
    }
    std::array<std::byte,80> shortSource{}; bits::Writer a(shortSource);
    std::array<std::byte,20> shortScene{}; bits::Writer b(shortScene);
    std::array<std::byte,18> shortGate{}; bits::Writer c(shortGate);
    check(!ikora::write_source(a) && !ikora::write_scene(b,true) && !ikora::write_gate(c,false), "short bodies reject");
}

std::vector<std::byte> sense_packet(unsigned eventCount, bool sourceRevision, bool following,
                                   std::uint32_t generation = ikora::kSceneGenerationWire) {
    std::vector<std::byte> packet(256);
    bits::Writer w(packet);
    const unsigned bodyBits = 75U + (sourceRevision ? 31U : 0U) + eventCount * 32U;
    check(w.write(1,64) && w.write(2,64) && w.write(0,1) && w.write(0,1), "sense envelope");
    check(w.write(1,1) && w.write(ikora::kRegistry,32)
        && w.write(56U+bodyBits+(following?155U:0U)+1U,32), "group envelope includes object-list terminator");
    check(w.write(1,1) && w.write(ikora::kRegistry,32) && w.write(44,7) && w.write(0x8001,16), "Scene identity");
    check(w.write(1,1) && w.write(generation,32) && w.write(0,1)
        && w.write(sourceRevision?1U:0U,1) && (!sourceRevision || w.write(1,31))
        && w.write(1,2) && w.write(eventCount,6), "reflected Scene fields");
    for (unsigned i=0; i<eventCount; ++i)
        check(w.write(i+1==eventCount ? ikora::kLatticeRelease : 0x5598C86B,32), "complete event array");
    check(w.write(7,32), "raw revision");
    if (following) check(w.write(1,1) && w.write(ikora::kRegistry,32) && w.write(31,7)
        && w.write(0x8018,16) && w.write(0xF000000030000000ULL,64) && w.write(1,35), "coalesced entrance monitor");
    check(w.write(0,1), "explicit group object-list terminator");
    check(w.write(0,1) && w.write(0,1), "outer sense terminators");
    std::size_t written{}; check(w.finish(written), "complete packet"); packet.resize(written); return packet;
}
void captured_lattice_release() {
    // Unmodified native packets from the 09F4D961 live run, packets 11 and 12.
    constexpr std::array<std::string_view,2> captures{
        "ffffffffffffffffffffffffffffffff3a002859e00000149d00142cf59000380ec0f96205e4aaa9400000000c00",
        "ffffffffffffffffffffffffffffffff3a002859e00000189d00142cf59000380ec0f96209e4aaa941566321ac0000001000"};
    ikora::Lattice lattice; lattice.begin();
    for (std::size_t index=0; index<captures.size(); ++index) {
        const auto hex=captures[index];
        std::vector<std::byte> packet(hex.size()/2);
        const auto nibble=[](char c) { return static_cast<unsigned>(c<='9'?c-'0':c-'a'+10); };
        for (std::size_t i=0; i<packet.size(); ++i)
            packet[i]=static_cast<std::byte>(16*nibble(hex[2*i])+nibble(hex[2*i+1]));
        sense::SenseUpdate update{}; std::size_t consumed{};
        check(sense::parse_sense_update(packet,update,consumed), "captured release packet parses");
        check(consumed==361+index*32 && update.objectCount==1, "captured packet boundary exact");
        const auto& object=update.objects[0];
        check(object.hasSceneOutput, "captured native release is decoded rather than opaque");
        const auto& output=object.sceneOutput;
        check(output.generationWire==ikora::kSceneGenerationWire && output.revision==3+index,
              "captured native generation and revision preserved");
        check(object.bodyBits==107U+index*32U,
              "captured Scene body excludes the group terminator");
        check(output.eventCount==index+1 && output.events[0]==ikora::kLatticeRelease,
              "captured native release hash preserved");
        if (index) check(output.events[1]==0x5598C86BU, "captured second timer hash preserved");
        check(lattice.observe(output,true)==(index==0) && lattice.released,
              "captured release opens once and stays open after second timer");
        auto malformed=packet;
        // The next bit is the group's explicit object-list terminator,
        // following the full native 32-bit revision. It is not Scene data.
        const auto tailBit=251U+75U+32U*(index+1U);
        malformed[tailBit/8U] |= static_cast<std::byte>(1U << (7U-tailBit%8U));
        sense::SenseUpdate rejected{}; std::size_t rejectedBits{};
        const bool accepted=sense::parse_sense_update(malformed,rejected,rejectedBits);
        check(!accepted || (rejected.objectCount==1 && !rejected.objects[0].hasSceneOutput),
              "nonzero group terminator cannot publish a release");
        packet.pop_back();
        check(!sense::parse_sense_update(packet,update,consumed) && update.objectCount==0,
              "truncated captured release cannot publish an event");
    }
}
void sense_and_lattice() {
    captured_lattice_release();
    for (unsigned count : {0U,1U,8U,32U}) for (bool source : {false,true})
        for (bool following : {false,true}) {
        auto packet=sense_packet(count,source,following,ikora::kSceneGenerationWire);
        sense::SenseUpdate update{}; std::size_t consumed{};
        check(sense::parse_sense_update(packet,update,consumed), "complete variable Scene packet parses");
        check(update.objectCount==(following?2U:1U) && update.objects[0].hasSceneOutput, "coalesced boundary preserved");
        if (following) {
            auto monitor=update.objects[1];
            check(ikora::monitor_entered(monitor,24), "coalesced native entrance admitted");
            monitor.bodySecond=9;
            check(ikora::monitor_entered(monitor,24), "later positive monitor revision admitted");
            check(!ikora::monitor_entered(monitor,20), "wrong monitor slot rejected");
            monitor.bodySecond=0;
            check(!ikora::monitor_entered(monitor,24), "zero monitor revision rejected");
            monitor.bodySecond=2; monitor.bodyFirst^=0x1000000000000000ULL;
            check(!ikora::monitor_entered(monitor,24), "exit or unrelated monitor state rejected");
        }
        const auto& output=update.objects[0].sceneOutput;
        check(output.eventCount==count && output.revision==7 && output.hasSourceRevision==source, "all Scene metadata decoded");
        check(update.objects[0].bodyBits==75U+(source?31U:0U)+count*32U,
              "native Scene ends at revision before the next object or group terminator");
        ikora::Lattice lattice;
        check(!lattice.observe(output,true), "unready binding rejects output");
        lattice.begin();
        check(!lattice.observe(output,false), "unaccepted opening rejects output");
        check(lattice.observe(output,true)==(count>0) && lattice.released==(count>0), "only authored release opens device");
        check(!lattice.observe(output,true), "duplicate revision cannot replay");
        if (count) check(output.events[count-1]==ikora::kLatticeRelease, "last of32 hashes remains available");
        packet.pop_back();
        check(!sense::parse_sense_update(packet,update,consumed) && update.objectCount==0, "truncated packet clears all receipts");
    }
    auto packet=sense_packet(33,true,false);
    sense::SenseUpdate update{}; std::size_t consumed{};
    check(!sense::parse_sense_update(packet,update,consumed), "oversized event array rejects");
    auto valid=sense_packet(1,true,false);
    check(sense::parse_sense_update(valid,update,consumed), "valid output for negative cases");
    auto output=update.objects[0].sceneOutput;
    ikora::Lattice lattice; lattice.begin();
    output.generationWire^=1;
    check(!lattice.observe(output,true), "foreign generation rejects");
    output.generationWire=ikora::kSceneGenerationWire; output.events[0]=0xE0651437U;
    check(!lattice.observe(output,true), "orb-child retirement is not release");
    output.events[0]=ikora::kLatticeRelease;
    check(!lattice.observe(output,true), "stale same-revision event cannot be substituted");
    output.revision=8;
    check(lattice.observe(output,true), "new qualified native event releases once");
    check(lattice.released, "release survives authority publication without begin");
    lattice.begin(); check(!lattice.released && lattice.revision==0, "new authenticated binding resets latch");
}

void packet_ordering() {
    const std::array<std::uint8_t,3> types{43,23,1};
    const std::array<std::uint16_t,3> indices{1,16,0};
    const std::array<std::uint8_t,1> carrierType{4};
    const std::array<std::uint16_t,1> carrierIndex{0};
    // Native object descriptors, not a flag in the packet, select the optional
    // sense tail. The installed Ikora source/Scene/gate all carry both flags.
    for (const std::uint8_t descriptorFlags : {std::uint8_t{0},std::uint8_t{3}}) {
    const std::array<std::uint8_t,3> flags{descriptorFlags,descriptorFlags,descriptorFlags};
    const std::array<std::uint8_t,1> carrierFlags{descriptorFlags};
    const auto consumeNativePayload = [descriptorFlags](bits::Reader& reader, std::size_t bodyBits) {
        const auto advertised=get(reader,32);
        const auto remaining=reader.remaining_bits();
        check(get(reader,1)==1 && get(reader,1)==static_cast<unsigned>(bodyBits != 0),
              "native auth reset and body presence");
        check(reader.skip(bodyBits), "native decoder consumes reflected authority body");
        if ((descriptorFlags & wire::kSlotSenseFlag) != 0)
            check(get(reader,1)==0, "native descriptor consumes absent sense tail before next object");
        check(remaining-reader.remaining_bits()==advertised,
              "native descriptor consumption equals object remainder boundary");
    };
    for (std::uint8_t stage = 0; stage <= wire::kOmegaForestStageSettled; ++stage) {
    if (stage == wire::kOmegaOpeningStageBaseline) continue;
    for (bool requested : {false,true}) for (bool portalEnabled : {false,true}) {
        wire::Snapshot snapshot{}; snapshot.lifetime=3;
        snapshot.seedAuthoredSensors=snapshot.omegaSceneAuthority=true;
        snapshot.omegaIkoraPortalRequested=requested;
        snapshot.omegaPortalMutation=true;
        snapshot.omegaPortalEntry=portalEnabled;
        snapshot.omegaOpeningStage=stage;
        snapshot.publishOmegaOpeningTransition = stage==wire::kOmegaOpeningStageTriggered
            || stage==wire::kOmegaOpeningStageCompleted || stage==wire::kOmegaForestStageTransition;
        snapshot.roster.groupCount=snapshot.roster.topLevelGroupCount=2;
        snapshot.roster.groups[0]={ikora::kRegistry,types,flags,indices};
        snapshot.roster.groups[1]={0xBA5F26EF,carrierType,carrierFlags,carrierIndex};
        std::array<std::byte,2048> packet{}; std::size_t written{};
        check(wire::encode_sensor_auth_update(snapshot,packet,written), "ordered native-source authority packet encodes");
        bits::Reader r(std::span(packet).first(written));
        check(r.skip(wire::kLatchBitWithoutGrant+1+wire::delta_bits(2,{})), "phase2 boundary");
        check(get(r,1)==1 && get(r,32)==ikora::kRegistry && get(r,32)==0, "first group is Ikora");
        check(get(r,1)==1 && get(r,32)==ikora::kRegistry && get(r,7)==2 && get(r,16)==0x8000,
              "source comes first despite reordered extraction");
        consumeNativePayload(r,641);
        check(get(r,1)==1 && get(r,32)==ikora::kRegistry && get(r,7)==44 && get(r,16)==0x8001,
              "Scene follows its authorized source");
        consumeNativePayload(r,requested?161U:129U);
        check(get(r,1)==1 && get(r,32)==ikora::kRegistry && get(r,7)==24 && get(r,16)==0x8010,
              "exact lattice follows Scene");
        consumeNativePayload(r,147);
        check(get(r,1)==0, "lattice body and group boundary");
        unsigned carrierCount=0, carrierRecords=0;
        while (get(r,1)) {
            check(get(r,32)==0xBA5F26EF && get(r,32)==0, "only carrier group may follow Ikora");
            while (get(r,1)) {
                check(get(r,32)==0xBA5F26EF && get(r,7)==5 && get(r,16)==0x8000, "exact carrier object");
                consumeNativePayload(r,portalEnabled ? 252U : 0U);
                carrierCount+=static_cast<unsigned>(portalEnabled);
                ++carrierRecords;
            }
        }
        check(carrierRecords<=1 && carrierCount==static_cast<unsigned>(portalEnabled),
              "carrier active exactly once only after release; initial inactive seed remains framed");
        // Output storage must be unchanged on invalid structural input, including missing source.
        auto missing=snapshot;
        missing.roster.groups[0]={ikora::kRegistry,std::span(types).first(2),std::span(flags).first(2),std::span(indices).first(2)};
        packet.fill(std::byte{0xA5}); written=17;
        check(!wire::encode_sensor_auth_update(missing,packet,written) && written==0
            && std::all_of(packet.begin(),packet.end(),[](std::byte b){return b==std::byte{0xA5};}), "missing source fails before touching packet");
    }
    }
    }
}

void lair_presentation() {
    namespace omega=sunrise::state::activity::omega;
    namespace presentation=sunrise::client::hooks::bootflow::omega_presentation;
    check(omega::in_lair_dialogue_010(-1492,481,-20), "actual010 approach polygon includes doorway entry");
    check(!omega::in_lair_dialogue_010(-1507,488,-20), "010 polygon excludes AABB corner");
    check(!omega::in_lair_dialogue_010(-1492,481,-23)
        && !omega::in_lair_dialogue_010(-1492,481,3), "010 exact vertical bounds");
    check(omega::in_lair_dialogue_050(-1460,210,-30), "actual arena polygon includes central playable point");
    check(!omega::in_lair_dialogue_050(-1540,150,-30), "polygon notch excludes AABB false positive");
    check(!omega::in_lair_dialogue_050(-1460,210,4)
        && !omega::in_lair_dialogue_050(-1460,210,-57), "authored vertical bounds enforced");
    omega::LairDialogue cue;
    cue.update(0,14,true,-1492,481,-20);
    check(cue.objective_event()==0x3517D4D5U && cue.requested_mask()==0,
          "010 sets nativePursue objective without synthesizing cinematic dialogue");
    cue.update(100000,14,true,-1492,420,-25);
    check(cue.pending_row()==255 && cue.requested_mask()==0, "elapsed time and Lair arrival cannot create dialogue");
    cue.dispatched(255,100000); cue.dispatched(12,100000);
    cue.cinematic_completed(1000); cue.cinematic_completed(2000);
    cue.update(1249,14,true,-1492,420,-25);
    check(cue.pending_row()==255 && cue.requested_mask()==(1U<<12), "receipt latches but cosmeticgap holds");
    cue.update(1250,14,true,-1492,420,-25);
    check(cue.pending_row()==12, "actual cinematic receipt plus250ms offers row12");
    cue.update(1251,14,true,-1460,210,-30);
    check(cue.objective_event()==0x31A51CEBU, "050 sets defenses objective independently of queued speech");
    check(cue.pending_row()==12 && cue.requested_mask()==((1U<<12)|(1U<<13)), "arena arrival queues behind current row12");
    cue.dispatched(13,1300);
    check(cue.pending_row()==12, "unoffered native index cannot retire current row");
    cue.dispatched(12,1300); cue.dispatched(12,2400);
    cue.update(3567,14,true,-1492,420,-25);
    check(cue.pending_row()==255, "native row12 duration retained after leaving volume");
    cue.update(3568,14,true,-1492,420,-25);
    check(cue.pending_row()==13, "queued row13 follows actual dispatch and native bankduration");
    cue.dispatched(13,3600); cue.update(90000,14,true,-1460,210,-30);
    check(cue.pending_row()==255, "revisiting arena cannot replay delivered row");
    cue.update(90001,14,true,-1492,481,-20);
    check(cue.objective_event()==0x31A51CEBU, "backtracking through010 cannot regress defenses objective");
    omega::LairDialogue direct;
    direct.update(1,11,true,-1460,210,-30);
    check(direct.requested_mask()==0, "same coordinates in another bubble cannot trigger");
    direct.update(2,14,false,-1460,210,-30);
    check(direct.requested_mask()==0, "absent player cannot trigger");
    direct.update(3,14,true,-1460,210,-30);
    check(direct.pending_row()==13 && direct.requested_mask()==(1U<<13), "direct arena arrival does not synthesize skipped cinematic");
    direct.cinematic_completed(4);
    check(direct.requested_mask()==(1U<<13), "late earlier cinematic cannot regress current cue");

    std::array<std::byte,presentation::kDialogueBytes> component{};
    const auto put=[&]<class T>(std::size_t offset,T value){std::memcpy(component.data()+offset,&value,sizeof value);};
    put(0,std::uint32_t{0x80F47BDA}); put(8,std::int64_t{0x1408});
    omega::Progress progress{};
    progress.lairDialogueRequestedMask=(1U<<12)|(1U<<13);
    progress.lairDialoguePendingRow=12;
    check(presentation::sync_dialogue(component,progress)==((1U<<12)|(1U<<13)), "client repair carries14records and exact requested rows");
    check(presentation::read<std::uint64_t>(component,0x188+12*32+8)==1
        && presentation::read<std::uint8_t>(component,0x188+12*32+28)==2, "only offered row12 has playback time/mode");
    check(presentation::read<std::uint64_t>(component,0x188+13*32+8)==0
        && presentation::read<std::uint8_t>(component,0x188+13*32+28)==0, "queued13 does not compete for native audio");
    check(presentation::sync_dialogue(component,progress)==0, "same repair is inert");
    progress.lairDialoguePendingRow=13;
    check(presentation::sync_dialogue(component,progress)==((1U<<12)|(1U<<13)), "receipt retires12 and offers13");
    check(presentation::read<std::int32_t>(component,0x188+12*32+24)==1
        && presentation::read<std::uint64_t>(component,0x188+12*32+8)==0
        && presentation::read<std::uint8_t>(component,0x188+12*32+28)==0, "historical generation retained without replay");
    std::array<std::byte,presentation::kDirectiveBytes> directive{};
    presentation::write<std::uint32_t>(directive,0,0x80F47BD4U);
    presentation::write<std::int64_t>(directive,8,0xB88);
    presentation::write<std::uint32_t>(directive,0x190,0x1EBF4621U);
    const auto beforeDirective=directive;
    progress.lairObjectiveEvent=0x3517D4D5U;
    check(!presentation::sync_directive(directive,progress) && directive==beforeDirective,
          "native Lair objective prevents stale Forest packet from restoring Crown locator");
    for (auto pending : {std::uint8_t{12},std::uint8_t{13},std::uint8_t{255}}) {
        wire::Snapshot snapshot{}; snapshot.omegaSceneAuthority=snapshot.omegaDialogueArm=true;
        snapshot.omegaLairDialogueRequestedMask=(1U<<12)|(1U<<13);
        snapshot.omegaLairDialoguePendingRow=pending;
        std::array<std::byte,3000> bytes{};bits::Writer writer(bytes);
        check(wire::write_auth_body(writer,snapshot,0x82FB58B7,53,2,false), "actual type53 extended wire writes");
        bits::Reader r(bytes);check(r.skip(55), "native root reference");
        for (unsigned row=0;row<128;++row) {
            check(get(r,64)==UINT64_MAX, "native no-deadline sentinel retained");
            const bool hasTime=get(r,1)!=0;
            if(hasTime) check(get(r,64)==1, "offered row time");
            check(r.skip(55), "native absent target reference");
            const auto generation=get(r,32);const auto mode=get(r,2);
            if(row==12||row==13) {
                check(generation==0x80000001U, "historical generation stays1 on wire");
                check(hasTime==(row==pending) && mode==(row==pending?3U:1U), "only selected new cue plays on native wire");
            }
        }
        check(3000U*8U-r.remaining_bits()==writer.bit_count(), "all128rows preserve final framing");
    }
}
}
int main() {
    authority(); sense_and_lattice(); packet_ordering(); lair_presentation();
    std::printf("omega_ikora: %u checks passed\n",checks);
}
