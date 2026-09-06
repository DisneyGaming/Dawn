#pragma once

#include <cstdint>
#include <span>

#include "../definition.h"
#include "records/domains.h"

namespace sunrise::state::build_data::cache {

/** Result of opening and checking the one build-data cache file. */
enum class LoadStatus {
    missing,
    loaded,
    stale,
    invalid,
};

/** Stable non-identity reason for one cache outcome. */
enum class LoadReason : std::uint8_t {
    none,
    notFound,
    match,
    format,
    identity,
    magic,
    truncatedHeader,
    counts,
    size,
    payload,
    checksum,
    io,
    close,
};

/** Independently reportable current-format identity differences. */
enum class Mismatch : std::uint32_t {
    none = 0,
    destinyTimestamp = 1U << 0U,
    destinySize = 1U << 1U,
    equipment = 1U << 2U,
    producerSource = 1U << 3U,
    producerImage = 1U << 4U,
};

[[nodiscard]] constexpr Mismatch operator|(Mismatch left, Mismatch right) noexcept {
    return static_cast<Mismatch>(static_cast<std::uint32_t>(left)
                                 | static_cast<std::uint32_t>(right));
}

/** Fixed diagnostic returned with every cache result; it never borrows file bytes. */
struct LoadDiagnostic {
    LoadStatus status{LoadStatus::invalid};
    LoadReason reason{LoadReason::io};
    std::uint32_t observedFormat{};
    bool observedFormatAvailable{};
    bool cachedIdentityAvailable{};
    BuildIdentity cachedIdentity{};
    Mismatch mismatches{Mismatch::none};
};

/** Final-name rule for one all-or-nothing cache write. */
enum class WriteDisposition {
    createOnly,
    replaceStale,
};

/**
 * Reads Destiny's PE identity and hashes the loaded Sunrise module's on-disk image.
 * @param configuredEquipmentHash Hash of the authored equipment and the score rules.
 * @param identity Receives the PE fields, configured-equipment hash, and producer digest.
 * @return True when Destiny is a valid PE image and the exact Sunrise image can be hashed.
 */
[[nodiscard]] bool current_build_identity(std::uint64_t configuredEquipmentHash,
                                          BuildIdentity& identity) noexcept;

/**
 * Loads one exact cache file into fixed caller storage.
 * @param path Null-terminated cache path.
 * @param expectedBuild Current Destiny, configuration, and Sunrise producer identity.
 * @param output Fixed caller storage for all generated domains.
 * @param counts Receives every checked domain count.
 * @return Missing, loaded, stale, or invalid. Never leaves partial counts.
 */
[[nodiscard]] LoadStatus load(const wchar_t* path,
                              const BuildIdentity& expectedBuild,
                              records::MutableDomains output,
                              records::DomainCounts& counts,
                              LoadDiagnostic* diagnostic = nullptr) noexcept;

/** Formats one complete bounded cache outcome record in stable mismatch order. */
[[nodiscard]] bool format_outcome_record(const BuildIdentity& expectedBuild,
                                         const LoadDiagnostic& diagnostic,
                                         std::span<char> output,
                                         std::size_t& length) noexcept;

/**
 * Creates a missing cache, or replaces one marked stale, in one step.
 * @param directory Null-terminated cache directory.
 * @param path Null-terminated final cache path.
 * @param build Current Destiny, configuration, and Sunrise producer identity.
 * @param domains Complete mapping domains.
 * @param disposition Create only, or replace a stale cache.
 * @return True when the file is on disk under its requested final name.
 */
[[nodiscard]] bool write(const wchar_t* directory,
                         const wchar_t* path,
                         const BuildIdentity& build,
                         records::Domains domains,
                         WriteDisposition disposition) noexcept;

} // namespace sunrise::state::build_data::cache
