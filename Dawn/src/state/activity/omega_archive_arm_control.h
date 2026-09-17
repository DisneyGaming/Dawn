#pragma once

#include <array>
#include <cstring>
#include <span>
#include "omega/omega_boss_authority.h"
#include "omega_boss_lift_action.h"

namespace dawn::state::activity::omega_archive_arm {
using Control=omega::boss_authority::ArmControl;
using Owner=omega_boss_lift::Owner;
struct NativeControl {
    std::uint32_t actor{UINT32_MAX},generation{},queueRevision{},revision{};
    bool domain{},applied{};
    float left{},right{};
};
template<class T> [[nodiscard]] T read(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};std::memcpy(&value,bytes.data()+offset,sizeof value);return value;
}
// AB6600 calls AB2D00 when auth+4C differs from member+AB0. AB2D00
// commits that revision, preserves flags for a zero mask and dispatches each
// scalar through 576420. Reject a foreign target/hash domain before issuing.
[[nodiscard]] inline NativeControl inspect(std::span<const std::byte> member,
                                           std::span<const std::byte> auth) noexcept {
    NativeControl out{};
    if(member.size()<0xAB4 || auth.size()<0x108
        || read<std::uint32_t>(member,0)!=0x80F4756DU
        || read<std::uint32_t>(member,4)!=0x80807D9DU
        || read<std::int64_t>(member,8)!=0xB58
        || read<std::uint8_t>(member,0x1D4)!=0 || read<std::uint8_t>(auth,6)==0) { return out; }
    out.actor=read<std::uint32_t>(member,0x21C);
    out.generation=read<std::uint32_t>(member,0x180);
    out.queueRevision=read<std::uint32_t>(member,0x190);
    if(out.actor==UINT32_MAX || !out.generation || out.generation!=read<std::uint32_t>(auth,0)
        || out.queueRevision!=read<std::uint32_t>(auth,0x100)) { return out; }
    out.revision=read<std::uint32_t>(member,0xAB0);
    out.domain=read<std::uint8_t>(auth,0x50)==0 && read<std::uint8_t>(auth,0x51)==0
        && read<std::uint32_t>(auth,0x54)==0 && read<std::uint32_t>(auth,0x70)==0x811C9DC5U
        && read<std::int8_t>(auth,0x74)==-1 && read<std::int16_t>(auth,0x76)==-1
        && read<std::int32_t>(auth,0x78)==-1;
    for(unsigned i=0;i<6;++i) out.domain=out.domain && read<std::uint32_t>(auth,0x58+i*4)==0x811C9DC5U;
    for(unsigned i=0;i<7;++i) out.domain=out.domain
        && read<std::uint32_t>(member,0xA90+i*4)==read<std::uint32_t>(auth,0x54+i*4);
    out.applied=out.domain && out.revision==read<std::uint32_t>(auth,0x4C);
    const auto count=read<std::uint32_t>(auth,0x7C);
    if(out.revision==0) { out.applied=out.applied && count==0; }
    else {
        out.applied=out.applied && count==2 && read<std::uint32_t>(auth,0x80)==0xA2AE120FU
            && read<std::uint32_t>(auth,0x88)==0x8496ABD2U;
        out.left=read<float>(auth,0x84);out.right=read<float>(auth,0x8C);
    }
    return out;
}
struct Status {
    Owner owner{};
    Control control{};
    bool pending{},completed{};
};
class Ledger final {
public:
    [[nodiscard]] const Status& status() const noexcept { return state_; }
    [[nodiscard]] bool prepare(const Owner& owner,bool right) noexcept {
        if(!owner.valid() || state_.pending || state_.control.high
            || state_.control.revision>=0x7FFFFFFEU
            || (state_.control.revision && (state_.owner.run!=owner.run
                || state_.control.generation!=owner.generation))) { return false; }
        state_={owner,{owner.generation,state_.control.revision+1,right,true},true,false};return true;
    }
    [[nodiscard]] bool acknowledge(const Owner& owner,const NativeControl& receipt,
                                    const std::array<float,4>& left,const std::array<float,4>& right) noexcept {
        const auto& control=state_.control;
        if(owner!=state_.owner || !state_.pending || !receipt.applied
            || receipt.actor!=owner.actor || receipt.generation!=owner.generation
            || receipt.queueRevision!=owner.revision || receipt.revision!=control.revision) { return false; }
        const float desiredLeft=control.high && !control.right?1.0F:0.0F;
        const float desiredRight=control.high && control.right?1.0F:0.0F;
        if(receipt.left!=desiredLeft || receipt.right!=desiredRight) { return false; }
        for(unsigned i=0;i<4;++i) if(left[i]!=desiredLeft || right[i]!=desiredRight) { return false; }
        state_.pending=false;return true;
    }
    [[nodiscard]] bool release(const Owner& owner,bool completed) noexcept {
        if(owner!=state_.owner || !state_.control.high || state_.control.revision>=0x7FFFFFFFU
            || (completed && state_.pending)) { return false; }
        ++state_.control.revision;state_.control.high=false;state_.pending=true;state_.completed=completed;return true;
    }
private:
    Status state_{};
};
} // namespace dawn::state::activity::omega_archive_arm
