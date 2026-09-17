#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "native_activation_contract.h"
#include "native_activation_registry.h"

namespace dawn::client::hooks::activity_lifecycle {

inline constexpr std::uint32_t kNativeActivePoolSnapshotAbiVersion = 1U;
inline constexpr std::uint32_t kNativeActivePoolMemberByteSize = 80U;
inline constexpr std::uint32_t kNativeActivePoolSnapshotByteSize = 128U;
inline constexpr std::uint32_t kNativeActivePoolProjectionByteSize = 80U;
inline constexpr std::uint32_t kNativeActivePoolHardSlotGuard = 0x2000U;
inline constexpr std::uint32_t kNativeActivePoolWrapperStride = 0x19540U;
inline constexpr std::uintptr_t kNativeActivePoolGlobalRva = 0x1F8AFF0U;

enum NativeActivePoolMemberFlag : std::uint32_t {
    nativePoolMemberAllocated = 0x00000001U,
    nativePoolMemberActive = 0x00000002U,
    nativePoolMemberHandlePresent = 0x00000004U,
    nativePoolMemberReconstructedHandleMatch = 0x00000008U,
    nativePoolMemberPairedActive = 0x00000010U,
    nativePoolMemberPairedHandleMatch = 0x00000020U,
    nativePoolMemberRegistryExact = 0x00000040U,
    nativePoolMemberIdentityMatch = 0x00000080U,
    nativePoolMemberModeMatch = 0x00000100U,
};

enum class NativeActivePoolSnapshotStatus : std::uint32_t {
    exact,
    noLease,
    unreadable,
    malformed,
    capacityExceeded,
    unstable,
    registryDiverged,
};

enum NativeActivePoolSnapshotFlag : std::uint32_t {
    nativePoolSnapshotImageAdmitted = 0x00000001U,
    nativePoolSnapshotStable = 0x00000002U,
    nativePoolSnapshotAlternateHandleEncoding = 0x00000004U,
    nativePoolSnapshotRegistryJoined = 0x00000008U,
    nativePoolSnapshotRegistryMutationDisabled = 0x00000010U,
};

/** Exact fixed member ABI recovered for the pinned x64 PC build. */
struct NativeActivePoolMemberV1 final {
    std::uint32_t byteSize{kNativeActivePoolMemberByteSize};
    std::uint32_t flags{};
    std::uintptr_t wrapper{};
    std::uintptr_t sensorTable{};
    std::uintptr_t pairedHost{};
    std::uint64_t rawIdentity{};
    std::uint64_t moduleGeneration{};
    std::uint64_t activationGeneration{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    std::uint32_t reconstructedHandle{kInvalidNativeActivityHandle};
    std::int32_t mode{};
    std::uint32_t slotIndex{};
    std::uint32_t nativeGeneration{};
    std::uint32_t reserved{};
};

/** Immutable borrowed header retained from preNative entry through postNative return. */
struct NativeActivePoolSnapshotV1 final {
    std::uint32_t abiVersion{kNativeActivePoolSnapshotAbiVersion};
    std::uint32_t byteSize{kNativeActivePoolSnapshotByteSize};
    NativeActivePoolSnapshotStatus status{NativeActivePoolSnapshotStatus::unreadable};
    std::uint32_t flags{};
    std::uint64_t snapshotEpoch{};
    std::uint64_t moduleGeneration{};
    std::uintptr_t poolGlobal{};
    std::uintptr_t storageBase{};
    std::uintptr_t bitmapWords{};
    const NativeActivePoolMemberV1* members{};
    std::uint32_t memberCount{};
    std::uint32_t memberCapacity{kNativeActivePoolPinnedCapacity};
    std::uint32_t scannedLimit{};
    std::uint32_t allocatedCount{};
    std::uint32_t exactRegistryCount{};
    std::uint32_t poolOnlyCount{};
    std::uint32_t registryOnlyCount{};
    std::uint32_t conflictCount{};
    std::uint32_t wrapperStride{};
    std::uint32_t generationOffset{};
    std::uint32_t generationStride{};
    std::uint32_t generationMask{};
    std::uint32_t poolTag{};
    std::uint32_t malformedCount{};
    std::uint32_t reserved[2]{};
};

/** Bounded normal-output projection: scalars and deterministic hashes only. */
struct NativeActivePoolProjectionV1 final {
    std::uint32_t abiVersion{kNativeActivePoolSnapshotAbiVersion};
    std::uint32_t byteSize{kNativeActivePoolProjectionByteSize};
    NativeActivePoolSnapshotStatus status{NativeActivePoolSnapshotStatus::unreadable};
    std::uint32_t flags{};
    std::uint64_t snapshotEpoch{};
    std::uint64_t moduleGeneration{};
    std::uint64_t membershipHash{};
    std::uint64_t detailHash{};
    std::uint32_t memberCount{};
    std::uint32_t allocatedCount{};
    std::uint32_t exactRegistryCount{};
    std::uint32_t poolOnlyCount{};
    std::uint32_t registryOnlyCount{};
    std::uint32_t conflictCount{};
    std::uint32_t malformedCount{};
    std::uint32_t reserved{};
};

static_assert(sizeof(std::uintptr_t) != 8U || sizeof(NativeActivePoolMemberV1) == 80U);
static_assert(sizeof(std::uintptr_t) != 8U || sizeof(NativeActivePoolSnapshotV1) == 128U);
static_assert(sizeof(NativeActivePoolProjectionV1) == 80U);
static_assert(std::is_standard_layout_v<NativeActivePoolMemberV1>);
static_assert(std::is_trivially_copyable_v<NativeActivePoolMemberV1>);
static_assert(std::is_standard_layout_v<NativeActivePoolSnapshotV1>);
static_assert(std::is_trivially_copyable_v<NativeActivePoolSnapshotV1>);

/** Successful output of packed-identity plus mapped-prefix admission. */
struct NativeActivePoolImageAdmission final {
    std::uintptr_t mappedBase{};
    std::size_t mappedSize{};
    std::uintptr_t globalDropTarget{};
    std::uintptr_t poolGlobal{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return mappedBase != 0U && mappedSize != 0U && globalDropTarget != 0U
               && poolGlobal != 0U;
    }
};

/** Admits the pinned installed packed identity and exact mapped global-drop prefix as one gate. */
[[nodiscard]] NativeActivationValidationResult admit_native_active_pool_image(
    const NativeActivationImageView& image,
    NativeActivePoolImageAdmission& output) noexcept;

/** Every game-memory load is routed through this non-throwing bounded reader. */
struct NativeActivePoolReader final {
    using Read = bool (*)(void* context,
                          std::uintptr_t address,
                          void* output,
                          std::size_t byteCount) noexcept;
    void* context{};
    Read read{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return read != nullptr;
    }
};

/** Guarded current-process reader used by the inert provider when a test seam is not supplied. */
[[nodiscard]] NativeActivePoolReader native_active_pool_process_reader() noexcept;

using NativeActivePoolSnapshotCallback =
    void (__fastcall *)(const NativeActivePoolSnapshotV1*) noexcept;
using NativeActivePoolOriginal = void (__fastcall *)() noexcept;

struct NativeActivePoolCallbacks final {
    NativeActivePoolSnapshotCallback preNative{};
    NativeActivePoolSnapshotCallback postNative{};
};

struct NativeActivePoolObservationResult final {
    NativeActivePoolSnapshotStatus status{NativeActivePoolSnapshotStatus::unreadable};
    std::uint32_t retired{};
    std::uint32_t stale{};
    bool originalCalled{};
};

/** Converts one fixed snapshot into a pointer-free scalar/hash record without allocation. */
[[nodiscard]] NativeActivePoolProjectionV1
project_native_active_pool_snapshot(const NativeActivePoolSnapshotV1& snapshot) noexcept;

/**
 * Inert source-only exact-membership provider. It installs no hook and owns no live registration.
 * A single non-waiting lease retains three copied members across both callbacks and the original.
 */
class NativeActivePoolMembershipProvider final {
public:
    NativeActivePoolMembershipProvider() noexcept = default;
    NativeActivePoolMembershipProvider(const NativeActivePoolMembershipProvider&) = delete;
    NativeActivePoolMembershipProvider& operator=(const NativeActivePoolMembershipProvider&) =
        delete;

    [[nodiscard]] NativeActivePoolObservationResult observe(
        const NativeActivePoolImageAdmission& image,
        NativeActivePoolReader reader,
        NativeActivationRegistry& registry,
        NativeActivePoolOriginal original,
        NativeActivePoolCallbacks callbacks = {}) noexcept;

    [[nodiscard]] bool idle() const noexcept;

private:
    std::array<NativeActivePoolMemberV1, kNativeActivePoolPinnedCapacity> members_{};
    std::array<NativeActivationToken, kNativeActivePoolPinnedCapacity> tokens_{};
    NativeActivePoolSnapshotV1 snapshot_{};
    std::atomic<std::uint64_t> nextEpoch_{1U};
    std::atomic_flag lease_ = ATOMIC_FLAG_INIT;
};

} // namespace dawn::client::hooks::activity_lifecycle
