#include "native_activation_validation.h"

#include <algorithm>
#include <limits>

namespace dawn::client::hooks::activity_lifecycle {

NativeActivationValidationResult validate_native_activation_image(
    const NativeActivationImageView& image,
    const ImageSha256& expectedInstalledPackedSha256,
    std::span<const NativeActivationTargetContract> contracts,
    std::span<std::uintptr_t> outputAddresses) noexcept {
    std::fill(outputAddresses.begin(), outputAddresses.end(), std::uintptr_t{});
    if (image.mappedBase == nullptr || image.mappedSize == 0U || contracts.empty()
        || contracts.size() != outputAddresses.size()) {
        return NativeActivationValidationResult::invalidArguments;
    }
    if (image.installedPackedOnDiskSha256 != expectedInstalledPackedSha256) {
        return NativeActivationValidationResult::imageHashMismatch;
    }

    for (const NativeActivationTargetContract& contract : contracts) {
        if (contract.prefix.empty()
            || contract.rva > static_cast<std::uintptr_t>(image.mappedSize)
            || contract.prefix.size()
                   > image.mappedSize - static_cast<std::size_t>(contract.rva)) {
            return NativeActivationValidationResult::targetOutOfRange;
        }
        const auto* const target = image.mappedBase + static_cast<std::size_t>(contract.rva);
        if (!std::equal(contract.prefix.begin(), contract.prefix.end(), target)) {
            return NativeActivationValidationResult::prefixMismatch;
        }
    }

    const std::uintptr_t imageBase = reinterpret_cast<std::uintptr_t>(image.mappedBase);
    for (std::size_t index = 0U; index < contracts.size(); ++index) {
        const std::uintptr_t rva = contracts[index].rva;
        if (rva > (std::numeric_limits<std::uintptr_t>::max)() - imageBase) {
            std::fill(outputAddresses.begin(), outputAddresses.end(), std::uintptr_t{});
            return NativeActivationValidationResult::targetOutOfRange;
        }
        outputAddresses[index] = imageBase + rva;
    }
    return NativeActivationValidationResult::valid;
}

} // namespace dawn::client::hooks::activity_lifecycle
