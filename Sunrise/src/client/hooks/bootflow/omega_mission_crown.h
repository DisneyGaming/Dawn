#pragma once
#include "omega_boss_graph.h"
#include "omega_boss_graph_observation.h"
#include "../../../state/activity/omega/omega_mission_state.h"
namespace sunrise::client::hooks::bootflow::omega_mission_crown {
namespace graph=omega_boss_graph;
namespace observation=omega_boss_graph_observation;
namespace mission=state::activity::omega::mission;
struct Cycle {
    std::uint32_t sequence{},deletion{},shield{},eyeEnd{};
    std::int16_t ordinal{};
    std::int32_t graph{};
};
inline constexpr std::array<Cycle,3> cycles{{
    {0x65D2379E,0x1D3CF86F,0xE9D4F854,0x773E312A,2,3},
    {0x65D2379D,0x1D3CF86C,0xE9D4F857,0x773E3129,3,2},
    {0x65D2379B,0x1D3CF86A,0xE9D4F851,0x160B3CA2,5,1}}};
enum class Node { unknown,idle,summon,deletion,hold,eyeOpening,eye,recover,death };
struct Observation { Node node{};std::int32_t index{-1};std::uint32_t clip{};bool terminal{}; };
inline bool decode(std::span<const std::byte> bytes,std::span<const std::byte> bank,unsigned cycle,
    bool result,Observation& out) noexcept {
    out={};
    if(cycle<1 || cycle>3 || bytes.size()<observation::kGraphBytes) return false;
    const auto g=cycles[cycle-1].graph;
    const auto node=observation::field<std::int32_t>(bytes,0xB0);
    const auto next=observation::field<std::int32_t>(bytes,0xA4);
    const auto count=cycle==3?7:8;
    if(node<0 || node>=count || next<0 || next>=count
        || observation::field<std::int32_t>(bytes,0xA8)!=g || observation::field<std::int32_t>(bytes,0xB4)!=g
        || observation::field<std::uint32_t>(bytes,0x10)!=observation::kGraphAsset
        || !observation::field<std::uint8_t>(bytes,0x21)) return false;
    constexpr std::array<std::array<int,8>,3> rows{{{1,0,17,16,1,13,14,10},{1,0,6,3,1,13,14,10},{1,0,6,11,12,4,13,-1}}};
    constexpr std::array<Node,8> first{Node::idle,Node::deletion,Node::hold,Node::eyeOpening,Node::idle,Node::summon,Node::eye,Node::recover};
    constexpr std::array<Node,7> last{Node::idle,Node::deletion,Node::hold,Node::eyeOpening,Node::death,Node::eye,Node::summon};
    const auto index=static_cast<std::size_t>(node);
    const auto semantic=cycle==3?last[index]:first[index];
    const bool loop=node==0 || semantic==Node::hold || semantic==Node::eye;
    if(observation::field<std::int32_t>(bytes,0x30)!=rows[cycle-1][index]
        || ((observation::field<std::uint8_t>(bytes,0x20)&1)!=0)!=loop
        || !observation::bank_clip(bank,rows[cycle-1][index],out.clip)) return false;
    const auto time=observation::field<float>(bytes,0x3C),limit=observation::field<float>(bytes,0x38);
    if(!std::isfinite(time)||!std::isfinite(limit)||time<0 || limit<=0 || time>limit+0.001F) return false;
    out.node=semantic;out.index=node;
    out.terminal=!result && !loop && node==next && semantic==Node::death;
    return result || out.terminal;
}
struct Track {
    mission::Token start{},command{};
    graph::EventLease lease{};
    std::uint32_t event{};
    unsigned cycle{};
    Node node{};
    bool queueClaimed{},accepted{},stopClaimed{},retired{},summoned{},recoverSeen{},deathFinished{},uncertain{};
    mission::Token eyeToken{};
    float eyePrevious{};
    bool eyeOwned{},eyeBusy{},eyeReleased{};
    unsigned refillSlot{8};
    std::array<std::byte,16> refillIdentity{};
};
struct HealthTrack {
    mission::Token token{};
    std::uint32_t handle{UINT32_MAX};
    float previous{};
    bool sampled{},crossed{};
    unsigned diagnosticSamples{};
};
}
