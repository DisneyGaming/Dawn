#pragma once
#include <cstdint>
#include <vector>

namespace sunrise::state::vendors {
// Sparse earned state, separate from the configured defaults. The native slot
// bindings determine which account or character bank receives each write.
struct Unlock {
    std::uint16_t slot{};std::int32_t value{};
    bool operator==(const Unlock&) const noexcept = default;
};
struct Unlocks {
    std::vector<Unlock> flags,values;
    bool operator==(const Unlocks&) const noexcept = default;
};
inline constexpr std::size_t kUnlockLimit=2048;
inline bool lookup(const std::vector<Unlock>& rows,std::uint16_t slot,std::int32_t& value) noexcept {
    for(const auto& row:rows) if(row.slot==slot) {value=row.value;return true;}return false;
}
inline bool store(std::vector<Unlock>& rows,std::uint16_t slot,std::int32_t value) noexcept {
    for(auto& row:rows) if(row.slot==slot) {row.value=value;return true;}
    if(rows.size()>=kUnlockLimit) {return false;}rows.push_back({slot,value});return true;
}
}
