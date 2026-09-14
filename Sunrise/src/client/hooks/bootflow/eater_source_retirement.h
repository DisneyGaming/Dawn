#pragma once
#include <array>
#include <cstdint>

namespace sunrise::client::hooks::bootflow::eater_source_retirement {
struct ResetState {
    std::uint32_t incoming{},applied{},owned{};
    std::uint8_t scheduling{};
    std::array<std::uint32_t,6> request{};
    std::array<std::uint32_t,8> queues{};
};
inline bool reset(std::uint32_t generation,const ResetState& state) noexcept {
    if(!generation || state.incoming!=generation || state.applied!=generation || state.owned
        // +0x638 retains the applied generation after native reset, not a count.
        || state.scheduling || state.request[0]!=generation || state.request[1]
        || state.request[2]!=UINT32_MAX || state.request[3]!=UINT32_MAX
        || state.request[4] || state.request[5]) return false;
    for(const auto value:state.queues) if(value) return false;
    return true;
}
enum class EntityState : std::uint8_t {unreadable,replaced,marked,live};
inline EntityState entity(std::uint32_t expected,std::uint32_t self,
                          std::uint32_t flags,bool readable) noexcept {
    if(!readable) return EntityState::unreadable;
    if(self!=expected) return EntityState::replaced;
    return flags&4U?EntityState::marked:EntityState::live;
}
struct Gate {
    std::uint32_t generation{};bool observed{};
    bool sample(std::uint32_t expected,bool nativeReset,bool entitiesSettled) noexcept {
        if(generation!=expected) {generation=expected;observed=false;}
        if(!nativeReset || !entitiesSettled) {observed=false;return false;}
        if(observed) return true;
        observed=true;return false;
    }
};
}
