#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include "omega_enemy_native_reference.h"
namespace sunrise::client::hooks::bootflow::gateway_native {
struct Ref { std::uint32_t handle{UINT32_MAX},kind{}; std::int64_t offset{}; };
struct Weak { std::uint32_t serial{UINT32_MAX},handle{UINT32_MAX}; friend bool operator==(Weak,Weak)=default; };
template<class T> T at(const std::byte* bytes) noexcept { T v{};std::memcpy(&v,bytes,sizeof v);return v; }
struct Read {
    std::uintptr_t image{};std::size_t copied{};
    bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
        if(address<0x10000 || address>UINTPTR_MAX-out.size() || copied+out.size()>32768) { return false; }
        copied+=out.size();SIZE_T n{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out.data(),out.size(),&n) && n==out.size();
    }
    template<class T> bool value(std::uintptr_t address,T& out) noexcept { return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}})); }
    bool resolve(std::uint32_t handle,std::uintptr_t& base,std::uintptr_t* allocation=nullptr) noexcept {
        if(handle==UINT32_MAX) { return false; }
        std::uintptr_t directory{},registry{};
        if(!value(image+0x2439C70,directory) || !value(directory,registry)) { return false; }
        const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(handle)>>13);
        const auto index=((static_cast<std::uint64_t>(shifted)|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
        std::array<std::byte,0x38> row{};
        if(!copy(registry+index*0x40,row)) { return false; }
        const auto stride=at<std::int32_t>(row.data()+0x30);
        if(stride<=0 || stride>0x100000) { return false; }
        const auto element=at<std::uintptr_t>(row.data()+8)+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride;
        std::uint64_t relocation{};if(!value(element+8,relocation)) { return false; }
        base=omega_enemy_native_reference::corrected_base(element,relocation,at<std::int32_t>(row.data()+0x34));
        if(allocation) { *allocation=element; }return base>=0x10000;
    }
    bool weak(Weak ref) noexcept {
        if(ref.handle==UINT32_MAX) { return false; }
        std::uintptr_t directory{},registry{},metadata{},head{},elements{};std::int32_t ds{};
        if(!value(image+0x2439C70,directory) || !value(directory,registry) || !value(directory+0x10,ds) || ds<=0 || ds>0x1000) { return false; }
        const auto index=((static_cast<std::int32_t>(ref.handle)>>31&0x3C00U)|0x3FFU)&(ref.handle>>13)&0xFFFFU;
        std::uint16_t count{};std::uint32_t offset{},stride{},serial{};
        return value(registry+static_cast<std::uintptr_t>(index)*ds+0x10,metadata) && value(metadata,head)
            && value(metadata+8,elements) && value(head+0x1C,count) && (ref.handle&0x1FFFU)<count
            && value(metadata+0x1C,offset) && value(metadata+0x20,stride) && stride>0 && stride<=0x100000
            && value(elements+offset+static_cast<std::uintptr_t>(ref.handle&0x1FFFU)*stride,serial) && serial==ref.serial;
    }
    bool entity_row(Weak entity,std::uintptr_t& row) noexcept {
        std::uintptr_t table{};std::uint32_t stride{},flags{};
        if(!weak(entity) || !value(image+0x1F93428,table) || !value(image+0x1F93430,stride) || stride<0x50 || stride>0x100000) { return false; }
        row=table+static_cast<std::uintptr_t>(entity.handle&0x1FFFU)*stride;
        return value(row+4,flags) && (flags&4U)==0;
    }
};
}
