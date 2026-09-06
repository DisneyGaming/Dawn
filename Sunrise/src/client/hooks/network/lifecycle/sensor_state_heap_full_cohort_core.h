#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap_full_cohort {

inline constexpr std::uint16_t kSchemaVersion = 1U;
inline constexpr std::size_t kHeaderBytes = 4096U;
inline constexpr std::size_t kEventBytes = 256U;
inline constexpr std::size_t kEventCapacity = 65'536U;
inline constexpr std::size_t kNormalEventCapacity = 65'472U;
inline constexpr std::size_t kCriticalEventCapacity = 64U;
inline constexpr std::uint64_t kFileBytes = 0x01001000ULL;
inline constexpr std::size_t kRecordCapacity = 4096U;
inline constexpr std::size_t kAllocationKeyCapacity = 65'536U;
inline constexpr std::size_t kFailureCounterCapacity = 64U;
inline constexpr std::size_t kFailureCounterCount = 62U;
inline constexpr std::size_t kSiteCount = 18U;
inline constexpr std::size_t kCoreHookCount = 11U;
inline constexpr std::size_t kPayloadHashCeiling = 1024U * 1024U;

using ImageSha256 = std::array<std::byte, 32U>;

inline constexpr ImageSha256 kPinnedPackedImageSha256{
    std::byte{0x81U}, std::byte{0x96U}, std::byte{0x43U}, std::byte{0x80U},
    std::byte{0x66U}, std::byte{0x4EU}, std::byte{0x7FU}, std::byte{0xCEU},
    std::byte{0xE3U}, std::byte{0xC6U}, std::byte{0x20U}, std::byte{0x08U},
    std::byte{0x5AU}, std::byte{0x15U}, std::byte{0x7FU}, std::byte{0xDEU},
    std::byte{0xAFU}, std::byte{0x91U}, std::byte{0xFEU}, std::byte{0xFAU},
    std::byte{0xCFU}, std::byte{0x72U}, std::byte{0x14U}, std::byte{0x90U},
    std::byte{0x78U}, std::byte{0x20U}, std::byte{0xF1U}, std::byte{0x88U},
    std::byte{0xBBU}, std::byte{0xEBU}, std::byte{0x4CU}, std::byte{0xEDU},
};

inline constexpr ImageSha256 kPinnedUnpackedImageSha256{
    std::byte{0x87U}, std::byte{0x13U}, std::byte{0xD1U}, std::byte{0x5EU},
    std::byte{0x3DU}, std::byte{0x05U}, std::byte{0xB2U}, std::byte{0x6FU},
    std::byte{0x9EU}, std::byte{0x25U}, std::byte{0x9EU}, std::byte{0x02U},
    std::byte{0xB0U}, std::byte{0xF2U}, std::byte{0x9BU}, std::byte{0xC1U},
    std::byte{0xE0U}, std::byte{0x00U}, std::byte{0xE4U}, std::byte{0xB0U},
    std::byte{0xC6U}, std::byte{0x2CU}, std::byte{0xA2U}, std::byte{0xCCU},
    std::byte{0x87U}, std::byte{0xC3U}, std::byte{0x85U}, std::byte{0x97U},
    std::byte{0x18U}, std::byte{0x6CU}, std::byte{0xC3U}, std::byte{0xBDU},
};

enum class SiteAction : std::uint8_t {
    coreDetour,
    externalOwner,
    validatedHelper,
    supportingEvidence,
};

struct SiteContract final {
    std::uint32_t rva{};
    std::uint32_t packedRawOffset{};
    std::uint8_t prefixLength{};
    SiteAction action{};
    std::array<std::byte, 17U> expectedMapped{};
    std::array<std::byte, 17U> packedRaw{};
    const char* role{};
};

template <std::size_t ExpectedSize, std::size_t RawSize>
[[nodiscard]] consteval SiteContract site(std::uint32_t rva,
                                          std::uint32_t rawOffset,
                                          SiteAction action,
                                          const std::array<std::byte, ExpectedSize>& expected,
                                          const std::array<std::byte, RawSize>& raw,
                                          const char* role) noexcept {
    static_assert(ExpectedSize <= 17U && RawSize <= 17U && ExpectedSize == RawSize);
    SiteContract output{};
    output.rva = rva;
    output.packedRawOffset = rawOffset;
    output.prefixLength = static_cast<std::uint8_t>(ExpectedSize);
    output.action = action;
    output.role = role;
    for (std::size_t index = 0U; index < ExpectedSize; ++index) {
        output.expectedMapped[index] = expected[index];
        output.packedRaw[index] = raw[index];
    }
    return output;
}

#define SHL1_BYTES(...) std::to_array<std::byte>({__VA_ARGS__})
#define SHL1_B(value) std::byte{0x##value##U}
inline constexpr std::array kSiteManifest{
    site(0x4D6EB0U, 0x4D64B0U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(8B),SHL1_B(C4),SHL1_B(57),SHL1_B(41),SHL1_B(56),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(58),SHL1_B(48),SHL1_B(89),SHL1_B(58),SHL1_B(10),SHL1_B(4C),SHL1_B(8B)),
         SHL1_BYTES(SHL1_B(FF),SHL1_B(71),SHL1_B(71),SHL1_B(70),SHL1_B(36),SHL1_B(D5),SHL1_B(30),SHL1_B(16),SHL1_B(DC),SHL1_B(14),SHL1_B(05),SHL1_B(5F),SHL1_B(DE),SHL1_B(E2),SHL1_B(1D),SHL1_B(E1)), "sensor_table_insert"),
    site(0x9FEE40U, 0x9FE440U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(10),SHL1_B(48),SHL1_B(89),SHL1_B(6C),SHL1_B(24),SHL1_B(18),SHL1_B(56),SHL1_B(57),SHL1_B(41),SHL1_B(56),SHL1_B(48),SHL1_B(83)),
         SHL1_BYTES(SHL1_B(35),SHL1_B(B5),SHL1_B(7B),SHL1_B(E5),SHL1_B(DB),SHL1_B(17),SHL1_B(7A),SHL1_B(20),SHL1_B(C0),SHL1_B(62),SHL1_B(80),SHL1_B(17),SHL1_B(93),SHL1_B(96),SHL1_B(68),SHL1_B(B4)), "record_construct"),
    site(0x9FD320U, 0x9FC920U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(10),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(18),SHL1_B(57),SHL1_B(48),SHL1_B(81),SHL1_B(EC),SHL1_B(40),SHL1_B(01)),
         SHL1_BYTES(SHL1_B(78),SHL1_B(12),SHL1_B(9D),SHL1_B(F4),SHL1_B(EC),SHL1_B(C7),SHL1_B(C3),SHL1_B(13),SHL1_B(10),SHL1_B(32),SHL1_B(EA),SHL1_B(24),SHL1_B(F4),SHL1_B(47),SHL1_B(B4),SHL1_B(F5)), "allocate_member"),
    site(0x323D40U, 0x323340U, SiteAction::validatedHelper,
         SHL1_BYTES(SHL1_B(0F),SHL1_B(B7),SHL1_B(C1),SHL1_B(48),SHL1_B(8D),SHL1_B(15),SHL1_B(36),SHL1_B(5E),SHL1_B(DE),SHL1_B(01),SHL1_B(48),SHL1_B(0F),SHL1_B(AF),SHL1_B(05),SHL1_B(3E),SHL1_B(5E)),
         SHL1_BYTES(SHL1_B(32),SHL1_B(6A),SHL1_B(D2),SHL1_B(B4),SHL1_B(B2),SHL1_B(52),SHL1_B(F7),SHL1_B(F7),SHL1_B(B0),SHL1_B(B5),SHL1_B(BD),SHL1_B(58),SHL1_B(A0),SHL1_B(3A),SHL1_B(FF),SHL1_B(B9)), "heap_resolver"),
    site(0x321A20U, 0x321020U, SiteAction::supportingEvidence,
         SHL1_BYTES(SHL1_B(40),SHL1_B(53),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(30),SHL1_B(33),SHL1_B(C0),SHL1_B(48),SHL1_B(8B),SHL1_B(DA),SHL1_B(89),SHL1_B(44),SHL1_B(24),SHL1_B(28),SHL1_B(89)),
         SHL1_BYTES(SHL1_B(7F),SHL1_B(CB),SHL1_B(68),SHL1_B(A2),SHL1_B(C0),SHL1_B(12),SHL1_B(A8),SHL1_B(64),SHL1_B(CA),SHL1_B(44),SHL1_B(00),SHL1_B(77),SHL1_B(F4),SHL1_B(33),SHL1_B(3A),SHL1_B(F4)), "relative_allocator"),
    site(0x321650U, 0x320C50U, SiteAction::supportingEvidence,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(10),SHL1_B(48),SHL1_B(89),SHL1_B(6C),SHL1_B(24),SHL1_B(18),SHL1_B(56),SHL1_B(57),SHL1_B(41),SHL1_B(54),SHL1_B(41),SHL1_B(56)),
         SHL1_BYTES(SHL1_B(19),SHL1_B(40),SHL1_B(3C),SHL1_B(92),SHL1_B(4F),SHL1_B(7A),SHL1_B(BE),SHL1_B(3B),SHL1_B(D8),SHL1_B(CE),SHL1_B(87),SHL1_B(25),SHL1_B(F0),SHL1_B(2E),SHL1_B(1D),SHL1_B(C5)), "allocator_extent"),
    site(0x9FEC60U, 0x9FE260U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(40),SHL1_B(53),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(20),SHL1_B(48),SHL1_B(8B),SHL1_B(D9),SHL1_B(0F),SHL1_B(B7),SHL1_B(49),SHL1_B(60),SHL1_B(E8),SHL1_B(CE),SHL1_B(50)),
         SHL1_BYTES(SHL1_B(AC),SHL1_B(C5),SHL1_B(A2),SHL1_B(23),SHL1_B(95),SHL1_B(2E),SHL1_B(8F),SHL1_B(90),SHL1_B(99),SHL1_B(C3),SHL1_B(2E),SHL1_B(B4),SHL1_B(0F),SHL1_B(89),SHL1_B(AE),SHL1_B(2A)), "receive_resolver"),
    site(0x4C4BA0U, 0x4C41A0U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(10),SHL1_B(57),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(30),SHL1_B(8B)),
         SHL1_BYTES(SHL1_B(95),SHL1_B(D5),SHL1_B(AF),SHL1_B(CC),SHL1_B(7F),SHL1_B(52),SHL1_B(15),SHL1_B(C8),SHL1_B(50),SHL1_B(B6),SHL1_B(5C),SHL1_B(67),SHL1_B(E8),SHL1_B(D6),SHL1_B(9A),SHL1_B(CA)), "initialize_writer"),
    site(0x4C7BC0U, 0x4C71C0U, SiteAction::supportingEvidence,
         SHL1_BYTES(SHL1_B(E9),SHL1_B(DB),SHL1_B(CF),SHL1_B(FF),SHL1_B(FF)),
         SHL1_BYTES(SHL1_B(D2),SHL1_B(FD),SHL1_B(0A),SHL1_B(CC),SHL1_B(E1)), "initializer_thunk"),
    site(0x4C72E0U, 0x4C68E0U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(20),SHL1_B(55),SHL1_B(56),SHL1_B(57),SHL1_B(41),SHL1_B(54),SHL1_B(41),SHL1_B(55),SHL1_B(41),SHL1_B(56),SHL1_B(41),SHL1_B(57)),
         SHL1_BYTES(SHL1_B(28),SHL1_B(93),SHL1_B(2B),SHL1_B(FC),SHL1_B(FE),SHL1_B(08),SHL1_B(73),SHL1_B(DA),SHL1_B(F0),SHL1_B(96),SHL1_B(F4),SHL1_B(50),SHL1_B(2F),SHL1_B(1D),SHL1_B(18),SHL1_B(98)), "decode_writer"),
    site(0x4D7470U, 0x4D6A70U, SiteAction::externalOwner,
         SHL1_BYTES(SHL1_B(40),SHL1_B(57),SHL1_B(41),SHL1_B(55),SHL1_B(41),SHL1_B(56),SHL1_B(41),SHL1_B(57),SHL1_B(B8),SHL1_B(98),SHL1_B(78),SHL1_B(00),SHL1_B(00),SHL1_B(E8),SHL1_B(AE),SHL1_B(56)),
         SHL1_BYTES(SHL1_B(BC),SHL1_B(A8),SHL1_B(84),SHL1_B(83),SHL1_B(AF),SHL1_B(96),SHL1_B(2E),SHL1_B(C3),SHL1_B(A9),SHL1_B(9A),SHL1_B(1C),SHL1_B(62),SHL1_B(5B),SHL1_B(26),SHL1_B(93),SHL1_B(80)), "receive_owner"),
    site(0x4D7C00U, 0x4D7200U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(48),SHL1_B(89),SHL1_B(6C),SHL1_B(24),SHL1_B(10),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(18),SHL1_B(57)),
         SHL1_BYTES(SHL1_B(30),SHL1_B(EA),SHL1_B(DC),SHL1_B(A9),SHL1_B(D7),SHL1_B(E0),SHL1_B(D8),SHL1_B(07),SHL1_B(22),SHL1_B(39),SHL1_B(DA),SHL1_B(18),SHL1_B(63),SHL1_B(68),SHL1_B(3E),SHL1_B(5A)), "sensor_table_remove"),
    site(0x9FE2C0U, 0x9FD8C0U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(57),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(50),SHL1_B(0F),SHL1_B(57),SHL1_B(C0),SHL1_B(C7),SHL1_B(44),SHL1_B(24)),
         SHL1_BYTES(SHL1_B(9A),SHL1_B(C9),SHL1_B(8E),SHL1_B(48),SHL1_B(BF),SHL1_B(DD),SHL1_B(F4),SHL1_B(1E),SHL1_B(BA),SHL1_B(8D),SHL1_B(42),SHL1_B(71),SHL1_B(AB),SHL1_B(7B),SHL1_B(46),SHL1_B(1D)), "record_destroy"),
    site(0x324D90U, 0x324390U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(10),SHL1_B(57),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(20),SHL1_B(48)),
         SHL1_BYTES(SHL1_B(BC),SHL1_B(27),SHL1_B(80),SHL1_B(9F),SHL1_B(98),SHL1_B(BC),SHL1_B(CE),SHL1_B(00),SHL1_B(1D),SHL1_B(53),SHL1_B(2A),SHL1_B(0A),SHL1_B(75),SHL1_B(2E),SHL1_B(5B),SHL1_B(72)), "relative_free"),
    site(0x321D70U, 0x321370U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(48),SHL1_B(89),SHL1_B(6C),SHL1_B(24),SHL1_B(10),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(18),SHL1_B(57)),
         SHL1_BYTES(SHL1_B(1C),SHL1_B(D0),SHL1_B(F0),SHL1_B(FD),SHL1_B(AC),SHL1_B(51),SHL1_B(B0),SHL1_B(63),SHL1_B(BB),SHL1_B(5A),SHL1_B(27),SHL1_B(D4),SHL1_B(4A),SHL1_B(3C),SHL1_B(C6),SHL1_B(4E)), "index_unlink"),
    site(0x3C8EB0U, 0x3C84B0U, SiteAction::externalOwner,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(10),SHL1_B(48),SHL1_B(89),SHL1_B(7C),SHL1_B(24),SHL1_B(18),SHL1_B(4C),SHL1_B(89)),
         SHL1_BYTES(SHL1_B(11),SHL1_B(41),SHL1_B(3D),SHL1_B(7D),SHL1_B(B9),SHL1_B(42),SHL1_B(7F),SHL1_B(0D),SHL1_B(9C),SHL1_B(B6),SHL1_B(94),SHL1_B(7A),SHL1_B(F7),SHL1_B(74),SHL1_B(0C),SHL1_B(66),SHL1_B(8B)), "drop_all"),
    site(0x3CC990U, 0x3CBF90U, SiteAction::coreDetour,
         SHL1_BYTES(SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(28),SHL1_B(E8),SHL1_B(17),SHL1_B(C5),SHL1_B(FF),SHL1_B(FF),SHL1_B(E8),SHL1_B(02),SHL1_B(BD),SHL1_B(02),SHL1_B(00),SHL1_B(E8),SHL1_B(BD)),
         SHL1_BYTES(SHL1_B(21),SHL1_B(C5),SHL1_B(98),SHL1_B(71),SHL1_B(CA),SHL1_B(90),SHL1_B(99),SHL1_B(B8),SHL1_B(41),SHL1_B(75),SHL1_B(E3),SHL1_B(C6),SHL1_B(B7),SHL1_B(3E),SHL1_B(0B),SHL1_B(2D)), "network_reset"),
    site(0x4DB4F0U, 0x4DAAF0U, SiteAction::supportingEvidence,
         SHL1_BYTES(SHL1_B(48),SHL1_B(89),SHL1_B(5C),SHL1_B(24),SHL1_B(08),SHL1_B(48),SHL1_B(89),SHL1_B(74),SHL1_B(24),SHL1_B(10),SHL1_B(57),SHL1_B(48),SHL1_B(83),SHL1_B(EC),SHL1_B(40)),
         SHL1_BYTES(SHL1_B(1F),SHL1_B(27),SHL1_B(0D),SHL1_B(2A),SHL1_B(8E),SHL1_B(17),SHL1_B(72),SHL1_B(5D),SHL1_B(25),SHL1_B(FC),SHL1_B(BC),SHL1_B(18),SHL1_B(B6),SHL1_B(64),SHL1_B(9C)), "table_sweep"),
};
#undef SHL1_B
#undef SHL1_BYTES

static_assert(kSiteManifest.size() == kSiteCount);

enum class FailureCounter : std::uint8_t {
    packedOpenFailure,
    packedReadFailure,
    packedHashMismatch,
    peIdentityMismatch,
    codeViewMismatch,
    mappedTargetBoundsFailure,
    mappedTargetReadFailure,
    mappedPrefixMismatch,
    ledgerCreateFailure,
    ledgerSizeFailure,
    ledgerMapFailure,
    ledgerPretouchFailure,
    attachFailure,
    originalPublicationFailure,
    receiveOwnerReadinessFailure,
    preReadyCalls,
    recordBusy,
    recordFull,
    recordGenerationExhausted,
    recordReadRace,
    staleRecordCommit,
    staleRecordAssociate,
    staleRecordRetire,
    missingDestroySnapshot,
    allocationIndexBusy,
    allocationIndexFull,
    allocationIndexReadRace,
    allocationIndexStaleBinding,
    trackedMissingPayloadKey,
    trackedMissingHeaderKey,
    normalEventFull,
    criticalEventFull,
    writerPairPartial,
    eventClaimExhausted,
    guardVirtualQueryReject,
    guardPageReject,
    guardAccessViolation,
    guardInPageError,
    guardMisalignment,
    checkedArithmeticFailure,
    payloadHashCeiling,
    tlsMissingInsert,
    tlsMissingConstructor,
    tlsMissingReceive,
    tlsMissingDestroy,
    tlsMissingDrop,
    tlsMisnest,
    lifecycleSnapshotUnavailable,
    dropTableSummaryOverflow,
    freeTicketBusy,
    freeTicketExhausted,
    trackedUnmatchedFree,
    trackedUnmatchedUnlink,
    destructorMemberOrderFailure,
    postDestroyCountRelativeFailure,
    postDestroySelectorFailure,
    postAssertContamination,
    flushViewFailure,
    flushFileFailure,
    detachDeferred,
    detachFailure,
    nonzeroInflightAtFinalize,
    count,
};
static_assert(static_cast<std::size_t>(FailureCounter::count) == kFailureCounterCount);

enum ValidBits : std::uint32_t {
    validActivityGeneration = 1U << 0U,
    validConnectionGeneration = 1U << 1U,
    validRegionGeneration = 1U << 2U,
    validDropGeneration = 1U << 3U,
    validPeerTable = 1U << 4U,
    validPeerViewDerived = 1U << 5U,
    validDatum = 1U << 6U,
    validRecord = 1U << 7U,
    validRecordGeneration = 1U << 8U,
    validSensorIdentity = 1U << 9U,
    validRecordMetadata = 1U << 10U,
    validMemberTriple = 1U << 11U,
    validHeap = 1U << 12U,
    validHeapBase = 1U << 13U,
    validPayloadRange = 1U << 14U,
    validHeader = 1U << 15U,
    validCurrentLinks = 1U << 16U,
    validPreviousRelation = 1U << 17U,
    validNextRelation = 1U << 18U,
    validStream = 1U << 19U,
    validStreamBits = 1U << 20U,
    validSchemaKey = 1U << 21U,
    validReceiveResolver = 1U << 22U,
    validWriterDestination = 1U << 23U,
    validNativeResult = 1U << 24U,
    validPayloadHash = 1U << 25U,
    validRecordHash = 1U << 26U,
    validDirectCallsite = 1U << 27U,
    validFreeOrdinal = 1U << 28U,
    validDestroyTls = 1U << 29U,
    validPairedEvent = 1U << 30U,
    validCriticalPersistence = 1U << 31U,
};

enum FlagBits : std::uint32_t {
    flagTargetAllocationTracked = 1U << 0U,
    flagGuardedReadFault = 1U << 1U,
    flagSelectorChanged = 1U << 2U,
    flagRelativePayloadChanged = 1U << 3U,
    flagAbsoluteDestinationChanged = 1U << 4U,
    flagWriterSchemaMismatch = 1U << 5U,
    flagWriterDestinationMismatch = 1U << 6U,
    flagCountOrExtentMismatch = 1U << 7U,
    flagPreviousPredicateBad = 1U << 8U,
    flagNextPredicateBad = 1U << 9U,
    flagDuplicateFree = 1U << 10U,
    flagActiveRecordReuse = 1U << 11U,
    flagCapacityLoss = 1U << 12U,
    flagPostFirstHeapAssert = 1U << 13U,
    flagQuiescingSkipped = 1U << 14U,
    flagWriteWatch = 1U << 15U,
};

enum InvariantBits : std::uint8_t {
    invariantPreviousGood = 1U << 0U,
    invariantNextGood = 1U << 1U,
    invariantHeadGood = 1U << 2U,
    invariantTailGood = 1U << 3U,
};

enum class EventKind : std::uint8_t {
    construct = 1U,
    allocation,
    resolve,
    initialize,
    decode,
    remove,
    destroy,
    free,
    unlink,
    drop,
    reset,
    assertion,
    control,
};

enum class EventPhase : std::uint8_t { enter = 1U, pre, post, exit };
enum class Member : std::uint8_t { none, receivedAuth, extractedSense, receivedSense };
enum class EventForm : std::uint8_t {
    heapSnapshot = 1U,
    writerMeta,
    writerLinks,
    dropSummary,
    control,
    receiveExit,
};
enum class FreezeReason : std::uint32_t {
    none,
    writerGoodToBad,
    duplicateFree,
    badPreviousPredicate,
    badNextPredicate,
    trackedReadFailure,
    exactHeapAssert,
    storageFailure,
    lifecycleStop,
};
enum class Verdict : std::uint32_t { inconclusive, conclusiveDefect, conclusiveBalanced };
enum class Readiness : std::uint32_t { disabled, unavailable, coreAttached, ready, quiescing };
enum class FlushState : std::uint32_t { none, requested, viewFailed, fileFailed, durable };
enum class DetachResult : std::uint32_t { none, deferred, failed, removed };

struct VerdictEvidence final {
    FreezeReason freezeReason{FreezeReason::none};
    std::uint64_t firstBadOperationSequence{};
    std::uint32_t completeCohorts{};
    std::uint32_t balancedCohorts{};
    bool imageIdentityComplete{};
    bool hooksAndOwnersComplete{};
    bool lifecycleGenerationsComplete{};
    bool victimEvidenceComplete{};
    bool eventStoreComplete{};
    bool registryComplete{};
    bool persistenceDurable{};
    bool detachRemoved{};
    bool contaminated{};
};

[[nodiscard]] constexpr bool concrete_defect(FreezeReason reason) noexcept {
    return reason == FreezeReason::writerGoodToBad || reason == FreezeReason::duplicateFree
           || reason == FreezeReason::badPreviousPredicate
           || reason == FreezeReason::badNextPredicate;
}

[[nodiscard]] constexpr Verdict reduce_verdict(const VerdictEvidence& evidence) noexcept {
    const bool complete = evidence.imageIdentityComplete && evidence.hooksAndOwnersComplete
                          && evidence.lifecycleGenerationsComplete
                          && evidence.victimEvidenceComplete && evidence.eventStoreComplete
                          && evidence.registryComplete && evidence.persistenceDurable
                          && evidence.detachRemoved && !evidence.contaminated;
    if (!complete) {
        return Verdict::inconclusive;
    }
    if (concrete_defect(evidence.freezeReason)
        && evidence.firstBadOperationSequence != 0U) {
        return Verdict::conclusiveDefect;
    }
    constexpr std::uint32_t kRequiredBalancedCohorts = 2U;
    if ((evidence.freezeReason == FreezeReason::none
         || evidence.freezeReason == FreezeReason::lifecycleStop)
        && evidence.firstBadOperationSequence == 0U
        && evidence.completeCohorts >= kRequiredBalancedCohorts
        && evidence.balancedCohorts == evidence.completeCohorts) {
        return Verdict::conclusiveBalanced;
    }
    return Verdict::inconclusive;
}

[[nodiscard]] constexpr FreezeReason classify_failure(std::uint32_t beforeFlags,
                                                      std::uint32_t afterFlags,
                                                      std::uint32_t freeOrdinal = 0U) noexcept {
    if ((afterFlags & flagGuardedReadFault) != 0U) {
        return FreezeReason::trackedReadFailure;
    }
    if (freeOrdinal > 1U) {
        return FreezeReason::duplicateFree;
    }
    if ((afterFlags & flagPreviousPredicateBad) != 0U) {
        return FreezeReason::badPreviousPredicate;
    }
    if ((afterFlags & flagNextPredicateBad) != 0U) {
        return FreezeReason::badNextPredicate;
    }
    constexpr std::uint32_t bad = flagPreviousPredicateBad | flagNextPredicateBad
                                  | flagCountOrExtentMismatch | flagSelectorChanged
                                  | flagRelativePayloadChanged | flagAbsoluteDestinationChanged;
    if ((beforeFlags & bad) == 0U && (afterFlags & bad) != 0U) {
        return FreezeReason::writerGoodToBad;
    }
    return FreezeReason::none;
}

struct HeapSnapshotPayload final {
    std::uint64_t relativePayload{};
    std::uint64_t heap{};
    std::uint64_t heapBase{};
    std::uint64_t relativeHeader{};
    std::uint64_t next{};
    std::uint64_t previous{};
    std::uint64_t previousNext{};
    std::uint64_t nextPrevious{};
    std::uint64_t head{};
    std::uint64_t tail{};
    std::uint64_t payloadHash{};
    std::uint64_t recordHash{};
    std::uint32_t sizeFlags{};
    std::uint32_t count{};
    std::uint32_t freeOrdinal{};
    std::uint32_t schema{};
    std::uint16_t selector{};
    std::uint8_t headerWidth{};
    std::uint8_t invariant{};
    std::uint32_t bitsBefore{};
    std::uint32_t bitsAfter{};
    std::uint32_t nativeFlags{};
};

struct WriterMetaPayload final {
    std::uint64_t stream{};
    std::uint64_t destination{};
    std::uint64_t resolverDestination{};
    std::uint64_t schemaKeyPointer{};
    std::uint64_t nativeResult{};
    std::uint64_t relativePayload{};
    std::uint64_t heap{};
    std::uint64_t heapBase{};
    std::uint64_t relativeHeader{};
    std::uint64_t payloadHash{};
    std::uint64_t recordHash{};
    std::uint32_t schemaKeyValue{};
    std::uint32_t schema{};
    std::uint32_t count{};
    std::uint32_t bitsBefore{};
    std::uint32_t bitsAfter{};
    std::uint32_t nativeFlags{};
    std::uint16_t selector{};
    std::uint8_t headerWidth{};
    std::uint8_t invariant{};
    std::uint32_t rawReturnRva{};
    std::uint64_t companionSequence{};
};

struct DropSummaryPayload final {
    std::uint64_t peerView{};
    std::uint32_t activeBefore{};
    std::uint32_t activeAfter{};
    std::uint32_t tableCount{};
    std::uint32_t distinctTables{};
    std::uint32_t recordHighWater{};
    std::uint32_t attachMask{};
    std::array<std::byte, 96U> reserved{};
};

struct ControlPayload final {
    std::uint32_t freezeReason{};
    std::uint32_t flushState{};
    std::uint64_t freezeSequence{};
    std::uint64_t assertQpc{};
    std::uint32_t assertThreadId{};
    std::uint32_t verdict{};
    std::uint64_t firstBadOperationSequence{};
    std::uint64_t normalClaimHighWater{};
    std::uint64_t criticalClaimHighWater{};
    std::array<std::byte, 72U> reserved{};
};

enum ReceiveExitValidBits : std::uint32_t {
    receiveExitBitsBefore = 1U << 0U,
    receiveExitBitsAfter = 1U << 1U,
    receiveExitLastRecordBits = 1U << 2U,
    receiveExitOwner = 1U << 3U,
    receiveExitDirty = 1U << 4U,
    receiveExitReceivedSense = 1U << 5U,
    receiveExitNativeResult = 1U << 6U,
};

struct ReceiveExitPayload final {
    std::uint64_t stream{};
    std::uint64_t nativeResult{};
    std::uint32_t bitsBefore{};
    std::uint32_t bitsAfter{};
    std::uint32_t lastRecordBits{};
    std::uint32_t owner{};
    std::uint32_t fieldValid{};
    std::uint8_t dirty{};
    std::uint8_t receivedSense{};
    std::array<std::byte, 90U> reserved{};
};

union EventPayload final {
    HeapSnapshotPayload heapSnapshot;
    WriterMetaPayload writerMeta;
    HeapSnapshotPayload writerLinks;
    DropSummaryPayload dropSummary;
    ControlPayload control;
    ReceiveExitPayload receiveExit;
    std::array<std::byte, 128U> bytes;
};

struct alignas(8) EventV1 final {
    std::atomic<std::uint64_t> commitSequence{};
    std::uint64_t qpc{};
    std::uint64_t tick{};
    std::uint32_t threadId{};
    std::uint32_t callerRva{};
    std::uint32_t valid{};
    std::uint32_t flags{};
    std::uint64_t activityGeneration{};
    std::uint64_t connectionGeneration{};
    std::uint64_t regionGeneration{};
    std::uint64_t dropGeneration{};
    std::uint64_t record{};
    std::uint64_t peerTable{};
    std::uint64_t sensorKey{};
    std::uint32_t recordGeneration{};
    std::uint32_t datum{};
    std::uint32_t timeoutOrdinal{};
    std::uint32_t resumeOrdinal{};
    std::uint32_t authSchema{};
    std::uint32_t senseSchema{};
    std::uint8_t kind{};
    std::uint8_t phase{};
    std::uint8_t member{};
    std::uint8_t form{};
    std::uint32_t operationOrdinal{};
    EventPayload payload{};
};

struct SiteManifestRecord final {
    std::uint32_t rva{};
    std::uint32_t packedRawOffset{};
    std::uint8_t prefixLength{};
    std::uint8_t action{};
    std::uint16_t reserved0{};
    std::array<std::byte, 17U> expectedMapped{};
    std::array<std::byte, 17U> observedMapped{};
    std::array<std::byte, 17U> packedRaw{};
    std::array<char, 32U> role{};
    std::array<std::byte, 17U> reserved1{};
};

struct alignas(8) Shl1Header final {
    std::array<char, 4U> magic{};
    std::uint16_t schemaVersion{};
    std::uint16_t headerBytes{};
    std::uint16_t eventBytes{};
    std::uint8_t pointerWidth{};
    std::uint8_t endianness{};
    std::uint32_t capacity{};
    std::uint32_t normalCapacity{};
    std::uint32_t criticalCapacity{};
    std::uint32_t processId{};
    std::uint32_t siteCount{};
    std::uint64_t runId{};
    std::uint64_t loadedImageBase{};
    std::uint32_t sizeOfImage{};
    std::uint32_t peTimestamp{};
    std::uint32_t peChecksum{};
    std::uint32_t entryRva{};
    std::uint64_t packedFileSize{};
    ImageSha256 packedSha256{};
    ImageSha256 unpackedSha256Provenance{};
    std::array<std::byte, 16U> codeViewGuid{};
    std::uint32_t codeViewAge{};
    std::atomic<std::uint32_t> attachMask{};
    std::atomic<std::uint32_t> validationMask{};
    std::atomic<std::uint32_t> readinessState{};
    std::uint64_t qpcFrequency{};
    std::atomic<std::uint64_t> normalClaim{};
    std::atomic<std::uint64_t> criticalClaim{};
    std::atomic<std::uint64_t> committedCount{};
    std::atomic<std::uint64_t> droppedCount{};
    std::atomic<std::uint64_t> freezeSequence{};
    std::atomic<std::uint64_t> freezeQpc{};
    std::atomic<std::uint32_t> freezeReason{};
    std::atomic<std::uint32_t> assertSeen{};
    std::atomic<std::uint32_t> flushState{};
    std::atomic<std::uint32_t> verdict{};
    std::atomic<std::uint32_t> recordHighWater{};
    std::atomic<std::uint32_t> allocationIndexHighWater{};
    std::atomic<std::uint32_t> eventHighWater{};
    std::atomic<std::uint32_t> detachResult{};
    std::atomic<std::uint32_t> inFlight{};
    std::atomic<std::uint32_t> textDrainIndex{};
    std::atomic<std::uint32_t> timeoutOrdinal{};
    std::atomic<std::uint32_t> resumeOrdinal{};
    std::atomic<std::uint64_t> dropGeneration{};
    std::uint32_t counterCount{};
    std::uint32_t siteManifestOffset{};
    std::uint32_t eventRegionOffset{};
    std::array<std::byte, 28U> reserved0{};
    std::array<std::atomic<std::uint64_t>, kFailureCounterCapacity> failureCounters{};
    std::array<std::byte, 192U> provenanceReserved{};
    std::array<SiteManifestRecord, kSiteCount> siteManifest{};
    std::array<std::byte, 32U> siteManifestReserved{};
    std::array<std::byte, 1024U> reserved1{};
};

static_assert(sizeof(HeapSnapshotPayload) == 128U);
static_assert(sizeof(WriterMetaPayload) == 128U);
static_assert(sizeof(DropSummaryPayload) == 128U);
static_assert(sizeof(ControlPayload) == 128U);
static_assert(sizeof(ReceiveExitPayload) == 128U);
static_assert(sizeof(EventPayload) == 128U);
static_assert(sizeof(EventV1) == kEventBytes);
static_assert(alignof(EventV1) >= 8U);
static_assert(offsetof(EventV1, qpc) == 8U);
static_assert(offsetof(EventV1, tick) == 16U);
static_assert(offsetof(EventV1, threadId) == 24U);
static_assert(offsetof(EventV1, callerRva) == 28U);
static_assert(offsetof(EventV1, valid) == 32U);
static_assert(offsetof(EventV1, flags) == 36U);
static_assert(offsetof(EventV1, activityGeneration) == 40U);
static_assert(offsetof(EventV1, connectionGeneration) == 48U);
static_assert(offsetof(EventV1, regionGeneration) == 56U);
static_assert(offsetof(EventV1, dropGeneration) == 64U);
static_assert(offsetof(EventV1, record) == 72U);
static_assert(offsetof(EventV1, peerTable) == 80U);
static_assert(offsetof(EventV1, sensorKey) == 88U);
static_assert(offsetof(EventV1, recordGeneration) == 96U);
static_assert(offsetof(EventV1, datum) == 100U);
static_assert(offsetof(EventV1, timeoutOrdinal) == 104U);
static_assert(offsetof(EventV1, resumeOrdinal) == 108U);
static_assert(offsetof(EventV1, authSchema) == 112U);
static_assert(offsetof(EventV1, senseSchema) == 116U);
static_assert(offsetof(EventV1, kind) == 120U);
static_assert(offsetof(EventV1, phase) == 121U);
static_assert(offsetof(EventV1, member) == 122U);
static_assert(offsetof(EventV1, form) == 123U);
static_assert(offsetof(EventV1, operationOrdinal) == 124U);
static_assert(offsetof(EventV1, payload) == 128U);
static_assert(offsetof(WriterMetaPayload, companionSequence) == 120U);
static_assert(sizeof(SiteManifestRecord) == 112U);
static_assert(sizeof(Shl1Header) == kHeaderBytes);
static_assert(offsetof(Shl1Header, attachMask) == 156U);
static_assert(offsetof(Shl1Header, qpcFrequency) == 168U);
static_assert(offsetof(Shl1Header, normalClaim) == 176U);
static_assert(offsetof(Shl1Header, failureCounters) == 320U);
static_assert(offsetof(Shl1Header, siteManifest) == 1024U);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);

inline void account(Shl1Header* header, FailureCounter counter, std::uint64_t amount = 1U) noexcept {
    if (header != nullptr) {
        header->failureCounters[static_cast<std::size_t>(counter)].fetch_add(
            amount, std::memory_order_relaxed);
        header->verdict.store(static_cast<std::uint32_t>(Verdict::inconclusive),
                              std::memory_order_relaxed);
    }
}

[[nodiscard]] inline bool checked_add(std::uintptr_t left,
                                      std::uint64_t right,
                                      std::uintptr_t& output) noexcept {
    if (right > (std::numeric_limits<std::uintptr_t>::max)() - left) {
        output = 0U;
        return false;
    }
    output = left + static_cast<std::uintptr_t>(right);
    return true;
}

[[nodiscard]] inline bool checked_sub(std::uint64_t left,
                                      std::uint64_t right,
                                      std::uint64_t& output) noexcept {
    if (left < right) {
        output = 0U;
        return false;
    }
    output = left - right;
    return true;
}

[[nodiscard]] inline bool align16_extent(std::uint32_t count,
                                         std::uint8_t headerWidth,
                                         std::uint32_t& output) noexcept {
    const std::uint64_t total = static_cast<std::uint64_t>(count) + headerWidth;
    if (total > 0x0FFFFFF0ULL) {
        output = 0U;
        return false;
    }
    output = static_cast<std::uint32_t>((total + 15U) & ~15ULL);
    return true;
}

[[nodiscard]] constexpr std::uint64_t pack_sensor_identity(std::uint32_t word,
                                                           std::uint8_t kind,
                                                           std::uint16_t index) noexcept {
    return word | (static_cast<std::uint64_t>(kind) << 32U)
           | (static_cast<std::uint64_t>(index) << 48U);
}

[[nodiscard]] inline bool exact_heap_assert(const char* text) noexcept {
    constexpr char expected[] = "index heap double-free? previous does not point back.";
    if (text == nullptr) {
        return false;
    }
    // Compare through the expected terminator, but stop at the first mismatch. Unlike memcmp over
    // sizeof(expected), this never reads past the terminator of a shorter caller string.
    for (std::size_t index = 0U; index < sizeof expected; ++index) {
        if (text[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

struct ImageView final {
    std::span<const std::byte> mapped{};
    ImageSha256 packedSha256{};
    std::uint64_t packedFileSize{};
    std::uint16_t machine{};
    std::uint16_t sectionCount{};
    std::uint32_t timestamp{};
    std::uint32_t imageSize{};
    std::uint32_t entryRva{};
    std::uint32_t checksum{};
    std::array<std::byte, 16U> codeViewGuid{};
    std::uint32_t codeViewAge{};
};

enum class ImageValidationResult : std::uint8_t {
    valid,
    hashMismatch,
    fileSizeMismatch,
    peMismatch,
    codeViewMismatch,
    targetBounds,
    prefixMismatch,
};

[[nodiscard]] inline ImageValidationResult
validate_image(const ImageView& image,
               std::array<std::uintptr_t, kCoreHookCount>& coreTargets,
               std::uintptr_t& heapResolver,
               std::array<std::array<std::byte, 17U>, kSiteCount>& observed) noexcept {
    coreTargets = {};
    heapResolver = 0U;
    observed = {};
    if (image.packedSha256 != kPinnedPackedImageSha256) {
        return ImageValidationResult::hashMismatch;
    }
    if (image.packedFileSize != 122'984'224ULL) {
        return ImageValidationResult::fileSizeMismatch;
    }
    if (image.machine != 0x8664U || image.sectionCount != 11U
        || image.timestamp != 0x5F43138BU || image.imageSize != 0x08A5EA00U
        || image.entryRva != 0x0187CDD8U || image.checksum != 0x0755867CU) {
        return ImageValidationResult::peMismatch;
    }
    constexpr std::array<std::byte, 16U> expectedGuid{
        std::byte{0xDFU}, std::byte{0xFBU}, std::byte{0xDCU}, std::byte{0x0DU},
        std::byte{0x68U}, std::byte{0xEBU}, std::byte{0x48U}, std::byte{0x41U},
        std::byte{0x8BU}, std::byte{0xFBU}, std::byte{0x7CU}, std::byte{0x76U},
        std::byte{0x18U}, std::byte{0xFFU}, std::byte{0xABU}, std::byte{0x03U},
    };
    if (image.codeViewGuid != expectedGuid || image.codeViewAge != 1U) {
        return ImageValidationResult::codeViewMismatch;
    }
    std::size_t coreIndex = 0U;
    for (std::size_t index = 0U; index < kSiteManifest.size(); ++index) {
        const SiteContract& contract = kSiteManifest[index];
        const std::size_t rva = contract.rva;
        const std::size_t length = contract.prefixLength;
        if (rva > image.mapped.size() || length > image.mapped.size() - rva) {
            coreTargets = {};
            heapResolver = 0U;
            observed = {};
            return ImageValidationResult::targetBounds;
        }
        std::memcpy(observed[index].data(), image.mapped.data() + rva, length);
        if (std::memcmp(observed[index].data(), contract.expectedMapped.data(), length) != 0) {
            coreTargets = {};
            heapResolver = 0U;
            observed = {};
            return ImageValidationResult::prefixMismatch;
        }
        if (contract.action == SiteAction::coreDetour) {
            if (coreIndex >= coreTargets.size()) {
                coreTargets = {};
                observed = {};
                return ImageValidationResult::prefixMismatch;
            }
            coreTargets[coreIndex++] = reinterpret_cast<std::uintptr_t>(image.mapped.data() + rva);
        } else if (contract.action == SiteAction::validatedHelper) {
            heapResolver = reinterpret_cast<std::uintptr_t>(image.mapped.data() + rva);
        }
    }
    if (coreIndex != coreTargets.size() || heapResolver == 0U) {
        coreTargets = {};
        heapResolver = 0U;
        observed = {};
        return ImageValidationResult::prefixMismatch;
    }
    return ImageValidationResult::valid;
}

struct EventClaim final {
    EventV1* first{};
    EventV1* second{};
    std::uint64_t firstSequence{};
    std::uint64_t secondSequence{};
    [[nodiscard]] explicit operator bool() const noexcept { return first != nullptr; }
};

class EventLedger final {
public:
    EventLedger() noexcept = default;
    EventLedger(Shl1Header* header,
                EventV1* events,
                std::size_t normalCapacity = kNormalEventCapacity,
                std::size_t criticalCapacity = kCriticalEventCapacity) noexcept
        : header_(header), events_(events), normalCapacity_(normalCapacity),
          criticalCapacity_(criticalCapacity) {
        reset_claim_states();
    }

    void bind(Shl1Header* header,
              EventV1* events,
              std::size_t normalCapacity = kNormalEventCapacity,
              std::size_t criticalCapacity = kCriticalEventCapacity) noexcept {
        header_ = header;
        events_ = events;
        normalCapacity_ = normalCapacity;
        criticalCapacity_ = criticalCapacity;
        reset_claim_states();
    }

    [[nodiscard]] EventClaim claim_normal() noexcept { return claim_normal_count(1U); }
    [[nodiscard]] EventClaim claim_pair() noexcept { return claim_normal_count(2U); }

    [[nodiscard]] EventClaim claim_critical() noexcept {
        if (header_ == nullptr || events_ == nullptr) {
            return {};
        }
        std::uint64_t state = criticalState_.load(std::memory_order_acquire);
        if ((state & kStoppedBit) != 0U || state >= criticalCapacity_) {
            header_->droppedCount.fetch_add(1U, std::memory_order_relaxed);
            account(header_, FailureCounter::criticalEventFull);
            return {};
        }
        if (!criticalState_.compare_exchange_strong(
                state,
                state + 1U,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            header_->droppedCount.fetch_add(1U, std::memory_order_relaxed);
            account(header_, FailureCounter::criticalEventFull);
            return {};
        }
        const std::uint64_t claimed = state;
        publish_claim_high_water(header_->criticalClaim, claimed + 1U);
        update_high_water(static_cast<std::uint32_t>(normalCapacity_ + claimed + 1U));
        const std::size_t index = normalCapacity_ + static_cast<std::size_t>(claimed);
        return EventClaim{events_ + index, nullptr, index + 1U, 0U};
    }

    void commit(EventV1& event, std::uint64_t sequence) noexcept {
        event.commitSequence.store(sequence, std::memory_order_release);
        if (header_ != nullptr) {
            header_->committedCount.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] bool freeze(FreezeReason reason,
                              std::uint64_t qpc,
                              std::uint64_t causingSequence) noexcept {
        if (header_ == nullptr || reason == FreezeReason::none) {
            return false;
        }
        const std::uint64_t state = normalState_.fetch_or(kStoppedBit, std::memory_order_acq_rel);
        if ((state & kStoppedBit) != 0U) {
            return false;
        }
        std::uint32_t expected = static_cast<std::uint32_t>(FreezeReason::none);
        if (!header_->freezeReason.compare_exchange_strong(
                expected,
                static_cast<std::uint32_t>(reason),
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return false;
        }
        header_->freezeSequence.store(causingSequence, std::memory_order_release);
        header_->freezeQpc.store(qpc, std::memory_order_release);
        header_->flushState.store(static_cast<std::uint32_t>(FlushState::requested),
                                  std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool freeze(FreezeReason reason, std::uint64_t qpc) noexcept {
        return freeze(reason, qpc, 0U);
    }

#if defined(SUNRISE_SENSOR_HEAP_FULL_COHORT_TEST)
    void testing_set_normal_state(std::uint64_t state) noexcept {
        normalState_.store(state, std::memory_order_release);
    }
    void testing_set_critical_state(std::uint64_t state) noexcept {
        criticalState_.store(state, std::memory_order_release);
    }
#endif

private:
    static constexpr std::uint64_t kStoppedBit = 1ULL << 63U;

    [[nodiscard]] EventClaim claim_normal_count(std::uint64_t count) noexcept {
        if (header_ == nullptr || events_ == nullptr || count == 0U) {
            return {};
        }
        std::uint64_t state = normalState_.load(std::memory_order_acquire);
        if ((state & kStoppedBit) != 0U) {
            return {};
        }
        if (state >= normalCapacity_ || count > normalCapacity_ - state) {
            header_->droppedCount.fetch_add(count, std::memory_order_relaxed);
            account(header_, FailureCounter::normalEventFull, count);
            if (count == 2U) {
                account(header_, FailureCounter::writerPairPartial);
            }
            return {};
        }
        if (state > (std::numeric_limits<std::uint64_t>::max)() - count
            || !normalState_.compare_exchange_strong(
                state,
                state + count,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            header_->droppedCount.fetch_add(count, std::memory_order_relaxed);
            account(header_, FailureCounter::eventClaimExhausted, count);
            if (count == 2U) {
                account(header_, FailureCounter::writerPairPartial);
            }
            return {};
        }
        const std::uint64_t claimed = state;
        publish_claim_high_water(header_->normalClaim, claimed + count);
        update_high_water(static_cast<std::uint32_t>(claimed + count));
        EventV1* const first = events_ + static_cast<std::size_t>(claimed);
        return EventClaim{first,
                          count == 2U ? first + 1U : nullptr,
                          claimed + 1U,
                          count == 2U ? claimed + 2U : 0U};
    }

    void update_high_water(std::uint32_t value) noexcept {
        std::uint32_t high = header_->eventHighWater.load(std::memory_order_relaxed);
        if (high < value) {
            (void)header_->eventHighWater.compare_exchange_strong(
                high, value, std::memory_order_relaxed, std::memory_order_relaxed);
        }
    }

    static void publish_claim_high_water(std::atomic<std::uint64_t>& target,
                                         std::uint64_t value) noexcept {
        std::uint64_t high = target.load(std::memory_order_relaxed);
        if (high < value) {
            (void)target.compare_exchange_strong(
                high, value, std::memory_order_relaxed, std::memory_order_relaxed);
        }
    }

    void reset_claim_states() noexcept {
        const std::uint64_t normal = header_ != nullptr
                                         ? header_->normalClaim.load(std::memory_order_relaxed)
                                         : 0U;
        const std::uint64_t critical = header_ != nullptr
                                           ? header_->criticalClaim.load(std::memory_order_relaxed)
                                           : 0U;
        normalState_.store(normal, std::memory_order_relaxed);
        criticalState_.store(critical, std::memory_order_relaxed);
    }

    Shl1Header* header_{};
    EventV1* events_{};
    std::size_t normalCapacity_{};
    std::size_t criticalCapacity_{};
    std::atomic<std::uint64_t> normalState_{};
    std::atomic<std::uint64_t> criticalState_{};
};

struct SensorIdentity final {
    std::uint32_t word{};
    std::uint8_t kind{};
    std::uint16_t index{};
    bool valid{};
};

struct MemberBaseline final {
    std::uint32_t count{};
    std::uint64_t relativePayload{};
    std::uint16_t selector{};
    std::uintptr_t heap{};
    std::uintptr_t heapBase{};
    std::uint64_t relativeHeader{};
    std::uint8_t headerWidth{};
    bool valid{};
};

struct RecordState final {
    std::uintptr_t record{};
    std::uintptr_t sensorTable{};
    std::uintptr_t peerView{};
    std::uint64_t sensorKey{};
    std::uint64_t activityGeneration{};
    std::uint64_t connectionGeneration{};
    std::uint64_t regionGeneration{};
    std::uint64_t lossMask{};
    std::uint32_t generation{};
    std::uint32_t datum{};
    std::uint32_t authSchema{};
    std::uint32_t senseSchema{};
    std::uint32_t owner{};
    std::uint8_t freeMask{};
    std::uint8_t unlinkMask{};
    std::uint8_t goodUnlinkMask{};
    bool lifecycleValid{};
    SensorIdentity identity{};
    std::array<MemberBaseline, 3U> members{};
    bool committed{};
    bool active{};
    bool retired{};
    bool tableValid{};
    bool datumValid{};
};

struct RecordToken final {
    std::uint32_t slot{};
    std::uint32_t generation{};
    std::uintptr_t record{};
    [[nodiscard]] explicit operator bool() const noexcept {
        return record > 1U && generation != 0U;
    }
};

enum class RegistryResult : std::uint8_t {
    success,
    invalid,
    busy,
    full,
    exhausted,
    stale,
    missing,
};

using ReuseObserver = void (*)(void*, const RecordState&) noexcept;

/**
 * Two-bank publication cell with reader registration. Writers are serialized by the owning slot's
 * odd/even version CAS. A writer touches only the inactive bank and only when its reader count is
 * zero; readers register before validating the version/bank pair. This preserves the one-attempt
 * hot-path contract without concurrent plain reads/writes or language-level data races.
 */
template <typename Value>
class SnapshotCell final {
public:
    void reset() noexcept {
        active_.store(0U, std::memory_order_relaxed);
        readers_[0].store(0U, std::memory_order_relaxed);
        readers_[1].store(0U, std::memory_order_relaxed);
        banks_[0] = {};
        banks_[1] = {};
    }

    [[nodiscard]] bool read(const std::atomic<std::uint32_t>& version,
                            Value& output) const noexcept {
        output = {};
        const std::uint32_t first = version.load(std::memory_order_seq_cst);
        if ((first & 1U) != 0U) {
            return false;
        }
        const std::uint8_t bank = active_.load(std::memory_order_seq_cst);
        readers_[bank].fetch_add(1U, std::memory_order_seq_cst);
        const std::uint32_t last = version.load(std::memory_order_seq_cst);
        const std::uint8_t confirmed = active_.load(std::memory_order_seq_cst);
        if (first != last || (last & 1U) != 0U || bank != confirmed) {
            readers_[bank].fetch_sub(1U, std::memory_order_seq_cst);
            return false;
        }
        output = banks_[bank];
        readers_[bank].fetch_sub(1U, std::memory_order_seq_cst);
        return true;
    }

    /** Called only while the owning version is odd. */
    [[nodiscard]] bool publish(const Value& value) noexcept {
        const std::uint8_t inactive = static_cast<std::uint8_t>(
            active_.load(std::memory_order_seq_cst) ^ 1U);
        if (readers_[inactive].load(std::memory_order_seq_cst) != 0U) {
            return false;
        }
        banks_[inactive] = value;
        active_.store(inactive, std::memory_order_seq_cst);
        return true;
    }

    /** Called only by the serialized writer while the owning version is odd. */
    [[nodiscard]] Value writer_copy() const noexcept {
        return banks_[active_.load(std::memory_order_seq_cst)];
    }

#if defined(SUNRISE_SENSOR_HEAP_FULL_COHORT_TEST)
    [[nodiscard]] Value testing_current(const std::atomic<std::uint32_t>& version) const noexcept {
        Value output{};
        (void)read(version, output);
        return output;
    }
#endif

private:
    std::array<Value, 2U> banks_{};
    mutable std::array<std::atomic<std::uint32_t>, 2U> readers_{};
    std::atomic<std::uint8_t> active_{};
};

static_assert(std::atomic<std::uint8_t>::is_always_lock_free);

template <std::size_t Capacity>
class RecordRegistry final {
    static_assert(Capacity != 0U);
    static constexpr std::uintptr_t kReserved = 1U;
    struct Slot final {
        std::atomic<std::uintptr_t> key{};
        std::atomic<std::uint32_t> version{};
        SnapshotCell<RecordState> state{};
        std::array<std::atomic<std::uint64_t>, 3U> freeTickets{};
    };

public:
    void bind(Shl1Header* header) noexcept { header_ = header; }

    void reset() noexcept {
        for (Slot& slot : slots_) {
            slot.key.store(0U, std::memory_order_relaxed);
            slot.version.store(0U, std::memory_order_relaxed);
            slot.state.reset();
            for (auto& ticket : slot.freeTickets) {
                ticket.store(0U, std::memory_order_relaxed);
            }
        }
        highWater_.store(0U, std::memory_order_relaxed);
    }

    [[nodiscard]] RegistryResult begin(std::uintptr_t record,
                                       RecordState seed,
                                       RecordToken& output,
                                       ReuseObserver observer = nullptr,
                                       void* observerContext = nullptr) noexcept {
        output = {};
        if (record <= kReserved) {
            account(header_, FailureCounter::recordBusy);
            return RegistryResult::invalid;
        }
        const std::size_t first = start(record);
        for (std::size_t offset = 0U; offset < Capacity; ++offset) {
            const std::size_t index = (first + offset) % Capacity;
            Slot& slot = slots_[index];
            const std::uintptr_t key = slot.key.load(std::memory_order_acquire);
            if (key == 0U) {
                std::uintptr_t expected = 0U;
                if (!slot.key.compare_exchange_strong(expected,
                                                      kReserved,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    account(header_, FailureCounter::recordBusy);
                    return RegistryResult::busy;
                }
                slot.version.store(1U, std::memory_order_release);
                seed.record = record;
                seed.generation = 1U;
                seed.active = true;
                seed.retired = false;
                if (!slot.state.publish(seed)) {
                    slot.version.store(0U, std::memory_order_release);
                    slot.key.store(0U, std::memory_order_release);
                    account(header_, FailureCounter::recordBusy);
                    return RegistryResult::busy;
                }
                reset_tickets(slot, 1U);
                slot.version.store(2U, std::memory_order_release);
                slot.key.store(record, std::memory_order_release);
                update_high_water();
                output = RecordToken{static_cast<std::uint32_t>(index), 1U, record};
                return RegistryResult::success;
            }
            if (key == kReserved) {
                account(header_, FailureCounter::recordBusy);
                return RegistryResult::busy;
            }
            if (key != record) {
                continue;
            }
            std::uint32_t version = slot.version.load(std::memory_order_acquire);
            if ((version & 1U) != 0U
                || !slot.version.compare_exchange_strong(version,
                                                         version + 1U,
                                                         std::memory_order_acq_rel,
                                                         std::memory_order_acquire)) {
                account(header_, FailureCounter::recordBusy);
                return RegistryResult::busy;
            }
            const RecordState previous = slot.state.writer_copy();
            if (previous.generation == (std::numeric_limits<std::uint32_t>::max)()) {
                slot.version.store(version + 2U, std::memory_order_release);
                account(header_, FailureCounter::recordGenerationExhausted);
                return RegistryResult::exhausted;
            }
            if (previous.active && observer != nullptr) {
                observer(observerContext, previous);
            }
            seed.record = record;
            seed.generation = previous.generation + 1U;
            seed.active = true;
            seed.retired = false;
            if (!slot.state.publish(seed)) {
                slot.version.store(version + 2U, std::memory_order_release);
                account(header_, FailureCounter::recordBusy);
                return RegistryResult::busy;
            }
            reset_tickets(slot, seed.generation);
            slot.version.store(version + 2U, std::memory_order_release);
            output = RecordToken{static_cast<std::uint32_t>(index), seed.generation, record};
            return RegistryResult::success;
        }
        account(header_, FailureCounter::recordFull);
        return RegistryResult::full;
    }

    [[nodiscard]] RegistryResult read(std::uintptr_t record,
                                      RecordState& output,
                                      RecordToken* token = nullptr) const noexcept {
        output = {};
        const Slot* const slot = find(record);
        if (slot == nullptr) {
            return RegistryResult::missing;
        }
        const std::uint32_t first = slot->version.load(std::memory_order_acquire);
        if ((first & 1U) != 0U) {
            account(header_, FailureCounter::recordReadRace);
            return RegistryResult::busy;
        }
        RecordState copy{};
        if (!slot->state.read(slot->version, copy)) {
            account(header_, FailureCounter::recordReadRace);
            return RegistryResult::busy;
        }
        output = copy;
        if (token != nullptr) {
            const std::size_t index = static_cast<std::size_t>(slot - slots_.data());
            *token = RecordToken{static_cast<std::uint32_t>(index), copy.generation, record};
        }
        return RegistryResult::success;
    }

    [[nodiscard]] RegistryResult read_slot(std::uint32_t slotIndex,
                                           std::uint32_t generation,
                                           RecordState& output,
                                           RecordToken* token = nullptr) const noexcept {
        output = {};
        if (slotIndex >= Capacity) {
            return RegistryResult::missing;
        }
        const Slot& slot = slots_[slotIndex];
        const std::uintptr_t key = slot.key.load(std::memory_order_acquire);
        if (key <= kReserved) {
            return RegistryResult::missing;
        }
        const std::uint32_t first = slot.version.load(std::memory_order_acquire);
        if ((first & 1U) != 0U) {
            account(header_, FailureCounter::recordReadRace);
            return RegistryResult::busy;
        }
        RecordState copy{};
        if (!slot.state.read(slot.version, copy)) {
            account(header_, FailureCounter::recordReadRace);
            return RegistryResult::busy;
        }
        if (copy.generation != generation || copy.record != key) {
            account(header_, FailureCounter::allocationIndexStaleBinding);
            return RegistryResult::stale;
        }
        output = copy;
        if (token != nullptr) {
            *token = RecordToken{slotIndex, generation, key};
        }
        return RegistryResult::success;
    }

    template <typename Update>
    [[nodiscard]] RegistryResult update(RecordToken token,
                                        Update&& updateState,
                                        FailureCounter staleCounter) noexcept {
        if (token.slot >= Capacity) {
            account(header_, staleCounter);
            return RegistryResult::stale;
        }
        Slot& slot = slots_[token.slot];
        std::uint32_t version = slot.version.load(std::memory_order_acquire);
        if ((version & 1U) != 0U
            || !slot.version.compare_exchange_strong(version,
                                                     version + 1U,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
            account(header_, FailureCounter::recordBusy);
            return RegistryResult::busy;
        }
        RecordState state = slot.state.writer_copy();
        if (slot.key.load(std::memory_order_acquire) != token.record
            || state.generation != token.generation) {
            slot.version.store(version + 2U, std::memory_order_release);
            account(header_, staleCounter);
            return RegistryResult::stale;
        }
        updateState(state);
        if (!slot.state.publish(state)) {
            slot.version.store(version + 2U, std::memory_order_release);
            account(header_, FailureCounter::recordBusy);
            return RegistryResult::busy;
        }
        slot.version.store(version + 2U, std::memory_order_release);
        return RegistryResult::success;
    }

    [[nodiscard]] RegistryResult commit(RecordToken token) noexcept {
        return update(token,
                      [](RecordState& state) noexcept { state.committed = true; },
                      FailureCounter::staleRecordCommit);
    }

    [[nodiscard]] RegistryResult retire(RecordToken token) noexcept {
        return update(token,
                      [](RecordState& state) noexcept {
                          state.active = false;
                          state.retired = true;
                      },
                      FailureCounter::staleRecordRetire);
    }

    [[nodiscard]] RegistryResult next_free_ordinal(RecordToken token,
                                                   Member member,
                                                   std::uint32_t& output) noexcept {
        output = 0U;
        const std::size_t memberIndex = member_to_index(member);
        if (token.slot >= Capacity || memberIndex >= 3U) {
            return RegistryResult::stale;
        }
        std::atomic<std::uint64_t>& ticket = slots_[token.slot].freeTickets[memberIndex];
        std::uint64_t value = ticket.load(std::memory_order_acquire);
        if (static_cast<std::uint32_t>(value >> 32U) != token.generation) {
            account(header_, FailureCounter::allocationIndexStaleBinding);
            return RegistryResult::stale;
        }
        const std::uint32_t ordinal = static_cast<std::uint32_t>(value);
        if (ordinal == (std::numeric_limits<std::uint32_t>::max)()) {
            account(header_, FailureCounter::freeTicketExhausted);
            return RegistryResult::exhausted;
        }
        const std::uint64_t desired =
            (static_cast<std::uint64_t>(token.generation) << 32U) | (ordinal + 1U);
        if (!ticket.compare_exchange_strong(
                value, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            account(header_, FailureCounter::freeTicketBusy);
            return RegistryResult::busy;
        }
        output = ordinal + 1U;
        return RegistryResult::success;
    }

    template <typename Visitor>
    void visit_active(Visitor&& visitor) const noexcept {
        for (const Slot& slot : slots_) {
            const std::uintptr_t key = slot.key.load(std::memory_order_acquire);
            if (key <= kReserved) {
                continue;
            }
            RecordState state{};
            if (read(key, state, nullptr) == RegistryResult::success && state.active) {
                visitor(state);
            }
        }
    }

    [[nodiscard]] std::uint32_t high_water() const noexcept {
        return highWater_.load(std::memory_order_relaxed);
    }

#if defined(SUNRISE_SENSOR_HEAP_FULL_COHORT_TEST)
    void testing_set_version(std::uintptr_t record, std::uint32_t version) noexcept {
        Slot* const slot = find_mutable(record);
        if (slot != nullptr) {
            slot->version.store(version, std::memory_order_release);
        }
    }
    void testing_set_generation(std::uintptr_t record, std::uint32_t generation) noexcept {
        Slot* const slot = find_mutable(record);
        if (slot != nullptr) {
            std::uint32_t version = slot->version.load(std::memory_order_acquire);
            if ((version & 1U) == 0U
                && slot->version.compare_exchange_strong(version, version + 1U)) {
                RecordState state = slot->state.writer_copy();
                state.generation = generation;
                (void)slot->state.publish(state);
                slot->version.store(version + 2U, std::memory_order_release);
            }
        }
    }
    void testing_set_free_ticket(RecordToken token, Member member, std::uint64_t value) noexcept {
        if (token.slot < Capacity && member_to_index(member) < 3U) {
            slots_[token.slot].freeTickets[member_to_index(member)].store(
                value, std::memory_order_release);
        }
    }
#endif

private:
    [[nodiscard]] static std::size_t start(std::uintptr_t record) noexcept {
        return (record >> 4U) % Capacity;
    }
    [[nodiscard]] static std::size_t member_to_index(Member member) noexcept {
        switch (member) {
        case Member::receivedAuth: return 0U;
        case Member::extractedSense: return 1U;
        case Member::receivedSense: return 2U;
        default: return 3U;
        }
    }
    static void reset_tickets(Slot& slot, std::uint32_t generation) noexcept {
        const std::uint64_t value = static_cast<std::uint64_t>(generation) << 32U;
        for (auto& ticket : slot.freeTickets) {
            ticket.store(value, std::memory_order_release);
        }
    }
    [[nodiscard]] const Slot* find(std::uintptr_t record) const noexcept {
        if (record <= kReserved) {
            return nullptr;
        }
        const std::size_t first = start(record);
        for (std::size_t offset = 0U; offset < Capacity; ++offset) {
            const Slot& slot = slots_[(first + offset) % Capacity];
            const std::uintptr_t key = slot.key.load(std::memory_order_acquire);
            if (key == record) {
                return &slot;
            }
            if (key == 0U) {
                return nullptr;
            }
        }
        return nullptr;
    }
    [[nodiscard]] Slot* find_mutable(std::uintptr_t record) noexcept {
        return const_cast<Slot*>(static_cast<const RecordRegistry*>(this)->find(record));
    }
    void update_high_water() noexcept {
        const std::uint32_t value = highWater_.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (header_ != nullptr) {
            header_->recordHighWater.store(value, std::memory_order_relaxed);
        }
    }

    std::array<Slot, Capacity> slots_{};
    Shl1Header* header_{};
    std::atomic<std::uint32_t> highWater_{};
};

enum class AllocationKeyKind : std::uint8_t { payload = 1U, header = 2U };

struct AllocationBinding final {
    std::uint32_t recordSlot{};
    std::uint32_t recordGeneration{};
    Member member{};
    bool retired{};
};

struct AllocationKey final {
    std::uintptr_t heap{};
    std::uint64_t relative{};
    AllocationKeyKind kind{};
};

struct AllocationPublication final {
    AllocationKey key{};
    AllocationBinding binding{};
};

[[nodiscard]] inline bool operator==(const AllocationKey& left,
                                     const AllocationKey& right) noexcept {
    return left.heap == right.heap && left.relative == right.relative && left.kind == right.kind;
}

template <std::size_t Capacity>
class AllocationIndex final {
    static_assert(Capacity != 0U);
    static constexpr std::uint64_t kReservedTag = 1U;
    struct Slot final {
        std::atomic<std::uint64_t> tag{};
        std::atomic<std::uint32_t> version{};
        SnapshotCell<AllocationPublication> publication{};
    };

public:
    void bind(Shl1Header* header) noexcept { header_ = header; }
    void reset() noexcept {
        for (Slot& slot : slots_) {
            slot.tag.store(0U, std::memory_order_relaxed);
            slot.version.store(0U, std::memory_order_relaxed);
            slot.publication.reset();
        }
        highWater_.store(0U, std::memory_order_relaxed);
    }

    [[nodiscard]] RegistryResult publish(AllocationKey key,
                                         AllocationBinding binding) noexcept {
        if (key.heap == 0U || key.relative == 0U || binding.recordGeneration == 0U
            || binding.member == Member::none) {
            account(header_, FailureCounter::allocationIndexStaleBinding);
            return RegistryResult::invalid;
        }
        const std::uint64_t wantedTag = make_tag(key);
        const std::size_t first = static_cast<std::size_t>(wantedTag % Capacity);
        for (std::size_t offset = 0U; offset < Capacity; ++offset) {
            Slot& slot = slots_[(first + offset) % Capacity];
            const std::uint64_t tag = slot.tag.load(std::memory_order_acquire);
            if (tag == 0U) {
                std::uint64_t empty = 0U;
                if (!slot.tag.compare_exchange_strong(empty,
                                                      kReservedTag,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    account(header_, FailureCounter::allocationIndexBusy);
                    return RegistryResult::busy;
                }
                slot.version.store(1U, std::memory_order_release);
                if (!slot.publication.publish(AllocationPublication{key, binding})) {
                    slot.version.store(0U, std::memory_order_release);
                    slot.tag.store(0U, std::memory_order_release);
                    account(header_, FailureCounter::allocationIndexBusy);
                    return RegistryResult::busy;
                }
                slot.version.store(2U, std::memory_order_release);
                slot.tag.store(wantedTag, std::memory_order_release);
                update_high_water();
                return RegistryResult::success;
            }
            if (tag == kReservedTag) {
                account(header_, FailureCounter::allocationIndexBusy);
                return RegistryResult::busy;
            }
            if (tag != wantedTag) {
                continue;
            }
            AllocationKey observedKey{};
            AllocationBinding observedBinding{};
            if (read_slot(slot, observedKey, observedBinding) != RegistryResult::success) {
                return RegistryResult::busy;
            }
            if (!(observedKey == key)) {
                continue;
            }
            std::uint32_t version = slot.version.load(std::memory_order_acquire);
            if ((version & 1U) != 0U
                || !slot.version.compare_exchange_strong(version,
                                                         version + 1U,
                                                         std::memory_order_acq_rel,
                                                         std::memory_order_acquire)) {
                account(header_, FailureCounter::allocationIndexBusy);
                return RegistryResult::busy;
            }
            const AllocationPublication previous = slot.publication.writer_copy();
            const bool exact = previous.binding.recordSlot == binding.recordSlot
                               && previous.binding.recordGeneration == binding.recordGeneration
                               && previous.binding.member == binding.member;
            if (!exact && !previous.binding.retired) {
                slot.version.store(version + 2U, std::memory_order_release);
                account(header_, FailureCounter::allocationIndexStaleBinding);
                return RegistryResult::stale;
            }
            if (!slot.publication.publish(AllocationPublication{key, binding})) {
                slot.version.store(version + 2U, std::memory_order_release);
                account(header_, FailureCounter::allocationIndexBusy);
                return RegistryResult::busy;
            }
            slot.version.store(version + 2U, std::memory_order_release);
            return RegistryResult::success;
        }
        account(header_, FailureCounter::allocationIndexFull);
        return RegistryResult::full;
    }

    [[nodiscard]] RegistryResult lookup(AllocationKey key,
                                        AllocationBinding& output) const noexcept {
        output = {};
        const std::uint64_t wantedTag = make_tag(key);
        const std::size_t first = static_cast<std::size_t>(wantedTag % Capacity);
        for (std::size_t offset = 0U; offset < Capacity; ++offset) {
            const Slot& slot = slots_[(first + offset) % Capacity];
            const std::uint64_t tag = slot.tag.load(std::memory_order_acquire);
            if (tag == 0U) {
                return RegistryResult::missing;
            }
            if (tag == kReservedTag) {
                account(header_, FailureCounter::allocationIndexReadRace);
                return RegistryResult::busy;
            }
            if (tag != wantedTag) {
                continue;
            }
            AllocationKey observedKey{};
            AllocationBinding binding{};
            const RegistryResult result = read_slot(slot, observedKey, binding);
            if (result != RegistryResult::success) {
                return result;
            }
            if (observedKey == key) {
                output = binding;
                return RegistryResult::success;
            }
        }
        return RegistryResult::missing;
    }

    [[nodiscard]] RegistryResult mark_retired(AllocationKey key,
                                               RecordToken token) noexcept {
        AllocationBinding binding{};
        if (lookup(key, binding) != RegistryResult::success
            || binding.recordSlot != token.slot
            || binding.recordGeneration != token.generation) {
            account(header_, FailureCounter::allocationIndexStaleBinding);
            return RegistryResult::stale;
        }
        binding.retired = true;
        return publish(key, binding);
    }

    [[nodiscard]] std::uint32_t high_water() const noexcept {
        return highWater_.load(std::memory_order_relaxed);
    }

#if defined(SUNRISE_SENSOR_HEAP_FULL_COHORT_TEST)
    void testing_set_version(AllocationKey key, std::uint32_t version) noexcept {
        Slot* const slot = find_mutable(key);
        if (slot != nullptr) {
            slot->version.store(version, std::memory_order_release);
        }
    }
#endif

private:
    [[nodiscard]] static std::uint64_t make_tag(AllocationKey key) noexcept {
        std::uint64_t value = static_cast<std::uint64_t>(key.heap);
        value ^= key.relative + 0x9E3779B97F4A7C15ULL + (value << 6U) + (value >> 2U);
        value ^= static_cast<std::uint64_t>(key.kind) * 0xD6E8FEB86659FD93ULL;
        value ^= value >> 33U;
        value *= 0xFF51AFD7ED558CCDULL;
        value ^= value >> 33U;
        value |= 2U;
        return value;
    }

    [[nodiscard]] RegistryResult read_slot(const Slot& slot,
                                           AllocationKey& key,
                                           AllocationBinding& binding) const noexcept {
        const std::uint32_t first = slot.version.load(std::memory_order_acquire);
        if ((first & 1U) != 0U) {
            account(header_, FailureCounter::allocationIndexReadRace);
            return RegistryResult::busy;
        }
        AllocationPublication publication{};
        if (!slot.publication.read(slot.version, publication)) {
            account(header_, FailureCounter::allocationIndexReadRace);
            return RegistryResult::busy;
        }
        key = publication.key;
        binding = publication.binding;
        return RegistryResult::success;
    }

    [[nodiscard]] Slot* find_mutable(AllocationKey key) noexcept {
        const std::uint64_t tag = make_tag(key);
        const std::size_t first = static_cast<std::size_t>(tag % Capacity);
        for (std::size_t offset = 0U; offset < Capacity; ++offset) {
            Slot& slot = slots_[(first + offset) % Capacity];
            const std::uint64_t observed = slot.tag.load(std::memory_order_acquire);
            if (observed == 0U) {
                return nullptr;
            }
            if (observed == tag) {
                AllocationKey publishedKey{};
                AllocationBinding binding{};
                if (read_slot(slot, publishedKey, binding) == RegistryResult::success
                    && publishedKey == key) {
                    return &slot;
                }
            }
        }
        return nullptr;
    }
    void update_high_water() noexcept {
        const std::uint32_t value = highWater_.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (header_ != nullptr) {
            header_->allocationIndexHighWater.store(value, std::memory_order_relaxed);
        }
    }

    std::array<Slot, Capacity> slots_{};
    Shl1Header* header_{};
    std::atomic<std::uint32_t> highWater_{};
};

struct HeapCapture final {
    HeapSnapshotPayload payload{};
    std::uint32_t valid{};
    std::uint32_t flags{};
};

/** Reader must provide copy(address,destination,size) and hash(address,size,output). */
template <typename Reader>
[[nodiscard]] HeapCapture capture_heap(Reader& reader,
                                       std::uintptr_t heap,
                                       std::uint64_t relativePayload,
                                       std::uint32_t count,
                                       std::uint16_t selector) noexcept {
    HeapCapture output{};
    output.payload.heap = heap;
    output.payload.relativePayload = relativePayload;
    output.payload.count = count;
    output.payload.selector = selector;
    if (heap == 0U) {
        output.flags |= flagGuardedReadFault;
        return output;
    }
    output.valid |= validHeap;
    std::uint8_t mode{};
    std::uintptr_t modeAddress{};
    if (!checked_add(heap, 0xA4U, modeAddress)
        || !reader.copy(modeAddress, &mode, sizeof mode)) {
        output.flags |= flagGuardedReadFault;
        return output;
    }
    output.payload.headerWidth = mode == 0U ? 0x20U : 0x40U;
    std::uintptr_t base{};
    if (!reader.copy(heap, &base, sizeof base)) {
        output.flags |= flagGuardedReadFault;
        return output;
    }
    output.payload.heapBase = base;
    output.valid |= validHeapBase;
    std::uint64_t relativeHeader{};
    if (!checked_sub(relativePayload, output.payload.headerWidth, relativeHeader)) {
        output.flags |= flagGuardedReadFault;
        return output;
    }
    output.payload.relativeHeader = relativeHeader;
    std::uintptr_t absoluteHeader{};
    if (!checked_add(base, relativeHeader, absoluteHeader)) {
        output.flags |= flagGuardedReadFault;
        return output;
    }
    struct HeaderFields final {
        std::uint32_t sizeFlags{};
        std::array<std::byte, 12U> padding{};
        std::uint64_t next{};
        std::uint64_t previous{};
    } fields{};
    if (!reader.copy(absoluteHeader, &fields, sizeof fields)) {
        output.flags |= flagGuardedReadFault;
        return output;
    }
    output.payload.sizeFlags = fields.sizeFlags;
    output.payload.next = fields.next;
    output.payload.previous = fields.previous;
    output.valid |= validHeader | validCurrentLinks;
    std::uint32_t expectedExtent{};
    if (!align16_extent(count, output.payload.headerWidth, expectedExtent)
        || (fields.sizeFlags & 0x0FFFFFFFU) != expectedExtent) {
        output.flags |= flagCountOrExtentMismatch;
    }

    std::uintptr_t relationAddress{};
    if (fields.previous == 0U) {
        if (checked_add(heap, 0x60U, relationAddress)
            && reader.copy(relationAddress,
                           &output.payload.head,
                           sizeof output.payload.head)) {
            output.valid |= validPreviousRelation;
            if (output.payload.head == relativeHeader) {
                output.payload.invariant |= invariantPreviousGood | invariantHeadGood;
            } else {
                output.flags |= flagPreviousPredicateBad;
            }
        } else {
            output.flags |= flagGuardedReadFault;
        }
    } else if (checked_add(base, fields.previous, relationAddress)
               && checked_add(relationAddress, 0x10U, relationAddress)
               && reader.copy(relationAddress,
                              &output.payload.previousNext,
                              sizeof output.payload.previousNext)) {
        output.valid |= validPreviousRelation;
        if (output.payload.previousNext == relativeHeader) {
            output.payload.invariant |= invariantPreviousGood;
        } else {
            output.flags |= flagPreviousPredicateBad;
        }
    } else {
        output.flags |= flagGuardedReadFault;
    }

    if (fields.next == 0U) {
        if (checked_add(heap, 0x68U, relationAddress)
            && reader.copy(relationAddress,
                           &output.payload.tail,
                           sizeof output.payload.tail)) {
            output.valid |= validNextRelation;
            if (output.payload.tail == relativeHeader) {
                output.payload.invariant |= invariantNextGood | invariantTailGood;
            } else {
                output.flags |= flagNextPredicateBad;
            }
        } else {
            output.flags |= flagGuardedReadFault;
        }
    } else if (checked_add(base, fields.next, relationAddress)
               && checked_add(relationAddress, 0x18U, relationAddress)
               && reader.copy(relationAddress,
                              &output.payload.nextPrevious,
                              sizeof output.payload.nextPrevious)) {
        output.valid |= validNextRelation;
        if (output.payload.nextPrevious == relativeHeader) {
            output.payload.invariant |= invariantNextGood;
        } else {
            output.flags |= flagNextPredicateBad;
        }
    } else {
        output.flags |= flagGuardedReadFault;
    }

    std::uintptr_t absolutePayload{};
    if (count <= kPayloadHashCeiling && checked_add(base, relativePayload, absolutePayload)
        && reader.hash(absolutePayload, count, output.payload.payloadHash)) {
        output.valid |= validPayloadRange | validPayloadHash;
    } else if (count != 0U) {
        output.flags |= flagGuardedReadFault;
    }
    return output;
}

template <typename Context>
class TlsScope final {
public:
    TlsScope(Context*& slot,
             Context& current,
             Shl1Header* header,
             bool& active) noexcept
        : slot_(slot), current_(&current), previous_(slot), header_(header), active_(active) {
        current.previous = previous_;
        slot_ = current_;
        active_ = true;
    }
    ~TlsScope() noexcept { exit(); }
    TlsScope(const TlsScope&) = delete;
    TlsScope& operator=(const TlsScope&) = delete;

    void exit() noexcept {
        if (!active_) {
            return;
        }
        if (slot_ != current_) {
            account(header_, FailureCounter::tlsMisnest);
        }
        slot_ = previous_;
        active_ = false;
    }

private:
    Context*& slot_;
    Context* current_{};
    Context* previous_{};
    Shl1Header* header_{};
    bool& active_;
};

} // namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap_full_cohort
