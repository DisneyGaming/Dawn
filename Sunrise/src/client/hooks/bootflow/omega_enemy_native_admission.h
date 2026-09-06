#pragma once

#include <cstdint>

namespace sunrise::client::hooks::bootflow::omega_enemy_native_admission {

/** A0D510 reports success in AL. Native forwarding must finish before the
 * current call-gate state is checked or a newly created actor is observed. */
template<class Original,class Accepting,class Observe>
std::uint64_t forward(Original original,Accepting accepting,Observe observe,
    void* instance,const void* context,std::uint32_t parent) noexcept {
    const std::uint64_t result=original(instance,context);
    if((result&0xFFU)!=0 && accepting()) { observe(parent); }
    return result;
}

/** Full serialized handles identify the parent and actor; table indices and
 * recycled pointer addresses do not. These fields are copied after creation. */
[[nodiscard]] constexpr bool identity(std::uint32_t expectedParent,
    std::uint32_t parentKind,std::uint32_t parentSelf,std::uint32_t parentActor,
    std::uint32_t actorHandle,std::uint32_t actorParent) noexcept {
    return expectedParent!=UINT32_MAX && parentKind==0x808082ECU
        && parentSelf==expectedParent && parentActor!=UINT32_MAX
        && actorHandle==parentActor && actorParent==expectedParent;
}

} // namespace sunrise::client::hooks::bootflow::omega_enemy_native_admission
