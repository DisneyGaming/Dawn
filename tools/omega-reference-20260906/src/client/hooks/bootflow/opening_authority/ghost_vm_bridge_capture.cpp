#include "ghost_vm_bridge_capture.h"

#include <algorithm>

namespace sunrise::client::hooks::bootflow::opening_authority::ghost_vm_bridge_capture {
namespace {

[[nodiscard]] consteval std::uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    return static_cast<std::uint8_t>(value - 'a' + 10);
}

template <std::size_t CharacterCount>
[[nodiscard]] consteval auto hex_bytes(const char (&text)[CharacterCount]) {
    static_assert(CharacterCount > 1U);
    static_assert(((CharacterCount - 1U) % 2U) == 0U);
    std::array<std::byte, (CharacterCount - 1U) / 2U> bytes{};
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        const std::uint8_t high = hex_nibble(text[index * 2U]);
        const std::uint8_t low = hex_nibble(text[index * 2U + 1U]);
        bytes[index] = std::byte{static_cast<std::uint8_t>((high << 4U) | low)};
    }
    return bytes;
}

[[nodiscard]] constexpr std::uint32_t rotate_right(std::uint32_t value,
                                                   std::uint32_t count) noexcept {
    return (value >> count) | (value << (32U - count));
}

constexpr std::array<std::uint32_t, 64U> kSha256RoundConstants{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U,
    0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU,
    0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU,
    0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU, 0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU,
    0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
    0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U, 0x19A4C116U,
    0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U,
    0xC67178F2U};

void sha256_compress(std::array<std::uint32_t, 8U>& state, const std::byte* block) noexcept {
    std::array<std::uint32_t, 64U> words{};
    for (std::size_t index = 0U; index < 16U; ++index) {
        const std::size_t offset = index * 4U;
        words[index] =
            (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[offset])) << 24U)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[offset + 1U])) << 16U)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[offset + 2U])) << 8U)
            | static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[offset + 3U]));
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
        const std::uint32_t s0 = rotate_right(words[index - 15U], 7U)
                                 ^ rotate_right(words[index - 15U], 18U)
                                 ^ (words[index - 15U] >> 3U);
        const std::uint32_t s1 = rotate_right(words[index - 2U], 17U)
                                 ^ rotate_right(words[index - 2U], 19U)
                                 ^ (words[index - 2U] >> 10U);
        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];
    for (std::size_t index = 0U; index < words.size(); ++index) {
        const std::uint32_t upperE =
            rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
        const std::uint32_t choose = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 =
            h + upperE + choose + kSha256RoundConstants[index] + words[index];
        const std::uint32_t upperA =
            rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = upperA + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

[[nodiscard]] bool is_zero_digest(const Sha256& digest) noexcept {
    return std::all_of(
        digest.begin(), digest.end(), [](std::byte value) { return value == std::byte{0U}; });
}

constexpr Sha256 kPcSha =
    hex_bytes("8713D15E3D05B26F9E259E02B0F29BC1E000E4B0C62CA2CC87C38597186CC3BD");
constexpr Sha256 kPs4Sha =
    hex_bytes("527586F13766FAFCC9157059831CECC1F82ABFB5399BB114049BB9E11339AA78");
constexpr Sha256 kMissionPackageSha =
    hex_bytes("9977BAAAE89BF07A815902EC81EECD289A7B1BA81E7E328CD0F67D1C25075F17");
constexpr Sha256 kNoSha{};

constexpr ArtifactDescriptor kPcArtifact{ArtifactKind::pc_unpacked_reference,
                                         L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe",
                                         145'091'072U,
                                         kPcSha,
                                         kPcPreferredImageBase,
                                         0U};
constexpr ArtifactDescriptor kPs4Artifact{ArtifactKind::ps4_eboot_reference,
                                          L"D:\\eboot-d2-0159.bin",
                                          37'824'851U,
                                          kPs4Sha,
                                          0U,
                                          kPs4CodeFileOffsetBias};
constexpr ArtifactDescriptor kMissionArtifact{
    ArtifactKind::mercury_mission_package,
    L"D:\\Destiny3\\packages\\w64_mercury_destination_activities_03a3_5.pkg",
    407'552U,
    kMissionPackageSha,
    0U,
    0U};
constexpr ArtifactDescriptor kExternalHostArtifact{
    ArtifactKind::external_retail_host_build, L"", 0U, kNoSha, 0U, 0U};

constexpr auto kType60CreatePrefix = hex_bytes("488BC4488958104889701855574156488D68A1");
constexpr Sha256 kType60CreatePrefixSha =
    hex_bytes("897AD50F09A854E7E6D1E4818611FCE28A8369EE656E202439F560BC51A4AFD4");
constexpr auto kType60StartPrefix = hex_bytes("48895C2410488974241848897C242055");
constexpr Sha256 kType60StartPrefixSha =
    hex_bytes("910387D611F5F9C79F433B80B3E779542995D6B5CC399DA69B3B45F860BCE871");
constexpr auto kType60ContainmentPrefix = hex_bytes("4883EC48488B054D8C63014833C44889442430");
constexpr Sha256 kType60ContainmentPrefixSha =
    hex_bytes("614627CEC1B0AF9237E38989693795161F06D330759DA4A859955BF753DEB957");
constexpr auto kMembershipSetPrefix = hex_bytes("40534883EC20488BD98BCAE82049FEFF");
constexpr Sha256 kMembershipSetPrefixSha =
    hex_bytes("C7B2069DA6B7309F8565C08110966EEC09EEB35CD632AD6DFB46BFADB17A116F");
constexpr auto kMembershipClearPrefix = hex_bytes("48895C2408574883EC208B5908488BF9");
constexpr Sha256 kMembershipClearPrefixSha =
    hex_bytes("27403DED6E5B0F7732E8D91C606C3A1F1B9A05522A17A92FA30687F88E4FC0C2");
constexpr auto kLocalRegistrationPrefix = hex_bytes("48895C240848896C2410488974241857");
constexpr Sha256 kLocalRegistrationPrefixSha =
    hex_bytes("442A19659FE9096DE3A84E01A789D0005AB6710232EAA0A6A807AB732A813780");
constexpr auto kSubscriberRegisterPrefix = hex_bytes("8B054ACF25024C8D0C40488D05FFCC2502");
constexpr Sha256 kSubscriberRegisterPrefixSha =
    hex_bytes("F2A7004EA8A4A20151FB2C2B34E86E564C2252F186DF0C45996864F93F2C73FE");
constexpr auto kEventDispatchPrefix = hex_bytes("48895C2420574883EC30488B057784BC01");
constexpr Sha256 kEventDispatchPrefixSha =
    hex_bytes("222FD80732903695DFC17DACC3882AC2E690C3E4A7BC9DB6E13374CBAF77CEA5");
constexpr auto kEventEnqueuePrefix = hex_bytes("40534883EC208BD9488D0D61CD2502E8CCF0E9FF");
constexpr Sha256 kEventEnqueuePrefixSha =
    hex_bytes("ED2838D412FF9F54F44A1350277580631738E8D6055A1D697D74CCEC799FB385");
constexpr auto kVmRunnerPrefix = hex_bytes("405641574881ECA8010000488B05D6180201");
constexpr Sha256 kVmRunnerPrefixSha =
    hex_bytes("E25A28D399D9C504FD032B064AC7CE8036F006885480623A49F1A5A5BFA18941");
constexpr auto kVmListPrefix = hex_bytes("48895C24084889742410574883EC2033DB");
constexpr Sha256 kVmListPrefixSha =
    hex_bytes("24D268243189C9809388D368AF7152C904BFA5121FB8F3606D181ED8EAD66A1F");
constexpr auto kVmNodePrefix = hex_bytes("415541564881EC38010000488B0556856801");
constexpr Sha256 kVmNodePrefixSha =
    hex_bytes("E3B87AB03BF1EA0A9C52D5D499E42C9FD9F07B71679F16C840634D4033A1F628");
constexpr auto kActivityRegisterPrefix =
    hex_bytes("4055415541564157488DAC2478FDFFFF4881EC88030000");
constexpr Sha256 kActivityRegisterPrefixSha =
    hex_bytes("20ACE3E7833F4FFB63453F1D8DF37E681D778366E19B7C6D68DB81FBC31A9E50");
constexpr auto kActivityCallbackPrefix =
    hex_bytes("4053405556574156488DAC24E8FDFFFF4881EC18030000");
constexpr Sha256 kActivityCallbackPrefixSha =
    hex_bytes("E125E72AE6F0CAE424F9257F2CA0496BB9D43CFE1E06BAF003C0739B1B55650B");
constexpr auto kType53ApplyPrefix = hex_bytes("40534883EC30448B02488BD94C8B4A08");
constexpr Sha256 kType53ApplyPrefixSha =
    hex_bytes("B3E5264D86040F38D29CA99111B05C671B40068AEADB4584B34854DF6756D81F");
constexpr auto kType53TickPrefix = hex_bytes("41564883EC50448B094C8BF1488B05DDFA4201");
constexpr Sha256 kType53TickPrefixSha =
    hex_bytes("15A11375CEDAF7BB1F6A7443CCE0B6FA02D1EBCF5955A4629A3AF047C862D49E");
constexpr auto kSelectedRowPrefix = hex_bytes("40534883EC30448B11458BC2488B058D044301");
constexpr Sha256 kSelectedRowPrefixSha =
    hex_bytes("95A2922317DDED14DA4980D6B0AA26F1FFA522871E1A82B9874053E7DC08A9CA");
constexpr auto kSubmitPrefix = hex_bytes("40534883EC30813AC59D1C81488BD9");
constexpr Sha256 kSubmitPrefixSha =
    hex_bytes("F35ED28142DD6383A40A8EC73929D33DB4CD4A0BFA71CF704DFFF642DC35DB56");
constexpr auto kActualStartPrefix = hex_bytes("4C8BDC5641564883EC78488B0567C36601");
constexpr Sha256 kActualStartPrefixSha =
    hex_bytes("1566D9E1A9D8285D4251395C8E7053533B0B5DFD0FC0F7397B6B588F08AC23D7");
constexpr auto kTeardownPrefix = hex_bytes("488BC4534883EC6048896808488970F0");
constexpr Sha256 kTeardownPrefixSha =
    hex_bytes("A660792618DAFBA739D49D506993E32BE06D8DFAB18D0C55444FEA4FFA2017E2");
constexpr auto kDelegateSetterPrefix = hex_bytes("48890D613FD801C3");
constexpr Sha256 kDelegateSetterPrefixSha =
    hex_bytes("1F30F90A74EE9B38A8942C69EE3E7B9226FC3D01EC341C6287B01AE629C09512");
constexpr auto kPs4ApplyPrefix = hex_bytes("554889E54156534883EC204C8B3596108801");
constexpr Sha256 kPs4ApplyPrefixSha =
    hex_bytes("C9FC3A83946A741DEF97C8FBD65CBDEEF32A4CE4F99DADC331850FD1F868F284");
constexpr auto kPs4TickPrefix = hex_bytes("554889E54157415641554154534883EC48");
constexpr Sha256 kPs4TickPrefixSha =
    hex_bytes("812F0BA5935AB1DD8EF60DFD851588D7684215C3A4104C98AE7AB7F559F1B0DE");
constexpr auto kPs4WrapperPrefix = hex_bytes("4531C0E908000000909090909090");
constexpr Sha256 kPs4WrapperPrefixSha =
    hex_bytes("BE0FB935EF2BDDBE9E2680EEB82F1CF808155A67B4F6E13A61F8A239BD295F9C");
constexpr auto kPs4CorePrefix = hex_bytes("554889E54157415641554154534883EC68");
constexpr Sha256 kPs4CorePrefixSha =
    hex_bytes("2CB7697087752F82EEFC1844D9D15E6AEA102FC7A268C8163C219445011C2917");

[[nodiscard]] constexpr ObservationBracket observation_bracket_for(NativeSurface surface,
                                                                   BoundaryKind kind) noexcept {
    if (kind == BoundaryKind::post_state || kind == BoundaryKind::callsite_after
        || kind == BoundaryKind::instruction_after
        || kind == BoundaryKind::function_return_after_original) {
        return ObservationBracket::after_original;
    }
    if (kind == BoundaryKind::instruction_before || kind == BoundaryKind::callsite_before) {
        return ObservationBracket::instruction_point;
    }
    if (surface == NativeSurface::type60_create_register
        || surface == NativeSurface::type60_containment
        || surface == NativeSurface::type60_membership_set
        || surface == NativeSurface::type60_membership_clear
        || surface == NativeSurface::voice_actual_start
        || surface == NativeSurface::voice_teardown_update) {
        return ObservationBracket::bracket_original;
    }
    return ObservationBracket::entry_only;
}

[[nodiscard]] NativeBoundaryDescriptor pc_boundary(NativeSurface surface,
                                                   BoundaryKind kind,
                                                   NativeAbi abi,
                                                   std::uintptr_t rva,
                                                   std::span<const std::byte> prefix = {},
                                                   Sha256 prefixSha = {},
                                                   std::size_t exactActiveBytes = 0U) noexcept {
    return {surface,
            Platform::pc_windows_x64,
            kind,
            abi,
            rva,
            static_cast<std::size_t>(rva),
            prefix,
            prefixSha,
            observation_bracket_for(surface, kind),
            exactActiveBytes,
            false};
}

[[nodiscard]] NativeBoundaryDescriptor ps4_boundary(NativeSurface surface,
                                                    BoundaryKind kind,
                                                    NativeAbi abi,
                                                    std::uintptr_t rva,
                                                    std::span<const std::byte> prefix = {},
                                                    Sha256 prefixSha = {},
                                                    std::size_t exactActiveBytes = 0U) noexcept {
    return {surface,
            Platform::ps4_sysv_amd64,
            kind,
            abi,
            rva,
            static_cast<std::size_t>(rva) + kPs4CodeFileOffsetBias,
            prefix,
            prefixSha,
            observation_bracket_for(surface, kind),
            exactActiveBytes,
            false};
}

[[nodiscard]] bool valid_context_shape(const CaptureContext& context) noexcept {
    if ((context.presence_mask & ~kCompleteContextMask) != 0U) {
        return false;
    }
    if (context_has(context, ContextField::destination) && context.destination != kOmegaScenario) {
        return false;
    }
    if (context_has(context, ContextField::build_provenance)) {
        if (context.origin == CaptureOrigin::pc_client
            && context.build.artifact() != ArtifactKind::pc_unpacked_reference) {
            return false;
        }
        if (context.origin == CaptureOrigin::ps4_client
            && context.build.artifact() != ArtifactKind::ps4_eboot_reference) {
            return false;
        }
        if (context.origin == CaptureOrigin::retail_host_external_boundary
            && context.build.artifact() != ArtifactKind::external_retail_host_build) {
            return false;
        }
        if (context.origin != CaptureOrigin::retail_host_external_boundary
            && (context.build.status() != BuildProvenanceStatus::exact_pinned_bytes_verified
                || !matches_pinned_artifact(context.build.artifact(), context.build.identity()))) {
            return false;
        }
        if (context.origin == CaptureOrigin::retail_host_external_boundary) {
            const ArtifactIdentity identity = context.build.identity();
            const bool hasHash =
                std::any_of(identity.sha256.begin(), identity.sha256.end(), [](std::byte value) {
                    return value != std::byte{0U};
                });
            if (context.build.status() != BuildProvenanceStatus::external_digest_unverified
                || identity.file_bytes == 0U || !hasHash) {
                return false;
            }
        }
    } else if (context.build.status() != BuildProvenanceStatus::absent) {
        return false;
    }
    return true;
}

[[nodiscard]] bool valid_header(const CaptureHeader& header) noexcept {
    return valid_context_shape(header.context);
}

template <typename Record>
[[nodiscard]] bool same_without_sequence(Record left, Record right) noexcept {
    left.header.sequence = 0U;
    right.header.sequence = 0U;
    return left == right;
}

[[nodiscard]] bool type60_phase(CapturePhase phase) noexcept {
    return phase == CapturePhase::type60_register || phase == CapturePhase::type60_start
           || phase == CapturePhase::type60_containment
           || phase == CapturePhase::type60_membership_set
           || phase == CapturePhase::type60_membership_clear;
}

[[nodiscard]] bool dynamic_phase(CapturePhase phase) noexcept {
    return phase == CapturePhase::subscriber_register || phase == CapturePhase::subscriber_remove
           || phase == CapturePhase::event_enqueue || phase == CapturePhase::event_dispatch
           || phase == CapturePhase::event_drain || phase == CapturePhase::activity_script_register
           || phase == CapturePhase::activity_script_callback || phase == CapturePhase::vm_runner
           || phase == CapturePhase::vm_node;
}

[[nodiscard]] bool presentation_phase(CapturePhase phase) noexcept {
    return phase == CapturePhase::row_zero_submit || phase == CapturePhase::actual_start
           || phase == CapturePhase::timed_presentation || phase == CapturePhase::deadline_drop
           || phase == CapturePhase::stop || phase == CapturePhase::free_record;
}

} // namespace

Sha256 sha256(std::span<const std::byte> bytes) noexcept {
    std::array<std::uint32_t, 8U> state{0x6A09E667U,
                                        0xBB67AE85U,
                                        0x3C6EF372U,
                                        0xA54FF53AU,
                                        0x510E527FU,
                                        0x9B05688CU,
                                        0x1F83D9ABU,
                                        0x5BE0CD19U};
    std::size_t offset = 0U;
    while (bytes.size() - offset >= 64U) {
        sha256_compress(state, bytes.data() + offset);
        offset += 64U;
    }

    std::array<std::byte, 128U> tail{};
    const std::size_t remaining = bytes.size() - offset;
    if (remaining != 0U) {
        std::copy_n(bytes.data() + offset, remaining, tail.data());
    }
    tail[remaining] = std::byte{0x80U};
    const std::size_t paddedBytes = remaining < 56U ? 64U : 128U;
    const std::uint64_t bitCount = static_cast<std::uint64_t>(bytes.size()) * 8U;
    for (std::size_t index = 0U; index < 8U; ++index) {
        tail[paddedBytes - 1U - index] =
            std::byte{static_cast<std::uint8_t>(bitCount >> (index * 8U))};
    }
    sha256_compress(state, tail.data());
    if (paddedBytes == 128U) {
        sha256_compress(state, tail.data() + 64U);
    }

    Sha256 output{};
    for (std::size_t index = 0U; index < state.size(); ++index) {
        output[index * 4U] = std::byte{static_cast<std::uint8_t>(state[index] >> 24U)};
        output[index * 4U + 1U] = std::byte{static_cast<std::uint8_t>(state[index] >> 16U)};
        output[index * 4U + 2U] = std::byte{static_cast<std::uint8_t>(state[index] >> 8U)};
        output[index * 4U + 3U] = std::byte{static_cast<std::uint8_t>(state[index])};
    }
    return output;
}

const ArtifactDescriptor& artifact_descriptor(ArtifactKind kind) noexcept {
    switch (kind) {
    case ArtifactKind::pc_unpacked_reference:
        return kPcArtifact;
    case ArtifactKind::ps4_eboot_reference:
        return kPs4Artifact;
    case ArtifactKind::mercury_mission_package:
        return kMissionArtifact;
    case ArtifactKind::external_retail_host_build:
    default:
        return kExternalHostArtifact;
    }
}

bool matches_pinned_artifact(ArtifactKind kind, const ArtifactIdentity& identity) noexcept {
    if (kind != ArtifactKind::pc_unpacked_reference && kind != ArtifactKind::ps4_eboot_reference
        && kind != ArtifactKind::mercury_mission_package) {
        return false;
    }
    const ArtifactDescriptor& descriptor = artifact_descriptor(kind);
    return identity.file_bytes == descriptor.file_bytes && identity.sha256 == descriptor.sha256;
}

ArtifactVerificationResult verify_pinned_artifact(ArtifactKind kind,
                                                  std::span<const std::byte> immutableFile,
                                                  VerifiedArtifact& output) noexcept {
    output = {};
    if (kind != ArtifactKind::pc_unpacked_reference && kind != ArtifactKind::ps4_eboot_reference
        && kind != ArtifactKind::mercury_mission_package) {
        return ArtifactVerificationResult::unsupported_artifact;
    }
    const ArtifactDescriptor& descriptor = artifact_descriptor(kind);
    if (immutableFile.size() != descriptor.file_bytes) {
        return ArtifactVerificationResult::exact_size_mismatch;
    }
    const Sha256 digest = sha256(immutableFile);
    if (digest != descriptor.sha256) {
        return ArtifactVerificationResult::sha256_mismatch;
    }
    output.kind_ = kind;
    output.identity_ = {descriptor.file_bytes, digest};
    output.verified_data_ = immutableFile.data();
    output.verified_size_ = immutableFile.size();
    output.valid_ = true;
    return ArtifactVerificationResult::verified;
}

PackageEntryProvenance package_entry_provenance(std::uint32_t tag) noexcept {
    switch (tag) {
    case 0x80F47B5BU:
        return {tag,
                0x03A3U,
                7003U,
                0x80809462U,
                316U,
                0x1DE10U,
                35U,
                0x295A0U,
                3U,
                0x4E800U,
                12804U,
                0x3U};
    case 0x80F47B4FU:
        return {tag,
                0x03A3U,
                6991U,
                0x80809C36U,
                4487U,
                0x1DD50U,
                35U,
                0x27FA0U,
                3U,
                0x4E800U,
                12804U,
                0x3U};
    case 0x80F47BC6U:
        return {tag,
                0x03A3U,
                7110U,
                0x80809462U,
                408U,
                0x1E4C0U,
                35U,
                0x2EAF0U,
                3U,
                0x4E800U,
                12804U,
                0x3U};
    case 0x80F47BDAU:
        return {tag,
                0x03A3U,
                7130U,
                0x80809C36U,
                5360U,
                0x1E600U,
                35U,
                0x2FA90U,
                3U,
                0x4E800U,
                12804U,
                0x3U};
    case 0x80F1FD07U:
        return {tag,
                0x038FU,
                7431U,
                0x80808D54U,
                6048U,
                0x1E8D0U,
                21U,
                0x5E40U,
                2U,
                0x12C800U,
                140317U,
                0x3U};
    default:
        return {};
    }
}

NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept {
    switch (surface) {
    case NativeSurface::type60_create_register:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::instance_opaque_create_context_bool,
                           0xA70500U,
                           kType60CreatePrefix,
                           kType60CreatePrefixSha);
    case NativeSurface::type60_start:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::instance_only,
                           0xA6E920U,
                           kType60StartPrefix,
                           kType60StartPrefixSha);
    case NativeSurface::type60_lifecycle_clear:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::instance_only, 0xA70300U);
    case NativeSurface::type60_containment:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::record_subject_bool,
                           0xA70E30U,
                           kType60ContainmentPrefix,
                           kType60ContainmentPrefixSha);
    case NativeSurface::type60_membership_set:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::runtime_entry_player,
                           0xA71E20U,
                           kMembershipSetPrefix,
                           kMembershipSetPrefixSha);
    case NativeSurface::type60_membership_clear:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::runtime_entry_player,
                           0xA71E40U,
                           kMembershipClearPrefix,
                           kMembershipClearPrefixSha);
    case NativeSurface::type60_local_registration:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::instance_token_registration_pair,
                           0x4E7750U,
                           kLocalRegistrationPrefix,
                           kLocalRegistrationPrefixSha);
    case NativeSurface::type60_authored_record_start:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::instance_token, 0x4E3DA0U);
    case NativeSurface::event_subscriber_register:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::callback_mask_context,
                           0x4E1870U,
                           kSubscriberRegisterPrefix,
                           kSubscriberRegisterPrefixSha);
    case NativeSurface::event_subscriber_remove:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::callback_only, 0x4E18A0U);
    case NativeSurface::event_enqueue:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::event_only,
                           0x4E17E0U,
                           kEventEnqueuePrefix,
                           kEventEnqueuePrefixSha);
    case NativeSurface::event_enqueue_payload:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::event_payload, 0x4E1820U);
    case NativeSurface::event_dispatch:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::event_only,
                           0x4E1600U,
                           kEventDispatchPrefix,
                           kEventDispatchPrefixSha);
    case NativeSurface::event_dispatch_payload:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::event_payload, 0x4E16A0U);
    case NativeSurface::event_drain:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::no_arguments, 0x4E1920U);
    case NativeSurface::activity_script_component_register:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::manager_slot_mode_value_bool,
                           0x17898A0U,
                           kActivityRegisterPrefix,
                           kActivityRegisterPrefixSha);
    case NativeSurface::activity_script_event_callback:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::manager_payload_event,
                           0x1789B6EU,
                           kActivityCallbackPrefix,
                           kActivityCallbackPrefixSha);
    case NativeSurface::vm_runner_tick:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::instance_only,
                           0x10881A0U,
                           kVmRunnerPrefix,
                           kVmRunnerPrefixSha);
    case NativeSurface::vm_list_sequence:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::list_context,
                           0xA214C0U,
                           kVmListPrefix,
                           kVmListPrefixSha);
    case NativeSurface::vm_node_dispatch:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::node_context,
                           0xA21520U,
                           kVmNodePrefix,
                           kVmNodePrefixSha);
    case NativeSurface::type53_apply:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::instance_packet_reference,
                           0x1009B60U,
                           kType53ApplyPrefix,
                           kType53ApplyPrefixSha);
    case NativeSurface::type53_decoded_body:
        return pc_boundary(
            surface, BoundaryKind::callsite_after, NativeAbi::observation_point, 0x1009B88U);
    case NativeSurface::type53_post_copy:
        return pc_boundary(
            surface, BoundaryKind::post_state, NativeAbi::observation_point, 0x1009BF9U);
    case NativeSurface::type53_tick:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::instance_only,
                           0x100A180U,
                           kType53TickPrefix,
                           kType53TickPrefixSha);
    case NativeSurface::type53_generation_compare:
        return pc_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x100A267U);
    case NativeSurface::type53_active_time_predicate:
        return pc_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x100A27BU);
    case NativeSurface::type53_record_reference_gate:
        return pc_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x100A30AU);
    case NativeSurface::type53_root_reference_gate:
        return pc_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x100A31AU);
    case NativeSurface::type53_root_predicate:
        return pc_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x100A32AU);
    case NativeSurface::type53_record_predicate:
        return pc_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x100A337U);
    case NativeSurface::type53_selected_row_call:
        return pc_boundary(
            surface, BoundaryKind::callsite_before, NativeAbi::component_row, 0x100A34EU);
    case NativeSurface::type53_post_terminal:
        return pc_boundary(
            surface, BoundaryKind::callsite_after, NativeAbi::observation_point, 0x100A353U);
    case NativeSurface::type53_post_generation_store:
        return pc_boundary(
            surface, BoundaryKind::post_state, NativeAbi::observation_point, 0x100A35EU);
    case NativeSurface::selected_row_extractor:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::component_row,
                           0x10097D0U,
                           kSelectedRowPrefix,
                           kSelectedRowPrefixSha);
    case NativeSurface::terminal_submit:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::terminal_selector_pair,
                           0xA38490U,
                           kSubmitPrefix,
                           kSubmitPrefixSha);
    case NativeSurface::terminal_core:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::terminal_selector_pair, 0xA38530U);
    case NativeSurface::voice_leaf_queue:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::terminal_selector_pair, 0xA3CB80U);
    case NativeSurface::voice_arbitrate:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::manager_only, 0xA3D3A0U);
    case NativeSurface::voice_actual_start:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::manager_runtime_handle,
                           0xA3D710U,
                           kActualStartPrefix,
                           kActualStartPrefixSha);
    case NativeSurface::timed_presentation_publish:
        return pc_boundary(
            surface, BoundaryKind::function_entry, NativeAbi::lookup_pair_timing, 0x1320380U);
    case NativeSurface::voice_teardown_update:
        return pc_boundary(surface,
                           BoundaryKind::function_entry,
                           NativeAbi::manager_only,
                           0xA3E2D0U,
                           kTeardownPrefix,
                           kTeardownPrefixSha);
    case NativeSurface::timed_delegate_install:
        return pc_boundary(
            surface, BoundaryKind::callsite_before, NativeAbi::delegate_pointer, 0x1322AD0U);
    case NativeSurface::timed_delegate_setter:
        return pc_boundary(surface,
                           BoundaryKind::exact_short_function,
                           NativeAbi::delegate_pointer,
                           0xA3A6D0U,
                           kDelegateSetterPrefix,
                           kDelegateSetterPrefixSha,
                           8U);
    case NativeSurface::ps4_type53_apply:
        return ps4_boundary(surface,
                            BoundaryKind::function_entry,
                            NativeAbi::instance_packet_reference,
                            0x004A5D10U,
                            kPs4ApplyPrefix,
                            kPs4ApplyPrefixSha);
    case NativeSurface::ps4_type53_decoded_body:
        return ps4_boundary(
            surface, BoundaryKind::callsite_after, NativeAbi::observation_point, 0x004A5D4EU);
    case NativeSurface::ps4_type53_post_copy:
        return ps4_boundary(
            surface, BoundaryKind::post_state, NativeAbi::observation_point, 0x004A5D65U);
    case NativeSurface::ps4_type53_tick:
        return ps4_boundary(surface,
                            BoundaryKind::function_entry,
                            NativeAbi::instance_only,
                            0x004A5D80U,
                            kPs4TickPrefix,
                            kPs4TickPrefixSha);
    case NativeSurface::ps4_type53_active_time_predicate:
        return ps4_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x004A5E79U);
    case NativeSurface::ps4_type53_record_reference_gate:
        return ps4_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x004A5F1EU);
    case NativeSurface::ps4_type53_root_reference_gate:
        return ps4_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x004A5F2BU);
    case NativeSurface::ps4_type53_root_predicate:
        return ps4_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x004A5F38U);
    case NativeSurface::ps4_type53_record_predicate:
        return ps4_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x004A5F48U);
    case NativeSurface::ps4_type53_terminal_call:
        return ps4_boundary(
            surface, BoundaryKind::callsite_before, NativeAbi::terminal_selector_pair, 0x004A6005U);
    case NativeSurface::ps4_type53_post_terminal:
        return ps4_boundary(
            surface, BoundaryKind::callsite_after, NativeAbi::observation_point, 0x004A600AU);
    case NativeSurface::ps4_type53_generation_store:
        return ps4_boundary(
            surface, BoundaryKind::instruction_before, NativeAbi::observation_point, 0x004A6012U);
    case NativeSurface::ps4_type53_post_generation_store:
        return ps4_boundary(
            surface, BoundaryKind::post_state, NativeAbi::observation_point, 0x004A601DU);
    case NativeSurface::ps4_terminal_wrapper:
        return ps4_boundary(surface,
                            BoundaryKind::exact_short_function,
                            NativeAbi::terminal_selector_pair,
                            0x001DC9B0U,
                            kPs4WrapperPrefix,
                            kPs4WrapperPrefixSha,
                            8U);
    case NativeSurface::ps4_terminal_core:
        return ps4_boundary(surface,
                            BoundaryKind::function_entry,
                            NativeAbi::terminal_selector_pair,
                            0x001DC9C0U,
                            kPs4CorePrefix,
                            kPs4CorePrefixSha);
    default:
        return {};
    }
}

bool native_prefix_matches(NativeSurface surface, std::span<const std::byte> observed) noexcept {
    const NativeBoundaryDescriptor descriptor = native_boundary(surface);
    return !descriptor.prefix.empty() && observed.size() >= descriptor.prefix.size()
           && std::equal(descriptor.prefix.begin(), descriptor.prefix.end(), observed.begin());
}

EndpointValidation validate_reference_endpoint(const VerifiedArtifact& artifactToken,
                                               std::span<const std::byte> referenceFile,
                                               NativeSurface surface) noexcept {
    if (!artifactToken.valid_) {
        return EndpointValidation::invalid_verified_artifact;
    }
    if (artifactToken.verified_data_ != referenceFile.data()
        || artifactToken.verified_size_ != referenceFile.size()) {
        return EndpointValidation::mapping_not_bound_to_verification;
    }
    const NativeBoundaryDescriptor descriptor = native_boundary(surface);
    if (descriptor.rva == 0U) {
        return EndpointValidation::invalid_surface;
    }
    const ArtifactKind expectedArtifact = descriptor.platform == Platform::pc_windows_x64
                                              ? ArtifactKind::pc_unpacked_reference
                                              : ArtifactKind::ps4_eboot_reference;
    if (artifactToken.kind_ != expectedArtifact
        || !matches_pinned_artifact(expectedArtifact, artifactToken.identity_)) {
        return EndpointValidation::wrong_artifact;
    }
    if (descriptor.prefix.empty()) {
        return EndpointValidation::no_prefix_anchor;
    }
    if (descriptor.file_offset > referenceFile.size()
        || descriptor.prefix.size() > referenceFile.size() - descriptor.file_offset) {
        return EndpointValidation::target_out_of_range;
    }
    return native_prefix_matches(surface, referenceFile.subspan(descriptor.file_offset))
               ? EndpointValidation::reference_anchor_valid
               : EndpointValidation::prefix_mismatch;
}

EndpointAttachability endpoint_attachability(NativeSurface surface) noexcept {
    return native_boundary(surface).rva == 0U
               ? EndpointAttachability::reference_anchor_only
               : EndpointAttachability::structural_owner_and_call_edge_required;
}

bool bind_verified_build(CaptureContext& context, const VerifiedArtifact& artifact) noexcept {
    if (!artifact.valid()
        || (artifact.kind() != ArtifactKind::pc_unpacked_reference
            && artifact.kind() != ArtifactKind::ps4_eboot_reference)) {
        return false;
    }
    if ((context.origin == CaptureOrigin::pc_client
         && artifact.kind() != ArtifactKind::pc_unpacked_reference)
        || (context.origin == CaptureOrigin::ps4_client
            && artifact.kind() != ArtifactKind::ps4_eboot_reference)
        || context.origin == CaptureOrigin::retail_host_external_boundary) {
        return false;
    }
    context.build.artifact_ = artifact.kind();
    context.build.identity_ = artifact.identity();
    context.build.status_ = BuildProvenanceStatus::exact_pinned_bytes_verified;
    context.presence_mask |= static_cast<std::uint32_t>(ContextField::build_provenance);
    return true;
}

bool bind_external_build_digest(CaptureContext& context,
                                const ArtifactIdentity& identity) noexcept {
    const bool hasHash = std::any_of(identity.sha256.begin(),
                                     identity.sha256.end(),
                                     [](std::byte value) { return value != std::byte{0U}; });
    if (context.origin != CaptureOrigin::retail_host_external_boundary || identity.file_bytes == 0U
        || !hasHash) {
        return false;
    }
    context.build.artifact_ = ArtifactKind::external_retail_host_build;
    context.build.identity_ = identity;
    context.build.status_ = BuildProvenanceStatus::external_digest_unverified;
    context.presence_mask |= static_cast<std::uint32_t>(ContextField::build_provenance);
    return true;
}

bool fully_correlated(const CaptureContext& context) noexcept {
    if (!valid_context_shape(context) || context.presence_mask != kCompleteContextMask) {
        return false;
    }
    return context.session_pseudonym != 0U && context.patch_epoch != 0U
           && context.destination == kOmegaScenario && context.roster_generation != 0U
           && context.activity_instance_generation != 0U && context.thread_id != 0U
           && context.call_id != 0U && context.monotonic_tick != 0U && context.return_rva != 0U
           && context.build.status() != BuildProvenanceStatus::absent
           && context.build.identity().file_bytes != 0U;
}

bool valid_evidence(const Type60Evidence& evidence) noexcept {
    if (!valid_header(evidence.header) || !type60_phase(evidence.header.phase)
        || !type60_has(evidence.presence_mask, Type60Field::exact_identity)
        || evidence.definition != kGhostVolumeDefinition
        || evidence.registry != kGhostVolumeRegistry || evidence.type != kGhostVolumeType
        || evidence.index != kGhostVolumeIndex || evidence.name_hash != kGhostVolumeNameHash
        || !type60_has(evidence.presence_mask, Type60Field::instance_pseudonym)
        || evidence.instance_pseudonym == 0U) {
        return false;
    }
    if (evidence.header.phase == CapturePhase::type60_register
        || evidence.header.phase == CapturePhase::type60_start) {
        return type60_has(evidence.presence_mask, Type60Field::record_pseudonym)
               && evidence.record_pseudonym != 0U
               && type60_has(evidence.presence_mask, Type60Field::registration_token)
               && (evidence.header.phase != CapturePhase::type60_register
                   || type60_has(evidence.presence_mask, Type60Field::registration_pair));
    }
    if (evidence.header.phase == CapturePhase::type60_containment) {
        const bool required =
            type60_has(evidence.presence_mask, Type60Field::record_pseudonym)
            && type60_has(evidence.presence_mask, Type60Field::subject_pseudonym)
            && type60_has(evidence.presence_mask, Type60Field::player_pseudonym)
            && type60_has(evidence.presence_mask, Type60Field::overlap_kind)
            && type60_has(evidence.presence_mask, Type60Field::world_latch)
            && type60_has(evidence.presence_mask, Type60Field::player_present_latch)
            && type60_has(evidence.presence_mask, Type60Field::inside_result);
        return required && evidence.record_pseudonym != 0U && evidence.subject_pseudonym != 0U
               && evidence.player_pseudonym != 0U
               && evidence.overlap_kind != OverlapObservationKind::unknown
               && evidence.inside == evidence.original_result;
    }
    const bool membershipFields =
        type60_has(evidence.presence_mask, Type60Field::record_pseudonym)
        && type60_has(evidence.presence_mask, Type60Field::player_pseudonym)
        && type60_has(evidence.presence_mask, Type60Field::player_index)
        && type60_has(evidence.presence_mask, Type60Field::membership_before)
        && type60_has(evidence.presence_mask, Type60Field::membership_after)
        && evidence.player_pseudonym != 0U && evidence.player_index < 64U;
    if (!membershipFields) {
        return false;
    }
    const std::uint64_t bit = std::uint64_t{1U} << evidence.player_index;
    return evidence.header.phase == CapturePhase::type60_membership_set
               ? evidence.membership_after == (evidence.membership_before | bit)
               : evidence.membership_after == (evidence.membership_before & ~bit);
}

bool valid_evidence(const DynamicBridgeEvidence& evidence) noexcept {
    if (!valid_header(evidence.header) || !dynamic_phase(evidence.header.phase)) {
        return false;
    }
    switch (evidence.boundary) {
    case DynamicBoundaryKind::subscriber_registration:
        return evidence.header.phase == CapturePhase::subscriber_register
               && dynamic_has(evidence.presence_mask, DynamicField::callback_rva)
               && dynamic_has(evidence.presence_mask, DynamicField::subscriber_context_pseudonym)
               && dynamic_has(evidence.presence_mask, DynamicField::subscriber_mask)
               && evidence.callback_rva != 0U && evidence.subscriber_context_pseudonym != 0U
               && evidence.subscriber_mask != 0U;
    case DynamicBoundaryKind::subscriber_removal:
        return evidence.header.phase == CapturePhase::subscriber_remove
               && dynamic_has(evidence.presence_mask, DynamicField::callback_rva)
               && evidence.callback_rva != 0U;
    case DynamicBoundaryKind::bus_enqueue:
        return evidence.header.phase == CapturePhase::event_enqueue
               && dynamic_has(evidence.presence_mask, DynamicField::event_ordinal)
               && dynamic_has(evidence.presence_mask, DynamicField::payload_length)
               && (evidence.payload_bytes == 0U
                       ? !dynamic_has(evidence.presence_mask, DynamicField::payload_sha256)
                       : dynamic_has(evidence.presence_mask, DynamicField::payload_sha256)
                             && !is_zero_digest(evidence.payload_sha256));
    case DynamicBoundaryKind::bus_dispatch:
        return evidence.header.phase == CapturePhase::event_dispatch
               && dynamic_has(evidence.presence_mask, DynamicField::callback_rva)
               && dynamic_has(evidence.presence_mask, DynamicField::subscriber_context_pseudonym)
               && dynamic_has(evidence.presence_mask, DynamicField::event_ordinal)
               && dynamic_has(evidence.presence_mask, DynamicField::payload_length)
               && evidence.callback_rva != 0U && evidence.subscriber_context_pseudonym != 0U
               && (evidence.payload_bytes == 0U
                       ? !dynamic_has(evidence.presence_mask, DynamicField::payload_sha256)
                       : dynamic_has(evidence.presence_mask, DynamicField::payload_sha256)
                             && !is_zero_digest(evidence.payload_sha256));
    case DynamicBoundaryKind::bus_drain:
        return evidence.header.phase == CapturePhase::event_drain;
    case DynamicBoundaryKind::activity_component_registration:
        return evidence.header.phase == CapturePhase::activity_script_register
               && dynamic_has(evidence.presence_mask, DynamicField::manager_pseudonym)
               && dynamic_has(evidence.presence_mask, DynamicField::activity_fields)
               && evidence.manager_pseudonym != 0U;
    case DynamicBoundaryKind::activity_event_callback:
        return evidence.header.phase == CapturePhase::activity_script_callback
               && dynamic_has(evidence.presence_mask, DynamicField::manager_pseudonym)
               && dynamic_has(evidence.presence_mask, DynamicField::event_ordinal)
               && dynamic_has(evidence.presence_mask, DynamicField::payload_length)
               && evidence.manager_pseudonym != 0U
               && (evidence.payload_bytes == 0U
                       ? !dynamic_has(evidence.presence_mask, DynamicField::payload_sha256)
                       : dynamic_has(evidence.presence_mask, DynamicField::payload_sha256)
                             && !is_zero_digest(evidence.payload_sha256));
    case DynamicBoundaryKind::vm_runner:
    case DynamicBoundaryKind::vm_list:
        return evidence.header.phase == CapturePhase::vm_runner
               && dynamic_has(evidence.presence_mask, DynamicField::vm_context_sha256)
               && !is_zero_digest(evidence.vm_context_sha256);
    case DynamicBoundaryKind::vm_node: {
        const bool ownerValid =
            dynamic_has(evidence.presence_mask, DynamicField::owner_provenance)
            && evidence.owner_provenance != OwnerProvenanceState::not_observed
            && (evidence.owner_provenance == OwnerProvenanceState::absent_observed
                || (evidence.owner_datum != 0U && evidence.owner_tag != 0U));
        return evidence.header.phase == CapturePhase::vm_node
               && dynamic_has(evidence.presence_mask, DynamicField::node_rva)
               && dynamic_has(evidence.presence_mask, DynamicField::node_class)
               && dynamic_has(evidence.presence_mask, DynamicField::vm_context_sha256)
               && evidence.node_rva != 0U && evidence.node_class != 0U && ownerValid
               && !is_zero_digest(evidence.vm_context_sha256);
    }
    default:
        return false;
    }
}

namespace {

[[nodiscard]] bool complete_record_zero(const RecordZeroScalars& record) noexcept {
    constexpr std::uint32_t required =
        static_cast<std::uint32_t>(RecordZeroField::root_reference)
        | static_cast<std::uint32_t>(RecordZeroField::value)
        | static_cast<std::uint32_t>(RecordZeroField::optional_presence)
        | static_cast<std::uint32_t>(RecordZeroField::record_reference)
        | static_cast<std::uint32_t>(RecordZeroField::generation)
        | static_cast<std::uint32_t>(RecordZeroField::mode);
    return (record.presence_mask & required) == required && record.mode <= 3U
           && !is_zero_digest(record.root_reference_sha256)
           && !is_zero_digest(record.record_reference_sha256)
           && (!record.optional_value_present
               || record_zero_has(record, RecordZeroField::optional_value));
}

[[nodiscard]] bool voice_has(const VoiceStateSnapshot& state, VoiceStateField field) noexcept {
    return (state.presence_mask & static_cast<std::uint32_t>(field)) != 0U;
}

} // namespace

bool valid_evidence(const SecurePayloadPrefixEvidence& evidence) noexcept {
    return valid_header(evidence.header) && dynamic_phase(evidence.header.phase)
           && evidence.privacy == PrivacyClass::secured_re_trace
           && evidence.copied_bytes <= kSecurePayloadPrefixBytes
           && evidence.copied_bytes <= evidence.payload_bytes
           && evidence.copied_bytes
                  == (std::min)(evidence.payload_bytes,
                                static_cast<std::uint32_t>(kSecurePayloadPrefixBytes))
           && !is_zero_digest(evidence.payload_sha256);
}

bool valid_evidence(const Type5PublicationEvidence& evidence) noexcept {
    if (!valid_header(evidence.header) || evidence.header.phase != CapturePhase::type5_publication
        || evidence.header.context.origin != CaptureOrigin::retail_host_external_boundary
        || evidence.activity_message_type != kActivityMessageAuthorityType
        || evidence.registry != kGlobalRegistry || evidence.type != kDialogueType
        || evidence.index != kDialogueIndex
        || !authority_has(evidence.presence_mask, AuthorityField::delivery)
        || !authority_has(evidence.presence_mask, AuthorityField::body_state)
        || !authority_has(evidence.presence_mask, AuthorityField::body_present)
        || !authority_has(evidence.presence_mask, AuthorityField::reset)
        || evidence.delivery == Type5Delivery::unknown) {
        return false;
    }
    if (!evidence.body_present) {
        return evidence.body_state == AuthorityBodyState::omitted
               && !authority_has(evidence.presence_mask, AuthorityField::body_sha256)
               && (!authority_has(evidence.presence_mask, AuthorityField::body_bit_count)
                   || evidence.body_bit_count == 0U);
    }
    if (!authority_has(evidence.presence_mask, AuthorityField::body_sha256)
        || !authority_has(evidence.presence_mask, AuthorityField::body_bit_count)
        || is_zero_digest(evidence.body_sha256) || evidence.body_bit_count < 19'767U
        || evidence.body_bit_count > 27'959U) {
        return false;
    }
    return evidence.body_state != AuthorityBodyState::active_record_zero
           || (authority_has(evidence.presence_mask, AuthorityField::record_zero)
               && complete_record_zero(evidence.record_zero));
}

bool valid_evidence(const Type53ApplyEvidence& evidence) noexcept {
    if (!valid_header(evidence.header) || evidence.header.phase != CapturePhase::type53_apply
        || evidence.header.context.origin == CaptureOrigin::retail_host_external_boundary
        || evidence.registry != kGlobalRegistry || evidence.type != kDialogueType
        || evidence.index != kDialogueIndex
        || !authority_has(evidence.presence_mask, AuthorityField::body_state)
        || !authority_has(evidence.presence_mask, AuthorityField::body_present)) {
        return false;
    }
    if (!evidence.body_present) {
        return evidence.body_state == AuthorityBodyState::omitted
               && !authority_has(evidence.presence_mask, AuthorityField::body_sha256);
    }
    if (!authority_has(evidence.presence_mask, AuthorityField::body_sha256)
        || is_zero_digest(evidence.body_sha256)) {
        return false;
    }
    return evidence.body_state != AuthorityBodyState::active_record_zero
           || (authority_has(evidence.presence_mask, AuthorityField::record_zero)
               && complete_record_zero(evidence.record_zero));
}

bool valid_evidence(const Type53ConsumerCorrelation& evidence) noexcept {
    constexpr std::uint32_t required =
        static_cast<std::uint32_t>(ConsumerCorrelationField::record_index)
        | static_cast<std::uint32_t>(ConsumerCorrelationField::bank)
        | static_cast<std::uint32_t>(ConsumerCorrelationField::selector);
    const bool modePresent =
        (evidence.presence_mask & static_cast<std::uint32_t>(ConsumerCorrelationField::mode)) != 0U;
    return valid_header(evidence.header)
           && (evidence.header.phase == CapturePhase::generation_gate
               || evidence.header.phase == CapturePhase::row_zero_submit)
           && evidence.provenance != ConsumerCorrelationProvenance::absent
           && (evidence.presence_mask & required) == required
           && evidence.record_index == kDialogueRecordIndex && evidence.bank == kDialogueBank
           && evidence.selector == kGhostSelector && (!modePresent || evidence.mode <= 3U);
}

bool valid_evidence(const PresentationEvidence& evidence) noexcept {
    if (!valid_header(evidence.header) || !presentation_phase(evidence.header.phase)
        || evidence.selector != kGhostSelector || evidence.bank != kDialogueBank
        || evidence.record_index != kDialogueRecordIndex) {
        return false;
    }
    const bool exactMapping = (evidence.header.phase == CapturePhase::row_zero_submit
                               && evidence.outcome == PresentationOutcome::submitted)
                              || (evidence.header.phase == CapturePhase::actual_start
                                  && evidence.outcome == PresentationOutcome::started)
                              || (evidence.header.phase == CapturePhase::timed_presentation
                                  && evidence.outcome == PresentationOutcome::timed_presented)
                              || (evidence.header.phase == CapturePhase::deadline_drop
                                  && evidence.outcome == PresentationOutcome::deadline_dropped)
                              || (evidence.header.phase == CapturePhase::stop
                                  && evidence.outcome == PresentationOutcome::stopped)
                              || (evidence.header.phase == CapturePhase::free_record
                                  && evidence.outcome == PresentationOutcome::freed);
    if (!exactMapping) {
        return false;
    }
    if (evidence.outcome == PresentationOutcome::submitted) {
        return true;
    }
    if (!voice_has(evidence.after, VoiceStateField::runtime_handle)
        || evidence.after.runtime_record_handle == 0U) {
        return false;
    }
    if (evidence.outcome == PresentationOutcome::started) {
        return voice_has(evidence.after, VoiceStateField::started) && evidence.after.started
               && voice_has(evidence.after, VoiceStateField::audio_handle)
               && evidence.after.audio_handle != 0U;
    }
    if (evidence.outcome == PresentationOutcome::timed_presented) {
        return voice_has(evidence.after, VoiceStateField::started) && evidence.after.started
               && voice_has(evidence.after, VoiceStateField::timed_enabled)
               && evidence.after.timed_presentation_enabled
               && voice_has(evidence.after, VoiceStateField::lookup_pair)
               && evidence.after.lookup_bank != 0U && evidence.after.lookup_hash != 0U;
    }
    return voice_has(evidence.before, VoiceStateField::runtime_handle)
           && evidence.before.runtime_record_handle == evidence.after.runtime_record_handle;
}

bool same_observation(const Type60Evidence& left, const Type60Evidence& right) noexcept {
    return same_without_sequence(left, right);
}

bool same_observation(const DynamicBridgeEvidence& left,
                      const DynamicBridgeEvidence& right) noexcept {
    return same_without_sequence(left, right);
}

bool same_observation(const SecurePayloadPrefixEvidence& left,
                      const SecurePayloadPrefixEvidence& right) noexcept {
    return same_without_sequence(left, right);
}

bool same_observation(const Type5PublicationEvidence& left,
                      const Type5PublicationEvidence& right) noexcept {
    return same_without_sequence(left, right);
}

bool same_observation(const Type53ApplyEvidence& left, const Type53ApplyEvidence& right) noexcept {
    return same_without_sequence(left, right);
}

bool same_observation(const Type53ConsumerCorrelation& left,
                      const Type53ConsumerCorrelation& right) noexcept {
    return same_without_sequence(left, right);
}

bool same_observation(const PresentationEvidence& left,
                      const PresentationEvidence& right) noexcept {
    return same_without_sequence(left, right);
}

bool CaptureRateLimiter::admit(CapturePhase phase, std::uint64_t window) noexcept {
    if (capture_criticality(phase) == CaptureCriticality::causal_transition) {
        return true;
    }
    const std::uint32_t window32 = static_cast<std::uint32_t>(window);
    std::uint64_t observed = window_and_count_.load(std::memory_order_acquire);
    for (;;) {
        const std::uint32_t observedWindow = static_cast<std::uint32_t>(observed >> 32U);
        const std::uint32_t observedCount = static_cast<std::uint32_t>(observed);
        if (observedWindow == window32 && observedCount >= high_frequency_limit_) {
            rate_limited_.fetch_add(1U, std::memory_order_relaxed);
            return false;
        }
        const std::uint32_t nextCount = observedWindow == window32 ? observedCount + 1U : 1U;
        const std::uint64_t desired = (static_cast<std::uint64_t>(window32) << 32U) | nextCount;
        if (window_and_count_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

GenerationDecision evaluate_generation(const GenerationInputs& inputs) noexcept {
    if (inputs.record_generation == inputs.processed_generation) {
        return {GenerationDisposition::suppressed_equal_generation, false, false, false};
    }
    if (inputs.mode > 3U) {
        return {GenerationDisposition::malformed_mode, false, false, false};
    }
    if (inputs.mode == 3U) {
        return {GenerationDisposition::unknown_unrecovered_mode, false, false, false};
    }
    if (!inputs.active_time_predicate) {
        return {GenerationDisposition::retry_inactive, false, false, true};
    }

    const bool durationFailure = inputs.mode != 2U && inputs.duration_expired;
    const bool referenceOrPredicateFailure =
        !inputs.record_reference_passed || !inputs.root_reference_passed
        || !inputs.root_predicate_passed || !inputs.record_predicate_passed;
    if (durationFailure || referenceOrPredicateFailure) {
        if (inputs.mode == 1U) {
            return {GenerationDisposition::consumed_without_submit, false, true, false};
        }
        return {GenerationDisposition::retry_eligibility_failure, false, false, true};
    }
    if (!inputs.selected_row_call_observed) {
        return {GenerationDisposition::incomplete_capture, false, false, false};
    }
    return {GenerationDisposition::submitted_and_consumed, true, true, false};
}

ReplayAssessment assess_replay(const ReplayEvidence& evidence) noexcept {
    ReplayAssessment result{};
    result.generation_equal = evidence.record_generation == evidence.processed_generation;
    result.selector_equal = evidence.selector_observed && evidence.previous_selector_observed
                            && evidence.selector == evidence.previous_selector;

    if (evidence.delivery == Type5Delivery::unknown) {
        result.disposition = ReplayDisposition::invalid;
        result.host_policy_required = true;
        return result;
    }
    if (evidence.body_state == AuthorityBodyState::omitted) {
        result.disposition = ReplayDisposition::no_body_in_snapshot_or_delta;
        result.host_policy_required = true;
        return result;
    }
    if (evidence.body_state == AuthorityBodyState::neutral) {
        result.disposition = ReplayDisposition::neutralized_by_host;
        result.host_policy_required = true;
        return result;
    }
    if (!evidence.processed_mirror_observed || evidence.mirror == MirrorContinuity::unknown
        || evidence.mirror == MirrorContinuity::replaced_initializer_unknown) {
        result.disposition = ReplayDisposition::unknown_processed_mirror;
        result.host_policy_required = evidence.component != ComponentContinuity::retained;
        return result;
    }
    if (evidence.component == ComponentContinuity::retained
        && evidence.mirror == MirrorContinuity::replaced_observed) {
        result.disposition = ReplayDisposition::invalid;
        return result;
    }
    if (result.generation_equal) {
        result.disposition = ReplayDisposition::suppressed_same_generation;
        return result;
    }
    result.disposition = ReplayDisposition::may_replay_different_generation;
    return result;
}

OriginalOnceLifecycle::CallScope::CallScope(OriginalOnceLifecycle& owner) noexcept : owner_(owner) {
    std::uint64_t observed = owner_.state_.load(std::memory_order_acquire);
    LifecyclePhase admittedPhase = LifecyclePhase::detached;
    for (;;) {
        admittedPhase = phase_of(observed);
        const std::uint32_t count = count_of(observed);
        if (admittedPhase == LifecyclePhase::detached
            || count == (std::numeric_limits<std::uint32_t>::max)()) {
            return;
        }
        const std::uint64_t desired = pack(admittedPhase, count + 1U);
        if (owner_.state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            owns_call_ = true;
            break;
        }
    }

    if (admittedPhase != LifecyclePhase::active || owner_.endpoint_owner_key_ == 0U) {
        return;
    }
    const bool sameSurfaceActive =
        std::find(active_observation_keys_.begin(),
                  active_observation_keys_.begin() + active_observation_count_,
                  owner_.endpoint_owner_key_)
        != active_observation_keys_.begin() + active_observation_count_;
    if (!sameSurfaceActive && active_observation_count_ < kMaximumNestedObservationSurfaces) {
        active_observation_keys_[active_observation_count_++] = owner_.endpoint_owner_key_;
        pushed_observation_key_ = true;
        observes_ = true;
    }
}

OriginalOnceLifecycle::CallScope::~CallScope() noexcept {
    if (pushed_observation_key_ && active_observation_count_ != 0U) {
        --active_observation_count_;
        active_observation_keys_[active_observation_count_] = 0U;
    }
    if (owns_call_) {
        std::uint64_t observed = owner_.state_.load(std::memory_order_acquire);
        for (;;) {
            const LifecyclePhase phase = phase_of(observed);
            const std::uint32_t count = count_of(observed);
            if (count == 0U) {
                break;
            }
            if (owner_.state_.compare_exchange_weak(observed,
                                                    pack(phase, count - 1U),
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                break;
            }
        }
    }
}

bool OriginalOnceLifecycle::activate() noexcept {
    std::uint64_t expected = pack(LifecyclePhase::detached, 0U);
    return state_.compare_exchange_strong(expected,
                                          pack(LifecyclePhase::active, 0U),
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
}

bool OriginalOnceLifecycle::begin_quiesce() noexcept {
    std::uint64_t observed = state_.load(std::memory_order_acquire);
    for (;;) {
        if (phase_of(observed) != LifecyclePhase::active) {
            return false;
        }
        if (state_.compare_exchange_weak(observed,
                                         pack(LifecyclePhase::quiescing, count_of(observed)),
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
}

bool OriginalOnceLifecycle::try_detach() noexcept {
    std::uint64_t expected = pack(LifecyclePhase::quiescing, 0U);
    return state_.compare_exchange_strong(expected,
                                          pack(LifecyclePhase::detached, 0U),
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
}

LifecycleSnapshot OriginalOnceLifecycle::snapshot() const noexcept {
    const std::uint64_t observed = state_.load(std::memory_order_acquire);
    return {phase_of(observed), count_of(observed)};
}

} // namespace sunrise::client::hooks::bootflow::opening_authority::ghost_vm_bridge_capture
