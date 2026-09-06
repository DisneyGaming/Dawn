#pragma once
#include "executor.h"

namespace sunrise::state::activity::coo::combat {
// This is an Omega mechanic-plugin binding, not a native roster slot. The
// plugin validates the owned actor and resolves native populations/Scenes from
// the accepted catalogs. The executor knows only requests and native facts.
inline constexpr Asset kBinding{0x95FB2E01U, 0x80F45253U, 0, 0};
enum class Control : std::uint32_t {
    none, left, right, both, depart, island1, island2, island3, crown1,
    wave1, wave2, deletion, rescue, route, expose, recover, die,
    crown2, escapeAdds, relocate, finalScene, crown3, ending
};
enum class Fact : std::uint32_t {
    none, initialFinished, summonFinished, cleared, departed, arrived,
    crownReady, deletionStarted, rescueReady, dunked, vulnerable, eyeThreshold,
    recovered, checkpoint, returnReady, finalDeparted, finalArrival, dead, deathFinished
};
struct Node final {
    std::string_view name;
    Control request{};
    std::array<Fact, 3> join{};
};
template<std::size_t N>
constexpr auto commands(const std::array<Node, N>& nodes) noexcept {
    std::array<std::array<CommandSpec, 4>, N> result{};
    for (std::size_t i=0;i<N;++i) {
        std::size_t n{};
        if(nodes[i].request!=Control::none) {
            result[i][n++]={Operation::mechanic,kBinding,static_cast<std::uint32_t>(nodes[i].request),Wait::requested};
        }
        for(const auto fact:nodes[i].join) {
            if(fact!=Fact::none) { result[i][n++]={Operation::observation,kBinding,static_cast<std::uint32_t>(fact),Wait::observed}; }
        }
    }
    return result;
}
template<std::size_t N>
constexpr auto steps(const std::array<Node, N>& nodes, const std::array<std::array<CommandSpec,4>,N>& specs) noexcept {
    std::array<Step,N> result{};
    for(std::size_t i=0;i<N;++i) {
        std::size_t count=nodes[i].request!=Control::none?1U:0U;
        for(const auto fact:nodes[i].join) { if(fact!=Fact::none) { ++count; } }
        result[i]={nodes[i].name,i==0?0U:std::uint32_t{1}<<(i-1),{specs[i].data(),count}};
    }
    return result;
}
inline constexpr std::array<Node,7> kLairNodes{{
    {"native intro summon",Control::none,{Fact::initialFinished}},
    {"left summon",Control::left,{Fact::summonFinished}},
    {"right summon",Control::right,{Fact::summonFinished}},
    {"required cohort deaths",Control::none,{Fact::cleared}},
    {"fold and path completion",Control::depart,{Fact::departed}},
    {"player arrival",Control::none,{Fact::arrived}},
    {"enter island A",Control::island1,{}}
}};
inline constexpr std::array<Node,5> kIslandANodes{{
    {"left summon",Control::left,{Fact::summonFinished}},
    {"required cohort deaths",Control::none,{Fact::cleared}},
    {"fold and path completion",Control::depart,{Fact::departed}},
    {"player arrival",Control::none,{Fact::arrived}},
    {"enter island B",Control::island2,{}}
}};
inline constexpr std::array<Node,5> kIslandBNodes{{
    {"right summon",Control::right,{Fact::summonFinished}},
    {"required cohort deaths",Control::none,{Fact::cleared}},
    {"fold and path completion",Control::depart,{Fact::departed}},
    {"player arrival",Control::none,{Fact::arrived}},
    {"enter island C",Control::island3,{}}
}};
inline constexpr std::array<Node,5> kIslandCNodes{{
    {"left summon",Control::left,{Fact::summonFinished}},
    {"required cohort deaths",Control::none,{Fact::cleared}},
    {"fold and path completion",Control::depart,{Fact::departed}},
    {"Crown arrival and cannon preparation",Control::none,{Fact::crownReady}},
    {"enter Crown",Control::crown1,{}}
}};
// The first three waves are kill barriers. The cycle-two Cabal escape cohort
// intentionally overlaps relocation: only its native summon must finish.
inline constexpr std::array<Node,15> kCrown1Nodes{{
    {"both summon",Control::both,{Fact::summonFinished}},
    {"first wave deaths",Control::none,{Fact::cleared}},
    {"select second wave",Control::wave1,{}},
    {"left summon",Control::left,{Fact::summonFinished}},
    {"second wave deaths",Control::none,{Fact::cleared}},
    {"select third wave",Control::wave2,{}},
    {"right summon",Control::right,{Fact::summonFinished}},
    {"third wave deaths",Control::none,{Fact::cleared}},
    {"deletion animation",Control::deletion,{Fact::deletionStarted}},
    {"Osiris rescue Scene",Control::rescue,{Fact::rescueReady}},
    {"charge route and dunk",Control::route,{Fact::dunked}},
    {"native eye exposure",Control::expose,{Fact::vulnerable}},
    {"native eye threshold",Control::none,{Fact::eyeThreshold}},
    {"recovery join",Control::recover,{Fact::recovered,Fact::checkpoint,Fact::returnReady}},
    {"enter second cycle",Control::crown2,{}}
}};
// Use a prefix of the common sequence and append the authored cycle-specific tail.
inline constexpr auto kCrown1Commands=commands(kCrown1Nodes);
inline constexpr auto kCrown1Steps=steps(kCrown1Nodes,kCrown1Commands);
inline constexpr auto kCrown2Nodes=[] {
    std::array<Node,19> result{};
    for(std::size_t i=0;i<14;++i) { result[i]=kCrown1Nodes[i]; }
    result[14]={"escape cohort",Control::escapeAdds,{}};
    result[15]={"escape summon completion",Control::right,{Fact::summonFinished}};
    result[16]={"final relocation fold and path",Control::relocate,{Fact::finalDeparted}};
    result[17]={"final cannon Scene and player arrival",Control::finalScene,{Fact::finalArrival}};
    result[18]={"enter third cycle",Control::crown3,{}};
    return result;
}();
inline constexpr auto kCrown3Nodes=[] {
    std::array<Node,15> result{};
    for(std::size_t i=0;i<13;++i) { result[i]=kCrown1Nodes[i]; }
    result[13]={"native death and animation join",Control::die,{Fact::dead,Fact::deathFinished}};
    result[14]={"ending handoff",Control::ending,{}};
    return result;
}();
#define COO_COMBAT_GRAPH(Name, Nodes) \
    inline constexpr auto Name##Commands=commands(Nodes); \
    inline constexpr auto Name##Steps=steps(Nodes,Name##Commands); \
    inline constexpr Definition Name{#Name,Schema::omegaArchive,Name##Steps};
COO_COMBAT_GRAPH(kLair,kLairNodes)
COO_COMBAT_GRAPH(kIslandA,kIslandANodes)
COO_COMBAT_GRAPH(kIslandB,kIslandBNodes)
COO_COMBAT_GRAPH(kIslandC,kIslandCNodes)
COO_COMBAT_GRAPH(kCrown2,kCrown2Nodes)
COO_COMBAT_GRAPH(kCrown3,kCrown3Nodes)
#undef COO_COMBAT_GRAPH
inline constexpr Definition kCrown1{"kCrown1",Schema::omegaArchive,{kCrown1Steps.data(),15}};
inline constexpr std::string_view kSectionRoles[]{"lair","island_a","island_b","island_c","crown_1","crown_2","crown_3"};
inline constexpr std::array<const Definition*,7> kSections{&kLair,&kIslandA,&kIslandB,&kIslandC,&kCrown1,&kCrown2,&kCrown3};
} // namespace sunrise::state::activity::coo::combat
