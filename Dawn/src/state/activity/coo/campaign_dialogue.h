#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "../strike_variants.h"

namespace dawn::state::activity::coo::campaign_dialogue {
// The shared 80F1FFB6/80F1FEC2 banks test unlock 51289EB0. Its dense
// 81319321 flag-table index is 4; native evaluated flags use 1=false, 2=true.
inline constexpr std::uint16_t kFlag = 4;
[[nodiscard]] constexpr std::optional<std::uint8_t> value(std::int16_t activity) noexcept {
    if (activity == 296 || activity == 298) { return std::uint8_t{2}; }
    if (strikes::find(activity)) { return std::uint8_t{1}; }
    return std::nullopt;
}
struct Flag { std::uint16_t slot{}; std::uint8_t value{}, padding{}; };
struct Flags { std::uint32_t count{}; std::array<Flag,100> rows{}; };
static_assert(sizeof(Flag)==4 && sizeof(Flags)==0x194);
// One game-thread lease. Restore only our flag; preserve unrelated live overrides.
struct Lease {
    bool active{};
    std::optional<std::uint8_t> original{};
    [[nodiscard]] bool apply(Flags& flags, std::int16_t activity, bool& changed) noexcept {
        changed=false;
        if (flags.count>flags.rows.size()) { return false; }
        std::size_t found=flags.count;
        for (std::size_t i=0;i<flags.count;++i) {
            if (flags.rows[i].slot!=kFlag) { continue; }
            if (found!=flags.count || flags.rows[i].value>2) { return false; }
            found=i;
        }
        auto desired=value(activity);
        if (!desired && !active) { return true; }
        if (desired && !active) {
            if (found==flags.count && flags.count==flags.rows.size()) { return false; }
            original=found==flags.count ? std::nullopt : std::optional{flags.rows[found].value};
        }
        if (!desired) { desired=original; }
        if (desired) {
            if (found==flags.count) {
                if (flags.count==flags.rows.size()) { return false; }
                flags.rows[flags.count++]={kFlag,*desired,0}; changed=true;
            } else if (flags.rows[found].value!=*desired) {
                flags.rows[found].value=*desired; changed=true;
            }
        } else if (found!=flags.count) {
            for (auto i=found+1;i<flags.count;++i) { flags.rows[i-1]=flags.rows[i]; }
            flags.rows[--flags.count]={}; changed=true;
        }
        active=value(activity).has_value();
        if (!active) { original.reset(); }
        return true;
    }
};
}
