#pragma once
#include "../coo/native_device_authority.h"
namespace sunrise::state::activity::gateway {
inline constexpr std::array<std::uint32_t,7> kPreparedDefinitions{0x80F470F1U,0x80F470F4U,0x80F46F11U,0x80F46F14U,0x80F46F17U,0x80F46F23U,0x80F46F26U};
inline bool preparation(std::span<const std::byte,16> prefix,std::span<const std::byte,0x44> state,std::uint32_t& generation,std::uint8_t& index) noexcept {
    std::uint32_t definition{},kind{};std::uint64_t offset{};
    std::memcpy(&definition,prefix.data(),4);std::memcpy(&kind,prefix.data()+4,4);std::memcpy(&offset,prefix.data()+8,8);
    if(kind!=0x80809928U || offset!=0x4C8 || !coo::native_device::inactive_state(state)) { return false; }
    for(std::uint8_t i=0;i<kPreparedDefinitions.size();++i) {
        if(kPreparedDefinitions[i]==definition) { std::memcpy(&generation,state.data(),4);index=i;return true; }
    }
    return false;
}
}
