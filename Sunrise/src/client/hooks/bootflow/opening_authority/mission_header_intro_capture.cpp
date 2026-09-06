#include "client/hooks/bootflow/opening_authority/mission_header_intro_capture.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winnt.h>
#endif

namespace sunrise::client::hooks::bootflow::opening_authority::mission_header_intro_capture {
namespace {

constexpr Digest256 digest(std::initializer_list<unsigned int> bytes) noexcept {
    Digest256 result{};
    std::size_t index{};
    for (const unsigned int value : bytes) {
        if (index < result.size()) {
            result[index++] = std::byte{static_cast<unsigned char>(value)};
        }
    }
    return result;
}

inline constexpr ArtifactDescriptor kPackedRuntime{
    ArtifactKind::pc_packed_runtime,
    L"D:\\Destiny3\\destiny2.exe",
    122'984'224U,
    digest({0x81, 0x96, 0x43, 0x80, 0x66, 0x4E, 0x7F, 0xCE,
            0xE3, 0xC6, 0x20, 0x08, 0x5A, 0x15, 0x7F, 0xDE,
            0xAF, 0x91, 0xFE, 0xFA, 0xCF, 0x72, 0x14, 0x90,
            0x78, 0x20, 0xF1, 0x88, 0xBB, 0xEB, 0x4C, 0xED})};
inline constexpr ArtifactDescriptor kUnpackedProvenance{
    ArtifactKind::pc_unpacked_provenance,
    L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe",
    145'091'072U,
    digest({0x87, 0x13, 0xD1, 0x5E, 0x3D, 0x05, 0xB2, 0x6F,
            0x9E, 0x25, 0x9E, 0x02, 0xB0, 0xF2, 0x9B, 0xC1,
            0xE0, 0x00, 0xE4, 0xB0, 0xC6, 0x2C, 0xA2, 0xCC,
            0x87, 0xC3, 0x85, 0x97, 0x18, 0x6C, 0xC3, 0xBD})};
inline constexpr ArtifactDescriptor kOmegaPackage{
    ArtifactKind::omega_activity_package,
    L"D:\\Destiny3\\packages\\w64_mercury_destination_activities_03a3_5.pkg",
    407'552U,
    digest({0x99, 0x77, 0xBA, 0xAA, 0xE8, 0x9B, 0xF0, 0x7A,
            0x81, 0x59, 0x02, 0xEC, 0x81, 0xEE, 0xCD, 0x28,
            0x9A, 0x7B, 0x1B, 0xA8, 0x1E, 0x7E, 0x32, 0x8C,
            0xD0, 0xF6, 0x7D, 0x1C, 0x25, 0x07, 0x5F, 0x17})};
inline constexpr ArtifactDescriptor kPs4Eboot{
    ArtifactKind::ps4_eboot_provenance,
    L"D:\\eboot-d2-0159.bin",
    37'824'851U,
    digest({0x52, 0x75, 0x86, 0xF1, 0x37, 0x66, 0xFA, 0xFC,
            0xC9, 0x15, 0x70, 0x59, 0x83, 0x1C, 0xEC, 0xC1,
            0xF8, 0x2A, 0xBF, 0xB5, 0x39, 0x9B, 0xB1, 0x14,
            0x04, 0x9B, 0xB9, 0xE1, 0x13, 0x39, 0xAA, 0x78})};

using Prefix = std::array<std::byte, kNativePrefixBytes>;
constexpr Prefix prefix(std::initializer_list<unsigned int> bytes) noexcept {
    Prefix result{};
    std::size_t index{};
    for (const unsigned int value : bytes) {
        if (index < result.size()) {
            result[index++] = std::byte{static_cast<unsigned char>(value)};
        }
    }
    return result;
}

#define PC_PREFIX(name, ...) inline constexpr Prefix name = prefix({__VA_ARGS__})
PC_PREFIX(kProducer, 0x48,0x89,0x5C,0x24,0x20,0x55,0x48,0x8B,0xEC,0x48,0x83,0xEC,0x70,0x33,0xC9,0xE8);
PC_PREFIX(kEnqueueAnchor, 0xE8,0x43,0x3F,0x03,0x00,0xBA,0x05,0x00,0x00,0x00,0x4C,0x8D,0x45,0xD0,0x48,0x8D);
PC_PREFIX(kQueueInsert, 0x48,0x8B,0xC4,0x57,0x48,0x83,0xEC,0x70,0x48,0x8B,0xF9,0x4C,0x63,0xCA,0x8B,0x89);
PC_PREFIX(kStateMachine, 0x48,0x89,0x70,0xD8,0x48,0x89,0x78,0xD0,0x4C,0x89,0x78,0xC0,0xE8,0x1B,0x6E,0x18);
PC_PREFIX(kStateGate, 0x83,0xFF,0x03,0x75,0x2B,0x80,0x3D,0xFC,0xD5,0xC3,0x01,0x00,0x74,0x22,0x80,0x3D);
PC_PREFIX(kProducerCallWindow, 0x80,0x3D,0xFC,0xD5,0xC3,0x01,0x00,0x74,0x22,0x80,0x3D,0xF4,0xD5,0xC3,0x01,0x00);
PC_PREFIX(kPendingClearWindow, 0xC6,0x05,0xD8,0xD5,0xC3,0x01,0x00,0x45,0x84,0xED,0x0F,0x85,0x22,0x02,0x00,0x00);
PC_PREFIX(kReset, 0x85,0xC9,0x75,0x13,0x88,0x0D,0x38,0xFD,0xC3,0x01,0x88,0x0D,0x33,0xFD,0xC3,0x01);
PC_PREFIX(kRearm, 0xC6,0x05,0xCC,0xEF,0xC3,0x01,0x01,0xC6,0x05,0xC6,0xEF,0xC3,0x01,0x00,0xC3,0x90);
PC_PREFIX(kReady, 0xC6,0x05,0x0D,0xE9,0xC3,0x01,0x01,0xC3,0x83,0xE2,0x03,0xE9,0x4A,0xA6,0x1F,0x04);
PC_PREFIX(kChannelClear, 0x48,0x83,0xEC,0x28,0xE8,0xE7,0x11,0xFD,0xFF,0x84,0xC0,0x74,0x37,0xE8,0x1E,0x19);
PC_PREFIX(kHudAccessor, 0x48,0x8D,0x05,0x09,0xD7,0xC5,0x01,0xC3,0x1C,0xBA,0xD6,0x95,0xF6,0x7F,0x00,0x00);
PC_PREFIX(kActivityCategory, 0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x0F,0xB7,0xDA,0xC7,0x01,0xFF);
PC_PREFIX(kPresentationCategory, 0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x83,0xFA,0x1A,0x0F,0x87,0xB3,0x00,0x00,0x00,0x48);
PC_PREFIX(kLocalizedPair, 0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x49);
PC_PREFIX(kQueueTick, 0x40,0x57,0x48,0x83,0xEC,0x40,0x83,0xB9,0x88,0x03,0x00,0x00,0xFF,0x48,0x8B,0xF9);
PC_PREFIX(kLazyResolver, 0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8D,0x4C,0x24,0x40,0xE8,0xDD);
PC_PREFIX(kRejectUnresolved, 0x0F,0x84,0x09,0x02,0x00,0x00,0x48,0x83,0xBF,0x80,0x03,0x00,0x00,0x10,0x0F,0x84);
PC_PREFIX(kRejectFull, 0x0F,0x84,0xFB,0x01,0x00,0x00,0x48,0x89,0x58,0x08,0x45,0x32,0xDB,0x83,0xBF,0x90);
PC_PREFIX(kAcceptCommit, 0x48,0x89,0x87,0x80,0x03,0x00,0x00,0x48,0x03,0xCE,0x74,0x16,0x66,0xC7,0x01,0x00);
PC_PREFIX(kCategoryWrapper, 0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8D,0x54,0x24,0x30,0x49,0x8B,0xD8,0xE8,0x6D);
PC_PREFIX(kCategoryGetter, 0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xDA,0xC6,0x02,0xFF,0xE8,0x6F,0x4C,0xFC);
PC_PREFIX(kTitleWrapper, 0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8D,0x54,0x24,0x30,0x49,0x8B,0xD8,0xE8,0x6D);
PC_PREFIX(kTitleGetter, 0x40,0x53,0x48,0x83,0xEC,0x20,0xC7,0x02,0xFF,0xFF,0xFF,0xFF,0x48,0x8B,0xDA,0xC7);
PC_PREFIX(kIconWrapper, 0x40,0x53,0x48,0x83,0xEC,0x30,0x48,0x8D,0x54,0x24,0x20,0x49,0x8B,0xD8,0xE8,0xDD);
PC_PREFIX(kIconGetter, 0x40,0x53,0x48,0x83,0xEC,0x30,0x48,0x8B,0xDA,0x48,0xC7,0x02,0xFF,0xFF,0xFF,0xFF);
PC_PREFIX(kVisualWrapper, 0x40,0x53,0x48,0x83,0xEC,0x30,0x48,0x8D,0x54,0x24,0x20,0x49,0x8B,0xD8,0xE8,0x2D);
PC_PREFIX(kVisualGetter, 0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0xC7,0x02,0xFF,0xFF,0xFF,0xFF,0x48,0x8B,0xDA);
PC_PREFIX(kReadyCaller, 0xE8,0x1A,0x28,0xE2,0xFF,0x8B,0x0D,0xC4,0x8B,0x1C,0x01,0x4C,0x8D,0x45,0x9F,0x48);
PC_PREFIX(kRearmCaller, 0xE8,0x00,0xF8,0xF6,0xFF,0xB0,0x01,0x48,0x83,0xC4,0x40,0x5B,0xC3,0x32,0xC0,0x48);
PC_PREFIX(kResetCaller, 0xE9,0x85,0xE9,0x6F,0x00,0xCC,0xA4,0xE1,0x45,0xA0,0x40,0x53,0x48,0x83,0xEC,0x20);
PC_PREFIX(kDirectCaller0, 0xE8,0x79,0xCA,0x6F,0x00,0x48,0x83,0xC4,0x28,0xC3,0x32,0xC0,0xF6,0xD8,0x1B,0xC9);
PC_PREFIX(kDirectCaller1, 0xE8,0x66,0xCA,0x6F,0x00,0x48,0x83,0xC4,0x28,0xC3,0xCC,0x8B,0xCA,0xE9,0x19,0x61);
#undef PC_PREFIX

struct SurfaceRow final {
    std::uintptr_t rva;
    const Prefix* prefix_bytes;
    BoundaryKind kind;
    InstrumentationMethod method;
    NativeAbi abi;
};

inline constexpr std::array<SurfaceRow, kNativeSurfaceCount> kSurfaceRows{{
    {kPcCommand5ProducerRva,&kProducer,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_noargs},
    {kPcCommand5EnqueueAnchorRva,&kEnqueueAnchor,BoundaryKind::callsite_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcQueueInsertRva,&kQueueInsert,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_queue_i32_payload},
    {kPcStateMachineRva,&kStateMachine,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcState3GateRva,&kStateGate,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcProducerCallWindowRva,&kProducerCallWindow,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcPendingClearRva,&kPendingClearWindow,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcResetDirectShowRva,&kReset,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_i32_mode},
    {kPcRearmRva,&kRearm,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_noargs},
    {kPcReadySetterRva,&kReady,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_noargs},
    {0x1355600U,&kChannelClear,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_noargs},
    {0x1353EF0U,&kHudAccessor,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::pointer_noargs},
    {0x1472BB0U,&kActivityCategory,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::category_resolver_internal},
    {0x13EB080U,&kPresentationCategory,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::presentation_category_internal},
    {0x13A6EC0U,&kLocalizedPair,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::localized_pair_internal},
    {kPcQueueTickRva,&kQueueTick,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_queue},
    {kPcLazyDispatchResolverRva,&kLazyResolver,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::void_queue},
    {kPcQueueRejectUnresolvedRva,&kRejectUnresolved,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcQueueRejectFullRva,&kRejectFull,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcQueueAcceptCommitRva,&kAcceptCommit,BoundaryKind::instruction_window,InstrumentationMethod::breakpoint_or_trampoline_with_register_frame,NativeAbi::instruction_frame_only},
    {kPcCategoryCuiWrapperRva,&kCategoryWrapper,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::opaque_wrapper_entry},
    {kPcCategoryCuiGetterRva,&kCategoryGetter,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::category_getter_context_out_u8},
    {kPcTitleCuiWrapperRva,&kTitleWrapper,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::opaque_wrapper_entry},
    {kPcTitleCuiGetterRva,&kTitleGetter,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::title_getter_context_out_pair},
    {kPcIconThemeCuiWrapperRva,&kIconWrapper,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::opaque_wrapper_entry},
    {kPcIconThemeCuiGetterRva,&kIconGetter,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::icon_getter_context_out_u64},
    {kPcVisualTupleCuiWrapperRva,&kVisualWrapper,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::opaque_wrapper_entry},
    {kPcVisualTupleCuiGetterRva,&kVisualGetter,BoundaryKind::function_entry,InstrumentationMethod::full_function_replacement,NativeAbi::visual_getter_context_out_16},
    {kPcReadyCallerRva,&kReadyCaller,BoundaryKind::callsite_window,InstrumentationMethod::caller_side_join,NativeAbi::instruction_frame_only},
    {kPcRearmCallerRva,&kRearmCaller,BoundaryKind::callsite_window,InstrumentationMethod::caller_side_join,NativeAbi::instruction_frame_only},
    {kPcResetCallerRva,&kResetCaller,BoundaryKind::callsite_window,InstrumentationMethod::caller_side_join,NativeAbi::instruction_frame_only},
    {kPcDirectShowCallerRvas[0],&kDirectCaller0,BoundaryKind::callsite_window,InstrumentationMethod::caller_side_join,NativeAbi::instruction_frame_only},
    {kPcDirectShowCallerRvas[1],&kDirectCaller1,BoundaryKind::callsite_window,InstrumentationMethod::caller_side_join,NativeAbi::instruction_frame_only},
}};

#define PS4_PREFIX(name, ...) inline constexpr Prefix name = prefix({__VA_ARGS__})
PS4_PREFIX(kPProducer,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x50,0x4C);
PS4_PREFIX(kPEnqueue,0x48,0x8D,0xB8,0x40,0x37,0x00,0x00,0x48,0x8D,0x55,0xA0,0xBE,0x05,0x00,0x00,0x00);
PS4_PREFIX(kPInsert,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC);
PS4_PREFIX(kPState,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC);
PS4_PREFIX(kPGate,0x41,0x83,0xFD,0x03,0x89,0x01,0x75,0x2F,0x48,0x8D,0x1D,0x94,0xA3,0x3F,0x04,0x8A);
PS4_PREFIX(kPReset,0x85,0xFF,0x74,0x05,0xE9,0x27,0x00,0x00,0x00,0x48,0x8D,0x05,0x54,0x99,0x3F,0x04);
PS4_PREFIX(kPRearm,0x48,0x8D,0x05,0x7E,0x99,0x3F,0x04,0x48,0x8D,0x0D,0x78,0x99,0x3F,0x04,0xC6,0x00);
PS4_PREFIX(kPReady,0x48,0x8D,0x05,0x2F,0x60,0x3F,0x04,0xC6,0x00,0x01,0xC3,0x90,0x90,0x90,0x90,0x90);
PS4_PREFIX(kPTick,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC);
PS4_PREFIX(kPCategory,0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0x89,0xD3,0xE8,0xA1,0x48,0xD0,0xFF,0x83);
PS4_PREFIX(kPTitle,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x50,0x48,0x89,0xD3,0xE8,0xCE,0x3D);
PS4_PREFIX(kPIcon,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x50,0x48,0x89,0xD3,0x49,0xBE,0x00);
PS4_PREFIX(kPVisual,0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x89,0xD3,0x49,0xBF);
#undef PS4_PREFIX

struct Ps4Row final { std::uintptr_t rva; std::uintptr_t file; const Prefix* bytes; };
inline constexpr std::array<Ps4Row,static_cast<std::size_t>(Ps4Surface::count)> kPs4Rows{{
    {0xFEB7D0U,0xFEF7D0U,&kPProducer},{0xFEB97AU,0xFEF97AU,&kPEnqueue},
    {0xFF0D70U,0xFF4D70U,&kPInsert},{0xFEAB70U,0xFEEB70U,&kPState},
    {0xFEAD62U,0xFEED62U,&kPGate},{0xFEB7A0U,0xFEF7A0U,&kPReset},
    {0xFEB780U,0xFEF780U,&kPRearm},{0xFEF0D0U,0xFF30D0U,&kPReady},
    {0xF1DCC0U,0xF21CC0U,&kPTick},{0xF19FF0U,0xF1DFF0U,&kPCategory},
    {0xF1AAC0U,0xF1EAC0U,&kPTitle},{0xF1C920U,0xF20920U,&kPIcon},
    {0xF1CA20U,0xF20A20U,&kPVisual},
}};

constexpr std::uint32_t rotr(std::uint32_t value, unsigned int count) noexcept {
    return (value >> count) | (value << (32U - count));
}

class Sha256State final {
public:
    void update(std::span<const std::byte> input) noexcept {
        for (const std::byte byte : input) {
            block_[block_bytes_++] = std::to_integer<std::uint8_t>(byte);
            ++total_bytes_;
            if (block_bytes_ == block_.size()) {
                transform();
                block_bytes_ = 0U;
            }
        }
    }

    Digest256 finish() noexcept {
        const std::uint64_t totalBits = total_bytes_ * 8U;
        block_[block_bytes_++] = 0x80U;
        if (block_bytes_ > 56U) {
            while (block_bytes_ < 64U) block_[block_bytes_++] = 0U;
            transform();
            block_bytes_ = 0U;
        }
        while (block_bytes_ < 56U) block_[block_bytes_++] = 0U;
        for (unsigned int index = 0U; index < 8U; ++index) {
            block_[63U - index] = static_cast<std::uint8_t>(totalBits >> (index * 8U));
        }
        transform();
        Digest256 result{};
        for (std::size_t index = 0U; index < state_.size(); ++index) {
            result[index * 4U] = std::byte{static_cast<std::uint8_t>(state_[index] >> 24U)};
            result[index * 4U + 1U] = std::byte{static_cast<std::uint8_t>(state_[index] >> 16U)};
            result[index * 4U + 2U] = std::byte{static_cast<std::uint8_t>(state_[index] >> 8U)};
            result[index * 4U + 3U] = std::byte{static_cast<std::uint8_t>(state_[index])};
        }
        return result;
    }

private:
    void transform() noexcept {
        static constexpr std::array<std::uint32_t,64U> k{
            0x428A2F98U,0x71374491U,0xB5C0FBCFU,0xE9B5DBA5U,0x3956C25BU,0x59F111F1U,0x923F82A4U,0xAB1C5ED5U,
            0xD807AA98U,0x12835B01U,0x243185BEU,0x550C7DC3U,0x72BE5D74U,0x80DEB1FEU,0x9BDC06A7U,0xC19BF174U,
            0xE49B69C1U,0xEFBE4786U,0x0FC19DC6U,0x240CA1CCU,0x2DE92C6FU,0x4A7484AAU,0x5CB0A9DCU,0x76F988DAU,
            0x983E5152U,0xA831C66DU,0xB00327C8U,0xBF597FC7U,0xC6E00BF3U,0xD5A79147U,0x06CA6351U,0x14292967U,
            0x27B70A85U,0x2E1B2138U,0x4D2C6DFCU,0x53380D13U,0x650A7354U,0x766A0ABBU,0x81C2C92EU,0x92722C85U,
            0xA2BFE8A1U,0xA81A664BU,0xC24B8B70U,0xC76C51A3U,0xD192E819U,0xD6990624U,0xF40E3585U,0x106AA070U,
            0x19A4C116U,0x1E376C08U,0x2748774CU,0x34B0BCB5U,0x391C0CB3U,0x4ED8AA4AU,0x5B9CCA4FU,0x682E6FF3U,
            0x748F82EEU,0x78A5636FU,0x84C87814U,0x8CC70208U,0x90BEFFFAU,0xA4506CEBU,0xBEF9A3F7U,0xC67178F2U};
        std::array<std::uint32_t,64U> words{};
        for (std::size_t i=0;i<16U;++i) {
            words[i]=(std::uint32_t{block_[i*4U]}<<24U)|(std::uint32_t{block_[i*4U+1U]}<<16U)|
                     (std::uint32_t{block_[i*4U+2U]}<<8U)|std::uint32_t{block_[i*4U+3U]};
        }
        for (std::size_t i=16U;i<64U;++i) {
            const std::uint32_t s0=rotr(words[i-15U],7U)^rotr(words[i-15U],18U)^(words[i-15U]>>3U);
            const std::uint32_t s1=rotr(words[i-2U],17U)^rotr(words[i-2U],19U)^(words[i-2U]>>10U);
            words[i]=words[i-16U]+s0+words[i-7U]+s1;
        }
        std::uint32_t a=state_[0],b=state_[1],c=state_[2],d=state_[3],e=state_[4],f=state_[5],g=state_[6],h=state_[7];
        for (std::size_t i=0;i<64U;++i) {
            const std::uint32_t s1=rotr(e,6U)^rotr(e,11U)^rotr(e,25U);
            const std::uint32_t ch=(e&f)^((~e)&g);
            const std::uint32_t temp1=h+s1+ch+k[i]+words[i];
            const std::uint32_t s0=rotr(a,2U)^rotr(a,13U)^rotr(a,22U);
            const std::uint32_t maj=(a&b)^(a&c)^(b&c);
            const std::uint32_t temp2=s0+maj;
            h=g;g=f;f=e;e=d+temp1;d=c;c=b;b=a;a=temp1+temp2;
        }
        state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
    }
    std::array<std::uint32_t,8U> state_{0x6A09E667U,0xBB67AE85U,0x3C6EF372U,0xA54FF53AU,0x510E527FU,0x9B05688CU,0x1F83D9ABU,0x5BE0CD19U};
    std::array<std::uint8_t,64U> block_{};
    std::size_t block_bytes_{};
    std::uint64_t total_bytes_{};
};

Digest256 hmac_sha256(const Digest256& key, std::span<const std::byte> bytes) noexcept {
    std::array<std::byte,64U> innerPad{};
    std::array<std::byte,64U> outerPad{};
    for (std::size_t i=0;i<64U;++i) {
        const std::byte keyByte=i<key.size()?key[i]:std::byte{};
        innerPad[i]=keyByte^std::byte{0x36U};
        outerPad[i]=keyByte^std::byte{0x5CU};
    }
    Sha256State inner;
    inner.update(innerPad);
    inner.update(bytes);
    const Digest256 innerDigest=inner.finish();
    Sha256State outer;
    outer.update(outerPad);
    outer.update(innerDigest);
    return outer.finish();
}

bool digest_zero(const Digest256& value) noexcept {
    return std::all_of(value.begin(),value.end(),[](std::byte b){return b==std::byte{};});
}

template <typename T>
void append_scalar(Sha256State& state,const T& value) noexcept {
    state.update(std::as_bytes(std::span{&value,std::size_t{1U}}));
}

bool safe_copy(void* destination,const void* source,std::size_t bytes) noexcept {
    if (destination==nullptr||source==nullptr||bytes==0U) return false;
#if defined(_WIN32) && defined(_MSC_VER)
    __try { std::memcpy(destination,source,bytes); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    std::memcpy(destination,source,bytes); return true;
#endif
}

template <typename T>
T read_scalar(const std::byte* bytes,std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value,bytes+offset,sizeof value);
    return value;
}

#if defined(_WIN32)
bool readable_region(const void* address,std::size_t bytes,bool requireExecutable) noexcept {
    const auto begin=reinterpret_cast<std::uintptr_t>(address);
    if (begin==0U||bytes==0U||begin>(std::numeric_limits<std::uintptr_t>::max)()-bytes) return false;
    std::uintptr_t cursor=begin;
    const std::uintptr_t end=begin+bytes;
    while (cursor<end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),&info,sizeof info)!=sizeof info) return false;
        if (info.State!=MEM_COMMIT||(info.Protect&PAGE_GUARD)!=0U||(info.Protect&PAGE_NOACCESS)!=0U) return false;
        if (requireExecutable) {
            const DWORD baseProtection=info.Protect&0xFFU;
            if (baseProtection!=PAGE_EXECUTE&&baseProtection!=PAGE_EXECUTE_READ&&
                baseProtection!=PAGE_EXECUTE_READWRITE&&baseProtection!=PAGE_EXECUTE_WRITECOPY) return false;
        }
        const auto regionEnd=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
        if (regionEnd<=cursor) return false;
        cursor=std::min(regionEnd,end);
    }
    return true;
}

bool random_secret(Digest256& output) noexcept {
    using BCryptGenRandomFn=LONG (WINAPI*)(void*,unsigned char*,unsigned long,unsigned long);
    HMODULE module=LoadLibraryW(L"bcrypt.dll");
    if (module==nullptr) return false;
    const auto function=reinterpret_cast<BCryptGenRandomFn>(GetProcAddress(module,"BCryptGenRandom"));
    const bool okay=function!=nullptr&&function(nullptr,reinterpret_cast<unsigned char*>(output.data()),
                                                static_cast<unsigned long>(output.size()),0x00000002UL)>=0;
    FreeLibrary(module);
    return okay&&!digest_zero(output);
}
#else
bool readable_region(const void*,std::size_t,bool) noexcept { return false; }
bool random_secret(Digest256&) noexcept { return false; }
#endif

bool read_file_at(const ArtifactDescriptor& artifact,std::uint64_t offset,std::span<std::byte> output) noexcept {
#if defined(_WIN32)
    const std::wstring path{artifact.path};
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER position{}; position.QuadPart=static_cast<LONGLONG>(offset);
    bool okay=SetFilePointerEx(file,position,nullptr,FILE_BEGIN)!=FALSE;
    DWORD read{};
    if (okay) okay=ReadFile(file,output.data(),static_cast<DWORD>(output.size()),&read,nullptr)!=FALSE&&read==output.size();
    CloseHandle(file);
    return okay;
#else
    (void)artifact;(void)offset;(void)output;return false;
#endif
}

bool exact_file(const ArtifactDescriptor& descriptor) noexcept {
    std::uint64_t bytes{}; Digest256 actual{};
    return sha256_file(descriptor.path,bytes,actual)&&bytes==descriptor.file_bytes&&actual==descriptor.sha256;
}

bool exact_pe_header_from_bytes(std::span<const std::byte> bytes) noexcept {
#if defined(_WIN32)
    if (bytes.size()<sizeof(IMAGE_DOS_HEADER)) return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0) return false;
    const std::size_t ntOffset=static_cast<std::size_t>(dos->e_lfanew);
    if (ntOffset>bytes.size()||sizeof(IMAGE_NT_HEADERS64)>bytes.size()-ntOffset) return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+ntOffset);
    return nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.Machine==kPinnedPeMachine&&
           nt->FileHeader.NumberOfSections==kPinnedPeSectionCount&&nt->FileHeader.TimeDateStamp==kPinnedPeTimestamp&&
           nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC&&nt->OptionalHeader.ImageBase==kPinnedPeImageBase&&
           nt->OptionalHeader.SizeOfImage==kPinnedMappedSizeOfImage&&nt->OptionalHeader.AddressOfEntryPoint==kPinnedPeEntryRva;
#else
    (void)bytes; return false;
#endif
}

struct MappedInspection final {
    AdmissionResult result{AdmissionResult::invalid_arguments};
    Digest256 cohort{};
};

MappedInspection inspect_mapped(const void* mappedBase,std::uint32_t mappedBytes,bool requireMain) noexcept {
    MappedInspection inspection{};
#if defined(_WIN32)
    if (mappedBase==nullptr||mappedBytes!=kPinnedMappedSizeOfImage) { inspection.result=AdmissionResult::mapped_bounds_mismatch; return inspection; }
    if (requireMain&&mappedBase!=GetModuleHandleW(nullptr)) { inspection.result=AdmissionResult::mapped_base_mismatch; return inspection; }
    if (!readable_region(mappedBase,0x600U,false)) { inspection.result=AdmissionResult::page_not_committed; return inspection; }
    std::array<std::byte,0x600U> headers{};
    if (!safe_copy(headers.data(),mappedBase,headers.size())||!exact_pe_header_from_bytes(headers)) {
        inspection.result=AdmissionResult::mapped_pe_mismatch; return inspection;
    }
    Sha256State cohort;
    const auto base=reinterpret_cast<std::uintptr_t>(mappedBase);
    for (std::size_t i=0;i<kSurfaceRows.size();++i) {
        const NativeBoundaryDescriptor descriptor=native_boundary(static_cast<NativeSurface>(i));
        if (descriptor.rva>mappedBytes||descriptor.prefix.size()>mappedBytes-descriptor.rva) {
            inspection.result=AdmissionResult::mapped_bounds_mismatch; return inspection;
        }
        const void* address=reinterpret_cast<const void*>(base+descriptor.rva);
        if (!readable_region(address,descriptor.prefix.size(),true)) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(address,&info,sizeof info)!=sizeof info||info.State!=MEM_COMMIT)
                inspection.result=AdmissionResult::page_not_committed;
            else inspection.result=AdmissionResult::page_not_executable;
            return inspection;
        }
        Prefix observed{};
        if (!safe_copy(observed.data(),address,observed.size())||!native_prefix_matches(static_cast<NativeSurface>(i),observed)) {
            inspection.result=AdmissionResult::post_decryption_not_ready; return inspection;
        }
        append_scalar(cohort,descriptor.rva);
        cohort.update(observed);
    }
    inspection.cohort=cohort.finish();
    inspection.result=AdmissionResult::admitted;
#else
    (void)mappedBase;(void)mappedBytes;(void)requireMain;
#endif
    return inspection;
}

Digest256 token_material(const AdmissionToken& token) noexcept {
    Sha256State state;
    append_scalar(state,token.kind()); append_scalar(state,token.owner_id());
    append_scalar(state,token.module_generation()); append_scalar(state,token.capture_epoch());
    append_scalar(state,token.mapped_base()); append_scalar(state,token.mapped_size());
    state.update(token.build_digest()); state.update(token.package_digest()); state.update(token.surface_cohort_digest());
    return state.finish();
}

std::uint64_t digest_u64(const Digest256& digestValue) noexcept {
    return read_scalar<std::uint64_t>(digestValue.data(),0U);
}

std::uint64_t monotonic_tick() noexcept {
#if defined(_WIN32)
    LARGE_INTEGER value{}; QueryPerformanceCounter(&value); return static_cast<std::uint64_t>(value.QuadPart);
#else
    return 0U;
#endif
}
std::uint64_t monotonic_frequency() noexcept {
#if defined(_WIN32)
    LARGE_INTEGER value{}; QueryPerformanceFrequency(&value); return static_cast<std::uint64_t>(value.QuadPart);
#else
    return 0U;
#endif
}
std::uint32_t current_thread_id() noexcept {
#if defined(_WIN32)
    return GetCurrentThreadId();
#else
    return 1U;
#endif
}

struct CallFrame final {
    CaptureOwner* owner{};
    NativeSurface surface{NativeSurface::command5_producer};
    GenerationSnapshot entry{};
    std::uint64_t call_id{};
    std::uint64_t parent_call_id{};
    std::uint64_t entry_tick{};
    std::uint32_t emitted{};
    std::uint32_t lost{};
    ProducerStage last_producer_stage{ProducerStage::entry};
    ProducerStageFacts producer_facts{};
    PayloadProjection producer_payload{};
    QueueProjection queue_pre{};
    PayloadProjection queue_argument_payload{};
    QueueDecisionMarker queue_marker{QueueDecisionMarker::none};
    std::uintptr_t inserted_record_identity{};
    QueueProjection tick_pre{};
    bool producer_started{};
    bool producer_payload_valid{};
    bool queue_pre_valid{};
    bool tick_pre_valid{};
};
inline constexpr std::size_t kTlsDepth=16U;
thread_local std::array<CallFrame,kTlsDepth> g_frames{};
thread_local std::size_t g_frame_depth{};

CallFrame* current_frame(CaptureOwner* owner) noexcept {
    for (std::size_t i=g_frame_depth;i>0U;--i) if (g_frames[i-1U].owner==owner) return &g_frames[i-1U];
    return nullptr;
}
CallFrame* nearest_surface_frame(CaptureOwner* owner,NativeSurface surface) noexcept {
    for (std::size_t i=g_frame_depth;i>0U;--i) if (g_frames[i-1U].owner==owner&&g_frames[i-1U].surface==surface) return &g_frames[i-1U];
    return nullptr;
}

template <typename Record>
Digest256 seal_record(const Digest256& key,Record record) noexcept {
    record.integrity_seal={};
    return hmac_sha256(key,std::as_bytes(std::span{&record,std::size_t{1U}}));
}

template <typename Record,std::size_t Capacity>
bool enqueue_record(FixedRecordQueue<Record,Capacity>& queue,Record& record,CallFrame* frame,const Digest256& key) noexcept {
    record.integrity_seal=seal_record(key,record);
    const QueuePushResult result=queue.push_canonical(record);
    if (frame!=nullptr) {
        if (result==QueuePushResult::enqueued) ++frame->emitted; else ++frame->lost;
    }
    return result==QueuePushResult::enqueued;
}

bool strict_producer_transition(CallFrame& frame,ProducerStage stage,ProducerDisposition disposition,const ProducerStageFacts& facts) noexcept {
    if (!frame.producer_started) {
        if (stage!=ProducerStage::entry||disposition!=ProducerDisposition::in_progress) return false;
        frame.producer_started=true; frame.last_producer_stage=stage; frame.producer_facts=facts; return true;
    }
    if (stage==ProducerStage::returned) {
        switch (disposition) {
        case ProducerDisposition::early_null_activity_manager: if (frame.last_producer_stage!=ProducerStage::entry) return false; break;
        case ProducerDisposition::early_investment_not_ready: if (frame.last_producer_stage!=ProducerStage::activity_manager||frame.producer_facts.activity_manager_identity==0U) return false; break;
        case ProducerDisposition::early_invalid_activity_index: if (frame.last_producer_stage!=ProducerStage::investment_ready||!frame.producer_facts.investment_ready) return false; break;
        case ProducerDisposition::early_profile_or_activity_ineligible: if (frame.last_producer_stage!=ProducerStage::activity_index||frame.producer_facts.activity_index!=kActivityIndex) return false; break;
        case ProducerDisposition::early_missing_client_activity_row: if (frame.last_producer_stage!=ProducerStage::eligibility||!frame.producer_facts.eligible||frame.producer_facts.activity_manager_identity==0U||!frame.producer_facts.investment_ready) return false; break;
        case ProducerDisposition::early_missing_display_row: if (frame.last_producer_stage!=ProducerStage::client_activity_row||frame.producer_facts.client_activity_row_identity==0U||!frame.producer_facts.eligible) return false; break;
        case ProducerDisposition::early_other_eligibility: if (frame.last_producer_stage<ProducerStage::activity_index||frame.last_producer_stage>=ProducerStage::queue_insert) return false; break;
        case ProducerDisposition::queue_attempted:
            if (frame.last_producer_stage!=ProducerStage::queue_insert||!frame.producer_payload_valid) return false; break;
        default: return false;
        }
        frame.last_producer_stage=stage; frame.producer_facts=facts; return true;
    }
    if (disposition!=ProducerDisposition::in_progress||static_cast<unsigned int>(stage)!=static_cast<unsigned int>(frame.last_producer_stage)+1U) return false;
    const ProducerStageFacts& previous=frame.producer_facts;
    switch (stage) {
    case ProducerStage::activity_manager: if (facts.activity_manager_identity==0U) return false; break;
    case ProducerStage::investment_ready: if (previous.activity_manager_identity==0U||!facts.investment_ready) return false; break;
    case ProducerStage::activity_index: if (!previous.investment_ready||facts.activity_index!=kActivityIndex||facts.definition_hash!=kActivityDefinitionHash) return false; break;
    case ProducerStage::eligibility: if (previous.activity_index!=kActivityIndex||!facts.eligible) return false; break;
    case ProducerStage::client_activity_row: if (!previous.eligible||facts.client_activity_row_identity==0U) return false; break;
    case ProducerStage::display_row: if (previous.client_activity_row_identity==0U||facts.display_row_identity==0U) return false; break;
    case ProducerStage::category: if (previous.display_row_identity==0U||facts.activity_type!=kStoryActivityType||facts.category_source!=kStoryTypeClientCategorySource||facts.presentation_category!=kActivityPresentationCategory) return false; break;
    case ProducerStage::title: if (previous.presentation_category!=kActivityPresentationCategory||facts.title!=kOmegaTitle) return false; break;
    case ProducerStage::payload_anchor: if (!frame.producer_payload_valid) return false; break;
    case ProducerStage::queue_insert: if (!frame.producer_payload_valid) return false; break;
    default: break;
    }
    frame.last_producer_stage=stage; frame.producer_facts=facts; return true;
}

CuiRegistrationIdentity registration_for(CuiValueKind kind) noexcept {
    switch(kind){case CuiValueKind::category:return kCategoryCuiRegistration;case CuiValueKind::title:return kTitleCuiRegistration;case CuiValueKind::icon_theme:return kIconThemeCuiRegistration;case CuiValueKind::visual_tuple:return kVisualTupleCuiRegistration;} return {};
}

} // namespace

Digest256 sha256_bytes(std::span<const std::byte> bytes) noexcept { Sha256State state; state.update(bytes); return state.finish(); }

bool sha256_file(std::wstring_view path,std::uint64_t& fileBytes,Digest256& output) noexcept {
    fileBytes=0U; output={};
#if defined(_WIN32)
    const std::wstring pathString{path};
    HANDLE file=CreateFileW(pathString.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart<0){CloseHandle(file);return false;}
    Sha256State state; std::array<std::byte,64U*1024U> buffer{};
    for(;;){DWORD read{};if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr)){CloseHandle(file);return false;}if(read==0U)break;state.update(std::span<const std::byte>{buffer.data(),read});}
    CloseHandle(file); fileBytes=static_cast<std::uint64_t>(size.QuadPart); output=state.finish(); return true;
#else
    (void)path; return false;
#endif
}

const ArtifactDescriptor& artifact_descriptor(ArtifactKind kind) noexcept {
    switch(kind){case ArtifactKind::pc_packed_runtime:return kPackedRuntime;case ArtifactKind::pc_unpacked_provenance:return kUnpackedProvenance;case ArtifactKind::omega_activity_package:return kOmegaPackage;case ArtifactKind::ps4_eboot_provenance:return kPs4Eboot;}return kPackedRuntime;
}

NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept {
    const std::size_t i=static_cast<std::size_t>(surface);if(i>=kSurfaceRows.size())return{};
    const SurfaceRow& row=kSurfaceRows[i];const bool window=row.kind!=BoundaryKind::function_entry;
    return {row.rva,*row.prefix_bytes,row.kind,row.method,row.abi,window?0xFFFFFFFFU:0U,window,true,false,false};
}
bool native_prefix_matches(NativeSurface surface,std::span<const std::byte> observed) noexcept {
    const auto descriptor=native_boundary(surface);return descriptor.prefix.size()==kNativePrefixBytes&&observed.size()>=descriptor.prefix.size()&&std::equal(descriptor.prefix.begin(),descriptor.prefix.end(),observed.begin());
}
Ps4BoundaryDescriptor ps4_boundary(Ps4Surface surface) noexcept {const std::size_t i=static_cast<std::size_t>(surface);if(i>=kPs4Rows.size())return{};const auto& row=kPs4Rows[i];return{row.rva,row.file,*row.bytes,true,false};}
bool ps4_prefix_matches(Ps4Surface surface,std::span<const std::byte> observed) noexcept {const auto descriptor=ps4_boundary(surface);return observed.size()>=descriptor.prefix.size()&&std::equal(descriptor.prefix.begin(),descriptor.prefix.end(),observed.begin());}

ReferenceAuditResult audit_pinned_reference_artifacts() noexcept {
    if(!exact_file(kPackedRuntime))return ReferenceAuditResult::packed_runtime_mismatch;
    if(!exact_file(kUnpackedProvenance))return ReferenceAuditResult::unpacked_runtime_mismatch;
    if(!exact_file(kOmegaPackage))return ReferenceAuditResult::package_mismatch;
    if(!exact_file(kPs4Eboot))return ReferenceAuditResult::ps4_mismatch;
    std::array<std::byte,0x600U> headers{};if(!read_file_at(kPackedRuntime,0U,headers)||!exact_pe_header_from_bytes(headers))return ReferenceAuditResult::pe_header_mismatch;
    for(std::size_t i=0;i<kSurfaceRows.size();++i){Prefix observed{};const auto descriptor=native_boundary(static_cast<NativeSurface>(i));if(!read_file_at(kUnpackedProvenance,descriptor.rva,observed))return ReferenceAuditResult::io_failure;if(!native_prefix_matches(static_cast<NativeSurface>(i),observed))return ReferenceAuditResult::pc_prefix_mismatch;}
    for(std::size_t i=0;i<kPs4Rows.size();++i){Prefix observed{};const auto descriptor=ps4_boundary(static_cast<Ps4Surface>(i));if(!read_file_at(kPs4Eboot,descriptor.file_offset,observed))return ReferenceAuditResult::io_failure;if(!ps4_prefix_matches(static_cast<Ps4Surface>(i),observed))return ReferenceAuditResult::ps4_prefix_mismatch;}
    return ReferenceAuditResult::exact;
}

AdmissionOwner::AdmissionOwner() noexcept {entropy_ready_=random_secret(secret_);owner_id_=entropy_ready_?digest_u64(sha256_bytes(secret_)):0U;}
Digest256 AdmissionOwner::seal_token(const AdmissionToken& token) const noexcept {const Digest256 material=token_material(token);return hmac_sha256(secret_,material);}
bool AdmissionOwner::verifies(const AdmissionToken& token) const noexcept {return entropy_ready_&&token.kind_!=AdmissionKind::invalid&&token.owner_id_==owner_id_&&token.module_generation_==module_generation_.load(std::memory_order_acquire)&&token.seal_==seal_token(token);}
void AdmissionOwner::invalidate() noexcept {module_generation_.fetch_add(1U,std::memory_order_acq_rel);}

AdmissionOutcome AdmissionOwner::admit_mapped(AdmissionKind kind,const void* mappedBase,std::uint32_t mappedBytes,bool requireMainModule) noexcept {
    AdmissionOutcome outcome{};if(!entropy_ready_){outcome.result=AdmissionResult::owner_entropy_unavailable;return outcome;}
    if(!exact_file(kPackedRuntime)){outcome.result=AdmissionResult::packed_runtime_mismatch;return outcome;}
    if(!exact_file(kOmegaPackage)){outcome.result=AdmissionResult::package_mismatch;return outcome;}
    const MappedInspection inspection=inspect_mapped(mappedBase,mappedBytes,requireMainModule);if(inspection.result!=AdmissionResult::admitted){outcome.result=inspection.result;return outcome;}
    AdmissionToken token{};token.kind_=kind;token.owner_id_=owner_id_;token.module_generation_=module_generation_.load(std::memory_order_acquire);token.capture_epoch_=next_capture_epoch_.fetch_add(1U,std::memory_order_acq_rel);token.mapped_base_=reinterpret_cast<std::uintptr_t>(mappedBase);token.mapped_size_=mappedBytes;token.build_digest_=kPackedRuntime.sha256;token.package_digest_=kOmegaPackage.sha256;token.surface_cohort_digest_=inspection.cohort;token.seal_=seal_token(token);outcome.result=AdmissionResult::admitted;outcome.token=token;return outcome;
}
AdmissionOutcome AdmissionOwner::admit_live() noexcept {
#if defined(_WIN32)
    HMODULE module=GetModuleHandleW(nullptr);if(module==nullptr){return{AdmissionResult::main_module_unavailable,{}};}
    std::array<wchar_t,32768U> path{};const DWORD chars=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));if(chars==0U||chars>=path.size())return{AdmissionResult::main_module_unavailable,{}};
    std::uint64_t bytes{};Digest256 actual{};if(!sha256_file(std::wstring_view{path.data(),chars},bytes,actual)||bytes!=kPackedRuntime.file_bytes||actual!=kPackedRuntime.sha256)return{AdmissionResult::main_module_path_mismatch,{}};
    return admit_mapped(AdmissionKind::live_main_module,module,kPinnedMappedSizeOfImage,true);
#else
    return{AdmissionResult::main_module_unavailable,{}};
#endif
}
#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
AdmissionOutcome AdmissionTestAccess::admit_protected_image(AdmissionOwner& owner,const void* base,std::uint32_t bytes) noexcept{return owner.admit_mapped(AdmissionKind::protected_test_image,base,bytes,false);}
#endif

bool valid_exact_generations(const GenerationSnapshot& value) noexcept {
    const auto valid=[](OwnerGeneration generation){return generation.owner_cookie!=0U&&generation.generation!=0U;};
    return valid(value.session)&&valid(value.activity)&&valid(value.world)&&valid(value.profile)&&valid(value.queue)&&valid(value.latch)&&value.activity_index==kActivityIndex&&value.activity_definition_hash==kActivityDefinitionHash&&value.activity_package_tag==kActivityPackageTag;
}

BoundedCaptureResult capture_command5_payload(const void* payload,PayloadProjection& output) noexcept {
    output={};if(payload==nullptr)return BoundedCaptureResult::null_pointer;std::array<std::byte,kCommand5PayloadBytes> copy{};if(!readable_region(payload,copy.size(),false)||!safe_copy(copy.data(),payload,copy.size()))return BoundedCaptureResult::unreadable;
    PayloadProjection captured{};captured.title.bank=read_scalar<std::uint32_t>(copy.data(),0U);captured.title.hash=read_scalar<std::uint32_t>(copy.data(),4U);captured.presentation_category=read_scalar<std::uint8_t>(copy.data(),8U);captured.icon_theme_dword=read_scalar<std::uint32_t>(copy.data(),0x0CU);captured.byte_count=static_cast<std::uint32_t>(copy.size());captured.visual_tuple_digest=sha256_bytes(std::span<const std::byte>{copy.data()+0x10U,0x10U});captured.full_payload_digest=sha256_bytes(copy);captured.exact_omega_identity=captured.title==kOmegaTitle&&captured.presentation_category==kActivityPresentationCategory;output=captured;return captured.exact_omega_identity?BoundedCaptureResult::complete:BoundedCaptureResult::identity_mismatch;
}

BoundedCaptureResult capture_queue_projection(const void* queue,QueueProjection& output) noexcept {
    output={};if(queue==nullptr)return BoundedCaptureResult::null_pointer;std::array<std::byte,kQueueProjectionBytes> copy{};if(!readable_region(queue,copy.size(),false)||!safe_copy(copy.data(),queue,copy.size()))return BoundedCaptureResult::unreadable;
    QueueProjection captured{};captured.queued_count=read_scalar<std::uint64_t>(copy.data(),kQueueCountOffset);captured.dispatch_definition_handle=read_scalar<std::int32_t>(copy.data(),kQueueDispatchHandleOffset);captured.current_sequence=read_scalar<std::int32_t>(copy.data(),kQueueCurrentSequenceOffset);captured.current_retire_or_supersede=read_scalar<std::uint8_t>(copy.data(),kQueueCurrentRetireOffset);captured.current_active=read_scalar<std::uint8_t>(copy.data(),kQueueCurrentActiveOffset);captured.current_command=read_scalar<std::uint16_t>(copy.data(),kQueueCurrentCommandOffset);captured.next_sequence=read_scalar<std::int32_t>(copy.data(),kQueueNextSequenceOffset);captured.authored_priority=read_scalar<std::uint32_t>(copy.data(),kQueueAuthoredCommandTableOffset+static_cast<std::size_t>(kActivityIntroCommand)*4U);captured.byte_count=static_cast<std::uint32_t>(copy.size());captured.exact_queue_digest=sha256_bytes(copy);
    if(captured.queued_count>kQueueCapacity||captured.current_retire_or_supersede>1U||captured.current_active>1U||captured.current_sequence< -1||captured.next_sequence<0){output=captured;return BoundedCaptureResult::invalid_arguments;}
    if(captured.current_active!=0U&&captured.current_command==kActivityIntroCommand){captured.current_payload_valid=capture_command5_payload(copy.data()+kQueueCurrentPayloadOffset,captured.current_payload)==BoundedCaptureResult::complete;}
    output=captured;return BoundedCaptureResult::complete;
}

LatchSnapshot capture_latch_scalars(std::int32_t lifecycle,std::uint8_t arm,std::uint8_t pending,std::uint8_t ready,std::uint32_t adjacent) noexcept {LatchSnapshot value{};value.lifecycle_state=lifecycle;value.arm=arm;value.pending=pending;value.ready=ready;value.adjacent_local_presentation_state=adjacent;Sha256State state;append_scalar(state,lifecycle);append_scalar(state,arm);append_scalar(state,pending);append_scalar(state,ready);append_scalar(state,adjacent);value.digest=state.finish();return value;}
bool valid_latch_snapshot(const LatchSnapshot& value) noexcept {return value.arm<=1U&&value.pending<=1U&&value.ready<=1U&&value==capture_latch_scalars(value.lifecycle_state,value.arm,value.pending,value.ready,value.adjacent_local_presentation_state);}
StateMachineExpectation expected_state_machine_step(const LatchSnapshot& pre,ProducerAttemptOutcome outcome) noexcept {StateMachineExpectation result{};result.attempt_outcome=outcome;if(!valid_latch_snapshot(pre))return result;auto post=pre;const std::uint32_t plus=static_cast<std::uint32_t>(pre.lifecycle_state)+1U;if(post.arm==0U&&plus>1U){post.arm=1U;post.pending=1U;post.ready=0U;result.armed=true;}result.attempted=post.lifecycle_state==3&&post.pending!=0U&&post.ready!=0U;if(result.attempted){if(outcome==ProducerAttemptOutcome::not_attempted){result.post=capture_latch_scalars(post.lifecycle_state,post.arm,post.pending,post.ready,post.adjacent_local_presentation_state);return result;}post.pending=0U;post.adjacent_local_presentation_state=0U;result.pending_cleared_after_attempt=true;}else if(outcome!=ProducerAttemptOutcome::not_attempted){result.post=capture_latch_scalars(post.lifecycle_state,post.arm,post.pending,post.ready,post.adjacent_local_presentation_state);return result;}result.post=capture_latch_scalars(post.lifecycle_state,post.arm,post.pending,post.ready,post.adjacent_local_presentation_state);result.valid_input=true;return result;}
LatchSnapshot expected_reset_post(const LatchSnapshot& pre) noexcept{return capture_latch_scalars(pre.lifecycle_state,0U,0U,0U,pre.adjacent_local_presentation_state);}
LatchSnapshot expected_rearm_post(const LatchSnapshot& pre) noexcept{return capture_latch_scalars(pre.lifecycle_state,pre.arm,1U,0U,pre.adjacent_local_presentation_state);}
LatchSnapshot expected_ready_post(const LatchSnapshot& pre) noexcept{return capture_latch_scalars(pre.lifecycle_state,pre.arm,pre.pending,1U,pre.adjacent_local_presentation_state);}

QueueInsertOutcome classify_queue_insert(const QueueProjection& pre,const QueueProjection& post,QueueDecisionMarker marker,std::uintptr_t recordIdentity) noexcept {
    if(pre.byte_count!=kQueueProjectionBytes||post.byte_count!=kQueueProjectionBytes||digest_zero(pre.exact_queue_digest)||digest_zero(post.exact_queue_digest))return QueueInsertOutcome::incoherent;
    switch(marker){case QueueDecisionMarker::reject_unresolved_at_131A64A:return pre.dispatch_definition_handle==-1&&pre==post?QueueInsertOutcome::rejected_definition_unresolved:QueueInsertOutcome::incoherent;case QueueDecisionMarker::reject_full_at_131A658:return pre.dispatch_definition_handle!=-1&&pre.queued_count==kQueueCapacity&&pre==post?QueueInsertOutcome::rejected_full:QueueInsertOutcome::incoherent;case QueueDecisionMarker::accepted_commit_at_131A7CE:return pre.dispatch_definition_handle!=-1&&pre.queued_count<kQueueCapacity&&post.queued_count==pre.queued_count+1U&&pre.exact_queue_digest!=post.exact_queue_digest&&recordIdentity!=0U?QueueInsertOutcome::accepted:QueueInsertOutcome::incoherent;case QueueDecisionMarker::none:return QueueInsertOutcome::raw_partial_no_decision_marker;}return QueueInsertOutcome::incoherent;
}

AtomicEpochIngress::Scope::Scope(AtomicEpochIngress& ingress) noexcept:ingress_(ingress){std::uint64_t state=ingress_.state_.load(std::memory_order_acquire);while((state&kClosedBit)==0U){if((state&~kClosedBit)==(kClosedBit-1U))break;if(ingress_.state_.compare_exchange_weak(state,state+1U,std::memory_order_acq_rel,std::memory_order_acquire)){admitted_=true;break;}}}
AtomicEpochIngress::Scope::~Scope(){if(admitted_)ingress_.state_.fetch_sub(1U,std::memory_order_release);}
void AtomicEpochIngress::open() noexcept{std::uint64_t expected=kClosedBit;state_.compare_exchange_strong(expected,0U,std::memory_order_acq_rel,std::memory_order_acquire);}
void AtomicEpochIngress::close() noexcept{state_.fetch_or(kClosedBit,std::memory_order_acq_rel);}
bool AtomicEpochIngress::idle() const noexcept{return(state_.load(std::memory_order_acquire)&~kClosedBit)==0U;}
bool AtomicEpochIngress::open_for_admission() const noexcept{return(state_.load(std::memory_order_acquire)&kClosedBit)==0U;}
std::uint32_t AtomicEpochIngress::active_calls() const noexcept{return static_cast<std::uint32_t>(state_.load(std::memory_order_acquire)&~kClosedBit);}

CaptureOwner::CaptureOwner(AdmissionOwner& admission,AdmissionToken token,GenerationSource generations) noexcept:admission_(admission),token_(token),generations_(generations){}
bool CaptureOwner::activate() noexcept {if(!admission_.verifies(token_)||generations_.read==nullptr)return false;if(token_.kind()!=AdmissionKind::live_main_module&&!allow_test_token_)return false;LifecyclePhase expected=LifecyclePhase::detached;if(!phase_.compare_exchange_strong(expected,LifecyclePhase::active,std::memory_order_acq_rel,std::memory_order_acquire))return false;lifecycle_epoch_.fetch_add(1U,std::memory_order_acq_rel);observation_ingress_.open();full_call_gate_.accept();return true;}
void CaptureOwner::begin_quiesce() noexcept{LifecyclePhase expected=LifecyclePhase::active;if(phase_.compare_exchange_strong(expected,LifecyclePhase::quiescing,std::memory_order_acq_rel,std::memory_order_acquire)){observation_ingress_.close();full_call_gate_.quiesce();}}
ProtectedDetachDisposition CaptureOwner::protected_detach(ProtectedDetachFunction detach,void* context) noexcept {begin_quiesce();if(detach==nullptr)return ProtectedDetachDisposition::failed;if(!observation_ingress_.idle()||!full_call_gate_.idle())return ProtectedDetachDisposition::deferred;const auto result=detach(context);if(result==ProtectedDetachDisposition::removed){admission_.invalidate();phase_.store(LifecyclePhase::detached,std::memory_order_release);}else if(result==ProtectedDetachDisposition::failed){phase_.store(LifecyclePhase::retained_failure,std::memory_order_release);}return result;}
LifecycleSnapshot CaptureOwner::lifecycle_snapshot() const noexcept{return{phase_.load(std::memory_order_acquire),lifecycle_epoch_.load(std::memory_order_acquire),full_call_gate_.active_calls(),observation_ingress_.active_calls(),observation_ingress_.open_for_admission(),phase_.load(std::memory_order_acquire)!=LifecyclePhase::detached};}
#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
void CaptureTestAccess::allow_test_admission(CaptureOwner& owner) noexcept{owner.allow_test_token_=true;}
#endif

bool CaptureOwner::begin_call(NativeSurface surface) noexcept {if(!admission_.verifies(token_)||g_frame_depth>=kTlsDepth)return false;GenerationSnapshot entry{};const bool read=generations_.read(generations_.context,entry);CallFrame frame{};frame.owner=this;frame.surface=surface;frame.entry=entry;frame.call_id=next_call_id_.fetch_add(1U,std::memory_order_acq_rel)+1U;frame.entry_tick=monotonic_tick();for(std::size_t i=g_frame_depth;i>0U;--i)if(g_frames[i-1U].owner==this){frame.parent_call_id=g_frames[i-1U].call_id;break;}if(!read)frame.entry={};g_frames[g_frame_depth++]=frame;return true;}
void CaptureOwner::end_call(ObserverFault fault,bool originalOnce) noexcept {CallFrame* frame=current_frame(this);if(frame==nullptr)return;GenerationSnapshot exit{};const bool exitRead=generations_.read(generations_.context,exit);CallBoundaryRecord record{};record.header=make_header(CapturePhase::call_boundary);record.exit_generations=exit;record.exit_tick=monotonic_tick();record.emitted_records=frame->emitted;record.lost_records=frame->lost;record.observer_fault=fault;record.original_called_once=originalOnce;record.generations_exact_at_exit=exitRead&&valid_exact_generations(frame->entry)&&exit==frame->entry;record.tls_stack_overflow=false;record.integrity_seal=seal_record(admission_.secret_,record);const auto push=call_records_.push_canonical(record);(void)push;--g_frame_depth;g_frames[g_frame_depth]={};}
bool CaptureOwner::current_call_available() const noexcept{return current_frame(const_cast<CaptureOwner*>(this))!=nullptr;}
RecordHeader CaptureOwner::make_header(CapturePhase phase) noexcept {RecordHeader header{};CallFrame* frame=current_frame(this);if(frame==nullptr)return header;GenerationSnapshot current{};const bool read=generations_.read(generations_.context,current);header.provenance={token_.owner_id(),token_.module_generation(),token_.capture_epoch(),token_.build_digest(),token_.package_digest(),token_.surface_cohort_digest()};header.entry_generations=frame->entry;header.phase_generations=current;header.record_sequence=next_record_sequence_.fetch_add(1U,std::memory_order_acq_rel)+1U;header.call_id=frame->call_id;header.parent_call_id=frame->parent_call_id;header.monotonic_tick=monotonic_tick();header.monotonic_frequency=monotonic_frequency();header.clock_domain_id=token_.owner_id();header.thread_id=current_thread_id();header.surface=frame->surface;header.phase=phase;header.generations_exact_at_phase=read&&valid_exact_generations(frame->entry)&&current==frame->entry;return header;}
Digest256 CaptureOwner::seal_bytes(std::span<const std::byte> bytes) const noexcept{return hmac_sha256(admission_.secret_,bytes);}

bool CaptureOwner::observe_producer_stage(ProducerStage stage,ProducerDisposition disposition,const ProducerStageFacts& facts) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::command5_producer);if(frame==nullptr||!strict_producer_transition(*frame,stage,disposition,facts))return false;ProducerRecord record{};record.header=make_header(stage==ProducerStage::returned?CapturePhase::producer_return:(stage==ProducerStage::entry?CapturePhase::producer_entry:CapturePhase::producer_stage));record.stage=stage;record.disposition=disposition;record.facts=facts;record.payload=frame->producer_payload;record.payload_present=frame->producer_payload_valid;record.trace_prefix_complete=true;return enqueue_record(producer_records_,record,frame,admission_.secret_);}
bool CaptureOwner::observe_producer_payload(const void* payload) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::command5_producer);if(frame==nullptr||frame->last_producer_stage!=ProducerStage::title)return false;PayloadProjection projection{};if(capture_command5_payload(payload,projection)!=BoundedCaptureResult::complete)return false;frame->producer_payload=projection;frame->producer_payload_valid=true;ProducerStageFacts facts=frame->producer_facts;if(!strict_producer_transition(*frame,ProducerStage::payload_anchor,ProducerDisposition::in_progress,facts))return false;ProducerRecord record{};record.header=make_header(CapturePhase::producer_payload_anchor);record.stage=ProducerStage::payload_anchor;record.facts=facts;record.payload=projection;record.payload_present=true;record.trace_prefix_complete=true;return enqueue_record(producer_records_,record,frame,admission_.secret_);}
bool CaptureOwner::observe_queue_decision(QueueDecisionMarker marker,std::uintptr_t identity) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::queue_insert);if(frame==nullptr||marker==QueueDecisionMarker::none)return false;frame->queue_marker=marker;frame->inserted_record_identity=identity;return true;}
bool CaptureOwner::observe_queue_insert_pre(const void* queue,const void* payload) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::queue_insert);if(frame==nullptr)return false;QueueProjection pre{};PayloadProjection argument{};if(capture_queue_projection(queue,pre)!=BoundedCaptureResult::complete||capture_command5_payload(payload,argument)!=BoundedCaptureResult::complete)return false;frame->queue_pre=pre;frame->queue_argument_payload=argument;frame->queue_pre_valid=true;return true;}
bool CaptureOwner::emit_queue_insert(const void* queue) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::queue_insert);if(frame==nullptr||!frame->queue_pre_valid)return false;QueueProjection post{};if(capture_queue_projection(queue,post)!=BoundedCaptureResult::complete)return false;QueueInsertRecord record{};record.header=make_header(CapturePhase::queue_insert);record.pre=frame->queue_pre;record.post=post;record.argument_payload=frame->queue_argument_payload;record.marker=frame->queue_marker;record.inserted_record_identity=frame->inserted_record_identity;record.queue_identity=reinterpret_cast<std::uintptr_t>(queue);record.outcome=classify_queue_insert(record.pre,record.post,record.marker,record.inserted_record_identity);return enqueue_record(queue_insert_records_,record,frame,admission_.secret_);}
bool CaptureOwner::emit_lazy_dispatch(std::int32_t preHandle,std::int32_t selectedHandle,std::uint32_t tag,std::uint32_t classId,std::uint32_t package,std::span<const std::byte> enumerated,std::span<const std::byte> row,bool selected) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::lazy_dispatch_resolver);if(frame==nullptr||enumerated.empty()||row.empty())return false;LazyDispatchRecord record{};record.header=make_header(CapturePhase::lazy_dispatch_selection);record.pre_handle=preHandle;record.selected_handle=selectedHandle;record.selected_tag=tag;record.selected_class=classId;record.selected_package=package;record.enumerated_count=static_cast<std::uint32_t>(enumerated.size()/sizeof(std::uint32_t));record.enumerated_resource_digest=sha256_bytes(enumerated);record.authored_row_digest=sha256_bytes(row);record.selected_from_enumeration=selected;return enqueue_record(lazy_dispatch_records_,record,frame,admission_.secret_);}
bool CaptureOwner::observe_queue_tick_pre(const void* queue) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::queue_tick);if(frame==nullptr)return false;frame->tick_pre_valid=capture_queue_projection(queue,frame->tick_pre)==BoundedCaptureResult::complete;return frame->tick_pre_valid;}
bool CaptureOwner::emit_queue_tick(const void* queue,QueueTickAction action,std::int32_t sequence,std::uint64_t duration,std::uint64_t elapsed,bool marker) noexcept {CallFrame* frame=nearest_surface_frame(this,NativeSurface::queue_tick);if(frame==nullptr||!frame->tick_pre_valid)return false;QueueProjection post{};if(capture_queue_projection(queue,post)!=BoundedCaptureResult::complete)return false;QueueTickRecord record{};record.header=make_header(action==QueueTickAction::retired?CapturePhase::queue_tick_retire:(action==QueueTickAction::superseded?CapturePhase::queue_tick_supersede:CapturePhase::queue_tick_promote));record.pre=frame->tick_pre;record.post=post;record.action=action;record.affected_sequence=sequence;record.duration_ticks=duration;record.elapsed_ticks=elapsed;record.exact_action_marker_observed=marker;return enqueue_record(queue_tick_records_,record,frame,admission_.secret_);}
bool CaptureOwner::emit_cui(CuiValueKind kind,bool wrapper,bool getter,const void* queue,LocalizedPair localized,std::uint64_t scalar,std::span<const std::byte> valueBytes) noexcept {CallFrame* frame=current_frame(this);if(frame==nullptr)return false;QueueProjection current{};if(capture_queue_projection(queue,current)!=BoundedCaptureResult::complete||valueBytes.empty())return false;CuiRecord record{};record.header=make_header(getter?CapturePhase::cui_getter:CapturePhase::cui_wrapper);record.value_kind=kind;record.registration=registration_for(kind);record.current_sequence=current.current_sequence;record.current_command=current.current_command;record.current_payload=current.current_payload;record.localized_value=localized;record.scalar_value=scalar;record.value_digest=sha256_bytes(valueBytes);record.wrapper_observed=wrapper;record.getter_observed=getter;record.command5_exact_join=current.current_active!=0U&&current.current_command==kActivityIntroCommand&&current.current_payload_valid;record.command8_neighbor_only=current.current_command==kNeighborActivityCommand&&(kind==CuiValueKind::category||kind==CuiValueKind::icon_theme||kind==CuiValueKind::visual_tuple);if(kind==CuiValueKind::title&&record.command8_neighbor_only)return false;return enqueue_record(cui_records_,record,frame,admission_.secret_);}
bool CaptureOwner::emit_localization(const void* queue,std::uint32_t resource,std::uint32_t node,std::uint32_t property,std::uint16_t mapIndex,std::uint16_t ordinal,LocalizedPair resolved,std::span<const std::byte> text) noexcept {CallFrame* frame=current_frame(this);if(frame==nullptr||text.empty())return false;QueueProjection current{};if(capture_queue_projection(queue,current)!=BoundedCaptureResult::complete)return false;LocalizationRecord record{};record.header=make_header(CapturePhase::category_localization);record.current_sequence=current.current_sequence;record.current_command=current.current_command;record.presentation_category=current.current_payload.presentation_category;record.instantiated_resource_tag=resource;record.instantiated_node_id=node;record.property_id=property;record.bank_map_index=mapIndex;record.bank_ordinal=ordinal;record.resolved_pair=resolved;record.payload_digest=current.current_payload.full_payload_digest;record.resolved_utf8_digest=sha256_bytes(text);const bool exact=current.current_active!=0U&&current.current_command==kActivityIntroCommand&&current.current_payload_valid&&resource!=0U&&node!=0U&&property!=0U&&mapIndex==kMissionBankMapIndex&&ordinal==kMissionBankOrdinal&&resolved==kMissionCatalogPair;record.resulting_edge_grade=exact?EvidenceGrade::runtime_observed:EvidenceGrade::inferred;return enqueue_record(localization_records_,record,frame,admission_.secret_);}
bool CaptureOwner::emit_latch(LatchBoundaryEvent event,const LatchSnapshot& pre,const LatchSnapshot& post,ProducerAttemptOutcome outcome,int argument,bool marker) noexcept {CallFrame* frame=current_frame(this);if(frame==nullptr||!valid_latch_snapshot(pre)||!valid_latch_snapshot(post))return false;bool valid=false;switch(event){case LatchBoundaryEvent::state_gate:{const auto expected=expected_state_machine_step(pre,outcome);valid=expected.valid_input&&expected.post==post&&marker==expected.pending_cleared_after_attempt;break;}case LatchBoundaryEvent::pending_clear:valid=marker&&pre.pending!=0U&&post.pending==0U;break;case LatchBoundaryEvent::reset_zero:valid=argument==0&&post==expected_reset_post(pre);break;case LatchBoundaryEvent::rearm:valid=post==expected_rearm_post(pre);break;case LatchBoundaryEvent::ready_setter:valid=post==expected_ready_post(pre);break;case LatchBoundaryEvent::direct_show_nonzero:valid=argument!=0&&pre==post&&outcome!=ProducerAttemptOutcome::not_attempted;break;}if(!valid)return false;LatchRecord record{};record.header=make_header(event==LatchBoundaryEvent::pending_clear?CapturePhase::latch_pending_clear:(event==LatchBoundaryEvent::rearm?CapturePhase::latch_rearm:(event==LatchBoundaryEvent::ready_setter?CapturePhase::latch_ready:(event==LatchBoundaryEvent::reset_zero?CapturePhase::latch_reset:CapturePhase::latch_gate))));record.event=event;record.pre=pre;record.post=post;record.attempt_outcome=outcome;record.wrapper_argument=argument;record.exact_pending_clear_marker=marker;return enqueue_record(latch_records_,record,frame,admission_.secret_);}

ClockCalibration CaptureOwner::calibrate_video_clock(std::uint64_t videoTick,std::uint64_t timescale) noexcept {ClockCalibration value{};if(!admission_.verifies(token_)||timescale==0U)return value;value.admission_owner_id=token_.owner_id();value.capture_epoch=token_.capture_epoch();value.clock_domain_id=next_clock_domain_id_.fetch_add(1U,std::memory_order_acq_rel)+1U;value.qpc_tick=monotonic_tick();value.video_tick=videoTick;value.video_timescale=timescale;value.seal=seal_bytes(std::as_bytes(std::span{&value,std::size_t{1U}}).first(offsetof(ClockCalibration,seal)));return value;}
bool CaptureOwner::verifies_calibration(const ClockCalibration& value) const noexcept {if(value.admission_owner_id!=token_.owner_id()||value.capture_epoch!=token_.capture_epoch()||value.video_timescale==0U)return false;ClockCalibration copy=value;copy.seal={};return value.seal==seal_bytes(std::as_bytes(std::span{&copy,std::size_t{1U}}).first(offsetof(ClockCalibration,seal)));}
bool CaptureOwner::emit_visible_frame(const ClockCalibration& calibration,VisibleFrameEdge edge,std::uint64_t frameIndex,std::uint64_t videoTick,std::int32_t sequence,const Digest256& payload,const Digest256& frameDigest,bool mission,bool omega) noexcept {CallFrame* frame=current_frame(this);if(frame==nullptr||!verifies_calibration(calibration)||digest_zero(payload)||digest_zero(frameDigest))return false;VisibleFrameRecord record{};record.header=make_header(edge==VisibleFrameEdge::first_visible?CapturePhase::visible_first_frame:CapturePhase::visible_last_frame);record.header.clock_domain_id=calibration.clock_domain_id;record.edge=edge;record.video_frame_index=frameIndex;record.video_tick=videoTick;record.video_timescale=calibration.video_timescale;record.current_sequence=sequence;record.payload_digest=payload;record.frame_digest=frameDigest;record.mission_label_visible=mission;record.omega_title_visible=omega;record.synchronized_clock=true;return enqueue_record(visible_frame_records_,record,frame,admission_.secret_);}

#define POP_IMPL(Type,member) QueuePopResult CaptureOwner::try_pop(Type& output) noexcept{return member.try_pop(output);}
POP_IMPL(ProducerRecord,producer_records_) POP_IMPL(QueueInsertRecord,queue_insert_records_) POP_IMPL(LazyDispatchRecord,lazy_dispatch_records_) POP_IMPL(QueueTickRecord,queue_tick_records_) POP_IMPL(CuiRecord,cui_records_) POP_IMPL(LocalizationRecord,localization_records_) POP_IMPL(VisibleFrameRecord,visible_frame_records_) POP_IMPL(LatchRecord,latch_records_) POP_IMPL(CallBoundaryRecord,call_records_)
#undef POP_IMPL

template <typename Record>
DefaultTelemetry base_telemetry(const Record& record) noexcept {DefaultTelemetry output{};output.provenance=record.header.provenance;output.entry_generations=record.header.entry_generations;output.phase_generations=record.header.phase_generations;output.record_sequence=record.header.record_sequence;output.call_id=record.header.call_id;output.parent_call_id=record.header.parent_call_id;output.monotonic_tick=record.header.monotonic_tick;output.monotonic_frequency=record.header.monotonic_frequency;output.clock_domain_id=record.header.clock_domain_id;output.thread_id=record.header.thread_id;output.phase=record.header.phase;output.generations_exact=record.header.generations_exact_at_phase;return output;}
bool default_telemetry(const ProducerRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.payload.full_payload_digest;output.command=kActivityIntroCommand;output.outcome=static_cast<std::uint8_t>(record.disposition);return record.header.call_id!=0U;}
bool default_telemetry(const QueueInsertRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.argument_payload.full_payload_digest;output.native_sequence=record.post.current_sequence;output.command=kActivityIntroCommand;output.outcome=static_cast<std::uint8_t>(record.outcome);return record.header.call_id!=0U;}
bool default_telemetry(const LazyDispatchRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.authored_row_digest;output.outcome=record.selected_from_enumeration?1U:0U;return record.header.call_id!=0U;}
bool default_telemetry(const QueueTickRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.post.exact_queue_digest;output.native_sequence=record.affected_sequence;output.command=record.post.current_command;output.outcome=static_cast<std::uint8_t>(record.action);return record.header.call_id!=0U;}
bool default_telemetry(const CuiRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.value_digest;output.native_sequence=record.current_sequence;output.command=record.current_command;output.outcome=static_cast<std::uint8_t>(record.value_kind);return record.header.call_id!=0U;}
bool default_telemetry(const LocalizationRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.resolved_utf8_digest;output.native_sequence=record.current_sequence;output.command=record.current_command;output.outcome=static_cast<std::uint8_t>(record.resulting_edge_grade);return record.header.call_id!=0U;}
bool default_telemetry(const VisibleFrameRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.frame_digest;output.native_sequence=record.current_sequence;output.command=kActivityIntroCommand;output.outcome=static_cast<std::uint8_t>(record.edge);return record.header.call_id!=0U;}
bool default_telemetry(const LatchRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.payload_or_state_digest=record.post.digest;output.command=kActivityIntroCommand;output.outcome=static_cast<std::uint8_t>(record.attempt_outcome);return record.header.call_id!=0U;}
bool default_telemetry(const CallBoundaryRecord& record,DefaultTelemetry& output) noexcept{output=base_telemetry(record);output.outcome=static_cast<std::uint8_t>(record.observer_fault);output.generations_exact=record.generations_exact_at_exit;return record.header.call_id!=0U;}

} // namespace sunrise::client::hooks::bootflow::opening_authority::mission_header_intro_capture
