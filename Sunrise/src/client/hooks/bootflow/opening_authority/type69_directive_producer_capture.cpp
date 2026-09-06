#include "client/hooks/bootflow/opening_authority/type69_directive_producer_capture.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cwchar>
#include <limits>

#pragma comment(lib, "bcrypt.lib")

namespace sunrise::client::hooks::bootflow::opening_authority::type69_capture {
namespace {

inline constexpr std::array<std::byte, 24U> kRuntimeResolverPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x70}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x97}, std::byte{0xA9}, std::byte{0x69},
    std::byte{0x01}, std::byte{0x48}, std::byte{0x33}, std::byte{0xC4},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x44}, std::byte{0x24}};
inline constexpr std::array<std::byte, 24U> kFullDispatchPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x7C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x55}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xEC}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x48}, std::byte{0x0F},
    std::byte{0xBE}, std::byte{0x42}, std::byte{0x60}, std::byte{0x48}};
inline constexpr std::array<std::byte, 24U> kCategoryResolverPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xEC}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x10}, std::byte{0x45}, std::byte{0x32}, std::byte{0xC9},
    std::byte{0x83}, std::byte{0xFA}, std::byte{0x0A}, std::byte{0x0F},
    std::byte{0x87}, std::byte{0xF2}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x63}, std::byte{0xC2}};
inline constexpr std::array<std::byte, 5U> kFullThunkBody{
    std::byte{0xE9}, std::byte{0xDB}, std::byte{0x18}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 24U> kFullHandlerPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0x6C}, std::byte{0x24}, std::byte{0xD1}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0xD8}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}};
inline constexpr std::array<std::byte, 16U> kFullBranchPrefix{
    std::byte{0x48}, std::byte{0x63}, std::byte{0x07}, std::byte{0x83},
    std::byte{0xF8}, std::byte{0xFF}, std::byte{0x0F}, std::byte{0x84},
    std::byte{0xAC}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0xF3}, std::byte{0x0F}, std::byte{0x10}, std::byte{0x57}};
inline constexpr std::array<std::byte, 16U> kFullPreCallPrefix{
    std::byte{0xE8}, std::byte{0x44}, std::byte{0xF7}, std::byte{0xEC},
    std::byte{0xFF}, std::byte{0xE9}, std::byte{0x90}, std::byte{0xFC},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0xBD},
    std::byte{0x5C}, std::byte{0x3F}, std::byte{0x7B}, std::byte{0x92}};
inline constexpr std::array<std::byte, 11U> kFullReturnPrefix{
    std::byte{0xE9}, std::byte{0x90}, std::byte{0xFC}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0xBD}, std::byte{0x5C},
    std::byte{0x3F}, std::byte{0x7B}, std::byte{0x92}};
inline constexpr std::array<std::byte, 19U> kFullTrampoline{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x11}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x49}, std::byte{0x08}, std::byte{0x49},
    std::byte{0x8B}, std::byte{0x42}, std::byte{0x18}, std::byte{0x4D},
    std::byte{0x8B}, std::byte{0x5C}, std::byte{0x02}, std::byte{0x30},
    std::byte{0x49}, std::byte{0xFF}, std::byte{0xE3}};
inline constexpr std::array<std::byte, 5U> kShortThunkBody{
    std::byte{0xE9}, std::byte{0xEB}, std::byte{0x8D}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 24U> kShortHandlerPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0x6C}, std::byte{0x24}, std::byte{0xD9},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xA0},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x44}};
inline constexpr std::array<std::byte, 16U> kShortBranchPrefix{
    std::byte{0x49}, std::byte{0x63}, std::byte{0x03}, std::byte{0x83},
    std::byte{0xF8}, std::byte{0xFF}, std::byte{0x0F}, std::byte{0x84},
    std::byte{0xC7}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x0C}, std::byte{0x80}};
inline constexpr std::array<std::byte, 16U> kShortPreCallPrefix{
    std::byte{0xE8}, std::byte{0x5B}, std::byte{0x84}, std::byte{0xEC},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xC4},
    std::byte{0xA0}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x41}, std::byte{0x5F}, std::byte{0x41}, std::byte{0x5E}};
inline constexpr std::array<std::byte, 11U> kShortReturnPrefix{
    std::byte{0x48}, std::byte{0x81}, std::byte{0xC4}, std::byte{0xA0},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x41},
    std::byte{0x5F}, std::byte{0x41}, std::byte{0x5E}};
inline constexpr std::array<std::byte, 19U> kShortTrampoline{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x01}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x49}, std::byte{0x08}, std::byte{0x49},
    std::byte{0x8B}, std::byte{0x40}, std::byte{0x18}, std::byte{0x4D},
    std::byte{0x8B}, std::byte{0x4C}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x49}, std::byte{0xFF}, std::byte{0xE1}};
inline constexpr std::array<std::byte, 17U> kEndpointActivationPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x81}, std::byte{0xC0}, std::byte{0x05}, std::byte{0x00},
    std::byte{0x00}};
inline constexpr std::array<std::byte, 18U> kEndpointBinderPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x5B}, std::byte{0x18}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x70},
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xD9}};
inline constexpr std::array<std::byte, 20U> kEndpointRegistrationCallPrefix{
    std::byte{0xE8}, std::byte{0xC5}, std::byte{0x5E}, std::byte{0xBC},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x7F},
    std::byte{0x50}, std::byte{0x48}, std::byte{0x83}, std::byte{0xED},
    std::byte{0x01}, std::byte{0x0F}, std::byte{0x85}, std::byte{0x27},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}};
inline constexpr std::array<std::byte, 19U> kEndpointRegistrationReturnPrefix{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x7F}, std::byte{0x50},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xED}, std::byte{0x01},
    std::byte{0x0F}, std::byte{0x85}, std::byte{0x27}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xB4}, std::byte{0x24}, std::byte{0x88}};
inline constexpr std::array<std::byte, 22U> kEndpointRegistrationTrampoline{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x01}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x49}, std::byte{0x08}, std::byte{0x49},
    std::byte{0x8B}, std::byte{0x40}, std::byte{0x18}, std::byte{0x4E},
    std::byte{0x8B}, std::byte{0x8C}, std::byte{0x00}, std::byte{0x90},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x49},
    std::byte{0xFF}, std::byte{0xE1}};
inline constexpr std::array<std::byte, 17U> kEndpointLookupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x50}, std::byte{0x33},
    std::byte{0xDB}};
inline constexpr std::array<std::byte, 17U> kEndpointCopyPrefix{
    std::byte{0x49}, std::byte{0x63}, std::byte{0xC0}, std::byte{0x4C},
    std::byte{0x8D}, std::byte{0x81}, std::byte{0x88}, std::byte{0x05},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0x0C}, std::byte{0x80}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xC2}};
inline constexpr std::array<std::byte, 16U> kReflectedDecoderPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
inline constexpr std::array<std::byte, 16U> kReflectedEncoderBody{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x28},
    std::byte{0xE8}, std::byte{0xD7}, std::byte{0xBA}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xB0}, std::byte{0x01}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xC4}, std::byte{0x28}, std::byte{0xC3}};
inline constexpr std::array<std::byte, 18U> kLargeSerializerPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x10},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x05}, std::byte{0xE6}, std::byte{0xDA},
    std::byte{0xBC}, std::byte{0x01}};
inline constexpr std::array<std::byte, 20U> kLargeSerializerCursorPrefix{
    std::byte{0x33}, std::byte{0xD2}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0x4C}, std::byte{0x24}, std::byte{0x20}, std::byte{0x0F},
    std::byte{0xB6}, std::byte{0xD8}, std::byte{0xE8}, std::byte{0x9A},
    std::byte{0x2B}, std::byte{0xE7}, std::byte{0xFF}, std::byte{0x84},
    std::byte{0xDB}, std::byte{0x74}, std::byte{0x1A}, std::byte{0x8B}};
inline constexpr std::array<std::byte, 20U> kSchemaTransportPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x40}};
inline constexpr std::array<std::byte, 20U> kSchemaTransportEncodeCallPrefix{
    std::byte{0xE8}, std::byte{0x74}, std::byte{0x07}, std::byte{0xFE},
    std::byte{0xFF}, std::byte{0x84}, std::byte{0xC0}, std::byte{0x74},
    std::byte{0x34}, std::byte{0x8B}, std::byte{0x86}, std::byte{0xC0},
    std::byte{0x5B}, std::byte{0x06}, std::byte{0x00}, std::byte{0x4D},
    std::byte{0x8B}, std::byte{0xCE}, std::byte{0x48}, std::byte{0x8B}};
inline constexpr std::array<std::byte, 20U> kSchemaTransportCallPrefix{
    std::byte{0x41}, std::byte{0xFF}, std::byte{0x52}, std::byte{0x28},
    std::byte{0x84}, std::byte{0xC0}, std::byte{0x74}, std::byte{0x04},
    std::byte{0xB3}, std::byte{0x01}, std::byte{0xEB}, std::byte{0x02},
    std::byte{0x32}, std::byte{0xDB}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0x4C}, std::byte{0x24}, std::byte{0x30}, std::byte{0xE8}};
inline constexpr std::array<std::byte, 5U> kDecodedPointerGetterBody{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x41}, std::byte{0x08}, std::byte{0xC3}};
inline constexpr std::array<std::byte, 15U> kType68ApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x40}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x02},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xF9}};
inline constexpr std::array<std::byte, 16U> kType68AuthoredResolutionPrefix{
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x4C},
    std::byte{0x8B}, std::byte{0xD1}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x33}, std::byte{0x08}, std::byte{0x43},
    std::byte{0x01}, std::byte{0x41}, std::byte{0x8B}, std::byte{0xD1}};
inline constexpr std::array<std::byte, 16U> kType68EventRowLookupPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x08},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x51}, std::byte{0x08},
    std::byte{0x45}, std::byte{0x33}, std::byte{0xC9}, std::byte{0x4D},
    std::byte{0x85}, std::byte{0xD2}, std::byte{0x7E}, std::byte{0x53}};
inline constexpr std::array<std::byte, 16U> kType68PostCachePrefix{
    std::byte{0xE8}, std::byte{0xC8}, std::byte{0xF6}, std::byte{0x41},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xC8},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x54}, std::byte{0x24},
    std::byte{0x60}, std::byte{0xE8}, std::byte{0x9B}, std::byte{0xFD}};
inline constexpr std::array<std::byte, 16U> kType68Lifecycle0CallPrefix{
    std::byte{0xE8}, std::byte{0x56}, std::byte{0x01}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xFF}, std::byte{0xC5}, std::byte{0x83},
    std::byte{0xFD}, std::byte{0x03}, std::byte{0x0F}, std::byte{0x8E},
    std::byte{0xFB}, std::byte{0xFE}, std::byte{0xFF}, std::byte{0xFF}};
inline constexpr std::array<std::byte, 16U> kType68Lifecycle0ReturnPrefix{
    std::byte{0xFF}, std::byte{0xC5}, std::byte{0x83}, std::byte{0xFD},
    std::byte{0x03}, std::byte{0x0F}, std::byte{0x8E}, std::byte{0xFB},
    std::byte{0xFE}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x4C},
    std::byte{0x8B}, std::byte{0x74}, std::byte{0x24}, std::byte{0x30}};
inline constexpr std::array<std::byte, 16U> kType68FormatterCallPrefix{
    std::byte{0xE8}, std::byte{0x6D}, std::byte{0xEB}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xE8}, std::byte{0xF8}, std::byte{0x41},
    std::byte{0x37}, std::byte{0x00}, std::byte{0x84}, std::byte{0xC0},
    std::byte{0x0F}, std::byte{0x84}, std::byte{0xE1}, std::byte{0x00}};
inline constexpr std::array<std::byte, 16U> kType68FormatterReturnPrefix{
    std::byte{0xE8}, std::byte{0xF8}, std::byte{0x41}, std::byte{0x37},
    std::byte{0x00}, std::byte{0x84}, std::byte{0xC0}, std::byte{0x0F},
    std::byte{0x84}, std::byte{0xE1}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xE8}, std::byte{0x0B}, std::byte{0x37}};
inline constexpr std::array<std::byte, 16U> kType68ManagerAddCallPrefix{
    std::byte{0xE8}, std::byte{0x5E}, std::byte{0x1D}, std::byte{0x37},
    std::byte{0x00}, std::byte{0xE9}, std::byte{0xCA}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0x8B},
    std::byte{0xC1}, std::byte{0x41}, std::byte{0x81}, std::byte{0xE1}};
inline constexpr std::array<std::byte, 16U> kType68ManagerAddReturnPrefix{
    std::byte{0xE9}, std::byte{0xCA}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x41}, std::byte{0x8B}, std::byte{0xC1},
    std::byte{0x41}, std::byte{0x81}, std::byte{0xE1}, std::byte{0xFF},
    std::byte{0x1F}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC1}};
inline constexpr std::array<std::byte, 20U> kDirectiveFormatterPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
inline constexpr std::array<std::byte, 20U> kManagerAddPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x4C}, std::byte{0x8B},
    std::byte{0x99}, std::byte{0x80}, std::byte{0x14}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xFA}};
inline constexpr std::array<std::byte, 20U> kManagerPresentationUpdatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}, std::byte{0x55},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xEC}, std::byte{0x48}};
inline constexpr std::array<std::byte, 20U> kPresentationQueuePrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x70},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9}, std::byte{0x4C},
    std::byte{0x63}, std::byte{0xCA}, std::byte{0x8B}, std::byte{0x89},
    std::byte{0x88}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 20U> kQueuePromotePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x83}, std::byte{0xB9},
    std::byte{0x88}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9},
    std::byte{0x75}, std::byte{0x0E}, std::byte{0x80}, std::byte{0xB9}};
inline constexpr std::array<std::byte, 20U> kCuiProviderPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x49}, std::byte{0x8B},
    std::byte{0xD8}, std::byte{0xE8}, std::byte{0x8E}, std::byte{0xA8},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8D}};

inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
inline constexpr std::uint64_t kCohortCookieSalt = 0x69434F484F525431ULL;
inline constexpr std::size_t kTlsStackCapacity = 16U;

std::atomic_uint64_t gNextOwnerInstance{1U};

struct DispatchStackEntry final {
    std::uint64_t owner_instance_id{};
    std::uint64_t capture_epoch{};
    std::uint64_t call_id{};
    DispatchKind kind{DispatchKind::full_apply};
    DescriptorProjection descriptor{};
};

struct EndpointBinderStackEntry final {
    std::uint64_t owner_instance_id{};
    std::uint64_t capture_epoch{};
    std::uint64_t call_id{};
    std::uintptr_t endpoint_owner{};
    std::uint32_t requested_count{};
    std::uint32_t observed_registrations{};
};

thread_local std::array<DispatchStackEntry, kTlsStackCapacity> gDispatchStack{};
thread_local std::size_t gDispatchDepth{};
thread_local std::array<EndpointBinderStackEntry, kTlsStackCapacity> gEndpointBinderStack{};
thread_local std::size_t gEndpointBinderDepth{};

[[nodiscard]] bool safe_copy_exact(void* destination,
                                   const void* source,
                                   std::size_t bytes) noexcept {
    if (destination == nullptr || source == nullptr || bytes == 0U) {
        return false;
    }
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(), source, destination, bytes, &copied) != FALSE
           && copied == bytes;
}

[[nodiscard]] bool add_pointer(std::uintptr_t base,
                               std::uintptr_t delta,
                               std::uintptr_t& output) noexcept {
    if (delta > (std::numeric_limits<std::uintptr_t>::max)() - base) {
        return false;
    }
    output = base + delta;
    return true;
}

[[nodiscard]] bool payload_equal(const Type69Payload& left,
                                 const Type69Payload& right) noexcept {
    return std::memcmp(&left, &right, sizeof left) == 0;
}

[[nodiscard]] std::uint64_t projection_hash(const DescriptorProjection& projection) noexcept {
    std::array<std::byte, kType69PayloadBytes + 1U> bytes{};
    std::memcpy(bytes.data(), &projection.payload, sizeof projection.payload);
    bytes.back() = std::byte{projection.type};
    return bounded_hash(bytes);
}

[[nodiscard]] bool executable_protection(DWORD protection) noexcept {
    const DWORD base = protection & 0xFFU;
    return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ
           || base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

[[nodiscard]] bool executable_range(const std::byte* address, std::size_t bytes) noexcept {
    const std::byte* cursor = address;
    const std::byte* const end = address + bytes;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(cursor, &info, sizeof info) != sizeof info || info.State != MEM_COMMIT
            || !executable_protection(info.Protect) || (info.Protect & PAGE_GUARD) != 0U) {
            return false;
        }
        const auto regionEnd = static_cast<const std::byte*>(info.BaseAddress) + info.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = (std::min)(regionEnd, end);
    }
    return true;
}

[[nodiscard]] std::uint32_t current_owner_mask(const OwnerGenerationSet& value) noexcept {
    std::uint32_t mask{};
    const auto add = [&mask, &value](OwnerField field, bool current) noexcept {
        const std::uint32_t bit = static_cast<std::uint32_t>(field);
        if ((value.presence_mask & bit) != 0U && current) {
            mask |= bit;
        }
    };
    add(OwnerField::activation,
        value.module_generation != 0U && value.activation_generation != 0U
            && value.activation_state == ActivationSnapshotState::current);
    add(OwnerField::activity,
        value.activity_session_id != 0U && value.activity_generation != 0U);
    add(OwnerField::connection, value.connection_generation != 0U);
    add(OwnerField::roster, value.roster_generation != 0U);
    add(OwnerField::registry_materialization,
        value.registry_materialization_generation != 0U);
    add(OwnerField::endpoint_vector, value.endpoint_vector_generation != 0U);
    add(OwnerField::authority, value.authority_generation != 0U);
    add(OwnerField::run_token, value.run_token != 0U);
    add(OwnerField::correlation_token, value.correlation_token != 0U);
    add(OwnerField::component, value.component_generation != 0U);
    return mask;
}

[[nodiscard]] std::size_t participant_index(NativeParticipant participant) noexcept {
    return static_cast<std::size_t>(participant);
}

[[nodiscard]] PcSurface participant_entry_surface(NativeParticipant participant) noexcept {
    switch (participant) {
    case NativeParticipant::resolver:
        return PcSurface::runtime_resolver_entry;
    case NativeParticipant::full_dispatch:
        return PcSurface::full_dispatch_entry;
    case NativeParticipant::full_callsite:
        return PcSurface::full_pre_subscriber_call;
    case NativeParticipant::short_dispatch:
        return PcSurface::short_dispatch_entry;
    case NativeParticipant::short_callsite:
        return PcSurface::short_pre_subscriber_call;
    case NativeParticipant::endpoint_binder:
        return PcSurface::endpoint_binder_entry;
    case NativeParticipant::endpoint_registration_callsite:
        return PcSurface::endpoint_registration_call;
    case NativeParticipant::large_serializer:
        return PcSurface::large_serializer_entry;
    case NativeParticipant::schema_transport:
        return PcSurface::schema_transport_entry;
    case NativeParticipant::reflected_decoder:
        return PcSurface::reflected_decoder_entry;
    case NativeParticipant::type68_apply:
        return PcSurface::type68_apply_entry;
    default:
        return PcSurface::count;
    }
}

[[nodiscard]] PcSurface participant_return_surface(NativeParticipant participant) noexcept {
    switch (participant) {
    case NativeParticipant::full_callsite:
        return PcSurface::full_post_subscriber_return;
    case NativeParticipant::short_callsite:
        return PcSurface::short_post_subscriber_return;
    case NativeParticipant::endpoint_registration_callsite:
        return PcSurface::endpoint_registration_return;
    default:
        return PcSurface::count;
    }
}

[[nodiscard]] bool push_dispatch(const DispatchStackEntry& entry,
                                 std::size_t& outputDepth) noexcept {
    if (gDispatchDepth == gDispatchStack.size()) {
        return false;
    }
    outputDepth = gDispatchDepth;
    gDispatchStack[gDispatchDepth++] = entry;
    return true;
}

void pop_dispatch(std::size_t expectedDepth, std::uint64_t callId) noexcept {
    if (gDispatchDepth == expectedDepth + 1U
        && gDispatchStack[expectedDepth].call_id == callId) {
        gDispatchStack[expectedDepth] = {};
        --gDispatchDepth;
    }
}

[[nodiscard]] bool push_endpoint_binder(const EndpointBinderStackEntry& entry,
                                        std::size_t& outputDepth) noexcept {
    if (gEndpointBinderDepth == gEndpointBinderStack.size()) {
        return false;
    }
    outputDepth = gEndpointBinderDepth;
    gEndpointBinderStack[gEndpointBinderDepth++] = entry;
    return true;
}

void pop_endpoint_binder(std::size_t expectedDepth, std::uint64_t callId) noexcept {
    if (gEndpointBinderDepth == expectedDepth + 1U
        && gEndpointBinderStack[expectedDepth].call_id == callId) {
        gEndpointBinderStack[expectedDepth] = {};
        --gEndpointBinderDepth;
    }
}

[[nodiscard]] EndpointBinderStackEntry* parent_endpoint_binder(
    const ParticipantLease& lease) noexcept {
    if (gEndpointBinderDepth == 0U) {
        return nullptr;
    }
    EndpointBinderStackEntry& entry = gEndpointBinderStack[gEndpointBinderDepth - 1U];
    const CaptureProvenance& provenance = lease.entry_provenance();
    if (entry.owner_instance_id != provenance.owner_instance_id
        || entry.capture_epoch != provenance.capture_epoch) {
        return nullptr;
    }
    return &entry;
}

[[nodiscard]] const DispatchStackEntry* parent_dispatch(
    const ParticipantLease& lease,
    DispatchKind expectedKind) noexcept {
    if (gDispatchDepth == 0U) {
        return nullptr;
    }
    const DispatchStackEntry& entry = gDispatchStack[gDispatchDepth - 1U];
    const CaptureProvenance& provenance = lease.entry_provenance();
    if (entry.owner_instance_id != provenance.owner_instance_id
        || entry.capture_epoch != provenance.capture_epoch || entry.kind != expectedKind) {
        return nullptr;
    }
    return &entry;
}

[[nodiscard]] SubscriberResolutionCandidate resolve_subscriber_candidate(
    const void* interfacePair,
    std::uint32_t methodSlot) noexcept {
    SubscriberResolutionCandidate result{};
    result.interface_pair = reinterpret_cast<std::uintptr_t>(interfacePair);
    result.method_slot = methodSlot;
    if (interfacePair == nullptr) {
        return result;
    }
    std::array<std::uintptr_t, 2U> pair{};
    if (!safe_copy_exact(pair.data(), interfacePair, sizeof pair)) {
        return result;
    }
    result.validity_mask |= static_cast<std::uint32_t>(ResolutionValidity::pair_readable);
    result.interface_datum = pair[0];
    result.endpoint_object = pair[1];
    if (result.interface_datum != 0U) {
        result.validity_mask |= static_cast<std::uint32_t>(ResolutionValidity::interface_datum);
    }
    if (result.endpoint_object != 0U) {
        result.validity_mask |= static_cast<std::uint32_t>(ResolutionValidity::endpoint_object);
        result.adjusted_this = result.endpoint_object;
        result.validity_mask |= static_cast<std::uint32_t>(ResolutionValidity::adjusted_this);
    }
    if (result.interface_datum == 0U) {
        return result;
    }
    std::uintptr_t metadataOffset{};
    if (!safe_copy_exact(&metadataOffset,
                         reinterpret_cast<const void*>(result.interface_datum + 0x18U),
                         sizeof metadataOffset)
        || !add_pointer(result.interface_datum, metadataOffset, result.interface_metadata)) {
        return result;
    }
    result.validity_mask |= static_cast<std::uint32_t>(ResolutionValidity::interface_metadata);
    std::uintptr_t methodAddress{};
    if (!add_pointer(result.interface_metadata, methodSlot, methodAddress)
        || !safe_copy_exact(&result.concrete_method,
                            reinterpret_cast<const void*>(methodAddress),
                            sizeof result.concrete_method)) {
        return result;
    }
    if (result.concrete_method != 0U) {
        result.validity_mask |= static_cast<std::uint32_t>(ResolutionValidity::concrete_method);
    }
    return result;
}

[[nodiscard]] TelemetryProvenance telemetry_provenance(
    const CaptureProvenance& value,
    std::uint64_t sequence) noexcept {
    const std::uint64_t duration = value.post_monotonic_tick >= value.pre_monotonic_tick
                                       ? value.post_monotonic_tick - value.pre_monotonic_tick
                                       : 0U;
    TelemetryProvenance output{};
    output.build_id = value.build_id;
    output.cohort_id = value.cohort_id;
    output.capture_epoch = value.capture_epoch;
    output.owner_instance_id = value.owner_instance_id;
    output.call_id = value.call_id;
    output.parent_call_id = value.parent_call_id;
    output.materialization_token = value.materialization_token;
    output.queue_sequence = sequence;
    output.entry_owner_revision = value.entry_owner_revision;
    output.exit_owner_revision = value.exit_owner_revision;
    output.pre_monotonic_tick = value.pre_monotonic_tick;
    output.post_monotonic_tick = value.post_monotonic_tick;
    output.duration_ticks = duration;
    output.caller_rva = value.caller_rva;
    output.entry_rva = pc_boundary(value.entry_surface).rva;
    output.return_rva = pc_boundary(value.return_surface).rva;
    output.owner_entry = value.owner_entry;
    output.owner_exit = value.owner_exit;
    output.producer_thread_id = value.producer_thread_id;
    output.required_owner_mask = value.required_owner_mask;
    output.entry_presence_mask = value.owner_entry.presence_mask;
    output.exit_presence_mask = value.owner_exit.presence_mask;
    output.current_owner_mask_at_entry = value.current_owner_mask_at_entry;
    output.current_owner_mask_at_exit = value.current_owner_mask_at_exit;
    output.entry_surface = value.entry_surface;
    output.return_surface = value.return_surface;
    output.participant = value.participant;
    output.owner_validation = value.owner_validation;
    output.binding = value.binding;
    output.platform = StaticInterfacePlatform::pc_win64;
    output.exact_owner_at_entry = value.exact_owner_at_entry;
    output.same_owner_at_publication = value.same_owner_at_publication;
    return output;
}

[[nodiscard]] bool telemetry_ready(const CaptureProvenance& provenance,
                                   std::uint64_t sequence) noexcept {
    return sequence != 0U && provenance.owner_validation != OwnerValidationState::not_published
           && provenance.post_monotonic_tick >= provenance.pre_monotonic_tick;
}

} // namespace

std::uint64_t bounded_hash(std::span<const std::byte> bytes) noexcept {
    std::uint64_t hash = kFnvOffset;
    for (const std::byte value : bytes) {
        hash ^= std::to_integer<std::uint8_t>(value);
        hash *= kFnvPrime;
    }
    return hash;
}

PcBoundaryDescriptor pc_boundary(PcSurface surface) noexcept {
    switch (surface) {
    case PcSurface::runtime_resolver_entry:
        return {0xA0F0E0U, kRuntimeResolverPrefix, PcAbi::runtime_resolver_win64,
                BoundaryKind::function_entry, 597U, false};
    case PcSurface::full_dispatch_entry:
        return {0xC620F0U, kFullDispatchPrefix, PcAbi::dispatch_win64,
                BoundaryKind::function_entry, 348U, false};
    case PcSurface::full_category_resolver_entry:
        return {0xC6CF60U, kCategoryResolverPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 1'041U, false};
    case PcSurface::full_thunk:
        return {0xC5FD80U, kFullThunkBody, PcAbi::internal_unexposed,
                BoundaryKind::thunk, 5U, false};
    case PcSurface::full_handler_entry:
        return {0xC61660U, kFullHandlerPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 1'905U, false};
    case PcSurface::full_type69_branch:
        return {0xC61CA9U, kFullBranchPrefix, PcAbi::internal_unexposed,
                BoundaryKind::instruction_window, 0U, false};
    case PcSurface::full_pre_subscriber_call:
        return {0xC61DC7U, kFullPreCallPrefix, PcAbi::full_subscriber_target_win64,
                BoundaryKind::callsite_window, 0U, false};
    case PcSurface::full_post_subscriber_return:
        return {0xC61DCCU, kFullReturnPrefix, PcAbi::full_subscriber_target_win64,
                BoundaryKind::return_window, 0U, false};
    case PcSurface::full_subscriber_trampoline:
        return {0xB31510U, kFullTrampoline, PcAbi::full_subscriber_target_win64,
                BoundaryKind::exact_function, 19U, false};
    case PcSurface::short_dispatch_entry:
        return {0xC693F0U, kFullDispatchPrefix, PcAbi::dispatch_win64,
                BoundaryKind::function_entry, 348U, false};
    case PcSurface::short_category_resolver_entry:
        return {0xC6D3A0U, kCategoryResolverPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 1'041U, false};
    case PcSurface::short_thunk:
        return {0xC5FD90U, kShortThunkBody, PcAbi::internal_unexposed,
                BoundaryKind::thunk, 5U, false};
    case PcSurface::short_handler_entry:
        return {0xC68B80U, kShortHandlerPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 1'703U, false};
    case PcSurface::short_type69_branch:
        return {0xC69142U, kShortBranchPrefix, PcAbi::internal_unexposed,
                BoundaryKind::instruction_window, 0U, false};
    case PcSurface::short_pre_subscriber_call:
        return {0xC69210U, kShortPreCallPrefix, PcAbi::short_subscriber_target_win64,
                BoundaryKind::callsite_window, 0U, false};
    case PcSurface::short_post_subscriber_return:
        return {0xC69215U, kShortReturnPrefix, PcAbi::short_subscriber_target_win64,
                BoundaryKind::return_window, 0U, false};
    case PcSurface::short_subscriber_trampoline:
        return {0xB31670U, kShortTrampoline, PcAbi::short_subscriber_target_win64,
                 BoundaryKind::exact_function, 19U, false};
    case PcSurface::endpoint_activation_entry:
        return {0xC6B920U, kEndpointActivationPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0x202U, false};
    case PcSurface::endpoint_binder_entry:
        return {0xC6B7A0U, kEndpointBinderPrefix, PcAbi::endpoint_binder_win64,
                BoundaryKind::function_entry, 0x179U, false};
    case PcSurface::endpoint_registration_call:
        return {0xC6B8E6U, kEndpointRegistrationCallPrefix,
                PcAbi::endpoint_registration_target_win64, BoundaryKind::callsite_window,
                0U, false};
    case PcSurface::endpoint_registration_return:
        return {0xC6B8EBU, kEndpointRegistrationReturnPrefix,
                PcAbi::endpoint_registration_target_win64, BoundaryKind::return_window,
                0U, false};
    case PcSurface::endpoint_registration_trampoline:
        return {0x18317B0U, kEndpointRegistrationTrampoline,
                PcAbi::endpoint_registration_target_win64, BoundaryKind::exact_function,
                kEndpointRegistrationTrampoline.size(), false};
    case PcSurface::endpoint_lookup_entry:
        return {0xC6BF00U, kEndpointLookupPrefix, PcAbi::endpoint_lookup_win64,
                BoundaryKind::function_entry, 0xAEU, false};
    case PcSurface::endpoint_copy_entry:
        return {0xC6BFB0U, kEndpointCopyPrefix, PcAbi::endpoint_copy_win64,
                BoundaryKind::function_entry, 0x38U, false};
    case PcSurface::reflected_decoder_entry:
        return {0x4C72E0U, kReflectedDecoderPrefix, PcAbi::reflected_decoder_win64,
                BoundaryKind::function_entry, 0x1CDU, false};
    case PcSurface::reflected_encoder_entry:
        return {0x4C78B0U, kReflectedEncoderBody, PcAbi::reflected_encoder_win64,
                BoundaryKind::exact_function, 0x10U, false};
    case PcSurface::large_serializer_entry:
        return {0x4DBF90U, kLargeSerializerPrefix, PcAbi::schema_serializer_win64,
                BoundaryKind::function_entry, 0xB1U, false};
    case PcSurface::large_serializer_cursor:
        return {0x4DBFF7U, kLargeSerializerCursorPrefix, PcAbi::internal_unexposed,
                BoundaryKind::instruction_window, 0U, false};
    case PcSurface::schema_transport_entry:
        return {0x4FB770U, kSchemaTransportPrefix, PcAbi::schema_transport_win64,
                BoundaryKind::function_entry, 0x12DU, false};
    case PcSurface::schema_transport_encode_call:
        return {0x4FB817U, kSchemaTransportEncodeCallPrefix,
                PcAbi::schema_transport_win64, BoundaryKind::callsite_window, 0U, false};
    case PcSurface::schema_transport_transport_call:
        return {0x4FB848U, kSchemaTransportCallPrefix, PcAbi::schema_transport_win64,
                BoundaryKind::callsite_window, 0U, false};
    case PcSurface::decoded_pointer_getter:
        return {0x4A6340U, kDecodedPointerGetterBody, PcAbi::internal_unexposed,
                BoundaryKind::exact_function, 5U, false};
    case PcSurface::type68_apply_entry:
        return {0x1009C00U, kType68ApplyPrefix, PcAbi::type68_apply_win64,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::type68_authored_resolution_entry:
        return {0x1009430U, kType68AuthoredResolutionPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::type68_event_row_lookup_entry:
        return {0x100A110U, kType68EventRowLookupPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::type68_post_cache_window:
        return {0x1009DF3U, kType68PostCachePrefix, PcAbi::internal_unexposed,
                BoundaryKind::instruction_window, 0U, false};
    case PcSurface::type68_lifecycle0_call:
        return {0x1009D75U, kType68Lifecycle0CallPrefix, PcAbi::internal_unexposed,
                BoundaryKind::callsite_window, 0U, false};
    case PcSurface::type68_lifecycle0_return:
        return {0x1009D7AU, kType68Lifecycle0ReturnPrefix, PcAbi::internal_unexposed,
                BoundaryKind::return_window, 0U, false};
    case PcSurface::type68_formatter_call:
        return {0x1009FCEU, kType68FormatterCallPrefix, PcAbi::internal_unexposed,
                BoundaryKind::callsite_window, 0U, false};
    case PcSurface::type68_formatter_return:
        return {0x1009FD3U, kType68FormatterReturnPrefix, PcAbi::internal_unexposed,
                BoundaryKind::return_window, 0U, false};
    case PcSurface::type68_manager_add_call:
        return {0x1009FEDU, kType68ManagerAddCallPrefix, PcAbi::internal_unexposed,
                BoundaryKind::callsite_window, 0U, false};
    case PcSurface::type68_manager_add_return:
        return {0x1009FF2U, kType68ManagerAddReturnPrefix, PcAbi::internal_unexposed,
                BoundaryKind::return_window, 0U, false};
    case PcSurface::directive_formatter_entry:
        return {0x1008B40U, kDirectiveFormatterPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::manager_add_entry:
        return {0x137BD50U, kManagerAddPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::manager_presentation_update_entry:
        return {0x1382710U, kManagerPresentationUpdatePrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::presentation_queue_entry:
        return {0x137A170U, kPresentationQueuePrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::queue_promote_entry:
        return {0x13A0220U, kQueuePromotePrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    case PcSurface::cui_provider_entry:
        return {0x138E980U, kCuiProviderPrefix, PcAbi::internal_unexposed,
                BoundaryKind::function_entry, 0U, false};
    default:
        return {};
    }
}

bool pc_prefix_matches(PcSurface surface, std::span<const std::byte> observed) noexcept {
    const PcBoundaryDescriptor boundary = pc_boundary(surface);
    return boundary.rva != 0U && !boundary.prefix.empty()
           && observed.size() >= boundary.prefix.size()
           && std::equal(boundary.prefix.begin(), boundary.prefix.end(), observed.begin());
}

ArtifactInspectionResult inspect_file_digest(const wchar_t* path,
                                             FileDigestInspection& output) noexcept {
    output = {};
    if (path == nullptr || *path == L'\0') {
        return ArtifactInspectionResult::invalid_arguments;
    }
    HANDLE file = CreateFileW(path,
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return ArtifactInspectionResult::open_failed;
    }
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0) {
        CloseHandle(file);
        return ArtifactInspectionResult::read_failed;
    }
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U);
    if (status >= 0) {
        status = BCryptCreateHash(algorithm, &hash, nullptr, 0U, nullptr, 0U, 0U);
    }
    std::array<std::byte, 64U * 1024U> buffer{};
    while (status >= 0) {
        DWORD read{};
        if (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)
            == FALSE) {
            status = static_cast<NTSTATUS>(-1);
            break;
        }
        if (read == 0U) {
            break;
        }
        status = BCryptHashData(hash,
                                reinterpret_cast<PUCHAR>(buffer.data()),
                                read,
                                0U);
    }
    if (status >= 0) {
        status = BCryptFinishHash(hash,
                                  reinterpret_cast<PUCHAR>(output.sha256.data()),
                                  static_cast<ULONG>(output.sha256.size()),
                                  0U);
    }
    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm, 0U);
    }
    CloseHandle(file);
    if (status < 0) {
        output = {};
        return ArtifactInspectionResult::hash_failed;
    }
    output.file_bytes = static_cast<std::uint64_t>(size.QuadPart);
    return ArtifactInspectionResult::complete;
}

ArtifactInspectionResult inspect_file_artifact(const wchar_t* path,
                                               ArtifactInspection& output) noexcept {
    output = {};
    if (path == nullptr || *path == L'\0') {
        return ArtifactInspectionResult::invalid_arguments;
    }
    HANDLE file = CreateFileW(path,
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return ArtifactInspectionResult::open_failed;
    }
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0) {
        CloseHandle(file);
        return ArtifactInspectionResult::read_failed;
    }

    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U);
    if (status < 0) {
        CloseHandle(file);
        return ArtifactInspectionResult::hash_failed;
    }
    status = BCryptCreateHash(algorithm, &hash, nullptr, 0U, nullptr, 0U, 0U);
    std::array<std::byte, 64U * 1024U> buffer{};
    if (status >= 0) {
        for (;;) {
            DWORD read{};
            if (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)
                == FALSE) {
                status = static_cast<NTSTATUS>(-1);
                break;
            }
            if (read == 0U) {
                break;
            }
            status = BCryptHashData(hash,
                                    reinterpret_cast<PUCHAR>(buffer.data()),
                                    read,
                                    0U);
            if (status < 0) {
                break;
            }
        }
    }
    if (status >= 0) {
        status = BCryptFinishHash(hash,
                                  reinterpret_cast<PUCHAR>(output.sha256.data()),
                                  static_cast<ULONG>(output.sha256.size()),
                                  0U);
    }
    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    if (status < 0) {
        CloseHandle(file);
        output = {};
        return ArtifactInspectionResult::hash_failed;
    }

    LARGE_INTEGER start{};
    if (SetFilePointerEx(file, start, nullptr, FILE_BEGIN) == FALSE) {
        CloseHandle(file);
        output = {};
        return ArtifactInspectionResult::read_failed;
    }
    IMAGE_DOS_HEADER dos{};
    DWORD read{};
    if (ReadFile(file, &dos, sizeof dos, &read, nullptr) == FALSE || read != sizeof dos
        || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) {
        CloseHandle(file);
        output = {};
        return ArtifactInspectionResult::invalid_pe;
    }
    LARGE_INTEGER ntOffset{};
    ntOffset.QuadPart = dos.e_lfanew;
    if (SetFilePointerEx(file, ntOffset, nullptr, FILE_BEGIN) == FALSE) {
        CloseHandle(file);
        output = {};
        return ArtifactInspectionResult::read_failed;
    }
    IMAGE_NT_HEADERS64 nt{};
    if (ReadFile(file, &nt, sizeof nt, &read, nullptr) == FALSE || read != sizeof nt
        || nt.Signature != IMAGE_NT_SIGNATURE) {
        CloseHandle(file);
        output = {};
        return ArtifactInspectionResult::invalid_pe;
    }
    CloseHandle(file);
    output.file_bytes = static_cast<std::uint64_t>(size.QuadPart);
    output.pe_machine = nt.FileHeader.Machine;
    output.optional_header_magic = nt.OptionalHeader.Magic;
    output.pe_size_of_image = nt.OptionalHeader.SizeOfImage;
    return ArtifactInspectionResult::complete;
}

LiveCohortValidationResult validate_live_pc_runtime_cohort(
    ValidatedRuntimeCohort& output) noexcept {
    output = {};
    HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr) {
        return LiveCohortValidationResult::main_module_unavailable;
    }
    std::array<wchar_t, 32'768U> path{};
    const DWORD pathLength = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (pathLength == 0U || pathLength >= path.size()) {
        return LiveCohortValidationResult::path_resolution_failed;
    }
    std::array<wchar_t, 32'768U> expectedPath{};
    if (GetFullPathNameW(kPinnedPackedRuntimePath,
                         static_cast<DWORD>(expectedPath.size()),
                         expectedPath.data(),
                         nullptr) == 0U
        || _wcsicmp(path.data(), expectedPath.data()) != 0) {
        return LiveCohortValidationResult::packed_path_mismatch;
    }
    ArtifactInspection artifact{};
    if (inspect_file_artifact(path.data(), artifact) != ArtifactInspectionResult::complete) {
        return LiveCohortValidationResult::packed_hash_mismatch;
    }
    if (artifact.file_bytes != kPinnedPackedRuntimeBytes) {
        return LiveCohortValidationResult::packed_size_mismatch;
    }
    if (artifact.sha256 != kPinnedPackedRuntimeSha256) {
        return LiveCohortValidationResult::packed_hash_mismatch;
    }

    IMAGE_DOS_HEADER dos{};
    if (!safe_copy_exact(&dos, module, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew <= 0) {
        return LiveCohortValidationResult::mapped_pe_invalid;
    }
    std::uintptr_t ntAddress{};
    if (!add_pointer(reinterpret_cast<std::uintptr_t>(module),
                     static_cast<std::uintptr_t>(dos.e_lfanew),
                     ntAddress)) {
        return LiveCohortValidationResult::mapped_pe_invalid;
    }
    IMAGE_NT_HEADERS64 nt{};
    if (!safe_copy_exact(&nt, reinterpret_cast<const void*>(ntAddress), sizeof nt)
        || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != kPinnedPcMachineAmd64
        || nt.OptionalHeader.Magic != kPinnedPe32PlusMagic) {
        return LiveCohortValidationResult::mapped_pe_invalid;
    }
    if (nt.OptionalHeader.SizeOfImage != kPinnedPcSizeOfImage) {
        return LiveCohortValidationResult::mapped_image_size_mismatch;
    }
    const auto* base = reinterpret_cast<const std::byte*>(module);
    std::uint64_t cohortHash = kFnvOffset;
    std::uint64_t surfaceMask{};
    for (std::size_t index = 0U; index < kPcSurfaceCount; ++index) {
        const PcSurface surface = static_cast<PcSurface>(index);
        const PcBoundaryDescriptor boundary = pc_boundary(surface);
        if (boundary.rva >= nt.OptionalHeader.SizeOfImage
            || boundary.prefix.size() > nt.OptionalHeader.SizeOfImage - boundary.rva) {
            return LiveCohortValidationResult::surface_out_of_range;
        }
        const std::byte* address = base + boundary.rva;
        if (!executable_range(address, boundary.prefix.size())) {
            return LiveCohortValidationResult::surface_page_not_executable;
        }
        std::array<std::byte, 24U> observed{};
        if (boundary.prefix.size() > observed.size()
            || !safe_copy_exact(observed.data(), address, boundary.prefix.size())
            || !pc_prefix_matches(surface, {observed.data(), boundary.prefix.size()})) {
            return LiveCohortValidationResult::cohort_prefix_mismatch;
        }
        cohortHash ^= boundary.rva;
        cohortHash *= kFnvPrime;
        cohortHash ^= bounded_hash({observed.data(), boundary.prefix.size()});
        cohortHash *= kFnvPrime;
        surfaceMask |= 1ULL << index;
    }
    const std::uint64_t buildId = bounded_hash(kPinnedPackedRuntimeSha256);
    output.validation_cookie_ = buildId ^ cohortHash
                                ^ reinterpret_cast<std::uintptr_t>(module) ^ kCohortCookieSalt;
    if (output.validation_cookie_ == 0U) {
        output.validation_cookie_ = kCohortCookieSalt;
    }
    output.build_id_ = buildId;
    output.cohort_id_ = cohortHash;
    output.module_base_ = reinterpret_cast<std::uintptr_t>(module);
    output.size_of_image_ = nt.OptionalHeader.SizeOfImage;
    output.surface_mask_ = surfaceMask;
    return LiveCohortValidationResult::valid;
}

bool runtime_record_projection(const RuntimeRecordImage& record,
                               Type69Payload& output,
                               std::uint8_t& type) noexcept {
    type = std::to_integer<std::uint8_t>(record[kRuntimeRecordTypeOffset]);
    std::memcpy(&output, record.data() + kRuntimeRecordBodyOffset, sizeof output);
    return type == kType69;
}

bool descriptor_projection(const void* descriptor, DescriptorProjection& output) noexcept {
    output = {};
    if (descriptor == nullptr
        || !safe_copy_exact(&output.payload, descriptor, sizeof output.payload)
        || !safe_copy_exact(&output.type,
                            static_cast<const std::byte*>(descriptor) + kActionTypeOffset,
                            sizeof output.type)) {
        output = {};
        return false;
    }
    return true;
}

bool resolver_failure_sentinel(const Type69Payload& payload) noexcept {
    constexpr std::array<float, 3U> zero{};
    return payload.endpoint_index == -1
           && payload.auxiliary == (std::numeric_limits<std::uint32_t>::max)()
           && payload.operation == 0xFFU && selectable_is_failure_sentinel(payload)
           && std::memcmp(payload.payload_xyz.data(), zero.data(), sizeof zero) == 0;
}

bool owner_fields_present(const OwnerGenerationSet& snapshot,
                          std::uint32_t requiredMask) noexcept {
    return (snapshot.presence_mask & requiredMask) == requiredMask
           && (current_owner_mask(snapshot) & requiredMask) == requiredMask;
}

CaptureCohortOwner::CaptureCohortOwner() noexcept {
    owner_instance_id_ = gNextOwnerInstance.fetch_add(1U, std::memory_order_relaxed);
    if (owner_instance_id_ == 0U) {
        owner_instance_id_ = gNextOwnerInstance.fetch_add(1U, std::memory_order_relaxed);
    }
    required_original_mask_ = (1ULL << kNativeParticipantCount) - 1U;
}

OwnerResult CaptureCohortOwner::begin_install(const ValidatedRuntimeCohort& cohort,
                                              std::uint64_t freshEpoch) noexcept {
    if (phase_.load(std::memory_order_acquire) != CohortPhase::detached) {
        return OwnerResult::wrong_phase;
    }
    if (!cohort.valid()) {
        return OwnerResult::invalid_cohort;
    }
    if (freshEpoch == 0U || freshEpoch <= last_detached_epoch_
        || freshEpoch == (std::numeric_limits<std::uint64_t>::max)()) {
        return OwnerResult::invalid_epoch;
    }
    capture_epoch_ = freshEpoch;
    build_id_ = cohort.build_id_;
    cohort_id_ = cohort.cohort_id_;
    for (auto& original : originals_) {
        original.store(0U, std::memory_order_relaxed);
    }
    next_call_id_.store(1U, std::memory_order_relaxed);
    producer_open_.store(false, std::memory_order_release);
    drain_open_.store(true, std::memory_order_release);
    drain_complete_.store(false, std::memory_order_release);
    phase_.store(CohortPhase::installing, std::memory_order_release);
    return OwnerResult::success;
}

OwnerResult CaptureCohortOwner::publish_original(NativeParticipant participant,
                                                 std::uintptr_t original) noexcept {
    const std::size_t index = participant_index(participant);
    if (phase_.load(std::memory_order_acquire) != CohortPhase::installing) {
        return OwnerResult::wrong_phase;
    }
    if (index >= originals_.size()) {
        return OwnerResult::invalid_participant;
    }
    if (original == 0U) {
        return OwnerResult::original_missing;
    }
    originals_[index].store(original, std::memory_order_release);
    return OwnerResult::success;
}

OwnerResult CaptureCohortOwner::publish_generations(
    const OwnerGenerationSet& snapshotValue) noexcept {
    std::uint64_t revision = generation_revision_.load(std::memory_order_relaxed);
    if ((revision & 1U) != 0U
        || !generation_revision_.compare_exchange_strong(revision,
                                                         revision + 1U,
                                                         std::memory_order_acq_rel,
                                                         std::memory_order_relaxed)) {
        return OwnerResult::generation_unavailable;
    }
    const std::array<std::uint64_t, 14U> words{
        snapshotValue.presence_mask,
        snapshotValue.module_generation,
        snapshotValue.activation_generation,
        snapshotValue.activity_session_id,
        snapshotValue.activity_generation,
        snapshotValue.connection_generation,
        snapshotValue.roster_generation,
        snapshotValue.registry_materialization_generation,
        snapshotValue.endpoint_vector_generation,
        snapshotValue.authority_generation,
        snapshotValue.run_token,
        snapshotValue.correlation_token,
        snapshotValue.component_generation,
        static_cast<std::uint64_t>(snapshotValue.activation_state)};
    for (std::size_t index = 0U; index < words.size(); ++index) {
        generation_words_[index].store(words[index], std::memory_order_relaxed);
    }
    generation_revision_.store(revision + 2U, std::memory_order_release);
    return OwnerResult::success;
}

bool CaptureCohortOwner::read_generations(OwnerGenerationSet& output,
                                          std::uint64_t& revision) const noexcept {
    for (std::size_t attempt = 0U; attempt < 3U; ++attempt) {
        const std::uint64_t before = generation_revision_.load(std::memory_order_acquire);
        if (before == 0U || (before & 1U) != 0U) {
            continue;
        }
        std::array<std::uint64_t, 14U> words{};
        for (std::size_t index = 0U; index < words.size(); ++index) {
            words[index] = generation_words_[index].load(std::memory_order_relaxed);
        }
        const std::uint64_t after = generation_revision_.load(std::memory_order_acquire);
        if (before != after || (after & 1U) != 0U) {
            continue;
        }
        output = OwnerGenerationSet{static_cast<std::uint32_t>(words[0]),
                                    words[1], words[2], words[3], words[4], words[5], words[6],
                                    words[7], words[8], words[9], words[10], words[11], words[12],
                                    static_cast<ActivationSnapshotState>(words[13])};
        revision = after;
        return true;
    }
    output = {};
    revision = 0U;
    return false;
}

OwnerResult CaptureCohortOwner::start_running() noexcept {
    if (phase_.load(std::memory_order_acquire) != CohortPhase::installing) {
        return OwnerResult::wrong_phase;
    }
    std::uint64_t mask{};
    for (std::size_t index = 0U; index < originals_.size(); ++index) {
        if (originals_[index].load(std::memory_order_acquire) != 0U) {
            mask |= 1ULL << index;
        }
    }
    if (mask != required_original_mask_) {
        return OwnerResult::originals_incomplete;
    }
    OwnerGenerationSet generations{};
    std::uint64_t revision{};
    if (!read_generations(generations, revision)
        || !owner_fields_present(generations, kNativeRequiredOwnerMask)) {
        return OwnerResult::generation_unavailable;
    }
    static_cast<void>(revision);
    producer_open_.store(true, std::memory_order_release);
    phase_.store(CohortPhase::running, std::memory_order_release);
    return OwnerResult::success;
}

OwnerResult CaptureCohortOwner::try_enter(NativeParticipant participant,
                                          CaptureTiming timing,
                                          ParticipantLease& output) noexcept {
    if (output.active()) {
        return OwnerResult::wrong_phase;
    }
    const std::size_t index = participant_index(participant);
    if (index >= active_calls_.size()) {
        return OwnerResult::invalid_participant;
    }
    active_calls_[index].fetch_add(1U, std::memory_order_acq_rel);
    if (phase_.load(std::memory_order_acquire) != CohortPhase::running
        || !producer_open_.load(std::memory_order_acquire)) {
        active_calls_[index].fetch_sub(1U, std::memory_order_release);
        return OwnerResult::producer_closed;
    }
    const std::uintptr_t original = originals_[index].load(std::memory_order_acquire);
    if (original == 0U) {
        active_calls_[index].fetch_sub(1U, std::memory_order_release);
        return OwnerResult::original_missing;
    }
    OwnerGenerationSet entry{};
    std::uint64_t revision{};
    if (!read_generations(entry, revision)) {
        active_calls_[index].fetch_sub(1U, std::memory_order_release);
        return OwnerResult::generation_unavailable;
    }
    const std::uint64_t callId = next_call_id_.fetch_add(1U, std::memory_order_relaxed);
    if (callId == 0U || callId == (std::numeric_limits<std::uint64_t>::max)()) {
        active_calls_[index].fetch_sub(1U, std::memory_order_release);
        return OwnerResult::call_id_exhausted;
    }
    CaptureProvenance provenance{};
    provenance.build_id = build_id_;
    provenance.cohort_id = cohort_id_;
    provenance.capture_epoch = capture_epoch_;
    provenance.owner_instance_id = owner_instance_id_;
    provenance.call_id = callId;
    provenance.entry_owner_revision = revision;
    provenance.pre_monotonic_tick = timing.pre_monotonic_tick;
    provenance.caller_rva = timing.caller_rva;
    provenance.owner_entry = entry;
    provenance.producer_thread_id = timing.producer_thread_id;
    provenance.required_owner_mask = kNativeRequiredOwnerMask;
    provenance.current_owner_mask_at_entry = current_owner_mask(entry);
    provenance.entry_surface = participant_entry_surface(participant);
    provenance.return_surface = participant_return_surface(participant);
    provenance.participant = participant;
    provenance.binding = BindingState::materialization_unobserved;
    provenance.exact_owner_at_entry =
        owner_fields_present(entry, provenance.required_owner_mask);
    output.owner_ = this;
    output.provenance_ = provenance;
    output.original_address_ = original;
    output.participant_ = participant;
    output.observe_ = true;
    return OwnerResult::success;
}

void CaptureCohortOwner::leave(NativeParticipant participant) noexcept {
    const std::size_t index = participant_index(participant);
    if (index < active_calls_.size()) {
        active_calls_[index].fetch_sub(1U, std::memory_order_release);
    }
}

void CaptureCohortOwner::quiesce() noexcept {
    CohortPhase expected = CohortPhase::running;
    if (phase_.compare_exchange_strong(expected,
                                       CohortPhase::quiescing,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed)) {
        producer_open_.store(false, std::memory_order_release);
    }
}

void CaptureCohortOwner::close_drain_admission() noexcept {
    drain_open_.store(false, std::memory_order_release);
}

OwnerResult CaptureCohortOwner::try_enter_drain(DrainLease& output) noexcept {
    if (output.active()) {
        return OwnerResult::wrong_phase;
    }
    active_drains_.fetch_add(1U, std::memory_order_acq_rel);
    if (!drain_open_.load(std::memory_order_acquire)) {
        active_drains_.fetch_sub(1U, std::memory_order_release);
        return OwnerResult::drain_open;
    }
    output.owner_ = this;
    return OwnerResult::success;
}

void CaptureCohortOwner::leave_drain() noexcept {
    active_drains_.fetch_sub(1U, std::memory_order_release);
}

OwnerResult CaptureCohortOwner::mark_drain_complete_if_empty(
    const CaptureQueueAggregate& queues) noexcept {
    if (queues.epoch() != capture_epoch_) {
        return OwnerResult::invalid_epoch;
    }
    if (!queues.empty()) {
        return OwnerResult::drain_incomplete;
    }
    return mark_drain_complete();
}

OwnerResult CaptureCohortOwner::mark_drain_complete() noexcept {
    if (drain_open_.load(std::memory_order_acquire)) {
        return OwnerResult::drain_open;
    }
    if (active_drains_.load(std::memory_order_acquire) != 0U) {
        return OwnerResult::drain_active;
    }
    for (const auto& active : active_calls_) {
        if (active.load(std::memory_order_acquire) != 0U) {
            return OwnerResult::active_calls;
        }
    }
    drain_complete_.store(true, std::memory_order_release);
    return OwnerResult::success;
}

OwnerResult CaptureCohortOwner::protected_detach(
    ProtectedDetachDisposition disposition) noexcept {
    const CohortPhase phase = phase_.load(std::memory_order_acquire);
    if (phase != CohortPhase::quiescing && phase != CohortPhase::protected_retained) {
        return OwnerResult::wrong_phase;
    }
    for (const auto& active : active_calls_) {
        if (active.load(std::memory_order_acquire) != 0U) {
            return OwnerResult::active_calls;
        }
    }
    if (drain_open_.load(std::memory_order_acquire)) {
        return OwnerResult::drain_open;
    }
    if (active_drains_.load(std::memory_order_acquire) != 0U) {
        return OwnerResult::drain_active;
    }
    if (!drain_complete_.load(std::memory_order_acquire)) {
        return OwnerResult::drain_incomplete;
    }
    if (disposition != ProtectedDetachDisposition::removed) {
        phase_.store(CohortPhase::protected_retained, std::memory_order_release);
        return OwnerResult::success;
    }
    for (auto& original : originals_) {
        original.store(0U, std::memory_order_release);
    }
    last_detached_epoch_ = capture_epoch_;
    capture_epoch_ = 0U;
    build_id_ = 0U;
    cohort_id_ = 0U;
    phase_.store(CohortPhase::detached, std::memory_order_release);
    return OwnerResult::success;
}

OwnerResult CaptureCohortOwner::reopen_retained_for_detach() noexcept {
    CohortPhase expected = CohortPhase::protected_retained;
    return phase_.compare_exchange_strong(expected,
                                          CohortPhase::quiescing,
                                          std::memory_order_acq_rel,
                                          std::memory_order_relaxed)
               ? OwnerResult::success
               : OwnerResult::wrong_phase;
}

OwnerSnapshot CaptureCohortOwner::snapshot() const noexcept {
    OwnerSnapshot result{};
    result.phase = phase_.load(std::memory_order_acquire);
    result.capture_epoch = capture_epoch_;
    result.last_detached_epoch = last_detached_epoch_;
    for (std::size_t index = 0U; index < originals_.size(); ++index) {
        if (originals_[index].load(std::memory_order_acquire) != 0U) {
            result.published_original_mask |= 1ULL << index;
        }
        result.active_calls[index] = active_calls_[index].load(std::memory_order_acquire);
    }
    result.active_drains = active_drains_.load(std::memory_order_acquire);
    result.producer_open = producer_open_.load(std::memory_order_acquire);
    result.drain_open = drain_open_.load(std::memory_order_acquire);
    result.drain_complete = drain_complete_.load(std::memory_order_acquire);
    return result;
}

ParticipantLease::~ParticipantLease() noexcept { release(); }

ParticipantLease::ParticipantLease(ParticipantLease&& other) noexcept {
    *this = std::move(other);
}

ParticipantLease& ParticipantLease::operator=(ParticipantLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
        provenance_ = other.provenance_;
        original_address_ = other.original_address_;
        participant_ = other.participant_;
        observe_ = other.observe_;
        other.original_address_ = 0U;
        other.participant_ = NativeParticipant::count;
        other.observe_ = false;
    }
    return *this;
}

void ParticipantLease::release() noexcept {
    if (owner_ != nullptr) {
        owner_->leave(participant_);
        owner_ = nullptr;
    }
    original_address_ = 0U;
    participant_ = NativeParticipant::count;
    observe_ = false;
}

bool ParticipantLease::revalidate_for_publication(CaptureProvenance& provenance,
                                                  std::uint64_t postTick) noexcept {
    if (owner_ == nullptr || provenance.call_id != provenance_.call_id
        || postTick < provenance.pre_monotonic_tick) {
        provenance.owner_validation = OwnerValidationState::provider_unstable;
        return false;
    }
    OwnerGenerationSet exit{};
    std::uint64_t revision{};
    provenance.post_monotonic_tick = postTick;
    if (!owner_->read_generations(exit, revision)) {
        provenance.owner_validation = OwnerValidationState::provider_unstable;
        return false;
    }
    provenance.owner_exit = exit;
    provenance.exit_owner_revision = revision;
    provenance.current_owner_mask_at_exit = current_owner_mask(exit);
    provenance.same_owner_at_publication =
        provenance.entry_owner_revision == revision && provenance.owner_entry == exit
        && (provenance.current_owner_mask_at_exit & provenance.required_owner_mask)
               == provenance.required_owner_mask;
    provenance.owner_validation = provenance.same_owner_at_publication
                                      ? OwnerValidationState::exact_same_owner
                                      : OwnerValidationState::readable_but_stale;
    return provenance.same_owner_at_publication;
}

DrainLease::~DrainLease() noexcept { release(); }

DrainLease::DrainLease(DrainLease&& other) noexcept {
    owner_ = std::exchange(other.owner_, nullptr);
}

DrainLease& DrainLease::operator=(DrainLease&& other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
    }
    return *this;
}

void DrainLease::release() noexcept {
    if (owner_ != nullptr) {
        owner_->leave_drain();
        owner_ = nullptr;
    }
}

CaptureBuildResult prepare_resolver_capture(PendingResolverCapture& pending,
                                            const ParticipantLease& lease,
                                            const std::uint16_t* activityRuntimeId,
                                            const void* record,
                                            std::uint8_t priorResult) noexcept {
    pending.provenance_ = {};
    pending.runtime_identity_ = 0U;
    pending.record_identity_ = 0U;
    pending.runtime_id_pre_ = 0U;
    pending.prior_result_ = 0U;
    pending.record_pre_.fill(std::byte{});
    pending.payload_pre_ = {};
    pending.ready_ = false;
    if (!lease.active() || lease.participant() != NativeParticipant::resolver) {
        return CaptureBuildResult::wrong_participant;
    }
    if (activityRuntimeId == nullptr || record == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    RuntimeRecordImage pre{};
    std::uint16_t runtimeId{};
    if (!safe_copy_exact(&runtimeId, activityRuntimeId, sizeof runtimeId)
        || !safe_copy_exact(pre.data(), record, pre.size())) {
        return CaptureBuildResult::unreadable;
    }
    Type69Payload payload{};
    std::uint8_t type{};
    if (!runtime_record_projection(pre, payload, type)) {
        return CaptureBuildResult::wrong_type;
    }
    pending.provenance_ = lease.entry_provenance();
    pending.runtime_identity_ = reinterpret_cast<std::uintptr_t>(activityRuntimeId);
    pending.record_identity_ = reinterpret_cast<std::uintptr_t>(record);
    pending.runtime_id_pre_ = runtimeId;
    pending.prior_result_ = priorResult;
    pending.record_pre_ = pre;
    pending.payload_pre_ = payload;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_resolver_capture(PendingResolverCapture& pending,
                                           const ParticipantLease& lease,
                                           const void* record,
                                           std::uint8_t originalResult,
                                           std::uint64_t postMonotonicTick,
                                           ResolverCaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (!lease.active() || lease.entry_provenance().call_id != pending.provenance_.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    if (postMonotonicTick < pending.provenance_.pre_monotonic_tick) {
        return CaptureBuildResult::timing_invalid;
    }
    if (reinterpret_cast<std::uintptr_t>(record) != pending.record_identity_) {
        return CaptureBuildResult::identity_mismatch;
    }
    RuntimeRecordImage post{};
    Type69Payload postPayload{};
    std::uint8_t postType{};
    const bool postValid = safe_copy_exact(post.data(), record, post.size())
                           && runtime_record_projection(post, postPayload, postType);
    ResolverCaptureRecord captured{};
    captured.provenance = pending.provenance_;
    captured.runtime_identity = pending.runtime_identity_;
    captured.record_identity = pending.record_identity_;
    captured.runtime_id_pre = pending.runtime_id_pre_;
    captured.prior_result = pending.prior_result_;
    captured.original_result = originalResult;
    captured.record_pre = pending.record_pre_;
    captured.record_post = post;
    captured.payload_pre = pending.payload_pre_;
    captured.payload_post = postPayload;
    captured.record_pre_hash = bounded_hash(captured.record_pre);
    captured.record_post_hash = postValid ? bounded_hash(captured.record_post) : 0U;
    captured.pre_valid = true;
    captured.post_valid = postValid;
    output = captured;
    return postValid ? CaptureBuildResult::complete : CaptureBuildResult::partial;
}

CaptureBuildResult prepare_dispatch_capture(PendingDispatchCapture& pending,
                                            const ParticipantLease& lease,
                                            const void* activityRuntime,
                                            const void* descriptor,
                                            const void* dynamicContext) noexcept {
    if (pending.stack_pushed_) {
        pop_dispatch(pending.stack_depth_, pending.provenance_.call_id);
    }
    pending.provenance_ = {};
    pending.activity_runtime_identity_ = 0U;
    pending.descriptor_identity_ = 0U;
    pending.dynamic_context_identity_ = 0U;
    pending.descriptor_pre_ = {};
    pending.kind_ = DispatchKind::full_apply;
    pending.stack_depth_ = 0U;
    pending.stack_pushed_ = false;
    pending.ready_ = false;
    const NativeParticipant participant = lease.participant();
    if (!lease.active()
        || (participant != NativeParticipant::full_dispatch
            && participant != NativeParticipant::short_dispatch)) {
        return CaptureBuildResult::wrong_participant;
    }
    if (activityRuntime == nullptr || descriptor == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    DescriptorProjection pre{};
    if (!descriptor_projection(descriptor, pre)) {
        return CaptureBuildResult::unreadable;
    }
    if (pre.type != kType69) {
        return CaptureBuildResult::wrong_type;
    }
    pending.provenance_ = lease.entry_provenance();
    pending.activity_runtime_identity_ = reinterpret_cast<std::uintptr_t>(activityRuntime);
    pending.descriptor_identity_ = reinterpret_cast<std::uintptr_t>(descriptor);
    pending.dynamic_context_identity_ = reinterpret_cast<std::uintptr_t>(dynamicContext);
    pending.descriptor_pre_ = pre;
    pending.kind_ = participant == NativeParticipant::full_dispatch
                        ? DispatchKind::full_apply
                        : DispatchKind::paired_short_form;
    const DispatchStackEntry stackEntry{pending.provenance_.owner_instance_id,
                                        pending.provenance_.capture_epoch,
                                        pending.provenance_.call_id,
                                        pending.kind_,
                                        pre};
    if (!push_dispatch(stackEntry, pending.stack_depth_)) {
        return CaptureBuildResult::no_parent_dispatch;
    }
    pending.stack_pushed_ = true;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

PendingDispatchCapture::~PendingDispatchCapture() noexcept {
    if (stack_pushed_) {
        pop_dispatch(stack_depth_, provenance_.call_id);
        stack_pushed_ = false;
    }
}

CaptureBuildResult finish_dispatch_capture(PendingDispatchCapture& pending,
                                           const ParticipantLease& lease,
                                           const void* descriptor,
                                           std::uint64_t postMonotonicTick,
                                           DispatchCaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (pending.stack_pushed_) {
        pop_dispatch(pending.stack_depth_, pending.provenance_.call_id);
        pending.stack_pushed_ = false;
    }
    if (!lease.active() || lease.entry_provenance().call_id != pending.provenance_.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    if (postMonotonicTick < pending.provenance_.pre_monotonic_tick) {
        return CaptureBuildResult::timing_invalid;
    }
    if (reinterpret_cast<std::uintptr_t>(descriptor) != pending.descriptor_identity_) {
        return CaptureBuildResult::identity_mismatch;
    }
    DescriptorProjection post{};
    const bool postValid = descriptor_projection(descriptor, post) && post.type == kType69;
    DispatchCaptureRecord captured{};
    captured.provenance = pending.provenance_;
    captured.activity_runtime_identity = pending.activity_runtime_identity_;
    captured.descriptor_identity = pending.descriptor_identity_;
    captured.dynamic_context_identity = pending.dynamic_context_identity_;
    captured.descriptor_pre = pending.descriptor_pre_;
    captured.descriptor_post = post;
    captured.descriptor_pre_hash = projection_hash(captured.descriptor_pre);
    captured.descriptor_post_hash = postValid ? projection_hash(captured.descriptor_post) : 0U;
    captured.kind = pending.kind_;
    captured.pre_valid = true;
    captured.post_valid = postValid;
    output = captured;
    return postValid ? CaptureBuildResult::complete : CaptureBuildResult::partial;
}

CaptureBuildResult prepare_full_callsite_capture(PendingCallsiteCapture& pending,
                                                 const ParticipantLease& lease,
                                                 const void* interfacePair,
                                                 std::uint8_t operation,
                                                 std::int32_t resolvedValue,
                                                 const float* payload4,
                                                 std::uint32_t auxiliary) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (!lease.active() || lease.participant() != NativeParticipant::full_callsite) {
        return CaptureBuildResult::wrong_participant;
    }
    if (interfacePair == nullptr || payload4 == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    const DispatchStackEntry* parent = parent_dispatch(lease, DispatchKind::full_apply);
    if (parent == nullptr) {
        return CaptureBuildResult::no_parent_dispatch;
    }
    std::array<float, 4U> payload{};
    if (!safe_copy_exact(payload.data(), payload4, sizeof payload)) {
        return CaptureBuildResult::unreadable;
    }
    pending.record_.provenance = lease.entry_provenance();
    pending.record_.provenance.parent_call_id = parent->call_id;
    pending.record_.provenance.binding = BindingState::materialization_unobserved;
    pending.record_.resolution =
        resolve_subscriber_candidate(interfacePair, static_cast<std::uint32_t>(kFullSubscriberMethodSlot));
    pending.record_.parent_descriptor = parent->descriptor;
    pending.record_.payload4 = payload;
    pending.record_.resolved_value = resolvedValue;
    pending.record_.auxiliary = auxiliary;
    pending.record_.operation = operation;
    pending.record_.kind = SubscriberKind::full_slot_30;
    pending.record_.arguments_match_parent =
        operation == parent->descriptor.payload.operation
        && auxiliary == parent->descriptor.payload.auxiliary
        && payload[0] == parent->descriptor.payload.payload_xyz[0]
        && payload[1] == parent->descriptor.payload.payload_xyz[1]
        && payload[2] == parent->descriptor.payload.payload_xyz[2] && payload[3] == 1.0F;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult prepare_short_callsite_capture(PendingCallsiteCapture& pending,
                                                  const ParticipantLease& lease,
                                                  const void* interfacePair,
                                                  std::uint32_t auxiliary) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (!lease.active() || lease.participant() != NativeParticipant::short_callsite) {
        return CaptureBuildResult::wrong_participant;
    }
    if (interfacePair == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    const DispatchStackEntry* parent =
        parent_dispatch(lease, DispatchKind::paired_short_form);
    if (parent == nullptr) {
        return CaptureBuildResult::no_parent_dispatch;
    }
    pending.record_.provenance = lease.entry_provenance();
    pending.record_.provenance.parent_call_id = parent->call_id;
    pending.record_.provenance.binding = BindingState::materialization_unobserved;
    pending.record_.resolution = resolve_subscriber_candidate(
        interfacePair, static_cast<std::uint32_t>(kShortSubscriberMethodSlot));
    pending.record_.parent_descriptor = parent->descriptor;
    pending.record_.auxiliary = auxiliary;
    pending.record_.kind = SubscriberKind::short_slot_48;
    pending.record_.arguments_match_parent = auxiliary == parent->descriptor.payload.auxiliary;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_callsite_capture(PendingCallsiteCapture& pending,
                                           const ParticipantLease& lease,
                                           std::uint64_t postMonotonicTick,
                                           CallsiteCaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (!lease.active()
        || lease.entry_provenance().call_id != pending.record_.provenance.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    if (postMonotonicTick < pending.record_.provenance.pre_monotonic_tick) {
        return CaptureBuildResult::timing_invalid;
    }
    pending.record_.return_observed = true;
    output = pending.record_;
    return CaptureBuildResult::complete;
}

CaptureBuildResult prepare_endpoint_binder_capture(PendingEndpointBinderCapture& pending,
                                                   const ParticipantLease& lease,
                                                   const void* endpointOwner,
                                                   std::uint32_t count) noexcept {
    if (pending.stack_pushed_) {
        pop_endpoint_binder(pending.stack_depth_, pending.provenance_.call_id);
    }
    pending.provenance_ = {};
    pending.endpoint_owner_ = 0U;
    pending.requested_count_ = 0U;
    pending.container_handle_pre_ = 0U;
    pending.cleared_field_pre_ = 0U;
    pending.owner_count_pre_ = 0U;
    pending.stack_depth_ = 0U;
    pending.stack_pushed_ = false;
    pending.ready_ = false;
    if (!lease.active() || lease.participant() != NativeParticipant::endpoint_binder) {
        return CaptureBuildResult::wrong_participant;
    }
    if (endpointOwner == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    const auto* bytes = static_cast<const std::byte*>(endpointOwner);
    if (!safe_copy_exact(&pending.container_handle_pre_,
                         bytes + kEndpointOwnerContainerHandleOffset,
                         sizeof pending.container_handle_pre_)
        || !safe_copy_exact(&pending.cleared_field_pre_,
                            bytes + kEndpointOwnerClearedOffset,
                            sizeof pending.cleared_field_pre_)
        || !safe_copy_exact(&pending.owner_count_pre_,
                            bytes + kEndpointOwnerCountOffset,
                            sizeof pending.owner_count_pre_)) {
        return CaptureBuildResult::unreadable;
    }
    pending.provenance_ = lease.entry_provenance();
    pending.provenance_.binding = BindingState::observed_candidate_not_joined;
    pending.endpoint_owner_ = reinterpret_cast<std::uintptr_t>(endpointOwner);
    pending.requested_count_ = count;
    const EndpointBinderStackEntry stackEntry{pending.provenance_.owner_instance_id,
                                               pending.provenance_.capture_epoch,
                                               pending.provenance_.call_id,
                                               pending.endpoint_owner_,
                                               count,
                                               0U};
    if (!push_endpoint_binder(stackEntry, pending.stack_depth_)) {
        return CaptureBuildResult::no_parent_dispatch;
    }
    pending.stack_pushed_ = true;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

PendingEndpointBinderCapture::~PendingEndpointBinderCapture() noexcept {
    if (stack_pushed_) {
        pop_endpoint_binder(stack_depth_, provenance_.call_id);
        stack_pushed_ = false;
    }
}

CaptureBuildResult finish_endpoint_binder_capture(PendingEndpointBinderCapture& pending,
                                                  const ParticipantLease& lease,
                                                  const void* endpointOwner,
                                                  bool originalResult,
                                                  std::uint64_t postMonotonicTick,
                                                  EndpointBinderCaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (pending.stack_pushed_) {
        pop_endpoint_binder(pending.stack_depth_, pending.provenance_.call_id);
        pending.stack_pushed_ = false;
    }
    if (!lease.active() || lease.participant() != NativeParticipant::endpoint_binder
        || lease.entry_provenance().call_id != pending.provenance_.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    if (postMonotonicTick < pending.provenance_.pre_monotonic_tick) {
        return CaptureBuildResult::timing_invalid;
    }
    if (reinterpret_cast<std::uintptr_t>(endpointOwner) != pending.endpoint_owner_) {
        return CaptureBuildResult::identity_mismatch;
    }
    EndpointBinderCaptureRecord captured{};
    captured.provenance = pending.provenance_;
    captured.endpoint_owner = pending.endpoint_owner_;
    captured.requested_count = pending.requested_count_;
    captured.container_handle_pre = pending.container_handle_pre_;
    captured.cleared_field_pre = pending.cleared_field_pre_;
    captured.owner_count_pre = pending.owner_count_pre_;
    captured.original_result = originalResult;
    captured.pre_valid = true;
    const auto* bytes = static_cast<const std::byte*>(endpointOwner);
    captured.post_valid = safe_copy_exact(&captured.container_handle_post,
                                          bytes + kEndpointOwnerContainerHandleOffset,
                                          sizeof captured.container_handle_post)
                          && safe_copy_exact(&captured.cleared_field_post,
                                             bytes + kEndpointOwnerClearedOffset,
                                             sizeof captured.cleared_field_post)
                          && safe_copy_exact(&captured.owner_count_post,
                                             bytes + kEndpointOwnerCountOffset,
                                             sizeof captured.owner_count_post);
    output = captured;
    return captured.post_valid ? CaptureBuildResult::complete : CaptureBuildResult::partial;
}

CaptureBuildResult prepare_endpoint_registration_capture(
    PendingEndpointRegistrationCapture& pending,
    const ParticipantLease& lease,
    const void* interfacePair,
    std::uint32_t ownerHandle) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (!lease.active()
        || lease.participant() != NativeParticipant::endpoint_registration_callsite) {
        return CaptureBuildResult::wrong_participant;
    }
    if (interfacePair == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    EndpointBinderStackEntry* parent = parent_endpoint_binder(lease);
    if (parent == nullptr) {
        return CaptureBuildResult::no_parent_dispatch;
    }
    if (parent->observed_registrations >= parent->requested_count) {
        return CaptureBuildResult::arguments_do_not_match_parent;
    }
    const std::uintptr_t pairAddress = reinterpret_cast<std::uintptr_t>(interfacePair);
    if (pairAddress < kEndpointInterfaceHalfOffset) {
        return CaptureBuildResult::identity_mismatch;
    }
    const std::uintptr_t rowAddress = pairAddress - kEndpointInterfaceHalfOffset;
    std::uintptr_t keyAddress{};
    if (!add_pointer(rowAddress, kEndpointRuntimeKeyOffset, keyAddress)
        || !safe_copy_exact(pending.record_.runtime_key_pre.data(),
                            reinterpret_cast<const void*>(keyAddress),
                            pending.record_.runtime_key_pre.size())) {
        return CaptureBuildResult::unreadable;
    }
    pending.record_.provenance = lease.entry_provenance();
    pending.record_.provenance.parent_call_id = parent->call_id;
    pending.record_.provenance.binding = BindingState::observed_candidate_not_joined;
    pending.record_.row_identity = rowAddress;
    pending.record_.interface_pair = pairAddress;
    pending.record_.owner_handle = ownerHandle;
    pending.record_.observed_ordinal = parent->observed_registrations++;
    pending.record_.runtime_key_pre_hash = bounded_hash(pending.record_.runtime_key_pre);
    pending.record_.registration_candidate = resolve_subscriber_candidate(
        interfacePair, static_cast<std::uint32_t>(kEndpointRegistrationMethodSlot));
    pending.record_.pre_valid = true;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_endpoint_registration_capture(
    PendingEndpointRegistrationCapture& pending,
    const ParticipantLease& lease,
    std::uint64_t postMonotonicTick,
    EndpointRegistrationCaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (!lease.active()
        || lease.participant() != NativeParticipant::endpoint_registration_callsite
        || lease.entry_provenance().call_id != pending.record_.provenance.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    if (postMonotonicTick < pending.record_.provenance.pre_monotonic_tick) {
        return CaptureBuildResult::timing_invalid;
    }
    std::uintptr_t keyAddress{};
    pending.record_.post_valid = add_pointer(pending.record_.row_identity,
                                             kEndpointRuntimeKeyOffset,
                                             keyAddress)
                                 && safe_copy_exact(pending.record_.runtime_key_post.data(),
                                                    reinterpret_cast<const void*>(keyAddress),
                                                    pending.record_.runtime_key_post.size());
    pending.record_.runtime_key_post_hash =
        pending.record_.post_valid ? bounded_hash(pending.record_.runtime_key_post) : 0U;
    pending.record_.return_observed = true;
    output = pending.record_;
    return pending.record_.post_valid ? CaptureBuildResult::complete
                                      : CaptureBuildResult::partial;
}

CaptureBuildResult prepare_type68_serializer_capture(PendingType68SerializerCapture& pending,
                                                     const ParticipantLease& lease,
                                                     std::uint32_t schema,
                                                     const void* decoded,
                                                     const void* outputBuffer,
                                                     const std::int32_t* outputBytes) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (!lease.active() || lease.participant() != NativeParticipant::large_serializer) {
        return CaptureBuildResult::wrong_participant;
    }
    if (schema != kType68AuthoritySchema) {
        return CaptureBuildResult::wrong_type;
    }
    if (decoded == nullptr || outputBuffer == nullptr || outputBytes == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    if (!safe_copy_exact(pending.record_.decoded_pre.data(),
                         decoded,
                         pending.record_.decoded_pre.size())) {
        return CaptureBuildResult::unreadable;
    }
    pending.record_.provenance = lease.entry_provenance();
    pending.record_.provenance.binding = BindingState::observed_candidate_not_joined;
    pending.record_.decoded_identity = reinterpret_cast<std::uintptr_t>(decoded);
    pending.record_.output_identity = reinterpret_cast<std::uintptr_t>(outputBuffer);
    pending.record_.output_bytes_identity = reinterpret_cast<std::uintptr_t>(outputBytes);
    pending.record_.decoded_pre_hash = bounded_hash(pending.record_.decoded_pre);
    pending.record_.schema = schema;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult observe_type68_serializer_cursor(PendingType68SerializerCapture& pending,
                                                    const ParticipantLease& lease,
                                                    std::uint32_t cursorBits,
                                                    bool writerError) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    if (!lease.active() || lease.participant() != NativeParticipant::large_serializer
        || lease.entry_provenance().call_id != pending.record_.provenance.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    pending.record_.cursor_bits = cursorBits;
    pending.record_.writer_error = writerError;
    pending.record_.cursor_observed = true;
    return CaptureBuildResult::complete;
}

CaptureBuildResult finish_type68_serializer_capture(PendingType68SerializerCapture& pending,
                                                    const ParticipantLease& lease,
                                                    const void* decoded,
                                                    const void* outputBuffer,
                                                    const std::int32_t* outputBytes,
                                                    bool originalResult,
                                                    std::uint64_t postMonotonicTick,
                                                    Type68SerializerCaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    pending.ready_ = false;
    if (!lease.active() || lease.participant() != NativeParticipant::large_serializer
        || lease.entry_provenance().call_id != pending.record_.provenance.call_id) {
        return CaptureBuildResult::wrong_participant;
    }
    if (postMonotonicTick < pending.record_.provenance.pre_monotonic_tick) {
        return CaptureBuildResult::timing_invalid;
    }
    if (reinterpret_cast<std::uintptr_t>(decoded) != pending.record_.decoded_identity
        || reinterpret_cast<std::uintptr_t>(outputBuffer) != pending.record_.output_identity
        || reinterpret_cast<std::uintptr_t>(outputBytes)
               != pending.record_.output_bytes_identity) {
        return CaptureBuildResult::identity_mismatch;
    }
    pending.record_.original_result = originalResult;
    pending.record_.decoded_post_valid = safe_copy_exact(pending.record_.decoded_post.data(),
                                                         decoded,
                                                         pending.record_.decoded_post.size());
    if (pending.record_.decoded_post_valid) {
        pending.record_.decoded_post_hash = bounded_hash(pending.record_.decoded_post);
    }
    const bool byteCountValid = safe_copy_exact(&pending.record_.output_bytes,
                                                outputBytes,
                                                sizeof pending.record_.output_bytes)
                                && pending.record_.output_bytes >= 0;
    pending.record_.wire_post_valid =
        byteCountValid
        && static_cast<std::uint32_t>(pending.record_.output_bytes) >= kType68RoundedBytes
        && safe_copy_exact(pending.record_.wire_post.data(),
                           outputBuffer,
                           pending.record_.wire_post.size());
    if (pending.record_.wire_post_valid) {
        // Deliberately hashes the bounded rounded image. Padding-bit normalization is not guessed.
        pending.record_.rounded_wire_hash = bounded_hash(pending.record_.wire_post);
    }
    pending.record_.exact_type68_shape =
        byteCountValid && pending.record_.cursor_observed
        && exact_type68_serialization_shape(pending.record_.schema,
                                            pending.record_.cursor_bits,
                                            static_cast<std::uint32_t>(pending.record_.output_bytes),
                                            pending.record_.writer_error)
        && pending.record_.decoded_post_valid && pending.record_.wire_post_valid;
    output = pending.record_;
    return pending.record_.exact_type68_shape ? CaptureBuildResult::complete
                                              : CaptureBuildResult::partial;
}

bool valid_raw_record(const ResolverCaptureRecord& record) noexcept {
    Type69Payload pre{};
    std::uint8_t type{};
    if (record.provenance.build_id == 0U || record.provenance.cohort_id == 0U
        || record.provenance.call_id == 0U || record.provenance.materialization_token != 0U
        || record.provenance.binding != BindingState::materialization_unobserved
        || record.provenance.participant != NativeParticipant::resolver || !record.pre_valid
        || record.record_pre_hash != bounded_hash(record.record_pre)
        || !runtime_record_projection(record.record_pre, pre, type)
        || !payload_equal(pre, record.payload_pre)) {
        return false;
    }
    if (!record.post_valid) {
        return true;
    }
    Type69Payload post{};
    return record.record_post_hash == bounded_hash(record.record_post)
           && runtime_record_projection(record.record_post, post, type)
           && payload_equal(post, record.payload_post);
}

bool valid_raw_record(const DispatchCaptureRecord& record) noexcept {
    const bool participantMatches =
        (record.kind == DispatchKind::full_apply
         && record.provenance.participant == NativeParticipant::full_dispatch)
        || (record.kind == DispatchKind::paired_short_form
            && record.provenance.participant == NativeParticipant::short_dispatch);
    return record.provenance.build_id != 0U && record.provenance.cohort_id != 0U
           && record.provenance.call_id != 0U && record.provenance.materialization_token == 0U
           && record.provenance.binding == BindingState::materialization_unobserved
           && participantMatches && record.pre_valid && record.descriptor_pre.type == kType69
           && record.descriptor_pre_hash == projection_hash(record.descriptor_pre)
           && (!record.post_valid
               || (record.descriptor_post.type == kType69
                   && record.descriptor_post_hash == projection_hash(record.descriptor_post)));
}

bool valid_raw_record(const CallsiteCaptureRecord& record) noexcept {
    const bool participantMatches =
        (record.kind == SubscriberKind::full_slot_30
         && record.provenance.participant == NativeParticipant::full_callsite
         && record.resolution.method_slot == kFullSubscriberMethodSlot)
        || (record.kind == SubscriberKind::short_slot_48
            && record.provenance.participant == NativeParticipant::short_callsite
            && record.resolution.method_slot == kShortSubscriberMethodSlot);
    return record.provenance.build_id != 0U && record.provenance.cohort_id != 0U
           && record.provenance.call_id != 0U && record.provenance.parent_call_id != 0U
           && record.provenance.materialization_token == 0U
           && record.provenance.binding == BindingState::materialization_unobserved
           && record.parent_descriptor.type == kType69 && participantMatches
           && record.return_observed;
}

bool valid_raw_record(const EndpointBinderCaptureRecord& record) noexcept {
    return record.provenance.build_id != 0U && record.provenance.cohort_id != 0U
           && record.provenance.call_id != 0U && record.provenance.parent_call_id == 0U
           && record.provenance.materialization_token == 0U
           && record.provenance.binding == BindingState::observed_candidate_not_joined
           && record.provenance.participant == NativeParticipant::endpoint_binder
           && record.endpoint_owner != 0U && record.pre_valid;
}

bool valid_raw_record(const EndpointRegistrationCaptureRecord& record) noexcept {
    return record.provenance.build_id != 0U && record.provenance.cohort_id != 0U
           && record.provenance.call_id != 0U && record.provenance.parent_call_id != 0U
           && record.provenance.materialization_token == 0U
           && record.provenance.binding == BindingState::observed_candidate_not_joined
           && record.provenance.participant
                  == NativeParticipant::endpoint_registration_callsite
           && record.row_identity != 0U
           && record.interface_pair == record.row_identity + kEndpointInterfaceHalfOffset
           && record.runtime_key_pre_hash == bounded_hash(record.runtime_key_pre)
           && (!record.post_valid
               || record.runtime_key_post_hash == bounded_hash(record.runtime_key_post))
           && record.registration_candidate.interface_pair == record.interface_pair
           && record.registration_candidate.method_slot == kEndpointRegistrationMethodSlot
           && record.pre_valid && record.return_observed;
}

bool valid_raw_record(const Type68SerializerCaptureRecord& record) noexcept {
    const bool base = record.provenance.build_id != 0U && record.provenance.cohort_id != 0U
                      && record.provenance.call_id != 0U
                      && record.provenance.parent_call_id == 0U
                      && record.provenance.materialization_token == 0U
                      && record.provenance.binding
                             == BindingState::observed_candidate_not_joined
                      && record.provenance.participant == NativeParticipant::large_serializer
                      && record.schema == kType68AuthoritySchema
                      && record.decoded_identity != 0U && record.output_identity != 0U
                      && record.output_bytes_identity != 0U
                      && record.decoded_pre_hash == bounded_hash(record.decoded_pre);
    if (!base) {
        return false;
    }
    if (record.decoded_post_valid
        && record.decoded_post_hash != bounded_hash(record.decoded_post)) {
        return false;
    }
    if (record.wire_post_valid
        && record.rounded_wire_hash != bounded_hash(record.wire_post)) {
        return false;
    }
    return !record.exact_type68_shape
           || (record.cursor_observed && record.decoded_post_valid && record.wire_post_valid
               && exact_type68_serialization_shape(record.schema,
                                                   record.cursor_bits,
                                                   static_cast<std::uint32_t>(record.output_bytes),
                                                   record.writer_error));
}

bool default_telemetry(const ResolverCaptureRecord& record,
                       ResolverTelemetry& output) noexcept {
    if (!valid_raw_record(record) || !telemetry_ready(record.provenance, record.sequence)) {
        return false;
    }
    const Type69Payload& payload = record.post_valid ? record.payload_post : record.payload_pre;
    output = ResolverTelemetry{telemetry_provenance(record.provenance, record.sequence),
                               record.record_pre_hash,
                               record.record_post_hash,
                               payload.endpoint_index,
                               payload.auxiliary,
                               payload.selectable_low,
                               payload.selectable_high,
                               payload.payload_xyz,
                               payload.operation,
                               record.original_result,
                               record.post_valid,
                               resolver_failure_sentinel(payload)};
    return true;
}

bool default_telemetry(const DispatchCaptureRecord& record,
                       DispatchTelemetry& output) noexcept {
    if (!valid_raw_record(record) || !telemetry_ready(record.provenance, record.sequence)) {
        return false;
    }
    output = DispatchTelemetry{telemetry_provenance(record.provenance, record.sequence),
                               record.descriptor_pre_hash,
                               record.descriptor_post_hash,
                               record.descriptor_pre.payload.endpoint_index,
                               record.descriptor_pre.payload.auxiliary,
                               record.descriptor_pre.payload.operation,
                               record.kind,
                               record.post_valid};
    return true;
}

bool default_telemetry(const CallsiteCaptureRecord& record,
                       CallsiteTelemetry& output) noexcept {
    if (!valid_raw_record(record) || !telemetry_ready(record.provenance, record.sequence)) {
        return false;
    }
    const std::array<std::uintptr_t, 6U> candidates{
        record.resolution.interface_pair,
        record.resolution.interface_datum,
        record.resolution.endpoint_object,
        record.resolution.interface_metadata,
        record.resolution.adjusted_this,
        record.resolution.concrete_method};
    output = CallsiteTelemetry{telemetry_provenance(record.provenance, record.sequence),
                               bounded_hash(std::as_bytes(std::span{candidates})),
                               record.parent_descriptor.payload.endpoint_index,
                               record.resolved_value,
                               record.auxiliary,
                               record.resolution.validity_mask,
                               record.resolution.method_slot,
                               record.operation,
                               record.kind,
                               record.arguments_match_parent,
                               record.return_observed};
    return true;
}

bool default_telemetry(const Type68SerializerCaptureRecord& record,
                       Type68SerializerTelemetry& output) noexcept {
    if (!valid_raw_record(record) || !telemetry_ready(record.provenance, record.sequence)) {
        return false;
    }
    output = Type68SerializerTelemetry{telemetry_provenance(record.provenance, record.sequence),
                                       record.decoded_pre_hash,
                                       record.decoded_post_hash,
                                       record.rounded_wire_hash,
                                       record.schema,
                                       record.cursor_bits,
                                       record.output_bytes,
                                       record.original_result,
                                       record.writer_error,
                                       record.exact_type68_shape};
    return true;
}

#if defined(SUNRISE_TYPE69_CAPTURE_TESTING)
ValidatedRuntimeCohort CaptureTestAccess::validated_cohort() noexcept {
    ValidatedRuntimeCohort result{};
    result.validation_cookie_ = kCohortCookieSalt;
    result.build_id_ = bounded_hash(kPinnedPackedRuntimeSha256);
    result.cohort_id_ = 0xC0690A11U;
    result.module_base_ = 0x140000000ULL;
    result.size_of_image_ = kPinnedPcSizeOfImage;
    result.surface_mask_ = (1ULL << kPcSurfaceCount) - 1U;
    return result;
}
#endif

} // namespace sunrise::client::hooks::bootflow::opening_authority::type69_capture
