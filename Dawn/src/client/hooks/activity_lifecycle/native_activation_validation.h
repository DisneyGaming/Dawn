#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::client::hooks::activity_lifecycle {

inline constexpr std::size_t kSha256DigestSize = 32U;
using ImageSha256 = std::array<std::byte, kSha256DigestSize>;

/**
 * Borrowed post-loader mapped image plus the independently computed identity of the installed
 * packed file. Prefix RVAs address mappedBase; they are never interpreted as packed raw offsets.
 */
struct NativeActivationImageView final {
    const std::byte* mappedBase{};
    std::size_t mappedSize{};
    ImageSha256 installedPackedOnDiskSha256{};
};

/** One fixed-RVA entry prefix. Prefixes are guards and are never used as byte scanners. */
struct NativeActivationTargetContract final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
};

enum class NativeActivationValidationResult : std::uint8_t {
    valid,
    invalidArguments,
    imageHashMismatch,
    targetOutOfRange,
    prefixMismatch,
};

/**
 * Validates one exact image identity and every fixed-RVA prefix as an all-or-none group.
 * Output addresses remain zeroed unless the digest and every complete prefix match.
 */
[[nodiscard]] NativeActivationValidationResult validate_native_activation_image(
    const NativeActivationImageView& image,
    const ImageSha256& expectedInstalledPackedSha256,
    std::span<const NativeActivationTargetContract> contracts,
    std::span<std::uintptr_t> outputAddresses) noexcept;

} // namespace dawn::client::hooks::activity_lifecycle
