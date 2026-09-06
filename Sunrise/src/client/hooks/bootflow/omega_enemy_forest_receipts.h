#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::client::hooks::bootflow::omega_enemy_forest {
inline constexpr std::size_t kWorkerBytes=0xDC4;
inline constexpr std::size_t kMaximumEntries=256,kMaximumPairs=128;
// Exact Forest-D palettes contain at most 504 authored rows; leave eight spare.
inline constexpr std::size_t kMaximumActorRows=512,kActorRowBytes=0x30,kEntryBytes=0x38;

template<typename T>
[[nodiscard]] inline T read(std::span<const std::byte> bytes,std::size_t at) noexcept {
    T out{};
    if(at<=bytes.size() && sizeof out<=bytes.size()-at) {
        std::memcpy(&out,bytes.data()+at,sizeof out);
    }
    return out;
}

[[nodiscard]] inline bool relative(std::uintptr_t base,std::int64_t displacement,
                                    std::size_t suffix,std::uintptr_t& result) noexcept {
    if(base>UINTPTR_MAX-suffix) { return false; }
    result=base+suffix;
    if(displacement>=0) {
        if(static_cast<std::uint64_t>(displacement)>UINTPTR_MAX-result) { return false; }
        result+=static_cast<std::uintptr_t>(displacement);
    } else {
        const auto amount=static_cast<std::uint64_t>(-(displacement+1))+1;
        if(amount>result) { return false; }
        result-=static_cast<std::uintptr_t>(amount);
    }
    return result>=0x10000;
}

struct Entry final {
    std::uint32_t encounter{};
    std::uint8_t kind{},state{},palette{},area{},gateway{},pending{};
};
[[nodiscard]] inline Entry entry(std::span<const std::byte> bytes) noexcept {
    return {read<std::uint32_t>(bytes,0x34),read<std::uint8_t>(bytes,0x18),
            read<std::uint8_t>(bytes,0x19),read<std::uint8_t>(bytes,0x22),
            read<std::uint8_t>(bytes,0x23),read<std::uint8_t>(bytes,0x24),
            read<std::uint8_t>(bytes,0x27)};
}

struct Totals final {
    std::uint32_t rows{},enabled{};
    std::int64_t remaining{},queued{};
};
/** Native request accounting from FEF750 / 1007350, not a health/kill counter. */
[[nodiscard]] inline bool actor_totals(std::span<const std::byte> bytes,
                                        std::int64_t count,Totals& totals) noexcept {
    totals={};
    if(count<0 || count>kMaximumActorRows
        || static_cast<std::size_t>(count)>bytes.size()/kActorRowBytes) { return false; }
    totals.rows=static_cast<std::uint32_t>(count);
    for(std::size_t i=0;i<static_cast<std::size_t>(count);++i) {
        const auto row=bytes.subspan(i*kActorRowBytes,kActorRowBytes);
        if(read<std::uint8_t>(row,0x2C)==0) { continue; }
        ++totals.enabled;
        totals.remaining+=read<std::int32_t>(row,0x20);
        totals.queued+=read<std::int32_t>(row,0x24);
    }
    return true;
}

[[nodiscard]] inline std::uint64_t hash(std::span<const std::byte> bytes) noexcept {
    std::uint64_t value=1469598103934665603ULL;
    for(auto b:bytes) { value^=std::to_integer<std::uint8_t>(b);value*=1099511628211ULL; }
    return value;
}
template<typename T>
inline void fold(std::uint64_t& value,const T& item) noexcept {
    for(auto b:std::as_bytes(std::span{&item,std::size_t{1}})) {
        value^=std::to_integer<std::uint8_t>(b);value*=1099511628211ULL;
    }
}
} // namespace sunrise::client::hooks::bootflow::omega_enemy_forest
