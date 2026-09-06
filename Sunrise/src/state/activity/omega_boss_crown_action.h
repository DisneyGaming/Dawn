#pragma once

#include "omega_boss_intro_action.h"
#include "omega_boss_lift_action.h"

namespace sunrise::state::activity::omega_boss_crown {
inline constexpr std::uint32_t kSequence=0x65D2379CU;
inline constexpr float kSummonSeconds=7.200000286102295F;
inline constexpr std::size_t kControllerBytes=0xB8;
// The native member copies the same kind-9 format as the accepted intro.
// This authored graph has only summon -> terminal idle; no event is needed.
[[nodiscard]] constexpr auto action() noexcept {
    auto queue=omega_presentation::boss_intro_action();
    for(unsigned i=0;i<4;++i) { queue[0x1C+i]=std::byte((kSequence>>(8*i))&0xFFU); }
    return queue;
}
[[nodiscard]] constexpr bool action_active(std::span<const std::byte> queue,std::int32_t head) noexcept {
    return head==0 && queue.size()>=0x20 && omega_boss_lift::detail::u32(queue,0)==1
        && queue[8]==std::byte{9} && omega_boss_lift::detail::u32(queue,0x18)==0xAFB11A12U
        && omega_boss_lift::detail::u32(queue,0x1C)==kSequence;
}
[[nodiscard]] constexpr auto stop_request() noexcept {
    auto request=omega_presentation::boss_intro_stop_request();
    for(unsigned i=0;i<4;++i) { request[4+i]=std::byte((kSequence>>(8*i))&0xFFU); }
    return request;
}
struct Owner final {
    std::uint64_t run{};
    std::uint32_t actor{UINT32_MAX},character{UINT32_MAX},entity{UINT32_MAX};
    std::uint32_t generation{},revision{},biped{UINT32_MAX};
    std::uint8_t island{4};
    bool operator==(const Owner&) const=default;
    [[nodiscard]] constexpr bool valid() const noexcept {
        return run!=0 && actor!=UINT32_MAX && character!=UINT32_MAX && entity!=UINT32_MAX
            && generation!=0 && biped!=UINT32_MAX && island==4;
    }
};
enum class Receipt : std::uint8_t { unavailable,summoning,idle };
/** Actual loaded indices, never the requested indices or camera status.
 * Original A889E0/F45A60 selects group0/sequence4, graph4/node0. Native
 * F47240 moves to node1 only at the end of authored clip80F45188. */
[[nodiscard]] inline Receipt parse(std::span<const std::byte> bytes,const Owner& owner) noexcept {
    namespace d=omega_boss_lift::detail;
    if(!owner.valid() || bytes.size()<kControllerBytes || d::u32(bytes,0)!=owner.entity
        || d::u32(bytes,4)!=owner.character || d::u32(bytes,8)!=0 || d::u32(bytes,0xC)!=4
        || d::u32(bytes,0x10)!=0x80F45178U || d::u32(bytes,0x14)!=owner.biped
        || bytes[0x21]==std::byte{} || d::u32(bytes,0xB4)!=4) { return Receipt::unavailable; }
    const auto node=d::u32(bytes,0xB0);
    const auto duration=d::f32(bytes,0x38),elapsed=d::f32(bytes,0x3C);
    if(!std::isfinite(duration) || !std::isfinite(elapsed) || elapsed<0.F) { return Receipt::unavailable; }
    if(node==0 && duration==kSummonSeconds && elapsed<=duration+0.001F) { return Receipt::summoning; }
    if(node==1 && duration==10.F) { return Receipt::idle; }
    return Receipt::unavailable;
}
enum class Phase : std::uint8_t { idle,claimed,playing,completed };
enum class Event : std::uint8_t { none,started,completed };
class CycleTracker final {
    Owner owner_{};
    std::uint64_t request_{},sample_{};
    Phase phase_{};
public:
    [[nodiscard]] const Owner& owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t request_id() const noexcept { return request_; }
    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] bool claim(const Owner& owner,std::uint64_t request) noexcept {
        if(!owner.valid() || request==0 || phase_!=Phase::idle) { return false; }
        owner_=owner;request_=request;sample_=0;phase_=Phase::claimed;return true;
    }
    [[nodiscard]] Event observe(const Owner& owner,std::uint64_t request,std::uint64_t sample,
        Receipt receipt) noexcept {
        if(owner!=owner_ || request!=request_ || sample==0 || sample<=sample_
            || phase_==Phase::idle || phase_==Phase::completed || receipt==Receipt::unavailable) { return Event::none; }
        sample_=sample;
        if(phase_==Phase::claimed && receipt==Receipt::summoning) { phase_=Phase::playing;return Event::started; }
        if(phase_==Phase::playing && receipt==Receipt::idle) { phase_=Phase::completed;return Event::completed; }
        return Event::none;
    }
};
} // namespace sunrise::state::activity::omega_boss_crown
