#pragma once
#include "native_box_identity.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
namespace dawn::client::hooks::bootflow::beyond_infinity_plate_timer {
inline constexpr std::uint32_t kDefinition=0x815B8B3BU,kKind=0x80804FCBU,kStateSchema=0x80804FCAU;
inline constexpr std::uintptr_t kDefinitionOffset=0x248U,kEndOffset=0x148U;
inline constexpr std::uint64_t kTicksPerSecond=673200U;
// 1006F20 consumes signed 32-bit elapsed/remaining milliseconds. Keep their
// constant sum representable; 35FC00 also multiplies native ticks by 2500.
inline constexpr std::uint64_t kMaximumDurationTicks=
    static_cast<std::uint64_t>((std::numeric_limits<std::int32_t>::max)())*kTicksPerSecond/1000U;
inline constexpr std::size_t kCommandBytes=0x70U,kEntryStride=0x60U,kStateBytes=0x48U,kStateOffset=0x20U;
struct EntryPoint { std::uintptr_t rva{};std::array<std::uint8_t,16> signature{}; };
// Pinned mapped image SHA256 63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e.
// Clock(source*) -> clock*; Now(clock*,uint64_t*) -> uint64_t*;
// Duration(uint64_t*,float seconds) -> uint64_t* (float in XMM1).
inline constexpr EntryPoint kClock{0x4ECC50U,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x44}};
inline constexpr EntryPoint kNow{0x3CAF00U,{0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xFA,0xE8,0x1E,0x01}};
inline constexpr EntryPoint kDuration{0x35F080U,{0x0F,0x57,0xC0,0x33,0xD2,0xF3,0x0F,0x5F,0xC8,0xF3,0x0F,0x10,0x05,0xD3,0x34,0x84}};
inline constexpr EntryPoint kApply{0x1003900U,{0x40,0x53,0x48,0x83,0xEC,0x20,0x0F,0xB7,0x41,0x2C,0x4C,0x8B,0xD2,0x25,0xFF,0x1F}};
inline constexpr EntryPoint kTick{0x1006F20U,{0x41,0x56,0x48,0x81,0xEC,0x80,0x00,0x00,0x00,0x0F,0xB6,0x41,0x30,0x4C,0x8B,0xF1}};
struct Command { std::array<std::byte,kCommandBytes> bytes{}; };
namespace detail {
template<class T> void put(Command& command,std::size_t offset,T value) noexcept {
    std::memcpy(command.bytes.data()+offset,&value,sizeof value);
}
inline Command envelope() noexcept {
    Command result{};put(result,0,std::int32_t{1});put(result,0x10,kStateSchema);return result;
}
}
// 1003900(component,command) retains native entity authority checks and marks
// the component changed. FEE1C0 scans count entries of 0x60 (96) bytes, matches
// schema at +0x10, then copies 0x48 (72) payload bytes from +0x20 to component+30.
// This command never writes computed progress+1B8 or completion latch+79.
inline Command stopped() noexcept { return detail::envelope(); }
inline bool start(Command& out,std::uint64_t durationTicks,std::uint64_t epoch) noexcept {
    out={};
    if(!durationTicks || durationTicks>kMaximumDurationTicks
        || epoch>=(std::numeric_limits<std::uint64_t>::max)()-durationTicks) { return false; }
    out=detail::envelope();
    detail::put(out,kStateOffset,std::uint8_t{1}); // Outer active.
    detail::put(out,kStateOffset+8,std::uint8_t{1}); // Timer advancing.
    detail::put(out,kStateOffset+0x18,durationTicks); // Upper bound; lower remains zero.
    detail::put(out,kStateOffset+0x28,durationTicks); // Remaining; elapsed remains zero.
    detail::put(out,kStateOffset+0x30,epoch);
    detail::put(out,kStateOffset+0x38,1.F);
    return true;
}
// The exact plate definition has a zero normalization-end offset. Native tick
// resolves component's definition tag and +8 offset before reading +148; this
// is not the live component+148 field. The linked 815B3829 timer has end=1 and
// must not be accepted as this plate. Read is injected; no process APIs here.
template<class Read> bool authored_end(Read& read,std::uintptr_t component,float& seconds) noexcept {
    seconds=0.F;std::array<std::byte,16> header{};std::uintptr_t definition{};
    if(!read.copy(component,header) || native_box_identity::at<std::uint32_t>(header,0)!=kDefinition
        || native_box_identity::at<std::uint32_t>(header,4)!=kKind
        || native_box_identity::at<std::uint64_t>(header,8)!=kDefinitionOffset
        || !read.resolve(kDefinition,definition) || definition<0x10000
        || definition>UINTPTR_MAX-kDefinitionOffset-kEndOffset-sizeof(float)
        || !read.value(definition+kDefinitionOffset+kEndOffset,seconds)) { return false; }
    return std::isfinite(seconds) && seconds==0.F;
}
// Accept only the endpoint and latch computed by the original 1006F20 tick.
// Source, object generation, salted entity and both component owners still
// require validation by the caller before and after the native callback.
inline bool complete(std::uint8_t active,std::uint8_t latched,float value,float remainingSeconds) noexcept {
    return active==1 && latched==1 && std::isfinite(value) && value==1.F
        && std::isfinite(remainingSeconds) && remainingSeconds>=0.F;
}
}
