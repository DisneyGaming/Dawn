#include "internal.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "records/format.h"

namespace sunrise::state::build_data::cache {
namespace {

constexpr std::string_view kHexDigits = "0123456789ABCDEF";

[[nodiscard]] bool append(std::span<char> output,
                          std::size_t& length,
                          std::string_view text) noexcept {
    if (length + text.size() >= output.size()) {
        return false;
    }
    std::memcpy(output.data() + length, text.data(), text.size());
    length += text.size();
    return true;
}

[[nodiscard]] bool append_digest(std::span<char> output,
                                 std::size_t& length,
                                 const core::provenance::Sha256Digest& digest) noexcept {
    if (length + digest.size() * 2 >= output.size()) {
        return false;
    }
    for (const std::byte byte : digest) {
        const auto value = std::to_integer<unsigned>(byte);
        output[length++] = kHexDigits[(value >> 4U) & 0xFU];
        output[length++] = kHexDigits[value & 0xFU];
    }
    return true;
}

template <typename... Arguments>
[[nodiscard]] bool append_format(std::span<char> output,
                                 std::size_t& length,
                                 const char* format,
                                 Arguments... arguments) noexcept {
    if (length >= output.size()) {
        return false;
    }
    const int written =
        std::snprintf(output.data() + length, output.size() - length, format, arguments...);
    if (written < 0 || static_cast<std::size_t>(written) >= output.size() - length) {
        return false;
    }
    length += static_cast<std::size_t>(written);
    return true;
}

[[nodiscard]] std::string_view result_name(LoadStatus status) noexcept {
    switch (status) {
    case LoadStatus::missing:
        return "missing";
    case LoadStatus::loaded:
        return "loaded";
    case LoadStatus::stale:
        return "stale";
    case LoadStatus::invalid:
        return "invalid";
    }
    return "invalid";
}

[[nodiscard]] std::string_view reason_name(LoadReason reason) noexcept {
    switch (reason) {
    case LoadReason::notFound:
        return "not_found";
    case LoadReason::match:
        return "match";
    case LoadReason::format:
        return "format";
    case LoadReason::magic:
        return "magic";
    case LoadReason::truncatedHeader:
        return "truncated_header";
    case LoadReason::counts:
        return "counts";
    case LoadReason::size:
        return "size";
    case LoadReason::payload:
        return "payload";
    case LoadReason::checksum:
        return "checksum";
    case LoadReason::close:
        return "close";
    case LoadReason::identity:
        return "identity";
    case LoadReason::none:
    case LoadReason::io:
        return "io";
    }
    return "io";
}

[[nodiscard]] bool has(Mismatch set, Mismatch value) noexcept {
    return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(value)) != 0;
}

[[nodiscard]] bool append_mismatch_reason(std::span<char> output,
                                          std::size_t& length,
                                          Mismatch mismatches) noexcept {
    struct Name {
        Mismatch bit;
        std::string_view text;
    };
    constexpr std::array<Name, 5> kNames{{
        {Mismatch::destinyTimestamp, "destiny_timestamp"},
        {Mismatch::destinySize, "destiny_size"},
        {Mismatch::equipment, "equipment"},
        {Mismatch::producerSource, "producer_source"},
        {Mismatch::producerImage, "producer_image"},
    }};
    bool first = true;
    for (const Name& name : kNames) {
        if (has(mismatches, name.bit)) {
            if ((!first && !append(output, length, ",")) || !append(output, length, name.text)) {
                return false;
            }
            first = false;
        }
    }
    return !first;
}

} // namespace

bool format_outcome_record(const BuildIdentity& expectedBuild,
                           const LoadDiagnostic& diagnostic,
                           std::span<char> output,
                           std::size_t& length) noexcept {
    length = 0;
    const std::string_view result = result_name(diagnostic.status);
    if (!append(output, length, "ev=build_data_cache result=")
        || !append(output, length, result) || !append(output, length, " reason=")) {
        return false;
    }
    if (diagnostic.reason == LoadReason::identity) {
        if (!append_mismatch_reason(output, length, diagnostic.mismatches)) {
            return false;
        }
    } else if (!append(output, length, reason_name(diagnostic.reason))) {
        return false;
    }
    if (!append_format(output,
                       length,
                       " format_expected=%u format_cached=",
                       records::kCacheFormatVersion)) {
        return false;
    }
    if (!diagnostic.observedFormatAvailable) {
        if (!append(output, length, "absent")) {
            return false;
        }
    } else if (!append_format(output, length, "%u", diagnostic.observedFormat)) {
        return false;
    }

    if (!append(output, length, " source_expected=")
        || !append_digest(output, length, expectedBuild.producerSourceSha256)
        || !append(output, length, " source_cached=")) {
        return false;
    }
    if (diagnostic.cachedIdentityAvailable) {
        if (!append_digest(output, length, diagnostic.cachedIdentity.producerSourceSha256)) {
            return false;
        }
    } else if (!append(output, length, "absent")) {
        return false;
    }

    if (!append(output, length, " image_expected=")
        || !append_digest(output, length, expectedBuild.producerImageSha256)
        || !append(output, length, " image_cached=")) {
        return false;
    }
    if (diagnostic.cachedIdentityAvailable) {
        if (!append_digest(output, length, diagnostic.cachedIdentity.producerImageSha256)) {
            return false;
        }
    } else if (!append(output, length, "absent")) {
        return false;
    }

    if (!append_format(output,
                       length,
                       " destiny_timestamp_expected=0x%08X destiny_timestamp_cached=",
                       expectedBuild.imageTimestamp)) {
        return false;
    }
    if (diagnostic.cachedIdentityAvailable) {
        if (!append_format(output, length, "0x%08X", diagnostic.cachedIdentity.imageTimestamp)) {
            return false;
        }
    } else if (!append(output, length, "absent")) {
        return false;
    }
    if (!append_format(output,
                       length,
                       " destiny_size_expected=0x%08X destiny_size_cached=",
                       expectedBuild.imageSize)) {
        return false;
    }
    if (diagnostic.cachedIdentityAvailable) {
        if (!append_format(output, length, "0x%08X", diagnostic.cachedIdentity.imageSize)) {
            return false;
        }
    } else if (!append(output, length, "absent")) {
        return false;
    }
    if (!append_format(output,
                       length,
                       " equipment_expected=0x%016llX equipment_cached=",
                       static_cast<unsigned long long>(expectedBuild.configuredEquipmentHash))) {
        return false;
    }
    if (diagnostic.cachedIdentityAvailable) {
        if (!append_format(
                output,
                length,
                "0x%016llX",
                static_cast<unsigned long long>(diagnostic.cachedIdentity.configuredEquipmentHash))) {
            return false;
        }
    } else if (!append(output, length, "absent")) {
        return false;
    }
    output[length] = '\0';
    return true;
}

} // namespace sunrise::state::build_data::cache
