#pragma once

#include "omega_boss_crown_action.h"

namespace dawn::state::activity::omega_boss_combat {
using Owner=omega_boss_crown::Owner;
struct Token final {
    Owner owner{};
    std::uint32_t epoch{};
    std::uint8_t cycle{};
    bool operator==(const Token&) const=default;
    [[nodiscard]] constexpr bool valid() const noexcept { return owner.valid() && cycle>=1 && cycle<=3; }
};
struct Graph final {
    std::uint32_t sequence{},ordinal{},record{},summonNode{},eyeNode{};
    std::uint32_t deletion{},shield{},eye{};
};
inline constexpr std::array<Graph,3> kGraphs{{
    {0x65D2379EU,2,3,5,6,0x1D3CF86FU,0xE9D4F854U,0x773E312AU},
    {0x65D2379DU,3,2,5,6,0x1D3CF86CU,0xE9D4F857U,0x773E3129U},
    {0x65D2379BU,5,1,6,5,0x1D3CF86AU,0xE9D4F851U,0x160B3CA2U}
}};
inline constexpr std::uint32_t kStunnedName=0x12FE76EEU,kStunnedResource=0x80F455A2U;
inline constexpr std::uint32_t kEyeRefillName=0xC57C61EBU,kEyeRefillResource=0x80F45564U;
struct NamedEventIdentity final {
    std::uint32_t slot{UINT32_MAX},entity{UINT32_MAX},component{UINT32_MAX};
    bool operator==(const NamedEventIdentity&) const=default;
    [[nodiscard]] constexpr bool valid() const noexcept { return slot<8 && entity!=UINT32_MAX && component!=UINT32_MAX; }
};
// E98B70 stores the created entity at manager+8 and its full event component
// handle at+C. Retain both so same-name reuse cannot cancel a replacement.
[[nodiscard]] inline NamedEventIdentity named_event_identity(std::span<const std::byte> bytes,
    std::uint32_t character,std::uint32_t animation,std::uint32_t name) noexcept {
    namespace d=omega_boss_lift::detail;
    if(bytes.size()<0x860 || character==UINT32_MAX || animation==UINT32_MAX
        || d::u32(bytes,0)!=character || d::u32(bytes,4)!=animation) { return {}; }
    NamedEventIdentity result{};unsigned matches{};
    for(unsigned i=0;i<8;++i) {
        const auto offset=0x20+i*0x108;
        if(d::u32(bytes,offset)==name) { ++matches;result={i,d::u32(bytes,offset+0x10),d::u32(bytes,offset+0x14)}; }
    }
    return matches==1 && result.valid()?result:NamedEventIdentity{};
}
// Original AB0750(95FB2E01/48/55, milestone4), curve80F47562 parameter70.
inline constexpr std::array<float,3> kFinalDestination{
    -1490.12109375F,-491.90216064453125F,-32.84910202026367F};
[[nodiscard]] constexpr const Graph* graph(std::uint8_t cycle) noexcept {
    return cycle>=1 && cycle<=3?&kGraphs[cycle-1]:nullptr;
}
template<std::size_t N> constexpr void write(std::array<std::byte,N>& bytes,std::size_t at,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i) { bytes[at+i]=std::byte((value>>(8*i))&255U); }
}
[[nodiscard]] constexpr auto action(std::uint8_t cycle) noexcept {
    auto result=omega_presentation::boss_intro_action();
    if(const auto* value=graph(cycle)) { write(result,0x1C,value->sequence); }
    else { result={}; }
    return result;
}
[[nodiscard]] constexpr auto request(std::uint8_t cycle,std::uint32_t event=0) noexcept {
    auto result=omega_presentation::boss_intro_stop_request();
    if(const auto* value=graph(cycle)) {
        write(result,4,value->sequence);write(result,8,event);
        if(event!=0) { result[0x60]=std::byte{0x5E}; }
    } else { result={}; }
    return result;
}
[[nodiscard]] constexpr bool action_active(std::span<const std::byte> bytes,std::int32_t head,std::uint8_t cycle) noexcept {
    const auto* value=graph(cycle);
    namespace d=omega_boss_lift::detail;
    return value!=nullptr && head==0 && bytes.size()>=0x20 && d::u32(bytes,0)==1
        && bytes[8]==std::byte{9} && d::u32(bytes,0x18)==0xAFB11A12U && d::u32(bytes,0x1C)==value->sequence;
}
[[nodiscard]] constexpr omega_presentation::BossSummonEventState event_state(
    std::span<const std::byte> bytes,std::uint8_t cycle,std::uint32_t event) noexcept {
    namespace d=omega_boss_lift::detail;
    const auto* value=graph(cycle);
    if(bytes.size()<0xB8 || value==nullptr || event==0) { return {}; }
    omega_presentation::BossSummonEventState state{true,d::u32(bytes,0x34),0};
    if(state.count>16) { return {}; }
    for(unsigned i=0;i<state.count;++i) {
        const auto offset=0x38+i*8;
        if(d::u32(bytes,offset)==event && d::u16(bytes,offset+4)==value->ordinal && bytes[offset+6]==std::byte{}) {
            const auto refs=std::to_integer<unsigned>(bytes[offset+7]);
            if(state.refs!=0 || refs==0 || refs>127) { return {}; }
            state.refs=refs;
        }
    }
    return state;
}
enum class Stage : std::uint8_t { unavailable,summoning,ready,deleting,deletionHold,eyeExposing,eyeVulnerable,recovery,recovered,dying,dead };
struct Receipt final { Stage stage{};bool valid{}; };
/** Original A889E0/F45A60/F47240 with actual assets proves these indices.
 * Native F4E660 returning zero at final node4 is clip completion; its own
 * non-looping/active and requested==loaded predicates are retained below.
 * We never derive mission completion from a host elapsed-time deadline. */
[[nodiscard]] inline Receipt parse(std::span<const std::byte> bytes,const Token& token,char nativeResult) noexcept {
    namespace d=omega_boss_lift::detail;
    if(!token.valid() || bytes.size()<0xB8) { return {}; }
    const auto& owner=token.owner;const auto& value=*graph(token.cycle);
    if(d::u32(bytes,0)!=owner.entity || d::u32(bytes,4)!=owner.character || d::u32(bytes,8)!=0
        || d::u32(bytes,0xC)!=value.ordinal || d::u32(bytes,0x10)!=0x80F45178U
        || d::u32(bytes,0x14)!=owner.biped || bytes[0x21]==std::byte{}
        || d::u32(bytes,0xB4)!=value.record) { return {}; }
    const auto node=d::u32(bytes,0xB0);const auto duration=d::f32(bytes,0x38),elapsed=d::f32(bytes,0x3C);
    if(!std::isfinite(duration) || !std::isfinite(elapsed) || elapsed<0.F || duration<=0.F
        || elapsed>duration+0.001F) { return {}; }
    Stage stage=Stage::unavailable;float expected{};
    if(node==value.summonNode) { stage=Stage::summoning;expected=7.200000286102295F; }
    else if(node==0) { stage=Stage::ready;expected=10.F; }
    else if(node==1) { stage=Stage::deleting;expected=12.566667556762695F; }
    else if(node==2) { stage=Stage::deletionHold;expected=6.566667079925537F; }
    else if(node==3) { stage=Stage::eyeExposing;expected=token.cycle==3?5.F:4.666666984558105F; }
    else if(node==value.eyeNode) { stage=Stage::eyeVulnerable;expected=token.cycle==3?8.666666984558105F:10.F; }
    else if(node==7 && token.cycle<3) { stage=Stage::recovery;expected=5.933333396911621F; }
    else if(node==4) {
        if(token.cycle<3) { stage=Stage::recovered;expected=10.F; }
        else {
            stage=Stage::dying;expected=7.833333492279053F;
            if(nativeResult==0 && (std::to_integer<unsigned>(bytes[0x20])&3U)==0
                && d::u32(bytes,0xA4)==node && d::u32(bytes,0xA8)==value.record
                && elapsed>=duration-0.0001F) { stage=Stage::dead; }
        }
    }
    return {stage,stage!=Stage::unavailable && std::abs(duration-expected)<0.00001F};
}
enum class Command : std::uint8_t { summon,deletion,shield,eye };
enum class Milestone : std::uint8_t { none,summonStarted,summonFinished,deletionStarted,deletionHold,eyeExposing,eyeVulnerable,recoveryStarted,recovered,deathStarted,deathFinished };
// CF9087D3 supplies the cycle 1/2 split-head layer and the native eye-region
// state selected by 80F451C7. Cycle 3 keeps its authored record4 pose but still
// needs that eye-region state; excluding it leaves the exposed eye immune.
inline constexpr std::uint32_t kEyeHoldName=0xCF9087D3U;
inline constexpr std::size_t kEyeHoldOrdinal=41;
struct EyeHold final {
    Token token{};
    std::uint32_t parent{UINT32_MAX};
    [[nodiscard]] constexpr bool valid() const noexcept { return token.valid() && parent!=UINT32_MAX; }
    [[nodiscard]] constexpr bool can_restore(std::uint32_t currentParent,float currentValue) const noexcept {
        return valid() && parent==currentParent && currentValue==1.F;
    }
};
class EyeHoldLedger final {
    EyeHold active_{};
    Token attempted_{};
public:
    [[nodiscard]] constexpr EyeHold active() const noexcept { return active_; }
    [[nodiscard]] constexpr bool claim(const Token& token,std::uint32_t parent,Stage stage,float baseline) noexcept {
        if(active_.valid() || !token.valid() || token.epoch==0 || parent==UINT32_MAX
            || stage!=Stage::eyeVulnerable || baseline!=0.F || token==attempted_) { return false; }
        attempted_=token;active_={token,parent};return true;
    }
    [[nodiscard]] constexpr EyeHold release(const Token& phase,bool retiring=false) noexcept {
        if(!active_.valid() || !phase.valid() || phase.owner!=active_.token.owner
            || phase.cycle!=active_.token.cycle || (!retiring && phase.epoch<active_.token.epoch)) { return {}; }
        const auto result=active_;active_={};return result;
    }
};
struct Outcome final { Milestone milestone{};Token token{};std::uint32_t releaseEvent{}; };
class Tracker final {
    Token graph_{},command_{};
    Command kind_{};
    Stage stage_{};
    std::uint64_t serial_{};
    std::uint32_t event_{};
    bool active_{},started_{},finished_{};
public:
    [[nodiscard]] const Token& graph_token() const noexcept { return graph_; }
    [[nodiscard]] const Token& command_token() const noexcept { return command_; }
    [[nodiscard]] Stage stage() const noexcept { return stage_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] std::uint32_t leased_event() const noexcept { return event_; }
    [[nodiscard]] bool begin(const Token& token) noexcept {
        if(active_ || !token.valid()) { return false; }
        graph_=command_=token;active_=true;return true;
    }
    [[nodiscard]] bool can_request(const Token& token,Command kind) const noexcept {
        if(!active_ || !token.valid() || token.owner!=graph_.owner || token.cycle!=graph_.cycle
            || token.epoch<=command_.epoch || !finished_ || event_!=0) { return false; }
        return (kind==Command::deletion && stage_==Stage::ready)
            || (kind==Command::shield && stage_==Stage::deletionHold)
            || (kind==Command::eye && stage_==Stage::eyeVulnerable);
    }
    [[nodiscard]] bool claim(const Token& token,Command kind) noexcept {
        if(!can_request(token,kind)) { return false; }
        command_=token;kind_=kind;started_=finished_=false;
        const auto& value=*graph(token.cycle);
        event_=kind==Command::deletion?value.deletion:kind==Command::shield?value.shield:value.eye;
        return true;
    }
    [[nodiscard]] Outcome observe(const Token& graphToken,std::uint64_t serial,Receipt receipt) noexcept {
        if(!active_ || graphToken!=graph_ || serial==0 || serial<=serial_ || !receipt.valid) { return {}; }
        serial_=serial;
        Stage start{},finish{};Milestone began{},ended{};
        switch(kind_) {
        case Command::summon:start=Stage::summoning;finish=Stage::ready;began=Milestone::summonStarted;ended=Milestone::summonFinished;break;
        case Command::deletion:start=Stage::deleting;finish=Stage::deletionHold;began=Milestone::deletionStarted;ended=Milestone::deletionHold;break;
        case Command::shield:start=Stage::eyeExposing;finish=Stage::eyeVulnerable;began=Milestone::eyeExposing;ended=Milestone::eyeVulnerable;break;
        case Command::eye:
            start=command_.cycle==3?Stage::dying:Stage::recovery;finish=command_.cycle==3?Stage::dead:Stage::recovered;
            began=command_.cycle==3?Milestone::deathStarted:Milestone::recoveryStarted;
            ended=command_.cycle==3?Milestone::deathFinished:Milestone::recovered;break;
        }
        if(!started_ && receipt.stage==start) {
            started_=true;stage_=start;const auto release=event_;event_=0;return {began,command_,release};
        }
        if(started_ && !finished_ && receipt.stage==finish) {
            finished_=true;stage_=finish;return {ended,command_,0};
        }
        return {};
    }
};
} // namespace dawn::state::activity::omega_boss_combat
