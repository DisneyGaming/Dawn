#pragma once

#include <Windows.h>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace sunrise::core::provenance {

inline constexpr std::size_t kSha256DigestSize = 32;
using Sha256Digest = std::array<std::byte, kSha256DigestSize>;

/** Content-derived metadata generated once for a frozen source candidate. */
struct BuildMetadata {
    unsigned schema{};
    std::string_view buildId;
    std::string_view gitHead;
    std::string_view gitBranch;
    bool dirty{};
    Sha256Digest sourceSha256{};
    std::string_view sourceSha256Hex;
    std::string_view configuration;
    std::string_view platform;
    std::string_view compiler;
    std::string_view toolset;
    std::string_view windowsSdk;
    unsigned cacheFormat{};
    unsigned settingsVersion{};
};

/** Immutable source claim paired with the exact loaded DLL image hash. */
struct RuntimeIdentity {
    const BuildMetadata* build{};
    Sha256Digest imageSha256{};
    std::array<char, MAX_PATH> moduleName{};
    std::size_t moduleNameLength{};
};

/** @return The generated, content-only metadata embedded in this DLL. */
[[nodiscard]] const BuildMetadata& metadata() noexcept;

/**
 * Captures the exact loaded module once and publishes it for the Core lifetime.
 * The supplied handle must own this implementation; a host or unrelated DLL is rejected.
 */
[[nodiscard]] bool capture(HMODULE module) noexcept;

/** @return The published runtime identity, or null before a successful capture. */
[[nodiscard]] const RuntimeIdentity* current() noexcept;

/** Clears the runtime image identity during Core shutdown or failed initialization unwind. */
void clear() noexcept;

/** Streams a file through Windows CNG SHA-256. Exposed for an independent test boundary. */
[[nodiscard]] bool file_sha256(const wchar_t* path, Sha256Digest& output) noexcept;

/** Formats the required bounded `ev=build_identity` record without paths or timestamps. */
[[nodiscard]] bool format_startup_record(std::span<char> output,
                                         std::size_t& length) noexcept;

} // namespace sunrise::core::provenance
