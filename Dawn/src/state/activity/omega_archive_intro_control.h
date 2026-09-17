#pragma once
#include "omega_archive_arm_control.h"
namespace dawn::state::activity::omega_archive_intro {
using Owner=omega_archive_arm::Owner;
using Program=omega::boss_authority::IntroProgram;
// Meaningful bytes of the existing native kind9 action, including default
// target and mode. Queue padding and unused fixed-capacity entries are ignored.
[[nodiscard]] inline bool matches_queue(std::span<const std::byte> queue,std::int32_t head,std::uint32_t sequence=0x65D2379FU) noexcept {
    namespace d=omega_boss_lift::detail;
    return head==0 && queue.size()>=0x30 && d::u32(queue,0)==1 && queue[8]==std::byte{9}
        && d::u32(queue,0x18)==0xAFB11A12U && d::u32(queue,0x1C)==sequence
        && d::u32(queue,0x20)==0x811C9DC5U && d::u32(queue,0x24)==0x811C9DC5U
        && queue[0x28]==std::byte{0xFF} && queue[0x2A]==std::byte{0xFF} && queue[0x2B]==std::byte{0xFF}
        && queue[0x2C]==std::byte{} && queue[0x2D]==std::byte{};
}
[[nodiscard]] inline bool matches_queue(std::span<const std::byte> queue,std::int32_t head,const Program& program) noexcept {
    if(program.departure<0) { return matches_queue(queue,head,program.sequence); }
    namespace d=omega_boss_lift::detail;
    return program.departure<=4 && head==0 && queue.size()>=0x30 && d::u32(queue,0)==1 && queue[8]==std::byte{9}
        && d::u32(queue,0x18)==0x1F992208U && d::u32(queue,0x1C)==0xCBFDCA32U
        && d::u32(queue,0x20)==0x811C9DC5U && d::u32(queue,0x24)==0x95FB2E01U
        && queue[0x28]==std::byte{48} && queue[0x2A]==std::byte{55} && queue[0x2B]==std::byte{}
        && queue[0x2C]==std::byte{} && queue[0x2D]==std::byte(program.departure);
}
struct Status { Owner owner{};Program program{};bool applied{};std::uint32_t incarnation{}; };
class Ledger final {
public:
    [[nodiscard]] const Status& status() const noexcept { return state_; }
    [[nodiscard]] bool request(const Owner& owner) noexcept {
        if(!owner.valid() || owner.island || owner.actionEpoch || owner.revision || state_.program.revision) { return false; }
        state_={owner,{owner.generation,1,true},false,0};return true;
    }
    [[nodiscard]] bool next(const Owner& owner,std::uint32_t sequence) noexcept {
        const auto& old=state_.owner;
        if(!state_.applied || !state_.program.play || state_.program.revision>=0x7FFFFFFEU
            || !owner.valid() || owner.run!=old.run || owner.actor!=old.actor || owner.character!=old.character
            || owner.entity!=old.entity || owner.generation!=old.generation || owner.fullBody!=old.fullBody
            || owner.revision!=state_.incarnation || owner.island!=4 || (owner.actionEpoch<old.actionEpoch || (owner.actionEpoch==old.actionEpoch && owner.island==old.island))
            || (sequence!=0x65D2379CU && sequence!=0x65D2379EU && sequence!=0x65D2379DU && sequence!=0x65D2379BU)) { return false; }
        state_.owner=owner;state_.program={owner.generation,state_.program.revision+1,true,sequence};state_.applied=false;return true;
    }
    [[nodiscard]] bool depart(const Owner& owner) noexcept {
        const auto& old=state_.owner;
        if(!state_.applied || !state_.program.play || state_.program.revision>=0x7FFFFFFEU
            || !owner.valid() || owner.run!=old.run || owner.actor!=old.actor || owner.character!=old.character
            || owner.entity!=old.entity || owner.generation!=old.generation || owner.fullBody!=old.fullBody
            || owner.revision!=state_.incarnation || owner.island<old.island || owner.island>4
            || owner.actionEpoch<old.actionEpoch
            || (state_.program.departure>=0 && owner.island<=state_.program.departure)) { return false; }
        state_.owner=owner;state_.program={owner.generation,state_.program.revision+1,true,0xCBFDCA32U,
            static_cast<std::int8_t>(owner.island)};state_.applied=false;return true;
    }
    [[nodiscard]] bool acknowledge(const Owner& nativeOwner) noexcept {
        auto expected=state_.owner;expected.revision=state_.program.revision;
        if(!state_.program.revision || state_.applied || nativeOwner!=expected) { return false; }
        if(!state_.incarnation && state_.program.play) { state_.incarnation=nativeOwner.revision; }
        state_.owner=nativeOwner;state_.owner.revision=state_.incarnation;state_.applied=true;return true;
    }
    [[nodiscard]] bool cancel() noexcept {
        if(!state_.program.revision || !state_.program.play || state_.applied || state_.program.revision>=0x7FFFFFFFU) { return false; }
        ++state_.program.revision;state_.program.play=false;return true;
    }
private:
    Status state_{};
};
} // namespace dawn::state::activity::omega_archive_intro
