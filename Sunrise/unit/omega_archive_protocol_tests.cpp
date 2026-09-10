#include <array>
#include <cstdio>
#include <cstdlib>

#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/bap/activity_message/sense_update.h"
#include "middleware/encoding/bit_writer.h"

namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits = sunrise::middleware::encoding::bits;

std::uint64_t digest = 1469598103934665603ULL;
void mix(std::uint64_t value) { digest = (digest ^ value) * 1099511628211ULL; }
#ifdef OMEGA_PORT_LOCAL
#include "population_packet_cases.h"
#endif

int main() {
#ifdef OMEGA_PORT_LOCAL
    population_packet_cases();
#endif
    // Exercise the native bodies over opening, Forest, Lair, Crown and ending
    // snapshots. Build this same fixture against the untouched archive as well.
    constexpr std::array<std::uint32_t, 15> registries{
        0x4786C0E0U, 0xD00142CFU, 0x82FB58B7U, 0xBA5F26EFU, 0x2763EC97U,
        0xF4D0E0B2U, 0x95FB2E01U, 0x99BD2FEBU, 0x0040BF06U, 0x0040BF05U,
        0x0040BF03U, 0x3A6CE17AU, 0x30A025E8U, 0xC40F2AF4U, 0x9D8076E4U};
    constexpr std::array<std::uint8_t, 14> types{1, 2, 4, 6, 11, 13, 17, 18, 23, 30, 35, 43, 68, 70};
    std::array<std::byte, 8192> buffer{};
    unsigned nonempty{}, packets{};
    for (std::uint8_t phase = 0; phase < 12; ++phase) {
        wire::Snapshot s{};
#ifdef OMEGA_PORT_LOCAL
        s.archiveOmega = true;
#endif
        s.playerKey = 123;
        s.lifetime = 3;
        s.seedAuthoredSensors = true;
        s.initializeMissionAuthorityRuntime = true;
        s.omegaSceneAuthority = true;
        s.omegaDialogueArm = true;
        s.omegaActiveDialogueRow = static_cast<std::uint8_t>(phase * 3);
        s.omegaDialogueGenerations.fill(phase + 1U);
        s.omegaObjectiveEvent = 0xC252E306U;
        s.omegaPortalPlayerHash = phase >= 1;
        s.omegaPortalEntry = phase >= 2;
        s.omegaIkoraLatticeReleased = phase >= 2;
        s.omegaForestVexEncounters = phase >= 3;
        s.omegaIntroRevision = 7;
        s.omegaIntroPlay = phase == 4;
        s.omegaBossGeneration = 7;
        s.omegaFirstLairGeneration = 7;
        s.omegaFirstLairLoose.fill(1);
        s.omegaFirstLairAnchor = phase >= 4;
        s.omegaFirstCannonActive = phase >= 5;
        s.omegaFinalCannonActive = phase >= 6;
        s.omegaCrownCycle = static_cast<std::uint8_t>(phase % 3 + 1);
        s.omegaCrownGeneration = 7;
        s.omegaCrownRestricted = phase >= 6;
        s.omegaCrownChargeEnabled = phase >= 7;
        s.omegaCrownChargeDunked = phase >= 8;
        s.omegaCrownEyeStatusActive = phase == 8;
        s.omegaCrownReturnLaunch = phase == 9;
        s.omegaCrownTransitLaunches = phase >= 6;
        s.omegaCrownTransitBridge = phase >= 7;
        s.omegaCrownTransitTarget = phase >= 8;
        s.omegaCrownFinalTraversal = phase >= 9;
        s.omegaCrownTransitCreated = UINT64_MAX;
        s.omegaRescueSourcesGeneration = 7;
        s.omegaRescueMarkerReadyMask = UINT16_MAX;
        s.omegaEndingRevision = 9;
        s.omegaEndingState = phase >= 10 ? 1 : 0;
        s.omegaEndingPlay = phase == 10;
        s.omegaEndingRetire = phase == 11;
        s.omegaEndingSeedRuntime = phase == 10;
        s.omegaMusicPresent = true;
        s.omegaMusic.activeCandidates = 1U << phase;
        for (const auto key : registries) {
            for (const auto type : types) {
                for (std::uint16_t index = 0; index < 96; ++index) {
                    const auto width = wire::auth_body_bits(s, key, type, index, type == 13);
                    mix(width);
                    if (width == 0) { continue; }
                    ++nonempty;
                    bits::Writer writer(buffer);
                    const bool ok = wire::write_auth_body(writer, s, key, type, index, type == 13);
                    mix(ok);
                    mix(writer.bit_count());
                    std::size_t written{};
                    if (!writer.finish(written)) { std::abort(); }
                    for (std::size_t byte = 0; byte < written; ++byte) {
                        mix(std::to_integer<unsigned char>(buffer[byte]));
                    }
                }
            }
        }
        constexpr std::array<std::uint8_t, 2> runtimeTypes{13, 17};
        constexpr std::array<std::uint8_t, 2> runtimeFlags{wire::kSlotAuthFlag, wire::kSlotAuthFlag};
        constexpr std::array<std::uint16_t, 2> runtimeIndices{0, 3};
        s.roster.groups[0] = {0x4786C0E0U, runtimeTypes, runtimeFlags, runtimeIndices};
        s.roster.playerKeyGroup = 0x4786C0E0U;
        s.roster.groupCount = s.roster.topLevelGroupCount = 1;
        s.phaseOneOnly = true;
        std::size_t written{};
        const bool ok = wire::encode_sensor_auth_update(s, buffer, written);
        mix(ok);
        mix(written);
        if (ok) { ++packets; }
        for (std::size_t byte = 0; byte < written; ++byte) {
            mix(std::to_integer<unsigned char>(buffer[byte]));
        }
    }
    std::printf("bodies=%u packets=%u digest=%016llX\n", nonempty, packets,
        static_cast<unsigned long long>(digest));
    return nonempty > 0 && packets == 12 ? 0 : 1;
}
