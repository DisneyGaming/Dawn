#pragma once
#include <cstdint>
#include <cstring>
#include <span>
namespace sunrise::client::hooks::bootflow::gateway_module_identity {
template<class T> T at(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T result{};if(offset<=bytes.size() && sizeof(T)<=bytes.size()-offset) { std::memcpy(&result,bytes.data()+offset,sizeof result); }return result;
}
// Exact package health component. Entity and component handles include salt.
inline bool health(std::span<const std::byte> bytes,std::uint32_t self,std::uint32_t entity) noexcept {
    return bytes.size()>=0x340 && self!=UINT32_MAX && entity!=UINT32_MAX
        && at<std::uint32_t>(bytes,0)==0x80F48026U && at<std::uint32_t>(bytes,4)==0x80804B8AU
        && at<std::uint64_t>(bytes,8)==0xB08U && at<std::uint32_t>(bytes,0x24)==self
        && at<std::uint32_t>(bytes,0x2C)==entity;
}
inline bool dead(std::span<const std::byte> bytes,std::uint32_t self,std::uint32_t entity) noexcept {
    return health(bytes,self,entity) && (at<std::uint8_t>(bytes,0x338)&1U)!=0;
}
}
