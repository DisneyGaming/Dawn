#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
namespace sunrise::client::hooks::bootflow::native_box_identity {
template<class T> T at(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T result{};if(offset<=bytes.size() && sizeof(T)<=bytes.size()-offset) { std::memcpy(&result,bytes.data()+offset,sizeof result); }return result;
}
// Shared native box health ABI. Both salted handles must match exactly.
inline bool health(std::span<const std::byte> bytes,std::uint32_t self,std::uint32_t entity) noexcept {
    return bytes.size()>=0x340 && self!=UINT32_MAX && entity!=UINT32_MAX
        && at<std::uint32_t>(bytes,0)==0x80F48026U && at<std::uint32_t>(bytes,4)==0x80804B8AU
        && at<std::uint64_t>(bytes,8)==0xB08U && at<std::uint32_t>(bytes,0x24)==self
        && at<std::uint32_t>(bytes,0x2C)==entity;
}
inline bool dead(std::span<const std::byte> bytes,std::uint32_t self,std::uint32_t entity) noexcept {
    return health(bytes,self,entity) && (at<std::uint8_t>(bytes,0x338)&1U)!=0;
}
struct Sample { std::uintptr_t address{};std::uint32_t health{},entity{};bool dead{}; };
// Health context shared by B804E0, CDCB60 and B7E3C0. This authenticates the
// health component only; a mission adapter must authenticate its source owner.
template<class Read> bool sample(Read& read,std::uintptr_t context,Sample& out) noexcept {
    std::array<std::byte,16> header{};
    if(context>UINTPTR_MAX-8 || !read.value(context+8,out.address) || !read.copy(out.address,header)
        || at<std::uint32_t>(header,0)!=0x80F48026U || at<std::uint32_t>(header,4)!=0x80804B8AU
        || at<std::uint64_t>(header,8)!=0xB08U) { return false; }
    std::array<std::byte,0x340> bytes{};std::uintptr_t resolved{};
    if(!read.copy(out.address,bytes)) { return false; }
    out.health=at<std::uint32_t>(bytes,0x24);out.entity=at<std::uint32_t>(bytes,0x2C);
    if(!health(bytes,out.health,out.entity) || !read.resolve(out.health,resolved) || resolved!=out.address) { return false; }
    out.dead=dead(bytes,out.health,out.entity);return true;
}
// Physical identity only; adapters retain their own mission/run semantics.
struct Owner {
    std::uintptr_t source{};
    std::uint32_t generation{},serial{UINT32_MAX},entity{UINT32_MAX},health{UINT32_MAX};
};
template<class Read> bool current(Read& read,const Owner& owner,const Sample& value,std::uint32_t definition) noexcept {
    if(owner.source<0x10000 || owner.source>UINTPTR_MAX-0x448 || !owner.generation
        || owner.serial==UINT32_MAX || owner.entity==UINT32_MAX || owner.health==UINT32_MAX
        || owner.health!=value.health || owner.entity!=value.entity) { return false; }
    std::array<std::byte,16> header{};std::uint32_t generation{},committed{},serial{},entity{};std::uint8_t active{};
    return read.copy(owner.source,header) && at<std::uint32_t>(header,0)==definition
        && at<std::uint32_t>(header,4)==0x80809928U && at<std::uint64_t>(header,8)==0x4C8U
        && read.value(owner.source+0x180,generation) && generation==owner.generation
        && read.value(owner.source+0x2F0,committed) && committed==generation
        && read.value(owner.source+0x188,active) && active==1
        && read.value(owner.source+0x440,serial) && serial==owner.serial
        && read.value(owner.source+0x444,entity) && entity==owner.entity;
}
}
