#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::state::activity::omega_boss_lift {

enum class Arm : std::uint8_t { left, right };
inline constexpr std::uint32_t kLeftProperty=0xA2AE120FU;  // panoptes_summon_left
inline constexpr std::uint32_t kRightProperty=0x8496ABD2U; // panoptes_summon_right
inline constexpr float kCycleSeconds=5.500000476837158F;
/** Graph 80F45178 record 0 node 4 (F3EC2190) plays two-arm summon clip
 * 80F45188. Authored node duration (panoptes-sequence-clip-map.json). */
inline constexpr float kSummonSeconds=7.200000286102295F;
inline constexpr std::size_t kFullBodyHeaderBytes=0x510;
inline constexpr std::size_t kGroupBytes=0x78;
inline constexpr std::size_t kGroupCount=6;

[[nodiscard]] constexpr bool valid_arm(Arm arm) noexcept { return arm==Arm::left || arm==Arm::right; }
[[nodiscard]] constexpr std::uint32_t property(Arm arm) noexcept {
    return arm==Arm::left ? kLeftProperty : arm==Arm::right ? kRightProperty : UINT32_MAX;
}
[[nodiscard]] constexpr std::uint16_t clip_index(Arm arm) noexcept {
    return arm==Arm::left ? 20 : arm==Arm::right ? 19 : UINT16_MAX;
}
[[nodiscard]] constexpr std::size_t group_index(Arm arm) noexcept { return arm==Arm::left ? 2 : 1; }

namespace detail {
// Callers check each fixed record's size before using these little-endian reads.
[[nodiscard]] constexpr std::uint32_t u32(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    std::uint32_t value{};
    for(unsigned i=0;i<4;++i) { value|=std::to_integer<std::uint32_t>(bytes[offset+i])<<(i*8); }
    return value;
}
[[nodiscard]] constexpr std::uint64_t u64(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    return u32(bytes,offset)|(static_cast<std::uint64_t>(u32(bytes,offset+4))<<32);
}
[[nodiscard]] constexpr std::uint16_t u16(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(bytes[offset])
        |(std::to_integer<unsigned>(bytes[offset+1])<<8));
}
[[nodiscard]] constexpr float f32(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    return std::bit_cast<float>(u32(bytes,offset));
}
[[nodiscard]] inline bool elapsed_valid(float value) noexcept {
    return std::isfinite(value) && value>=0.F && value<kCycleSeconds;
}
} // namespace detail

/** F4E660 post-return state. Requested indices are intentionally not used:
 * B0/B4 only change after the native clip loader succeeds. The controller is
 * embedded state, not a typed component. No process-pointer resolution here. */
[[nodiscard]] inline bool intro_flight(std::span<const std::byte> controller,
    std::uint32_t entity,std::uint32_t character,std::uint32_t bipedAnimation) noexcept {
    if(controller.size()<0xB8 || entity==UINT32_MAX || character==UINT32_MAX
        || bipedAnimation==UINT32_MAX) { return false; }
    const auto duration=detail::f32(controller,0x38),elapsed=detail::f32(controller,0x3C);
    return detail::u32(controller,0)==entity && detail::u32(controller,4)==character
        && detail::u32(controller,8)==0 && detail::u32(controller,0xC)==1
        && detail::u32(controller,0x10)==0x80F45178U && detail::u32(controller,0x14)==bipedAnimation
        && controller[0x21]!=std::byte{0} && detail::u32(controller,0xB0)==1
        && detail::u32(controller,0xB4)==0 && duration==8.700000762939453F
        && std::isfinite(elapsed) && elapsed>=0.F && elapsed<=duration;
}

/** Node 4 loaded in graph record 0: the authored two-arm summon has started.
 * The native transition from waiting node 3 on event C0F9C866 is the only way
 * to reach this node, so its loaded state is the summon-start receipt. Elapsed
 * is the native clip clock; callers must not derive a spawn time from it. */
[[nodiscard]] inline bool intro_summon(std::span<const std::byte> controller,
    std::uint32_t entity,std::uint32_t character,std::uint32_t bipedAnimation) noexcept {
    if(controller.size()<0xB8 || entity==UINT32_MAX || character==UINT32_MAX
        || bipedAnimation==UINT32_MAX) { return false; }
    const auto duration=detail::f32(controller,0x38),elapsed=detail::f32(controller,0x3C);
    return detail::u32(controller,0)==entity && detail::u32(controller,4)==character
        && detail::u32(controller,8)==0 && detail::u32(controller,0xC)==1
        && detail::u32(controller,0x10)==0x80F45178U && detail::u32(controller,0x14)==bipedAnimation
        && controller[0x21]!=std::byte{0} && detail::u32(controller,0xB0)==4
        && detail::u32(controller,0xB4)==0 && duration==kSummonSeconds
        && std::isfinite(elapsed) && elapsed>=0.F && elapsed<=duration;
}

[[nodiscard]] inline bool intro_terminal_idle(std::span<const std::byte> controller,
    std::uint32_t entity,std::uint32_t character,std::uint32_t bipedAnimation) noexcept {
    if(controller.size()<0xB8 || entity==UINT32_MAX || character==UINT32_MAX
        || bipedAnimation==UINT32_MAX) { return false; }
    return detail::u32(controller,0)==entity && detail::u32(controller,4)==character
        // Native10C6760 ->1063850 ->F48F70 stores selector ordinals here:
        // group0 / sequence1 is (AFB11A12,65D2379F); graph record0 is separate.
        && detail::u32(controller,8)==0 && detail::u32(controller,0xC)==1
        && detail::u32(controller,0x10)==0x80F45178U && detail::u32(controller,0x14)==bipedAnimation
        && controller[0x21]!=std::byte{0} && detail::u32(controller,0xB0)==2
        && detail::u32(controller,0xB4)==0;
}

struct OverlayReceipt final {
    bool valid{},active{},newInstance{};
    Arm arm{Arm::left};
    std::uint16_t clip{UINT16_MAX};
    float weight{},elapsed{};
};

/** Header and group storage are copied from the same verified native component.
 * Root owns the handle/backlink resolution and the relative offset at +508.
 * Ambiguous simultaneous rows fail closed rather than choosing a blend layer. */
[[nodiscard]] inline OverlayReceipt parse_overlay(std::span<const std::byte> header,
    std::span<const std::byte> groups,std::uint32_t entity,std::uint32_t fullBody,Arm arm) noexcept {
    if(!valid_arm(arm) || entity==UINT32_MAX || fullBody==UINT32_MAX
        || header.size()<kFullBodyHeaderBytes || groups.size()<kGroupBytes*kGroupCount) { return {}; }
    if(detail::u32(header,0)!=0x815B5A41U || detail::u32(header,4)!=0x80803640U
        || detail::u64(header,8)!=0x15B8 || detail::u32(header,0x24)!=fullBody
        || detail::u32(header,0x2C)!=entity || detail::u32(header,0x500)!=kGroupCount
        || detail::u64(header,0x508)==0) { return {}; }
    const auto group=groups.subspan(group_index(arm)*kGroupBytes,kGroupBytes);
    const auto count=detail::u32(group,0x70);
    if(count>4) { return {}; }
    OverlayReceipt result{true,false,false,arm,clip_index(arm),0,0};
    for(std::size_t i=0;i<count;++i) {
        const auto row=group.subspan(i*0x1C,0x1C);
        if(row[0x12]>std::byte{1} || row[0x13]>std::byte{1}) { return {}; }
        //10474A0 clears every row's marker before104B7A0 updates/compacts it.
        // A member tick can observe that intermediate state; it is unavailable
        // evidence, not proof that the animation has stopped.
        if(row[0x13]==std::byte{0}) { return {}; }
        const auto weight=detail::f32(row,4);
        if(!std::isfinite(weight) || weight<0.F || weight>1.F) { return {}; }
        if(weight<=0.0001F) { continue; }
        const auto elapsed=detail::f32(row,0x18);
        if(result.active || detail::u16(row,0xC)!=clip_index(arm) || !detail::elapsed_valid(elapsed)) { return {}; }
        result.active=true;result.newInstance=row[0x12]!=std::byte{0};
        result.weight=weight;result.elapsed=elapsed;
    }
    return result;
}

struct Owner final {
    std::uint64_t run{};
    std::uint32_t actor{UINT32_MAX},character{UINT32_MAX},entity{UINT32_MAX};
    std::uint32_t generation{},revision{},fullBody{UINT32_MAX};
    std::uint8_t island{};
    std::uint32_t actionEpoch{};
    bool operator==(const Owner&) const = default;
    [[nodiscard]] constexpr bool valid() const noexcept {
        return run!=0 && actor!=UINT32_MAX && character!=UINT32_MAX && entity!=UINT32_MAX
            && generation!=0 && fullBody!=UINT32_MAX && island<=4;
    }
};
enum class CyclePhase : std::uint8_t { idle,claimed,playing,completed,interrupted };
enum class CycleEvent : std::uint8_t { none,started,advanced,completed,interrupted };

[[nodiscard]] inline bool zero_baseline(const std::array<float,4>& values) noexcept {
    for(const auto value:values) { if(value!=0.F) { return false; } }
    return true; // NaN/inf fail the comparison above; signed zero is native zero.
}

/** Only tracks one owned native action; does not enable encounter sources or
 * issue native writes. The caller claims before setting the property, observes
 * it after native updates, then restores zero through576420 on completion.
 * requestId and sampleSerial are monotonically increasing within the run. */
class CycleTracker final {
public:
    void begin_run(std::uint64_t run) noexcept {
        if(run==run_) { return; }
        *this={};run_=run;
    }
    [[nodiscard]] bool claim(const Owner& owner,std::uint64_t requestId,Arm arm,
        const std::array<float,4>& baseline) noexcept {
        if(!owner.valid() || owner.run!=run_ || !valid_arm(arm) || requestId==0
            || requestId<=requestId_ || !zero_baseline(baseline)
            || phase_==CyclePhase::claimed || phase_==CyclePhase::playing) { return false; }
        owner_=owner;requestId_=requestId;arm_=arm;phase_=CyclePhase::claimed;
        lastSerial_=0;lastElapsed_=0;return true;
    }
    [[nodiscard]] CycleEvent observe(const Owner& owner,std::uint64_t requestId,
        std::uint64_t sampleSerial,const OverlayReceipt& receipt) noexcept {
        if(owner!=owner_ || owner.run!=run_ || requestId!=requestId_ || sampleSerial==0
            || sampleSerial<=lastSerial_ || (phase_!=CyclePhase::claimed && phase_!=CyclePhase::playing)) {
            return CycleEvent::none;
        }
        lastSerial_=sampleSerial;
        if(!receipt.valid || receipt.arm!=arm_ || receipt.clip!=clip_index(arm_)) { return CycleEvent::none; }
        if(!receipt.active) {
            if(phase_==CyclePhase::claimed) { return CycleEvent::none; }
            phase_=CyclePhase::interrupted;return CycleEvent::interrupted;
        }
        if(!std::isfinite(receipt.weight) || receipt.weight<=0.0001F || receipt.weight>1.F
            || !detail::elapsed_valid(receipt.elapsed)) { return CycleEvent::none; }
        if(phase_==CyclePhase::claimed) {
            phase_=CyclePhase::playing;lastElapsed_=receipt.elapsed;return CycleEvent::started;
        }
        if(receipt.elapsed==lastElapsed_) { return CycleEvent::none; }
        // A recreated native row is a restart, not the old cycle completing.
        if(receipt.newInstance) { phase_=CyclePhase::interrupted;return CycleEvent::interrupted; }
        if(receipt.elapsed>lastElapsed_) {
            lastElapsed_=receipt.elapsed;return CycleEvent::advanced;
        }
        // Native elapsed wraps near5.5->0. Reject a small backwards jump or a
        // truncated/stale row. Sampling once per native update keeps this bound.
        if(lastElapsed_>=kCycleSeconds*0.5F && receipt.elapsed<kCycleSeconds*0.5F) {
            phase_=CyclePhase::completed;lastElapsed_=receipt.elapsed;return CycleEvent::completed;
        }
        phase_=CyclePhase::interrupted;return CycleEvent::interrupted;
    }
    [[nodiscard]] CyclePhase phase() const noexcept { return phase_; }
    [[nodiscard]] const Owner& owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t request_id() const noexcept { return requestId_; }
    [[nodiscard]] Arm arm() const noexcept { return arm_; }
    [[nodiscard]] float elapsed() const noexcept { return lastElapsed_; }
private:
    Owner owner_{};
    std::uint64_t run_{},requestId_{},lastSerial_{};
    Arm arm_{Arm::left};
    CyclePhase phase_{CyclePhase::idle};
    float lastElapsed_{};
};

} // namespace dawn::state::activity::omega_boss_lift
