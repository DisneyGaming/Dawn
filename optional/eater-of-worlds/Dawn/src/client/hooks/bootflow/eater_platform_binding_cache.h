#pragma once
#include "coo_native_components.h"
#include "eater_platform_contact_native.h"

namespace dawn::client::hooks::bootflow::eater_platform_binding_cache {
namespace contact=eater_platform_contact_native;
struct Definition {
    std::uint32_t config{},kind{};
    std::int64_t offset{};
};
inline constexpr Definition kPhysics{contact::kPlatformPhysicsConfig,
    contact::kPhysicsKind,contact::kPlatformPhysicsOffset};
inline constexpr Definition kVolume{contact::kPlatformVolumeConfig,
    contact::kPlatformVolumeKind,contact::kPlatformVolumeOffset};
struct Component {
    std::uintptr_t address{};
    std::uint32_t self{UINT32_MAX};
    friend bool operator==(const Component&,const Component&)=default;
};
struct Components {
    Component physics{},volume{},device{};
    std::uintptr_t body{},shape{};
    friend bool operator==(const Components&,const Components&)=default;
};
template<class T> T at(const std::byte* bytes) noexcept {
    T value{};std::memcpy(&value,bytes,sizeof value);return value;
}

// A cache entry only avoids resource metadata traversal. Every use still
// resolves the complete component self handle and authenticates its owner.
template<class Read>
bool identify(Read& read,std::uintptr_t address,std::uint32_t entity,
              Definition definition,Component& out) noexcept {
    out={};std::array<std::byte,0x30> bytes{};std::uintptr_t resolved{};
    if(entity==UINT32_MAX || !contact::can_add(address,bytes.size())
        || !read.copy(address,bytes)
        || at<std::uint32_t>(bytes.data())!=definition.config
        || at<std::uint32_t>(bytes.data()+4)!=definition.kind
        || at<std::int64_t>(bytes.data()+8)!=definition.offset
        || at<std::uint32_t>(bytes.data()+0x2C)!=entity) return false;
    const auto self=at<std::uint32_t>(bytes.data()+0x24);
    if(self==UINT32_MAX || !read.resolve(self,resolved) || resolved!=address) return false;
    out={address,self};return true;
}
template<class Read>
bool current_component(Read& read,const Component& cached,std::uint32_t entity,
                       Definition definition) noexcept {
    Component current{};
    return identify(read,cached.address,entity,definition,current) && current==cached;
}
template<class Read>
bool current(Read& read,std::uintptr_t image,std::uint32_t entity,Definition device,
             const Components& cached) noexcept {
    std::uintptr_t body{},shape{};
    return current_component(read,cached.physics,entity,kPhysics)
        && current_component(read,cached.volume,entity,kVolume)
        && current_component(read,cached.device,entity,device)
        && contact::platform_volume_body(read,image,cached.volume.address,entity,body)
        && body==cached.body && read.value(body+0x20,shape) && shape==cached.shape;
}
template<class Read>
bool discover(Read& read,std::uintptr_t image,std::uint32_t bundle,std::uint32_t entity,
              Definition device,Components& out) noexcept {
    out={};Components found{};
    const auto find=[&](Definition definition,Component& component) noexcept {
        std::uintptr_t address{};
        return coo_native::component<Read,1024>(read,bundle,entity,definition.kind,address)
            && identify(read,address,entity,definition,component);
    };
    if(!find(kPhysics,found.physics) || !find(kVolume,found.volume) || !find(device,found.device)
        || !contact::platform_volume_body(read,image,found.volume.address,entity,found.body)
        || !read.value(found.body+0x20,found.shape)) return false;
    out=found;return true;
}

struct SampleKey {
    std::uint64_t run{};
    std::uint32_t player{UINT32_MAX},attempt{};
    std::size_t platform{56};
    friend bool operator==(const SampleKey&,const SampleKey&)=default;
};
// Used under the platform cache lock. Skipped polls publish no contact and
// cannot award dwell; >250 ms gaps still reset dwell in ReactorState::contact.
struct SampleGate {
    SampleKey key{};
    std::uint64_t last{};
    bool claim(SampleKey next,std::uint64_t now) noexcept {
        if(!now || !next.run || next.player==UINT32_MAX || next.platform>=56) return false;
        if(next==key && last && (now<=last || now-last<50)) return false;
        key=next;last=now;return true;
    }
};
} // namespace dawn::client::hooks::bootflow::eater_platform_binding_cache
