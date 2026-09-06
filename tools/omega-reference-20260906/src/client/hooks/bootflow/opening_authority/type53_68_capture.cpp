#include "client/hooks/bootflow/opening_authority/type53_68_capture.h"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace sunrise::client::hooks::bootflow::opening_authority {
namespace {

inline constexpr std::array<std::byte, 5U> kSchemaResolverExactBody{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x41}, std::byte{0x08}, std::byte{0xC3}};
inline constexpr std::array<std::byte, 16U> kType53ApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x02}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A}, std::byte{0x08}};
inline constexpr std::array<std::byte, 19U> kType53TickPrefix{
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x50}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x09}, std::byte{0x4C}, std::byte{0x8B}, std::byte{0xF1},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0xDD},
    std::byte{0xFA}, std::byte{0x42}, std::byte{0x01}};
inline constexpr std::array<std::byte, 19U> kType53SelectedRowPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x11}, std::byte{0x45}, std::byte{0x8B}, std::byte{0xC2},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0x8D},
    std::byte{0x04}, std::byte{0x43}, std::byte{0x01}};
inline constexpr std::array<std::byte, 15U> kType53TerminalWrapperPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x81}, std::byte{0x3A},
    std::byte{0xC5}, std::byte{0x9D}, std::byte{0x1C}, std::byte{0x81},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9}};
inline constexpr std::array<std::byte, 16U> kType53TerminalCorePrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x53},
    std::byte{0x55}, std::byte{0x56}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x60}, std::byte{0x8B}, std::byte{0x42}, std::byte{0x04}};
inline constexpr std::array<std::byte, 17U> kPresentationStartPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x78}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x67}, std::byte{0xC3}, std::byte{0x66},
    std::byte{0x01}};
inline constexpr std::array<std::byte, 21U> kType68CreateTailPrefix{
    std::byte{0xC7}, std::byte{0x81}, std::byte{0x00}, std::byte{0x0B},
    std::byte{0x00}, std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0xB0}, std::byte{0x01},
    std::byte{0x66}, std::byte{0xC7}, std::byte{0x81}, std::byte{0x04},
    std::byte{0x0B}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}};
inline constexpr std::array<std::byte, 15U> kType68LifecycleAPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x40}};
inline constexpr std::array<std::byte, 14U> kType68LifecycleBPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}};
inline constexpr std::array<std::byte, 15U> kType68ApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x40}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x02},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xF9}};
inline constexpr std::array<std::byte, 16U> kType68ContentResolverPrefix{
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x4C},
    std::byte{0x8B}, std::byte{0xD1}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x33}, std::byte{0x08}, std::byte{0x43},
    std::byte{0x01}, std::byte{0x41}, std::byte{0x8B}, std::byte{0xD1}};
inline constexpr std::array<std::byte, 15U> kType68ReconcilePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}};
inline constexpr std::array<std::byte, 16U> kType68InstallPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x6C}, std::byte{0x24},
    std::byte{0xA0}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x60}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 16U> kManagerReadyPrefix{
    std::byte{0x80}, std::byte{0x3D}, std::byte{0xF1}, std::byte{0x9A},
    std::byte{0xC3}, std::byte{0x01}, std::byte{0x00}, std::byte{0x74},
    std::byte{0x12}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x05},
    std::byte{0x5F}, std::byte{0x86}, std::byte{0xC3}, std::byte{0x01}};
inline constexpr std::array<std::byte, 12U> kManagerGetExactBody{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x05}, std::byte{0x48},
    std::byte{0x91}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xE0}, std::byte{0xF8}, std::byte{0xC3}};
inline constexpr std::array<std::byte, 17U> kManagerAddPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x4C}, std::byte{0x8B},
    std::byte{0x99}, std::byte{0x80}, std::byte{0x14}, std::byte{0x00},
    std::byte{0x00}};
inline constexpr std::array<std::byte, 15U> kManagerStatusPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}};
inline constexpr std::array<std::byte, 15U> kManagerTerminalPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x55}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x28}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xD9}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x50}};
inline constexpr std::array<std::byte, 15U> kManagerEntryMaterializePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}};
inline constexpr std::array<std::byte, 15U> kManagerMaterializeWalkPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x4C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x53}, std::byte{0x55}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x55}, std::byte{0x41}, std::byte{0x56}};

inline constexpr std::array<std::byte, 7U> kType53ResolverReturnBytes{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x8B}, std::byte{0x80},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 4U> kType53PostCommitBytes{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xC4}, std::byte{0x30}};
inline constexpr std::array<std::byte, 5U> kType53TerminalPreBytes{
    std::byte{0xE8}, std::byte{0x7D}, std::byte{0xF4}, std::byte{0xFF}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 3U> kType53TerminalPostBytes{
    std::byte{0x8B}, std::byte{0x46}, std::byte{0x18}};
inline constexpr std::array<std::byte, 3U> kType53GenerationBytes{
    std::byte{0x41}, std::byte{0xFF}, std::byte{0xC7}};
inline constexpr std::array<std::byte, 3U> kType68ResolverReturnBytes{
    std::byte{0x49}, std::byte{0x8B}, std::byte{0xCF}};
inline constexpr std::array<std::byte, 7U> kType68ContentReturnBytes{
    std::byte{0x83}, std::byte{0xBF}, std::byte{0xF8}, std::byte{0x02},
    std::byte{0x00}, std::byte{0x00}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 5U> kType68ReconcilePreBytes{
    std::byte{0xE8}, std::byte{0xD5}, std::byte{0xFB}, std::byte{0xFF}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 5U> kType68ReconcilePostBytes{
    std::byte{0xE9}, std::byte{0x8A}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 5U> kType68RemovePreBytes{
    std::byte{0xE8}, std::byte{0x5E}, std::byte{0x45}, std::byte{0x37}, std::byte{0x00}};
inline constexpr std::array<std::byte, 7U> kType68RemovePostBytes{
    std::byte{0x81}, std::byte{0x7E}, std::byte{0x10}, std::byte{0xC5},
    std::byte{0x9D}, std::byte{0x1C}, std::byte{0x81}};
inline constexpr std::array<std::byte, 5U> kType68InstallPreBytes{
    std::byte{0xE8}, std::byte{0x56}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 2U> kType68InstallPostBytes{
    std::byte{0xFF}, std::byte{0xC5}};
inline constexpr std::array<std::byte, 5U> kType68PostCommitBytes{
    std::byte{0xE8}, std::byte{0xC8}, std::byte{0xF6}, std::byte{0x41}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 4U> kType68PostRefreshBytes{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xC4}, std::byte{0x40}};
inline constexpr std::array<std::byte, 5U> kType68ManagerReadyBytes{
    std::byte{0xE8}, std::byte{0xF8}, std::byte{0x41}, std::byte{0x37}, std::byte{0x00}};
inline constexpr std::array<std::byte, 5U> kType68ManagerAddPreBytes{
    std::byte{0xE8}, std::byte{0x5E}, std::byte{0x1D}, std::byte{0x37}, std::byte{0x00}};
inline constexpr std::array<std::byte, 5U> kType68ManagerAddPostBytes{
    std::byte{0xE9}, std::byte{0xCA}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 5U> kType68AlternateReadyBytes{
    std::byte{0xE8}, std::byte{0x25}, std::byte{0x41}, std::byte{0x37}, std::byte{0x00}};
inline constexpr std::array<std::byte, 5U> kType68AlternateAddPreBytes{
    std::byte{0xE8}, std::byte{0x7F}, std::byte{0x1E}, std::byte{0x37}, std::byte{0x00}};
inline constexpr std::array<std::byte, 4U> kType68AlternateAddPostBytes{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x4D}, std::byte{0x50}};
inline constexpr std::array<std::byte, 5U> kManagerMaterializePreBytes{
    std::byte{0xE8}, std::byte{0x98}, std::byte{0xFA}, std::byte{0xFF}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 7U> kManagerMaterializePostBytes{
    std::byte{0x48}, std::byte{0x81}, std::byte{0xC3}, std::byte{0x48},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 5U> kManagerTerminalPreBytes{
    std::byte{0xE8}, std::byte{0x07}, std::byte{0xAA}, std::byte{0xFF}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 2U> kManagerTerminalPostBytes{
    std::byte{0x84}, std::byte{0xC0}};

inline constexpr std::size_t kMaximumPrefixBytes = 21U;
inline constexpr std::array<NativeSurface, kRequiredNativeSurfaceCount> kRequiredSurfaces{
    NativeSurface::schema_resolver,
    NativeSurface::type53_apply,
    NativeSurface::type53_tick,
    NativeSurface::type53_selected_row_dispatch,
    NativeSurface::type53_terminal_wrapper,
    NativeSurface::type53_terminal_core,
    NativeSurface::type53_presentation_start,
    NativeSurface::type68_create_tail,
    NativeSurface::type68_lifecycle_a,
    NativeSurface::type68_lifecycle_b_update,
    NativeSurface::type68_apply,
    NativeSurface::type68_content_resolver,
    NativeSurface::type68_same_identity_reconcile,
    NativeSurface::type68_install,
    NativeSurface::manager_ready,
    NativeSurface::manager_get,
    NativeSurface::manager_add,
    NativeSurface::manager_status_update,
    NativeSurface::manager_entry_materialize,
    NativeSurface::manager_terminal_predicate,
    NativeSurface::manager_materialize_walk,
};
inline constexpr std::array<CaptureWindow, kRequiredCaptureWindowCount> kRequiredWindows{
    CaptureWindow::type53_apply_entry,
    CaptureWindow::type53_resolver_return,
    CaptureWindow::type53_post_commit,
    CaptureWindow::type53_terminal_pre,
    CaptureWindow::type53_terminal_post,
    CaptureWindow::type53_generation_consumed,
    CaptureWindow::type53_terminal_wrapper,
    CaptureWindow::type53_terminal_core,
    CaptureWindow::type53_presentation_start,
    CaptureWindow::type68_apply_entry,
    CaptureWindow::type68_resolver_return,
    CaptureWindow::type68_content_return,
    CaptureWindow::type68_reconcile_pre,
    CaptureWindow::type68_reconcile_post,
    CaptureWindow::type68_remove_pre,
    CaptureWindow::type68_remove_post,
    CaptureWindow::type68_install_pre,
    CaptureWindow::type68_install_post,
    CaptureWindow::type68_post_commit,
    CaptureWindow::type68_post_refresh,
    CaptureWindow::type68_manager_ready,
    CaptureWindow::type68_manager_add_pre,
    CaptureWindow::type68_manager_add_post,
    CaptureWindow::type68_alternate_builder_return,
    CaptureWindow::type68_alternate_add_pre,
    CaptureWindow::type68_alternate_add_post,
    CaptureWindow::type68_manager_materialize,
    CaptureWindow::type68_manager_materialize_post,
    CaptureWindow::type68_manager_terminal,
    CaptureWindow::type68_manager_terminal_post,
};
inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] bool safe_copy_exact(void* destination,
                                   const void* source,
                                   std::size_t bytes) noexcept {
    if (destination == nullptr || source == nullptr || bytes == 0U) {
        return false;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    || GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
                ? EXCEPTION_EXECUTE_HANDLER
                : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
#else
#error "The Type-53/68 observer requires Windows MSVC structured exception handling"
#endif
}

[[nodiscard]] const void* offset_address(const void* base, std::size_t offset) noexcept {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
    if (address == 0U || address > (std::numeric_limits<std::uintptr_t>::max)() - offset) {
        return nullptr;
    }
    return reinterpret_cast<const void*>(address + offset);
}

template <typename Value>
[[nodiscard]] bool read_mapped(const RuntimeImageView& image,
                               std::size_t offset,
                               Value& value) noexcept {
    if (offset > image.mapped_image.size()
        || sizeof value > image.mapped_image.size() - offset) {
        return false;
    }
    return safe_copy_exact(&value, image.mapped_image.data() + offset, sizeof value);
}

[[nodiscard]] bool validate_pe(const RuntimeImageView& image,
                               PackedMappedPeIdentity& outputIdentity) noexcept {
    outputIdentity = {};
    if (image.main_module_base == nullptr
        || image.main_module_base != image.mapped_image.data()
        || image.mapped_image.size() != kPinnedMappedSizeOfImage) {
        return false;
    }
    std::uint16_t dosMagic{};
    std::uint32_t ntOffset{};
    if (!read_mapped(image, 0U, dosMagic) || dosMagic != 0x5A4DU
        || !read_mapped(image, 0x3CU, ntOffset)) {
        return false;
    }
    std::uint32_t peSignature{};
    std::uint16_t machine{};
    std::uint16_t sections{};
    std::uint32_t timestamp{};
    std::uint16_t optionalHeaderBytes{};
    std::uint16_t characteristics{};
    std::uint16_t optionalMagic{};
    std::uint32_t entryPoint{};
    std::uint64_t preferredImageBase{};
    std::uint32_t sectionAlignment{};
    std::uint32_t fileAlignment{};
    std::uint32_t sizeOfImage{};
    std::uint32_t sizeOfHeaders{};
    std::uint32_t checksum{};
    std::uint16_t subsystem{};
    std::uint16_t dllCharacteristics{};
    const std::size_t optionalOffset = static_cast<std::size_t>(ntOffset) + 24U;
    const bool fixedFieldsValid =
           read_mapped(image, ntOffset, peSignature) && peSignature == 0x00004550U
           && read_mapped(image, ntOffset + 4U, machine)
           && machine == kPinnedPeMachine
           && read_mapped(image, ntOffset + 6U, sections)
           && sections == kPinnedPeSections
           && read_mapped(image, ntOffset + 8U, timestamp)
           && timestamp == kPinnedPeTimestamp
           && read_mapped(image, ntOffset + 20U, optionalHeaderBytes)
           && optionalHeaderBytes == kPinnedPeOptionalHeaderBytes
           && read_mapped(image, ntOffset + 22U, characteristics)
           && characteristics == kPinnedPeCharacteristics
           && read_mapped(image, optionalOffset, optionalMagic) && optionalMagic == 0x20BU
           && read_mapped(image, optionalOffset + 16U, entryPoint)
           && entryPoint == kPinnedPeEntryPointRva
           && read_mapped(image, optionalOffset + 24U, preferredImageBase)
           && preferredImageBase == kPinnedPackedPreferredImageBase
           && read_mapped(image, optionalOffset + 32U, sectionAlignment)
           && sectionAlignment == kPinnedPeSectionAlignment
           && read_mapped(image, optionalOffset + 36U, fileAlignment)
           && fileAlignment == kPinnedPackedPeFileAlignment
           && read_mapped(image, optionalOffset + 56U, sizeOfImage)
           && sizeOfImage == kPinnedMappedSizeOfImage
           && read_mapped(image, optionalOffset + 60U, sizeOfHeaders)
           && sizeOfHeaders == kPinnedPeHeadersBytes
           && read_mapped(image, optionalOffset + 64U, checksum)
           && checksum == kPinnedPackedPeChecksum
           && read_mapped(image, optionalOffset + 68U, subsystem)
           && subsystem == kPinnedPeSubsystem
           && read_mapped(image, optionalOffset + 70U, dllCharacteristics)
           && dllCharacteristics == kPinnedPackedPeDllCharacteristics;
    if (!fixedFieldsValid) {
        return false;
    }

    std::uint32_t debugDirectoryRva{};
    std::uint32_t debugDirectoryBytes{};
    constexpr std::size_t kDataDirectoryOffset = 112U;
    constexpr std::size_t kDebugDirectoryIndex = 6U;
    const std::size_t debugEntryOffset =
        optionalOffset + kDataDirectoryOffset + kDebugDirectoryIndex * 8U;
    if (!read_mapped(image, debugEntryOffset, debugDirectoryRva)
        || !read_mapped(image, debugEntryOffset + 4U, debugDirectoryBytes)
        || debugDirectoryBytes == 0U || debugDirectoryBytes % 28U != 0U
        || debugDirectoryRva > image.mapped_image.size()
        || debugDirectoryBytes > image.mapped_image.size() - debugDirectoryRva) {
        return false;
    }

    for (std::size_t offset = 0U; offset < debugDirectoryBytes; offset += 28U) {
        const std::size_t entry = static_cast<std::size_t>(debugDirectoryRva) + offset;
        std::uint32_t type{};
        std::uint32_t dataBytes{};
        std::uint32_t dataRva{};
        if (!read_mapped(image, entry + 12U, type)
            || !read_mapped(image, entry + 16U, dataBytes)
            || !read_mapped(image, entry + 20U, dataRva)) {
            return false;
        }
        if (type != 2U) {
            continue;
        }
        std::uint32_t signature{};
        std::array<std::byte, 16U> guid{};
        std::uint32_t age{};
        if (dataBytes < 24U || dataRva > image.mapped_image.size()
            || dataBytes > image.mapped_image.size() - dataRva
            || !read_mapped(image, dataRva, signature) || signature != 0x53445352U
            || !safe_copy_exact(guid.data(), image.mapped_image.data() + dataRva + 4U,
                                guid.size())
            || !read_mapped(image, dataRva + 20U, age) || guid != kPinnedCodeViewGuid
            || age != kPinnedCodeViewAge) {
            return false;
        }
        outputIdentity = PackedMappedPeIdentity{preferredImageBase,
                                                fileAlignment,
                                                checksum,
                                                dllCharacteristics,
                                                guid,
                                                age};
        return outputIdentity == kPinnedPackedMappedPeIdentity;
    }
    return false;
}

[[nodiscard]] bool committed_executable_range(const void* address,
                                              std::size_t bytes) noexcept {
    if (address == nullptr || bytes == 0U) {
        return false;
    }
#if defined(_WIN32)
    std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor > (std::numeric_limits<std::uintptr_t>::max)() - bytes) {
        return false;
    }
    const std::uintptr_t end = cursor + bytes;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),
                         &information,
                         sizeof information)
            != sizeof information) {
            return false;
        }
        const DWORD protection = information.Protect & 0xFFU;
        const bool executable = protection == PAGE_EXECUTE
                                || protection == PAGE_EXECUTE_READ
                                || protection == PAGE_EXECUTE_READWRITE
                                || protection == PAGE_EXECUTE_WRITECOPY;
        if (information.State != MEM_COMMIT || !executable
            || (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
            return false;
        }
        const std::uintptr_t regionBase =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress);
        if (regionBase > (std::numeric_limits<std::uintptr_t>::max)()
            - information.RegionSize) {
            return false;
        }
        const std::uintptr_t regionEnd = regionBase + information.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = (std::min)(regionEnd, end);
    }
    return true;
#else
    return false;
#endif
}

struct ClassRecord final {
    std::uint64_t class_id{};
    std::uintptr_t table{};
    std::uint64_t count{};
};

struct HandlerRecord final {
    std::uintptr_t function{};
    std::uint64_t dispatch_key{};
};

[[nodiscard]] bool pointer_at_rva(const RuntimeImageView& image,
                                  std::uintptr_t rva,
                                  std::uintptr_t& output) noexcept {
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(image.mapped_image.data());
    if (rva > (std::numeric_limits<std::uintptr_t>::max)() - base) {
        return false;
    }
    output = base + rva;
    return true;
}

[[nodiscard]] bool validate_class_tables(const RuntimeImageView& image) noexcept {
    ClassRecord dialogue{};
    ClassRecord directive{};
    std::uintptr_t dialogueTable{};
    std::uintptr_t directiveTable{};
    if (!read_mapped(image, kDialogueClassRecordRva, dialogue)
        || !read_mapped(image, kDirectiveClassRecordRva, directive)
        || !pointer_at_rva(image, kDialogueHandlerTableRva, dialogueTable)
        || !pointer_at_rva(image, kDirectiveHandlerTableRva, directiveTable)
        || dialogue.class_id != kDialogueComponentClass || dialogue.table != dialogueTable
        || dialogue.count != 2U || directive.class_id != kDirectiveComponentClass
        || directive.table != directiveTable || directive.count != 4U) {
        return false;
    }

    std::array<HandlerRecord, 2U> dialogueHandlers{};
    std::array<HandlerRecord, 4U> directiveHandlers{};
    if (!safe_copy_exact(dialogueHandlers.data(),
                         image.mapped_image.data() + kDialogueHandlerTableRva,
                         sizeof dialogueHandlers)
        || !safe_copy_exact(directiveHandlers.data(),
                            image.mapped_image.data() + kDirectiveHandlerTableRva,
                            sizeof directiveHandlers)) {
        return false;
    }
    std::uintptr_t type53Apply{};
    std::uintptr_t type53Tick{};
    std::uintptr_t type68Create{};
    std::uintptr_t type68LifecycleA{};
    std::uintptr_t type68LifecycleB{};
    std::uintptr_t type68Apply{};
    if (!pointer_at_rva(image, 0x1009B60U, type53Apply)
        || !pointer_at_rva(image, 0x100A180U, type53Tick)
        || !pointer_at_rva(image, 0x1009610U, type68Create)
        || !pointer_at_rva(image, 0x10091F0U, type68LifecycleA)
        || !pointer_at_rva(image, 0x100A3A0U, type68LifecycleB)
        || !pointer_at_rva(image, 0x1009C00U, type68Apply)) {
        return false;
    }
    return dialogueHandlers[0].function == type53Apply
           && dialogueHandlers[0].dispatch_key == kAuthorityDispatchKey
           && dialogueHandlers[1].function == type53Tick
           && dialogueHandlers[1].dispatch_key == kTickDispatchKey
           && directiveHandlers[0].function == type68Create
           && directiveHandlers[0].dispatch_key == kCreateDispatchKey
           && directiveHandlers[1].function == type68LifecycleA
           && directiveHandlers[1].dispatch_key == kTickDispatchKey
           && directiveHandlers[2].function == type68LifecycleB
           && directiveHandlers[2].dispatch_key == kTickDispatchKey
           && directiveHandlers[3].function == type68Apply
           && directiveHandlers[3].dispatch_key == kAuthorityDispatchKey;
}

[[nodiscard]] bool same_build(const RuntimeCohortEvidence& left,
                              const RuntimeCohortEvidence& right) noexcept {
    return left.packed_disk.file_bytes == right.packed_disk.file_bytes
           && left.packed_disk.sha256 == right.packed_disk.sha256
           && left.mapped_pe == right.mapped_pe
           && left.mapped_image_base == right.mapped_image_base
           && left.mapped_size_of_image == right.mapped_size_of_image
           && left.mapped_prefix_cohort_id == right.mapped_prefix_cohort_id
           && left.required_surface_mask == right.required_surface_mask
           && left.required_window_mask == right.required_window_mask
           && left.required_surface_count == right.required_surface_count
           && left.required_window_count == right.required_window_count
           && left.pe_identity_valid == right.pe_identity_valid
           && left.post_decryption_ready == right.post_decryption_ready
           && left.selected_pages_committed_executable
                  == right.selected_pages_committed_executable
           && left.class_tables_valid == right.class_tables_valid
           && left.all_prefixes_valid == right.all_prefixes_valid;
}

[[nodiscard]] bool same_call(const CaptureMetadata& left,
                             const CaptureMetadata& right) noexcept {
    return left.capture_epoch == right.capture_epoch && left.call_id == right.call_id
           && left.producer_thread_id == right.producer_thread_id
           && same_build(left.build, right.build);
}

template <typename Body>
[[nodiscard]] bool copy_cache(const void* instance, Body& body) noexcept {
    Body temporary{};
    if (!safe_copy_exact(temporary.data(),
                         offset_address(instance, kPcAuthorityCacheOffset),
                         temporary.size())) {
        return false;
    }
    body = temporary;
    return true;
}

[[nodiscard]] bool context_field_values_valid(const CaptureContext& context) noexcept {
    if ((context.presence_mask & ~kKnownContextPresenceMask) != 0U) {
        return false;
    }
    const bool activationPresent =
        has_context_field(context.presence_mask, ContextPresence::activation);
    if (activationPresent != static_cast<bool>(context.activation)
        || (!activationPresent
            && (context.activation != activity_lifecycle::NativeActivationToken{}
                || context.activation_state
                       != activity_lifecycle::NativeActivationState::empty))
        || (activationPresent
            && (context.activation_state < activity_lifecycle::NativeActivationState::active
                || context.activation_state
                       > activity_lifecycle::NativeActivationState::retired))) {
        return false;
    }
    const bool nativeIdentityPresent =
        has_context_field(context.presence_mask, ContextPresence::native_identity);
    if (nativeIdentityPresent != (context.native_identity != 0U)) {
        return false;
    }
    const bool activityPresent =
        has_context_field(context.presence_mask, ContextPresence::activity);
    if (activityPresent != static_cast<bool>(context.activity)
        || (!activityPresent && context.activity != state::activity::ActivityInstanceKey{})) {
        return false;
    }
    const bool rosterPresent =
        has_context_field(context.presence_mask, ContextPresence::roster_generation);
    if (rosterPresent != static_cast<bool>(context.roster_generation)) {
        return false;
    }
    const bool authorityPresent =
        has_context_field(context.presence_mask, ContextPresence::authority_publication);
    if (authorityPresent != static_cast<bool>(context.authority_publication)) {
        return false;
    }
    const bool runPresent = has_context_field(context.presence_mask, ContextPresence::run_token);
    if (runPresent != (context.run_token != 0U)) {
        return false;
    }
    const bool correlationPresent =
        has_context_field(context.presence_mask, ContextPresence::correlation_token);
    return correlationPresent == (context.correlation_token != 0U);
}

[[nodiscard]] bool present_context_equal(const CaptureContext& entry,
                                         const CaptureContext& exit) noexcept {
    if (entry.presence_mask != exit.presence_mask || !context_field_values_valid(entry)
        || !context_field_values_valid(exit)) {
        return false;
    }
    return (!has_context_field(entry.presence_mask, ContextPresence::activation)
            || entry.activation == exit.activation)
           && (!has_context_field(entry.presence_mask, ContextPresence::native_identity)
               || entry.native_identity == exit.native_identity)
           && (!has_context_field(entry.presence_mask, ContextPresence::activity)
               || entry.activity == exit.activity)
           && (!has_context_field(entry.presence_mask, ContextPresence::roster_generation)
               || entry.roster_generation == exit.roster_generation)
           && (!has_context_field(entry.presence_mask, ContextPresence::authority_publication)
               || entry.authority_publication == exit.authority_publication)
           && (!has_context_field(entry.presence_mask, ContextPresence::run_token)
               || entry.run_token == exit.run_token)
           && (!has_context_field(entry.presence_mask, ContextPresence::correlation_token)
               || entry.correlation_token == exit.correlation_token);
}

[[nodiscard]] DefaultContextFields project_default_context(
    const CaptureContext& context) noexcept {
    DefaultContextFields output{};
    output.presence_mask = context.presence_mask;
    output.activation_key = context.activation.key;
    output.native_identity = context.native_identity;
    output.activity = context.activity;
    output.roster_generation = context.roster_generation;
    output.authority_publication = context.authority_publication;
    output.run_token = context.run_token;
    output.correlation_token = context.correlation_token;
    output.activation_state = context.activation_state;
    output.complete_activation_token_present = static_cast<bool>(context.activation);
    return output;
}

template <typename Record>
[[nodiscard]] bool apply_record_common_valid(const Record& record,
                                             const StaticConsumerProvenance& expected,
                                             std::uint32_t schema,
                                             CaptureWindow entryWindow,
                                             CaptureWindow resolverWindow,
                                             CaptureWindow commitWindow) noexcept {
    if (record.instance_identity == 0U || record.static_consumer != expected
        || !record.static_consumer.statically_pinned || !record.valid.entry_wrapper_valid
        || record.entry_wrapper.schema != schema
        || !context_field_values_valid(record.entry_context)
        || !context_field_values_valid(record.resolver_context)
        || !context_field_values_valid(record.exit_context)
        || !valid_capture_metadata(record.telemetry.entry)
        || record.telemetry.entry.window != entryWindow
        || !valid_capture_metadata(record.telemetry.commit)
        || record.telemetry.commit.window != commitWindow
        || !same_call(record.telemetry.entry, record.telemetry.commit)
        || (record.valid.exact_owner_at_entry
            && !exact_owner_at_entry(record.entry_context))
        || (record.valid.same_owner_current_at_resolver
            && (!record.valid.exact_owner_at_entry
                || !same_owner_current_at_exit(record.entry_context,
                                               record.resolver_context)))
        || (record.valid.same_owner_current_at_exit
            && (!record.valid.exact_owner_at_entry
                || !same_owner_current_at_exit(record.entry_context, record.exit_context)))) {
        return false;
    }
    if (record.valid.native_resolver_observed) {
        if (!valid_capture_metadata(record.telemetry.resolver)
            || record.telemetry.resolver.window != resolverWindow
            || !same_call(record.telemetry.entry, record.telemetry.resolver)) {
            return false;
        }
    } else if (record.valid.native_resolver_succeeded || record.valid.decoded_copy_valid
               || record.valid.decoded_canonical || record.valid.cache_before_valid
               || record.valid.same_owner_current_at_resolver
               || record.resolver_context.presence_mask != 0U
               || record.telemetry.resolver.capture_epoch != 0U
               || record.telemetry.resolver.call_id != 0U
               || record.telemetry.resolver.native_rva != 0U) {
        return false;
    }
    if (record.valid.decoded_copy_valid) {
        if (!record.valid.native_resolver_observed || !record.valid.native_resolver_succeeded
            || record.resolver_body_fingerprint
                   != bounded_body_fingerprint(record.resolver_body_pre)) {
            return false;
        }
        const bool canonical = [&record]() noexcept {
            if constexpr (std::is_same_v<Record, DialogueApplyRecord>) {
                return canonical_dialogue_body(record.resolver_body_pre);
            } else {
                return canonical_directive_body(record.resolver_body_pre);
            }
        }();
        if (record.valid.decoded_canonical != canonical) {
            return false;
        }
    } else if (record.resolver_body_fingerprint != 0U
               || !std::all_of(record.resolver_body_pre.begin(),
                               record.resolver_body_pre.end(),
                               [](std::byte value) noexcept { return value == std::byte{}; })) {
        return false;
    } else if (record.valid.decoded_canonical) {
        return false;
    }
    if (record.valid.cache_before_valid) {
        if (!record.valid.same_owner_current_at_resolver
            || record.cache_before_fingerprint == 0U) {
            return false;
        }
    } else if (record.cache_before_fingerprint != 0U) {
        return false;
    }
    if (record.valid.cache_after_valid) {
        if (record.post_commit_cache_fingerprint
            != bounded_body_fingerprint(record.post_commit_cache)) {
            return false;
        }
        const bool canonical = [&record]() noexcept {
            if constexpr (std::is_same_v<Record, DialogueApplyRecord>) {
                return canonical_dialogue_body(record.post_commit_cache);
            } else {
                return canonical_directive_body(record.post_commit_cache);
            }
        }();
        if (record.valid.cache_after_canonical != canonical) {
            return false;
        }
    } else if (record.post_commit_cache_fingerprint != 0U) {
        return false;
    } else if (record.valid.cache_after_canonical) {
        return false;
    }
    return record.valid.decoded_copy_valid || record.valid.cache_after_valid;
}

} // namespace

NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept {
    switch (surface) {
    case NativeSurface::schema_resolver:
        return {0x4A6340U,
                kSchemaResolverExactBody,
                NativeAbi::packet_schema_resolver,
                kSchemaResolverExactBody.size(),
                false};
    case NativeSurface::type53_apply:
        return {0x1009B60U, kType53ApplyPrefix, NativeAbi::instance_packet, 0U, true};
    case NativeSurface::type53_tick:
        return {0x100A180U, kType53TickPrefix, NativeAbi::instance_only, 0U, true};
    case NativeSurface::type53_selected_row_dispatch:
        return {0x10097D0U, kType53SelectedRowPrefix, NativeAbi::component_index, 0U, true};
    case NativeSurface::type53_terminal_wrapper:
        return {0xA38490U, kType53TerminalWrapperPrefix, NativeAbi::terminal_selector, 0U, true};
    case NativeSurface::type53_terminal_core:
        return {0xA38530U, kType53TerminalCorePrefix, NativeAbi::terminal_selector, 0U, true};
    case NativeSurface::type53_presentation_start:
        return {0xA3D710U, kPresentationStartPrefix, NativeAbi::component_index, 0U, true};
    case NativeSurface::type68_create_tail:
        return {0x1009610U, kType68CreateTailPrefix, NativeAbi::create_instance_bool, 22U, true};
    case NativeSurface::type68_lifecycle_a:
        return {0x10091F0U, kType68LifecycleAPrefix, NativeAbi::instance_only, 0U, true};
    case NativeSurface::type68_lifecycle_b_update:
        return {0x100A3A0U, kType68LifecycleBPrefix, NativeAbi::instance_only, 0U, true};
    case NativeSurface::type68_apply:
        return {0x1009C00U, kType68ApplyPrefix, NativeAbi::instance_packet, 0U, true};
    case NativeSurface::type68_content_resolver:
        return {0x1009430U,
                kType68ContentResolverPrefix,
                NativeAbi::content_resolver,
                0U,
                false};
    case NativeSurface::type68_same_identity_reconcile:
        return {0x10098C0U,
                kType68ReconcilePrefix,
                NativeAbi::component_content_new_old,
                0U,
                true};
    case NativeSurface::type68_install:
        return {0x1009ED0U, kType68InstallPrefix, NativeAbi::component_record_content, 0U, true};
    case NativeSurface::manager_ready:
        return {0x137E1D0U, kManagerReadyPrefix, NativeAbi::no_args_bool, 0U, true};
    case NativeSurface::manager_get:
        return {0x137D6F0U, kManagerGetExactBody, NativeAbi::no_args_pointer, 12U, false};
    case NativeSurface::manager_add:
        return {0x137BD50U, kManagerAddPrefix, NativeAbi::manager_event, 0U, true};
    case NativeSurface::manager_status_update:
        return {0x137E2C0U,
                kManagerStatusPrefix,
                NativeAbi::manager_hash_status_aux,
                0U,
                true};
    case NativeSurface::manager_entry_materialize:
        return {0x1382710U, kManagerEntryMaterializePrefix, NativeAbi::entry_only, 0U, true};
    case NativeSurface::manager_terminal_predicate:
        return {0x137DC10U, kManagerTerminalPrefix, NativeAbi::entry_only, 0U, true};
    case NativeSurface::manager_materialize_walk:
        return {0x1382C30U, kManagerMaterializeWalkPrefix, NativeAbi::instance_only, 0U, true};
    default:
        return {};
    }
}

bool native_prefix_matches(NativeSurface surface,
                           std::span<const std::byte> observed) noexcept {
    const NativeBoundaryDescriptor boundary = native_boundary(surface);
    return boundary.rva != 0U && !boundary.prefix.empty()
           && observed.size() >= boundary.prefix.size()
           && std::equal(boundary.prefix.begin(), boundary.prefix.end(), observed.begin());
}

std::span<const NativeSurface> required_native_surfaces() noexcept {
    return kRequiredSurfaces;
}

std::span<const CaptureWindow> required_capture_windows() noexcept {
    return kRequiredWindows;
}

RuntimeCohortResult validate_runtime_cohort(const RuntimeImageView& image,
                                            std::span<const NativeSurface> selectedSurfaces,
                                            std::span<const CaptureWindow> selectedWindows,
                                            std::span<std::uintptr_t> outputAddresses,
                                            std::span<std::uintptr_t> outputWindowAddresses,
                                            RuntimeCohortEvidence& outputEvidence) noexcept {
    std::fill(outputAddresses.begin(), outputAddresses.end(), std::uintptr_t{});
    std::fill(outputWindowAddresses.begin(), outputWindowAddresses.end(), std::uintptr_t{});
    outputEvidence = {};
    if (selectedSurfaces.size() != outputAddresses.size()
        || selectedWindows.size() != outputWindowAddresses.size()
        || image.mapped_image.empty()) {
        return RuntimeCohortResult::invalid_arguments;
    }
    for (std::size_t index = 0U; index < selectedSurfaces.size(); ++index) {
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (selectedSurfaces[prior] == selectedSurfaces[index]) {
                return RuntimeCohortResult::duplicate_surface;
            }
        }
    }
    if (selectedSurfaces.size() != kRequiredSurfaces.size()
        || selectedWindows.size() != kRequiredWindows.size()
        || !std::equal(selectedSurfaces.begin(), selectedSurfaces.end(),
                       kRequiredSurfaces.begin())
        || !std::equal(selectedWindows.begin(), selectedWindows.end(),
                       kRequiredWindows.begin())) {
        return RuntimeCohortResult::incomplete_manifest;
    }
    if (!matches_pinned_packed_disk(image.packed_disk)) {
        return RuntimeCohortResult::packed_disk_identity_mismatch;
    }
    PackedMappedPeIdentity mappedPe{};
    if (!validate_pe(image, mappedPe)) {
        return RuntimeCohortResult::pe_identity_mismatch;
    }
    if (!image.post_decryption_ready) {
        return RuntimeCohortResult::not_post_decryption_ready;
    }
    if (!image.selected_pages_committed_executable) {
        return RuntimeCohortResult::page_contract_failed;
    }

    std::uint64_t cohort = kFnvOffset;
    for (std::size_t index = 0U; index < selectedSurfaces.size(); ++index) {
        const NativeBoundaryDescriptor boundary = native_boundary(selectedSurfaces[index]);
        if (boundary.rva == 0U || boundary.prefix.empty()
            || boundary.prefix.size() > kMaximumPrefixBytes || boundary.rva > image.mapped_image.size()
            || boundary.prefix.size() > image.mapped_image.size() - boundary.rva) {
            return RuntimeCohortResult::target_out_of_range;
        }
        if (!committed_executable_range(image.mapped_image.data() + boundary.rva,
                                        boundary.prefix.size())) {
            return RuntimeCohortResult::page_contract_failed;
        }
        std::array<std::byte, kMaximumPrefixBytes> observed{};
        if (!safe_copy_exact(observed.data(),
                             image.mapped_image.data() + boundary.rva,
                             boundary.prefix.size())
            || !native_prefix_matches(
                selectedSurfaces[index], {observed.data(), boundary.prefix.size()})) {
            return RuntimeCohortResult::prefix_mismatch;
        }
        for (std::size_t byte = 0U; byte < sizeof boundary.rva; ++byte) {
            cohort ^= static_cast<std::uint8_t>(boundary.rva >> (byte * 8U));
            cohort *= kFnvPrime;
        }
        for (const std::byte value : boundary.prefix) {
            cohort ^= std::to_integer<std::uint8_t>(value);
            cohort *= kFnvPrime;
        }
    }
    for (std::size_t index = 0U; index < selectedWindows.size(); ++index) {
        const CaptureWindowDescriptor descriptor = capture_window(selectedWindows[index]);
        if (!descriptor.instruction_window_proven || descriptor.rva == 0U
            || descriptor.instruction_bytes.empty()
            || descriptor.instruction_length != descriptor.instruction_bytes.size()
            || descriptor.rva > image.mapped_image.size()
            || descriptor.instruction_length > image.mapped_image.size() - descriptor.rva) {
            return RuntimeCohortResult::target_out_of_range;
        }
        const std::byte* const address = image.mapped_image.data() + descriptor.rva;
        if (!committed_executable_range(address, descriptor.instruction_length)) {
            return RuntimeCohortResult::page_contract_failed;
        }
        if (!std::equal(descriptor.instruction_bytes.begin(),
                        descriptor.instruction_bytes.end(), address)) {
            return RuntimeCohortResult::prefix_mismatch;
        }
        for (std::size_t byte = 0U; byte < sizeof descriptor.rva; ++byte) {
            cohort ^= static_cast<std::uint8_t>(descriptor.rva >> (byte * 8U));
            cohort *= kFnvPrime;
        }
        for (const std::byte value : descriptor.instruction_bytes) {
            cohort ^= std::to_integer<std::uint8_t>(value);
            cohort *= kFnvPrime;
        }
    }
    if (!validate_class_tables(image)) {
        return RuntimeCohortResult::class_table_mismatch;
    }

    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(image.mapped_image.data());
    for (std::size_t index = 0U; index < selectedSurfaces.size(); ++index) {
        const std::uintptr_t rva = native_boundary(selectedSurfaces[index]).rva;
        if (rva > (std::numeric_limits<std::uintptr_t>::max)() - base) {
            std::fill(outputAddresses.begin(), outputAddresses.end(), std::uintptr_t{});
            return RuntimeCohortResult::target_out_of_range;
        }
        outputAddresses[index] = base + rva;
    }
    for (std::size_t index = 0U; index < selectedWindows.size(); ++index) {
        const std::uintptr_t rva = capture_window(selectedWindows[index]).rva;
        if (rva > (std::numeric_limits<std::uintptr_t>::max)() - base) {
            std::fill(outputAddresses.begin(), outputAddresses.end(), std::uintptr_t{});
            std::fill(outputWindowAddresses.begin(), outputWindowAddresses.end(),
                      std::uintptr_t{});
            return RuntimeCohortResult::target_out_of_range;
        }
        outputWindowAddresses[index] = base + rva;
    }
    outputEvidence.packed_disk = image.packed_disk;
    outputEvidence.mapped_pe = mappedPe;
    outputEvidence.mapped_image_base = base;
    outputEvidence.mapped_size_of_image = kPinnedMappedSizeOfImage;
    outputEvidence.mapped_prefix_cohort_id = cohort;
    outputEvidence.required_surface_mask = (std::uint64_t{1U} << kRequiredSurfaces.size()) - 1U;
    outputEvidence.required_window_mask = (std::uint64_t{1U} << kRequiredWindows.size()) - 1U;
    outputEvidence.required_surface_count = static_cast<std::uint32_t>(kRequiredSurfaces.size());
    outputEvidence.required_window_count = static_cast<std::uint32_t>(kRequiredWindows.size());
    outputEvidence.pe_identity_valid = true;
    outputEvidence.post_decryption_ready = true;
    outputEvidence.selected_pages_committed_executable = true;
    outputEvidence.class_tables_valid = true;
    outputEvidence.all_prefixes_valid = true;
    return RuntimeCohortResult::valid;
}

CaptureWindowDescriptor capture_window(CaptureWindow window) noexcept {
    const auto entry = [](std::uintptr_t rva,
                          std::span<const std::byte> bytes,
                          CapturePhase phase) noexcept {
        return CaptureWindowDescriptor{rva,
                                       bytes,
                                       bytes.size(),
                                       phase,
                                       CaptureProbeKind::function_entry,
                                       CapturePlacement::reviewed_function_entry,
                                       true,
                                       true};
    };
    const auto ownerWindow = [](std::uintptr_t rva,
                                std::span<const std::byte> bytes,
                                CapturePhase phase,
                                CaptureProbeKind kind) noexcept {
        return CaptureWindowDescriptor{rva,
                                       bytes,
                                       bytes.size(),
                                       phase,
                                       kind,
                                       CapturePlacement::owner_probe_not_recovered,
                                       true,
                                       false};
    };
    const auto resolverReturn = [](std::uintptr_t rva,
                                   std::span<const std::byte> bytes) noexcept {
        return CaptureWindowDescriptor{rva,
                                       bytes,
                                       bytes.size(),
                                       CapturePhase::resolver_return,
                                       CaptureProbeKind::resolver_return_address_filter,
                                       CapturePlacement::reviewed_return_address_filter,
                                       true,
                                       false};
    };
    switch (window) {
    case CaptureWindow::type53_apply_entry:
        return entry(0x1009B60U, kType53ApplyPrefix, CapturePhase::apply_entry);
    case CaptureWindow::type53_resolver_return:
        return resolverReturn(kType53ResolverReturnRva, kType53ResolverReturnBytes);
    case CaptureWindow::type53_post_commit:
        return ownerWindow(0x1009BF9U, kType53PostCommitBytes, CapturePhase::post_commit,
                           CaptureProbeKind::interior_instruction);
    case CaptureWindow::type53_terminal_pre:
        return ownerWindow(0x100A34EU, kType53TerminalPreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type53_terminal_post:
        return ownerWindow(0x100A353U, kType53TerminalPostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type53_generation_consumed:
        return ownerWindow(0x100A35EU, kType53GenerationBytes,
                           CapturePhase::generation_consumed,
                           CaptureProbeKind::interior_instruction);
    case CaptureWindow::type53_terminal_wrapper:
        return entry(0xA38490U, kType53TerminalWrapperPrefix, CapturePhase::pre_call);
    case CaptureWindow::type53_terminal_core:
        return entry(0xA38530U, kType53TerminalCorePrefix, CapturePhase::pre_call);
    case CaptureWindow::type53_presentation_start:
        return entry(0xA3D710U, kPresentationStartPrefix, CapturePhase::presentation_start);
    case CaptureWindow::type53_presentation_end_unrecovered:
        return {0U,
                {},
                0U,
                CapturePhase::presentation_end_unrecovered,
                CaptureProbeKind::unrecovered,
                CapturePlacement::unrecovered,
                false,
                false};
    case CaptureWindow::type68_apply_entry:
        return entry(0x1009C00U, kType68ApplyPrefix, CapturePhase::apply_entry);
    case CaptureWindow::type68_resolver_return:
        return resolverReturn(kType68ResolverReturnRva, kType68ResolverReturnBytes);
    case CaptureWindow::type68_content_return:
        return ownerWindow(0x1009C40U, kType68ContentReturnBytes, CapturePhase::content_return,
                           CaptureProbeKind::interior_instruction);
    case CaptureWindow::type68_reconcile_pre:
        return ownerWindow(0x1009CE6U, kType68ReconcilePreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_reconcile_post:
        return ownerWindow(0x1009CEBU, kType68ReconcilePostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type68_remove_pre:
        return ownerWindow(0x1009D5DU, kType68RemovePreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_remove_post:
        return ownerWindow(0x1009D62U, kType68RemovePostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type68_install_pre:
        return ownerWindow(0x1009D75U, kType68InstallPreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_install_post:
        return ownerWindow(0x1009D7AU, kType68InstallPostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type68_post_commit:
        return ownerWindow(0x1009DF3U, kType68PostCommitBytes, CapturePhase::post_commit,
                           CaptureProbeKind::interior_instruction);
    case CaptureWindow::type68_post_refresh:
        return ownerWindow(0x1009E0FU, kType68PostRefreshBytes, CapturePhase::post_refresh,
                           CaptureProbeKind::interior_instruction);
    case CaptureWindow::type68_manager_ready:
        return ownerWindow(0x1009FD3U, kType68ManagerReadyBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_manager_add_pre:
        return ownerWindow(0x1009FEDU, kType68ManagerAddPreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_manager_add_post:
        return ownerWindow(0x1009FF2U, kType68ManagerAddPostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type68_alternate_builder_return:
        return ownerWindow(0x100A0A6U, kType68AlternateReadyBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_alternate_add_pre:
        return ownerWindow(0x100A0BCU, kType68AlternateAddPreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_alternate_add_post:
        return ownerWindow(0x100A0C1U, kType68AlternateAddPostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type68_manager_materialize:
        return ownerWindow(0x1382C73U, kManagerMaterializePreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_manager_materialize_post:
        return ownerWindow(0x1382C78U, kManagerMaterializePostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    case CaptureWindow::type68_manager_terminal:
        return ownerWindow(0x1383204U, kManagerTerminalPreBytes, CapturePhase::pre_call,
                           CaptureProbeKind::callsite_pre);
    case CaptureWindow::type68_manager_terminal_post:
        return ownerWindow(0x1383209U, kManagerTerminalPostBytes, CapturePhase::post_call,
                           CaptureProbeKind::callsite_post_fallthrough);
    default:
        return {};
    }
}

bool valid_capture_metadata(const CaptureMetadata& metadata,
                            bool allowUnrecoveredWindow) noexcept {
    const CaptureWindowDescriptor window = capture_window(metadata.window);
    const bool buildValid = matches_pinned_packed_disk(metadata.build.packed_disk)
                            && metadata.build.mapped_size_of_image == kPinnedMappedSizeOfImage
                            && metadata.build.mapped_prefix_cohort_id != 0U
                            && metadata.build.pe_identity_valid
                            && metadata.build.post_decryption_ready
                            && metadata.build.selected_pages_committed_executable
                            && metadata.build.class_tables_valid
                            && metadata.build.all_prefixes_valid;
    if (!buildValid || metadata.capture_epoch == 0U || metadata.monotonic_tick == 0U
        || metadata.call_id == 0U
        || metadata.producer_thread_id == 0U || metadata.caller_rva == 0U
        || metadata.caller_rva >= metadata.build.mapped_size_of_image
        || metadata.phase != window.phase || metadata.native_rva != window.rva) {
        return false;
    }
    return window.instruction_window_proven
           || (allowUnrecoveredWindow
               && metadata.window == CaptureWindow::type53_presentation_end_unrecovered
               && metadata.native_rva == 0U);
}

std::uint64_t bounded_body_fingerprint(std::span<const std::byte> body) noexcept {
    std::uint64_t hash = kFnvOffset;
    for (const std::byte value : body) {
        hash ^= std::to_integer<std::uint8_t>(value);
        hash *= kFnvPrime;
    }
    return hash;
}

bool dialogue_record_fields(const DialogueBody& body,
                            std::size_t index,
                            DialogueRecordFields& output) noexcept {
    if (index >= kDialogueRecordCount) {
        return false;
    }
    DialogueRecordLayout record{};
    std::memcpy(&record,
                body.data() + kDialogueRecordOffset + index * kDialogueRecordStride,
                sizeof record);
    output = DialogueRecordFields{record.value,
                                  record.optional_value_storage,
                                  record.reference,
                                  record.generation,
                                  record.mode};
    return true;
}

bool directive_entry_fields(const DirectiveBody& body,
                            std::size_t index,
                            DirectiveEntryFields& output) noexcept {
    if (index >= kDirectiveEntryCount) {
        return false;
    }
    DirectiveEntryLayout entry{};
    std::memcpy(&entry,
                body.data() + kDirectiveEntryOffset + index * kDirectiveEntryStride,
                sizeof entry);
    output = DirectiveEntryFields{entry.event_key, entry.discriminator, entry.lifecycle};
    return true;
}

bool canonical_dialogue_body(const DialogueBody& body) noexcept {
    for (std::size_t index = 0U; index < kDialogueRecordCount; ++index) {
        DialogueRecordFields fields{};
        if (!dialogue_record_fields(body, index, fields) || fields.mode > 3U) {
            return false;
        }
    }
    return true;
}

bool canonical_directive_body(const DirectiveBody& body) noexcept {
    DirectiveDecodedLayout layout{};
    std::memcpy(&layout, body.data(), sizeof layout);
    if (!directive_selector_valid(layout.selector)) {
        return false;
    }
    for (const DirectiveEntryLayout& entry : layout.entries) {
        if (!directive_lifecycle_valid(entry.lifecycle) || entry.scalar.boolean > 1U
            || entry.auxiliary_enum_a > 3U || entry.auxiliary_enum_b > 7U) {
            return false;
        }
        for (const DirectiveTargetLayout& target : entry.targets) {
            if (target.boolean > 1U) {
                return false;
            }
        }
    }
    return true;
}

bool exact_owner_at_entry(const CaptureContext& context) noexcept {
    return context_field_values_valid(context)
           && has_context_field(context.presence_mask, ContextPresence::activation)
           && static_cast<bool>(context.activation)
           && context.activation_state == activity_lifecycle::NativeActivationState::active;
}

bool fully_correlated(const CaptureContext& context) noexcept {
    return context.presence_mask == kKnownContextPresenceMask
           && context_field_values_valid(context) && exact_owner_at_entry(context);
}

bool same_owner_current_at_exit(const CaptureContext& entry,
                                const CaptureContext& exit) noexcept {
    return exact_owner_at_entry(entry) && context_field_values_valid(exit)
           && exit.activation_state == activity_lifecycle::NativeActivationState::active
           && present_context_equal(entry, exit);
}

CaptureBuildResult capture_entry_wrapper(PendingDialogueCapture& pending,
                                         const CaptureContext& context,
                                         CaptureMetadata metadata,
                                         const void* instance,
                                         const PacketReference16* packet) noexcept {
    pending.entry_context_ = {};
    pending.resolver_context_ = {};
    pending.entry_metadata_ = {};
    pending.resolver_metadata_ = {};
    pending.instance_identity_ = 0U;
    pending.entry_wrapper_ = {};
    pending.resolver_body_.fill(std::byte{});
    pending.cache_before_fingerprint_ = 0U;
    pending.cache_before_valid_ = false;
    pending.resolver_observed_ = false;
    pending.resolver_succeeded_ = false;
    pending.decoded_copy_valid_ = false;
    pending.decoded_canonical_ = false;
    pending.ready_ = false;
    if (!valid_capture_metadata(metadata)
        || metadata.window != CaptureWindow::type53_apply_entry) {
        return CaptureBuildResult::invalid_metadata;
    }
    if (instance == nullptr || packet == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    std::uint32_t definition{};
    EntryWrapperFields12 wrapper{};
    if (!safe_copy_exact(&definition, instance, sizeof definition)
        || !safe_copy_exact(&wrapper.schema, packet, sizeof wrapper.schema)
        || !safe_copy_exact(&wrapper.resolver_input,
                            offset_address(packet, kPacketResolverInputOffset),
                            sizeof wrapper.resolver_input)) {
        return CaptureBuildResult::unreadable;
    }
    if (definition != kDialogueDefinition) {
        return CaptureBuildResult::wrong_definition;
    }
    if (wrapper.schema != kDialogueAuthoritySchema) {
        return CaptureBuildResult::wrong_schema;
    }
    pending.entry_context_ = context;
    pending.entry_metadata_ = metadata;
    pending.instance_identity_ = reinterpret_cast<std::uintptr_t>(instance);
    pending.entry_wrapper_ = wrapper;
    pending.ready_ = true;
    return CaptureBuildResult::entry_ready;
}

CaptureBuildResult capture_entry_wrapper(PendingDirectiveCapture& pending,
                                         const CaptureContext& context,
                                         CaptureMetadata metadata,
                                         const void* instance,
                                         const PacketReference16* packet) noexcept {
    pending.entry_context_ = {};
    pending.resolver_context_ = {};
    pending.entry_metadata_ = {};
    pending.resolver_metadata_ = {};
    pending.instance_identity_ = 0U;
    pending.entry_wrapper_ = {};
    pending.resolver_body_.fill(std::byte{});
    pending.cache_before_fingerprint_ = 0U;
    pending.cache_before_valid_ = false;
    pending.resolver_observed_ = false;
    pending.resolver_succeeded_ = false;
    pending.decoded_copy_valid_ = false;
    pending.decoded_canonical_ = false;
    pending.ready_ = false;
    if (!valid_capture_metadata(metadata)
        || metadata.window != CaptureWindow::type68_apply_entry) {
        return CaptureBuildResult::invalid_metadata;
    }
    if (instance == nullptr || packet == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    std::uint32_t definition{};
    EntryWrapperFields12 wrapper{};
    if (!safe_copy_exact(&definition, instance, sizeof definition)
        || !safe_copy_exact(&wrapper.schema, packet, sizeof wrapper.schema)
        || !safe_copy_exact(&wrapper.resolver_input,
                            offset_address(packet, kPacketResolverInputOffset),
                            sizeof wrapper.resolver_input)) {
        return CaptureBuildResult::unreadable;
    }
    if (definition != kDirectiveDefinition) {
        return CaptureBuildResult::wrong_definition;
    }
    if (wrapper.schema != kDirectiveAuthoritySchema) {
        return CaptureBuildResult::wrong_schema;
    }
    pending.entry_context_ = context;
    pending.entry_metadata_ = metadata;
    pending.instance_identity_ = reinterpret_cast<std::uintptr_t>(instance);
    pending.entry_wrapper_ = wrapper;
    pending.ready_ = true;
    return CaptureBuildResult::entry_ready;
}

CaptureBuildResult capture_resolver_result(PendingDialogueCapture& pending,
                                           const CaptureContext& resolverContext,
                                           CaptureMetadata metadata,
                                           const void* decodedRax) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    if (!valid_capture_metadata(metadata)
        || metadata.window != CaptureWindow::type53_resolver_return
        || !same_call(pending.entry_metadata_, metadata)) {
        return CaptureBuildResult::phase_mismatch;
    }
    pending.resolver_context_ = resolverContext;
    pending.resolver_metadata_ = metadata;
    pending.resolver_observed_ = true;
    pending.resolver_succeeded_ = decodedRax != nullptr;
    if (same_owner_current_at_exit(pending.entry_context_, resolverContext)) {
        DialogueBody cache{};
        if (copy_cache(reinterpret_cast<const void*>(pending.instance_identity_), cache)) {
            pending.cache_before_valid_ = true;
            pending.cache_before_fingerprint_ = bounded_body_fingerprint(cache);
        }
    }
    if (decodedRax == nullptr
        || !safe_copy_exact(
            pending.resolver_body_.data(), decodedRax, pending.resolver_body_.size())) {
        return CaptureBuildResult::resolver_failed;
    }
    pending.decoded_copy_valid_ = true;
    pending.decoded_canonical_ = canonical_dialogue_body(pending.resolver_body_);
    return CaptureBuildResult::resolver_captured;
}

CaptureBuildResult capture_resolver_result(PendingDirectiveCapture& pending,
                                           const CaptureContext& resolverContext,
                                           CaptureMetadata metadata,
                                           const void* decodedRax) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    if (!valid_capture_metadata(metadata)
        || metadata.window != CaptureWindow::type68_resolver_return
        || !same_call(pending.entry_metadata_, metadata)) {
        return CaptureBuildResult::phase_mismatch;
    }
    pending.resolver_context_ = resolverContext;
    pending.resolver_metadata_ = metadata;
    pending.resolver_observed_ = true;
    pending.resolver_succeeded_ = decodedRax != nullptr;
    if (same_owner_current_at_exit(pending.entry_context_, resolverContext)) {
        DirectiveBody cache{};
        if (copy_cache(reinterpret_cast<const void*>(pending.instance_identity_), cache)) {
            pending.cache_before_valid_ = true;
            pending.cache_before_fingerprint_ = bounded_body_fingerprint(cache);
        }
    }
    if (decodedRax == nullptr
        || !safe_copy_exact(
            pending.resolver_body_.data(), decodedRax, pending.resolver_body_.size())) {
        return CaptureBuildResult::resolver_failed;
    }
    pending.decoded_copy_valid_ = true;
    pending.decoded_canonical_ = canonical_directive_body(pending.resolver_body_);
    return CaptureBuildResult::resolver_captured;
}

CaptureBuildResult capture_post_commit(PendingDialogueCapture& pending,
                                       const CaptureContext& exitContext,
                                       CaptureMetadata metadata,
                                       const void* instance,
                                       DialogueApplyRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (!valid_capture_metadata(metadata)
        || metadata.window != CaptureWindow::type53_post_commit
        || !same_call(pending.entry_metadata_, metadata)) {
        return CaptureBuildResult::phase_mismatch;
    }
    if (reinterpret_cast<std::uintptr_t>(instance) != pending.instance_identity_) {
        return CaptureBuildResult::instance_mismatch;
    }
    DialogueBody cache{};
    const bool cacheValid = copy_cache(instance, cache);
    if (!pending.decoded_copy_valid_ && !cacheValid) {
        return CaptureBuildResult::unreadable;
    }
    DialogueApplyRecord record{};
    record.entry_context = pending.entry_context_;
    record.resolver_context = pending.resolver_context_;
    record.exit_context = exitContext;
    record.telemetry = {pending.entry_metadata_, pending.resolver_metadata_, metadata};
    record.instance_identity = pending.instance_identity_;
    record.static_consumer = kDialogueProvenance;
    record.entry_wrapper = pending.entry_wrapper_;
    record.resolver_body_pre = pending.resolver_body_;
    record.post_commit_cache = cache;
    record.cache_before_fingerprint = pending.cache_before_fingerprint_;
    record.resolver_body_fingerprint = pending.decoded_copy_valid_
                                           ? bounded_body_fingerprint(record.resolver_body_pre)
                                           : 0U;
    record.post_commit_cache_fingerprint =
        cacheValid ? bounded_body_fingerprint(record.post_commit_cache) : 0U;
    record.valid = ApplyValidity{true,
                                 pending.resolver_observed_,
                                 pending.resolver_succeeded_,
                                 pending.decoded_copy_valid_,
                                 pending.decoded_canonical_,
                                 pending.cache_before_valid_,
                                 cacheValid,
                                 cacheValid && canonical_dialogue_body(cache),
                                 exact_owner_at_entry(pending.entry_context_),
                                 same_owner_current_at_exit(pending.entry_context_,
                                                            pending.resolver_context_),
                                 same_owner_current_at_exit(pending.entry_context_, exitContext)};
    if (!valid_raw_record(record)) {
        return CaptureBuildResult::consumed;
    }
    output = record;
    return pending.decoded_copy_valid_ && cacheValid ? CaptureBuildResult::complete
                                                    : CaptureBuildResult::partial;
}

CaptureBuildResult capture_post_commit(PendingDirectiveCapture& pending,
                                       const CaptureContext& exitContext,
                                       CaptureMetadata metadata,
                                       const void* instance,
                                       DirectiveApplyRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (!valid_capture_metadata(metadata)
        || metadata.window != CaptureWindow::type68_post_commit
        || !same_call(pending.entry_metadata_, metadata)) {
        return CaptureBuildResult::phase_mismatch;
    }
    if (reinterpret_cast<std::uintptr_t>(instance) != pending.instance_identity_) {
        return CaptureBuildResult::instance_mismatch;
    }
    DirectiveBody cache{};
    const bool cacheValid = copy_cache(instance, cache);
    if (!pending.decoded_copy_valid_ && !cacheValid) {
        return CaptureBuildResult::unreadable;
    }
    DirectiveApplyRecord record{};
    record.entry_context = pending.entry_context_;
    record.resolver_context = pending.resolver_context_;
    record.exit_context = exitContext;
    record.telemetry = {pending.entry_metadata_, pending.resolver_metadata_, metadata};
    record.instance_identity = pending.instance_identity_;
    record.static_consumer = kDirectiveProvenance;
    record.entry_wrapper = pending.entry_wrapper_;
    record.resolver_body_pre = pending.resolver_body_;
    record.post_commit_cache = cache;
    record.cache_before_fingerprint = pending.cache_before_fingerprint_;
    record.resolver_body_fingerprint = pending.decoded_copy_valid_
                                           ? bounded_body_fingerprint(record.resolver_body_pre)
                                           : 0U;
    record.post_commit_cache_fingerprint =
        cacheValid ? bounded_body_fingerprint(record.post_commit_cache) : 0U;
    record.valid = ApplyValidity{true,
                                 pending.resolver_observed_,
                                 pending.resolver_succeeded_,
                                 pending.decoded_copy_valid_,
                                 pending.decoded_canonical_,
                                 pending.cache_before_valid_,
                                 cacheValid,
                                 cacheValid && canonical_directive_body(cache),
                                 exact_owner_at_entry(pending.entry_context_),
                                 same_owner_current_at_exit(pending.entry_context_,
                                                            pending.resolver_context_),
                                 same_owner_current_at_exit(pending.entry_context_, exitContext)};
    if (!valid_raw_record(record)) {
        return CaptureBuildResult::consumed;
    }
    output = record;
    return pending.decoded_copy_valid_ && cacheValid ? CaptureBuildResult::complete
                                                    : CaptureBuildResult::partial;
}

bool valid_raw_record(const DialogueApplyRecord& record) noexcept {
    return apply_record_common_valid(record,
                                     kDialogueProvenance,
                                     kDialogueAuthoritySchema,
                                     CaptureWindow::type53_apply_entry,
                                     CaptureWindow::type53_resolver_return,
                                     CaptureWindow::type53_post_commit);
}

bool valid_raw_record(const DirectiveApplyRecord& record) noexcept {
    return apply_record_common_valid(record,
                                     kDirectiveProvenance,
                                     kDirectiveAuthoritySchema,
                                     CaptureWindow::type68_apply_entry,
                                     CaptureWindow::type68_resolver_return,
                                     CaptureWindow::type68_post_commit);
}

SemanticDedupeKey semantic_dedupe_key(const DialogueApplyRecord& record) noexcept {
    const bool eligible = valid_raw_record(record) && fully_correlated(record.entry_context)
                          && record.valid.same_owner_current_at_exit
                          && record.valid.decoded_copy_valid
                          && record.valid.decoded_canonical;
    return SemanticDedupeKey{record.entry_context,
                             record.telemetry.entry.capture_epoch,
                             record.static_consumer,
                             record.resolver_body_fingerprint,
                             eligible};
}

SemanticDedupeKey semantic_dedupe_key(const DirectiveApplyRecord& record) noexcept {
    const bool eligible = valid_raw_record(record) && fully_correlated(record.entry_context)
                          && record.valid.same_owner_current_at_exit
                          && record.valid.decoded_copy_valid
                          && record.valid.decoded_canonical;
    return SemanticDedupeKey{record.entry_context,
                             record.telemetry.entry.capture_epoch,
                             record.static_consumer,
                             record.resolver_body_fingerprint,
                             eligible};
}

bool valid_type53_terminal_event(const Type53TerminalEvent& event) noexcept {
    const CaptureWindow window = event.metadata.window;
    const bool terminalWindow = window == CaptureWindow::type53_terminal_pre
                                || window == CaptureWindow::type53_terminal_post
                                || window == CaptureWindow::type53_generation_consumed
                                || window == CaptureWindow::type53_terminal_wrapper
                                || window == CaptureWindow::type53_terminal_core;
    return terminalWindow && valid_capture_metadata(event.metadata)
           && event.static_consumer == kDialogueProvenance
           && event.record_index < kDialogueRecordCount && event.record.mode <= 3U
           && fully_correlated(event.context)
           && (!event.generation_store_observed
               || window == CaptureWindow::type53_generation_consumed)
           && (window != CaptureWindow::type53_generation_consumed
               || (event.generation_store_observed
                   && event.processed_generation_after == event.record.generation))
           && (window != CaptureWindow::type53_terminal_post
               || event.terminal_handle_valid)
           && (!event.terminal_handle_valid || event.terminal_handle != 0U)
           && context_field_values_valid(event.context);
}

bool valid_type53_presentation_event(const Type53PresentationEvent& event) noexcept {
    const bool start = event.stage == PresentationStage::start
                       && event.metadata.window == CaptureWindow::type53_presentation_start
                       && valid_capture_metadata(event.metadata);
    // The end/teardown boundary remains unrecovered. Its descriptor can express absence, but no
    // event at an invented RVA is admitted as evidence.
    const bool end = false;
    return (start || end) && event.terminal_call_id != 0U && event.terminal_handle != 0U
           && event.presentation_handle != 0U && event.explicit_handle_correlation
           && fully_correlated(event.context);
}

bool valid_type68_content_event(const Type68ContentEvent& event) noexcept {
    return valid_capture_metadata(event.metadata)
           && event.metadata.window == CaptureWindow::type68_content_return
           && fully_correlated(event.context) && event.content_resolved
           && event.content_definition != 0U && event.content_bank == kDirectiveBank
           && event.content_identity_fingerprint != 0U;
}

bool valid_type68_transition_event(const Type68TransitionEvent& event) noexcept {
    if (!valid_capture_metadata(event.metadata) || !fully_correlated(event.context)
        || !event.inside_exact_apply_call || event.record_index >= kDirectiveEntryCount
        || !directive_lifecycle_valid(event.lifecycle)) {
        return false;
    }
    const CaptureWindow window = event.metadata.window;
    const bool postWindow = event.metadata.phase == CapturePhase::post_call;
    if (event.outcome_observed != postWindow) {
        return false;
    }
    switch (event.kind) {
    case Type68TransitionKind::reconcile:
        return window == CaptureWindow::type68_reconcile_pre
               || window == CaptureWindow::type68_reconcile_post;
    case Type68TransitionKind::remove:
    case Type68TransitionKind::status:
        return window == CaptureWindow::type68_remove_pre
               || window == CaptureWindow::type68_remove_post;
    case Type68TransitionKind::install:
        return window == CaptureWindow::type68_install_pre
               || window == CaptureWindow::type68_install_post;
    default:
        return false;
    }
}

bool valid_manager_snapshot(const ManagerSnapshot& snapshot) noexcept {
    if (!snapshot.valid || !snapshot.readiness_true || !snapshot.aligned_identity
        || !snapshot.exact_owner_current || snapshot.count > kManagerMaximumEntries) {
        return false;
    }
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        if (snapshot.entries[index].kind != kManagerEntryKind) {
            return false;
        }
    }
    return true;
}

bool valid_type68_manager_delta_event(const Type68ManagerDeltaEvent& event) noexcept {
    const CaptureWindow window = event.metadata.window;
    const bool managerWindow = window == CaptureWindow::type68_post_refresh
                               || window == CaptureWindow::type68_manager_add_post
                               || window == CaptureWindow::type68_alternate_builder_return
                               || window == CaptureWindow::type68_alternate_add_post
                               || window == CaptureWindow::type68_manager_materialize
                               || window == CaptureWindow::type68_manager_terminal;
    return managerWindow && valid_capture_metadata(event.metadata)
           && fully_correlated(event.context) && valid_manager_snapshot(event.before)
           && valid_manager_snapshot(event.after) && event.inside_exact_apply_call
           && ((window != CaptureWindow::type68_manager_add_post
                && window != CaptureWindow::type68_alternate_add_post)
               || (event.add_outcome_observed && event.event_hash != 0U));
}

bool default_log_fields(const DialogueApplyRecord& record,
                        DialogueDefaultLogFields& output) noexcept {
    if (!valid_raw_record(record) || !record.valid.native_resolver_succeeded
        || !record.valid.decoded_copy_valid || !record.valid.decoded_canonical) {
        return false;
    }
    DialogueRecordFields recordZero{};
    if (!dialogue_record_fields(record.resolver_body_pre, 0U, recordZero)) {
        return false;
    }
    output = DialogueDefaultLogFields{record.telemetry,
                                      project_default_context(record.entry_context),
                                      record.static_consumer,
                                      record.sequence,
                                      record.resolver_body_fingerprint,
                                      record.post_commit_cache_fingerprint,
                                      recordZero.generation,
                                      recordZero.mode,
                                      record.valid};
    return true;
}

bool default_log_fields(const DirectiveApplyRecord& record,
                        DirectiveDefaultLogFields& output) noexcept {
    if (!valid_raw_record(record) || !record.valid.native_resolver_succeeded
        || !record.valid.decoded_copy_valid || !record.valid.decoded_canonical) {
        return false;
    }
    DirectiveDefaultLogFields fields{};
    fields.telemetry = record.telemetry;
    fields.entry_context = project_default_context(record.entry_context);
    fields.static_consumer = record.static_consumer;
    fields.sequence = record.sequence;
    fields.resolver_body_fingerprint = record.resolver_body_fingerprint;
    fields.post_commit_cache_fingerprint = record.post_commit_cache_fingerprint;
    fields.valid = record.valid;
    for (std::size_t index = 0U; index < fields.entries.size(); ++index) {
        if (!directive_entry_fields(record.resolver_body_pre, index, fields.entries[index])) {
            return false;
        }
    }
    std::memcpy(&fields.selector,
                record.resolver_body_pre.data() + kDirectiveSelectorOffset,
                sizeof fields.selector);
    output = fields;
    return true;
}

} // namespace sunrise::client::hooks::bootflow::opening_authority
