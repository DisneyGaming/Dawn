#pragma once
#include "mercury_populations.h"
#include "patrol_replenishment.h"
#include <array>
#include <span>
#include <string_view>
#include <utility>

namespace sunrise::server::runtime::activity::mercury::freeroam {
enum class Faction : std::uint8_t { cabal, vex };
enum class WarPhase : std::uint8_t { unavailable, announcement, active, intermission, complete, failed };
inline constexpr std::uint32_t kAnnouncementIncident=0xA2DD920AU;
inline constexpr std::size_t kMaximumAuthoredActorsPerRequest=3;
inline constexpr std::size_t kReceiptLedgerCapacity=64;
inline constexpr std::size_t kNativeProvisionalCapacity=256;
inline constexpr std::size_t kNativeEventMailboxCapacity=1024;
struct Patrol final {
    std::size_t capability{};Faction faction{};std::string_view placement;bool largeArea{};
    // Zero baseline follows the destination-wide count. Reviewed ranges are
    // user-directed policy for this exact native source, not recovered retail
    // quotas. Initial requests use baseline; renewals vary deterministically.
    std::uint8_t minimum{},baseline{},maximum{};
};
inline constexpr std::array<Patrol,27> kPatrols{{
    // Mixed minor/major Legionary alternatives cannot receive tier-specific
    // counts. The possible major makes one the conservative hard-cap target.
    {1,Faction::cabal,"pf_lighthouse_ca_pond_a",true,1,1,1},
    {7,Faction::vex,"pf_lighthouse_vx_center_left_b",false,2,3,4},       // Harpy
    {8,Faction::cabal,"pf_lighthouse_ca_blocks_right_a",false,1,1,1},   // Unresolved type; conservative singleton
    {9,Faction::cabal,"pf_lighthouse_ca_cannon_forest_a",false,2,2,2},  // minor Centurion
    {10,Faction::cabal,"pf_lighthouse_ca_cannon_tower_a",false,1,1,1},  // Phalanx
    {11,Faction::cabal,"pf_lighthouse_ca_crater_right_a",false,2,2,2},  // minor Centurion
    {12,Faction::vex,"pf_lighthouse_vx_center_right_a",false,1,1,1},    // Minotaur, minor/major
    {13,Faction::vex,"pf_lighthouse_vx_crater_cannon_a",false,2,2,3},   // Goblin escort; sibling26 consumes one shared Goblin slot
    {14,Faction::vex,"pf_lighthouse_vx_steps_a",false,2,3,4},           // Goblin, not a pillar sentry
    {15,Faction::cabal,"pf_lighthouse_ca_cannon_mid_a_hotspot",true,4,4,6}, // Legionary
    // Mercury Defender is a named Major leader even when the package member
    // selector only resolves the underlying Hobgoblin template. Keep every
    // selected Defender-bearing hotspot at one; fixed_singleton() also stops
    // the reconstructed faction-war wave from adding requests to this lease.
    {16,Faction::vex,"pf_lighthouse_vx_cannon_mid_a_hotspot",true,1,1,1},   // Mercury Defender (Hobgoblin)
    {17,Faction::cabal,"pf_lighthouse_ca_crater_cannon_a_hotspot",true,2,2,2}, // Psion
    {18,Faction::vex,"pf_lighthouse_vx_crater_left_a_hotspot",true,1,1,1}, // Mercury Defender (Hobgoblin)
    // The hotspot selector authors a miniboss Centurion. The user's observed
    // yellow-bar Mercury Conqueror by the Infinite Forest gate therefore owns
    // one leader lease; the separate cannon-forest minor Centurions stay two.
    {19,Faction::cabal,"pf_lighthouse_ca_forest_a_hotspot",true,1,1,1},
    // All six native choices are minor Hobgoblins. No exact live observation
    // ties this source to the named Defender, so retain the ordinary range.
    {20,Faction::vex,"pf_lighthouse_vx_forest_a_hotspot",true,1,2,3},
    {21,Faction::vex,"pf_lighthouse_vx_pond_a_hotspot",true,1,1,1},     // Hydra/Minotaur
    // Reviewed host composition follows Prima's ordinary 2+1 Cabal cluster.
    // Native sibling ownership is exact; retail concurrency at this tower
    // remains inferred pending live attribution. Keep this cohort fixed.
    {23,Faction::cabal,"pf_lighthouse_ca_cannon_tower_a",false,2,2,2},  // Legionary
    // Whole-registry sibling budgets, freshly pinned to native ownership and
    // exact fallback point records. No additional hotspot/event is activated.
    {0,Faction::cabal,"pf_lighthouse_ca_pond_a",false,2,2,2},          // Legionary/Psion alternatives + existing one leader-capable source
    {22,Faction::cabal,"pf_lighthouse_ca_cannon_forest_a",false,2,2,4}, // Legionaries + existing two minor Centurions
    {24,Faction::cabal,"pf_lighthouse_ca_crater_right_a",false,4,4,4}, // Legionaries + existing two minor Centurions
    {25,Faction::vex,"pf_lighthouse_vx_center_right_a",false,3,3,3},  // Goblins; mixed-rank second category stays zero
    {26,Faction::vex,"pf_lighthouse_vx_crater_cannon_a",false,1,1,1}, // Minor/major Goblin alternative, never multiplied
    {27,Faction::vex,"pf_lighthouse_vx_steps_a",false,1,2,2},         // Hobgoblins + existing Goblins
    {28,Faction::vex,"pf_lighthouse_vx_cannon_mid_a_hotspot",false,2,3,3}, // Goblin escort for existing singleton Defender
    {29,Faction::vex,"pf_lighthouse_vx_crater_left_a_hotspot",false,2,3,3}, // Goblin escort for existing singleton Defender
    {30,Faction::cabal,"pf_lighthouse_ca_forest_a_hotspot",false,1,1,1}, // Unresolved infantry type; existing Centurion remains one
    {31,Faction::vex,"pf_lighthouse_vx_pond_a_hotspot",false,2,2,3},   // Harpy escort for existing singleton Hydra
}};
struct Wave final {
    std::array<std::size_t,2> capabilities{};
    std::array<Faction,2> factions{{Faction::cabal,Faction::vex}};
    std::array<std::string_view,2> placements{};
};
inline constexpr std::array<Wave,4> kWaves{{
    {{{15,16}},{{Faction::cabal,Faction::vex}},{{"cannon_middle","cannon_middle"}}},
    {{{17,18}},{{Faction::cabal,Faction::vex}},{{"crater","crater"}}},
    {{{19,20}},{{Faction::cabal,Faction::vex}},{{"forest","forest"}}},
    // Cabal pond source1 is the proven fallback already requested by the
    // persistent graph; source0 consumed without creating an actor in r1.
    {{{1,21}},{{Faction::cabal,Faction::vex}},{{"pond","pond"}}},
}};
inline constexpr std::size_t kWarWaveCount=kWaves.size();
struct Configuration final {
    std::uint32_t respawnMilliseconds{30000};
    std::uint32_t interwaveMilliseconds{8000};
    std::array<std::uint8_t,kWarWaveCount> waveRequests{{2,3,4,5}};
    std::uint8_t normalPatrolRequests{3},largePatrolRequests{4};
    bool populations{true},war{true};
};
[[nodiscard]] constexpr std::uint8_t patrol_target(const Patrol& patrol,
    const Configuration& configuration,std::uint32_t sequence=0) noexcept {
    if(!patrol.baseline)return patrol.largeArea
        ?configuration.largePatrolRequests:configuration.normalPatrolRequests;
    if(!sequence || patrol.minimum==patrol.maximum)return patrol.baseline;
    const auto& source=kPopulations[patrol.capability];
    auto mixed=source.registry->key^(std::uint32_t{source.slot}<<16)
        ^sequence*0x9E3779B9U;
    mixed^=mixed>>16;mixed*=0x7FEB352DU;mixed^=mixed>>15;
    return static_cast<std::uint8_t>(patrol.minimum
        +mixed%(patrol.maximum-patrol.minimum+1U));
}
[[nodiscard]] constexpr std::uint8_t patrol_maximum(const Patrol& patrol,
    const Configuration& configuration) noexcept {
    return patrol.baseline?patrol.maximum:patrol.largeArea
        ?configuration.largePatrolRequests:configuration.normalPatrolRequests;
}
[[nodiscard]] constexpr bool valid(const Configuration& value) noexcept {
    if(value.populations && (!value.respawnMilliseconds || value.respawnMilliseconds>3600000))return false;
    if(value.war && (!value.interwaveMilliseconds || value.interwaveMilliseconds>300000))return false;
    std::uint8_t largestWave{};
    for(const auto requests:value.waveRequests) {
        if(!requests || requests>5)return false;
        if(requests>largestWave)largestWave=requests;
    }
    // Three actors per request is a conservative capacity bound, not a fixed
    // native request multiplier. Reviewed per-source targets replace their
    // global area target in this sum; they are not added as other sources.
    if(!value.normalPatrolRequests || value.normalPatrolRequests>21
        || !value.largePatrolRequests
        || static_cast<unsigned>(value.largePatrolRequests)+largestWave>21)return false;
    std::size_t patrolRequests{};
    for(const auto& patrol:kPatrols) {
        if(patrol.baseline && (!patrol.minimum || patrol.minimum>patrol.baseline
            || patrol.baseline>patrol.maximum || patrol.maximum>21))return false;
        if(!patrol.baseline && (patrol.minimum || patrol.maximum))return false;
        const auto target=patrol_maximum(patrol,value);
        if(!target || static_cast<std::size_t>(target)*kMaximumAuthoredActorsPerRequest
                >kReceiptLedgerCapacity
            || (patrol.largeArea && static_cast<std::size_t>(target+largestWave)
                *kMaximumAuthoredActorsPerRequest>kReceiptLedgerCapacity))return false;
        if(value.populations)patrolRequests+=target;
    }
    const auto warRequests=value.war?std::size_t{2}*largestWave:0U;
    const auto simultaneousActors=(patrolRequests+warRequests)*kMaximumAuthoredActorsPerRequest;
    // Admission, death and retirement can each enqueue one exact receipt for
    // every simultaneously requested actor before the owner drains the bridge.
    if(simultaneousActors>kNativeProvisionalCapacity
        || simultaneousActors*3>kNativeEventMailboxCapacity)return false;
    return true;
}
template<class Document>
[[nodiscard]] bool configure(const Document& document,Configuration& output) noexcept {
    const auto parameter=[&](std::string_view name) noexcept {return document.views().parameter(name);};
    const auto* populations=parameter("freeroam_population_enabled");
    const auto* respawn=parameter("freeroam_respawn_ms");
    const auto* war=parameter("faction_war_enabled");
    const auto* interwave=parameter("faction_war_interwave_ms");
    if(!populations || !respawn || !war || !interwave)return false;
    const auto* normalPatrol=parameter("freeroam_normal_patrol_count");
    const auto* largePatrol=parameter("freeroam_large_patrol_count");
    if(!normalPatrol || !largePatrol || normalPatrol->value>UINT8_MAX || largePatrol->value>UINT8_MAX)return false;
    Configuration candidate{};candidate.respawnMilliseconds=respawn->value;
    candidate.interwaveMilliseconds=interwave->value;
    candidate.normalPatrolRequests=static_cast<std::uint8_t>(normalPatrol->value);
    candidate.largePatrolRequests=static_cast<std::uint8_t>(largePatrol->value);
    candidate.populations=populations->value!=0;candidate.war=war->value!=0;
    constexpr std::array<std::string_view,kWarWaveCount> names{{
        "faction_war_wave_1_count","faction_war_wave_2_count",
        "faction_war_wave_3_count","faction_war_wave_4_count"}};
    for(std::size_t i=0;i<names.size();++i) {
        const auto* value=parameter(names[i]);if(!value || value->value>UINT8_MAX)return false;
        candidate.waveRequests[i]=static_cast<std::uint8_t>(value->value);
    }
    if(!valid(candidate))return false;output=candidate;return true;
}
[[nodiscard]] constexpr bool join_window(std::uint8_t localMinute) noexcept {
    return localMinute<60 && localMinute%15<3;
}
struct Diagnostics final {
    WarPhase war{};std::size_t wave{};bool joinEligible{},announcementPending{},announcementObserved{};
    std::array<bool,kPatrols.size()> cooling{};
};

// Native source names and definitions prove faction and placement. The cadence
// is reconstructed. Renewal requires the consumed mirror and real actor
// retirement receipts; no death or retirement is inferred from elapsed time.
class Director final {
public:
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,
        std::uint8_t localMinute,Configuration configuration={}) noexcept {
        if(owner_ || !owner || !boot || !valid(configuration))return false;
        owner_=owner;boot_=boot;configuration_=configuration;
        joinEligible_=configuration.war && join_window(localMinute);
        phase_=joinEligible_?WarPhase::announcement:WarPhase::unavailable;
        announcementPending_=joinEligible_;return true;
    }
    // Presentation is advisory. Gameplay does not wait for it because no safe
    // native incident dispatcher was recovered.
    [[nodiscard]] bool take_announcement(std::uint32_t& incident) noexcept {
        incident=0;if(!announcementPending_)return false;
        announcementPending_=false;incident=kAnnouncementIncident;return true;
    }
    [[nodiscard]] bool announcement_observed(std::uint32_t incident) noexcept {
        if(!joinEligible_ || announcementObserved_ || incident!=kAnnouncementIncident)return false;
        announcementObserved_=true;return true;
    }
    [[nodiscard]] bool wave_clear_pending() const noexcept {return waveClearPending_;}
    // The runtime calls this only after the receipt bridge atomically confirms
    // both wave leases have no queued, provisional or unidentified births.
    [[nodiscard]] bool commit_wave_clear() noexcept {
        if(phase_!=WarPhase::active || !waveClearPending_ || !lastNow_)return false;
        phase_=WarPhase::intermission;waveClearAt_=lastNow_;waveClearPending_=false;return true;
    }
    template<class Ledger>
    [[nodiscard]] bool update(std::uint64_t now,std::uint32_t bubble,bool arrived,
        population::Service& service,std::span<const Ledger> ledgers,
        std::span<const std::uint8_t> nativePending) noexcept {
        if(!owner_ || service.owner()!=owner_ || service.boot()!=boot_
            || ledgers.size()<kPopulations.size() || nativePending.size()<kPopulations.size()
            || (lastNow_ && now<lastNow_))return fail();
        lastNow_=now;if(!arrived || bubble!=15)return true;
        if(configuration_.populations && !start_patrols(service,bubble))return fail();
        if(phase_==WarPhase::announcement && wave_ready(service,nativePending,0)
            && !publish_wave(service,ledgers,0,bubble))return fail();
        if(phase_==WarPhase::active) {
            const auto cleared=wave_cleared(service,ledgers,nativePending,wave_);
            if(cleared<0)return fail();
            waveClearPending_=cleared>0;
        } else if(phase_==WarPhase::intermission && now-waveClearAt_>=configuration_.interwaveMilliseconds) {
            if(wave_+1==kWarWaveCount)phase_=WarPhase::complete;
            else if(wave_ready(service,nativePending,wave_+1)
                && !publish_wave(service,ledgers,wave_+1,bubble))return fail();
        }
        if(configuration_.populations && !update_patrols(now,service,ledgers,nativePending,bubble))return fail();
        return phase_!=WarPhase::failed;
    }
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        Diagnostics result{phase_,wave_,joinEligible_,announcementPending_,announcementObserved_};
        for(std::size_t i=0;i<kPatrols.size();++i)result.cooling[i]=patrol_[i].cooling;
        return result;
    }
private:
    struct PatrolState final {
        std::uint64_t clearAt{};std::uint32_t requestSequence{};bool started{},cooling{};
        patrol_replenishment::Credits credits{};
    };
    [[nodiscard]] std::uint8_t patrol_target(const Patrol& patrol,
        std::uint32_t sequence) const noexcept {
        return freeroam::patrol_target(patrol,configuration_,sequence);
    }
    [[nodiscard]] bool request(population::Service& service,std::size_t capability,
        std::uint8_t target,std::uint32_t bubble,bool renewal=false) noexcept {
        if(capability>=kPopulations.size() || service.last_request()==UINT64_MAX)return false;
        const auto& source=kPopulations[capability];
        const population::Command command{owner_,service.revision(),service.last_request()+1,
            source.registry->key,source.slot,target,boot_};
        const auto result=renewal?service.renew(command,bubble):service.request(command,bubble);
        return result==population::Result::accepted;
    }
    [[nodiscard]] bool start_patrols(population::Service& service,std::uint32_t bubble) noexcept {
        auto staged=service;auto states=patrol_;
        for(std::size_t i=0;i<kPatrols.size();++i)if(!states[i].started) {
            const auto& patrol=kPatrols[i];const auto capability=patrol.capability;
            if(staged.target(capability))states[i].started=true;
            else if(!request(staged,capability,patrol_target(patrol,0),bubble))return false;
            else states[i].started=true;
            states[i].requestSequence=1;
        }
        service=std::move(staged);patrol_=states;return true;
    }
    [[nodiscard]] bool replenish(population::Service& service,std::size_t capability,
        std::uint8_t count,std::uint32_t bubble) noexcept {
        if(capability>=kPopulations.size() || service.last_request()==UINT64_MAX)return false;
        const auto& source=kPopulations[capability];
        return service.replenish({owner_,service.revision(),service.last_request()+1,
            source.registry->key,source.slot,count,boot_},bubble)==population::Result::accepted;
    }
    [[nodiscard]] bool war_owns(std::size_t capability) const noexcept {
        if(phase_!=WarPhase::active && phase_!=WarPhase::intermission)return false;
        for(const auto candidate:kWaves[wave_].capabilities)if(candidate==capability)return true;
        return false;
    }
    [[nodiscard]] static constexpr bool fixed_singleton(std::size_t capability) noexcept {
        for(const auto& patrol:kPatrols)if(patrol.capability==capability)
            return patrol.minimum==1 && patrol.baseline==1 && patrol.maximum==1;
        return false;
    }
    [[nodiscard]] bool wave_ready(const population::Service& service,
        std::span<const std::uint8_t> nativePending,std::size_t wave) const noexcept {
        if(wave>=kWaves.size())return false;
        for(const auto capability:kWaves[wave].capabilities)
            if(service.renewal(capability).pending || nativePending[capability])return false;
        return true;
    }
    template<class Ledger>
    [[nodiscard]] int settled(const population::Service& service,const Ledger& ledger,
        std::size_t capability,bool nativePending,std::size_t minimumAdmitted=1) const noexcept {
        const auto counts=ledger.counts();
        if(counts.failed || counts.dead>counts.admitted)return -1;
        if(nativePending || !service.consumed(capability) || counts.admitted<minimumAdmitted
            || counts.alive || counts.resident || counts.dead!=counts.admitted)return 0;
        return 1;
    }
    template<class Ledger>
    [[nodiscard]] bool update_patrols(std::uint64_t now,population::Service& service,
        std::span<const Ledger> ledgers,std::span<const std::uint8_t> nativePending,
        std::uint32_t bubble) noexcept {
        auto staged=service;auto states=patrol_;
        for(std::size_t i=0;i<kPatrols.size();++i) {
            auto& state=states[i];const auto capability=kPatrols[i].capability;
            if(war_owns(capability)){state.cooling=false;continue;}
            if(staged.renewal(capability).pending){state.cooling=false;continue;}
            patrol_replenishment::Counts replacements{};
            if(!patrol_replenishment::ready(state.credits,now,configuration_.respawnMilliseconds,
                staged,capability,ledgers[capability],kPopulations[capability].categories==2,nativePending[capability]!=0,
                {{patrol_target(kPatrols[i],state.requestSequence-1),0}},replacements))return false;
            const auto clear=settled(staged,ledgers[capability],capability,nativePending[capability]!=0);
            if(clear<0)return false;
            if(!clear){
                if(replacements.total()
                    && (!replenish(staged,capability,replacements.lane[0],bubble)
                        || !state.credits.commit(now,replacements)))return false;
                state.cooling=false;continue;
            }
            if(!state.cooling){state.cooling=true;state.clearAt=now;continue;}
            if(now-state.clearAt<configuration_.respawnMilliseconds)continue;
            if(!request(staged,capability,patrol_target(kPatrols[i],state.requestSequence),bubble,true))return false;
            ++state.requestSequence;state.cooling=false;
        }
        service=std::move(staged);patrol_=states;return true;
    }
    template<class Ledger>
    [[nodiscard]] int wave_cleared(const population::Service& service,
        std::span<const Ledger> ledgers,std::span<const std::uint8_t> nativePending,
        std::size_t wave) const noexcept {
        for(std::size_t side=0;side<2;++side) {
            const auto capability=kWaves[wave].capabilities[side];
            const auto state=settled(service,ledgers[capability],capability,
                nativePending[capability]!=0,waveMinimumAdmitted_[side]);
            if(state<=0)return state;
        }
        return 1;
    }
    template<class Ledger>
    [[nodiscard]] bool publish_wave(population::Service& service,std::span<const Ledger> ledgers,
        std::size_t wave,std::uint32_t bubble) noexcept {
        auto staged=service;auto minimumAdmitted=waveMinimumAdmitted_;
        for(std::size_t side=0;side<2;++side) {
            const auto capability=kWaves[wave].capabilities[side];
            const auto counts=ledgers[capability].counts();if(counts.failed)return false;
            const auto current=staged.target(capability);
            const auto singleton=fixed_singleton(capability);
            if(singleton && current>1)return false;
            const auto target=singleton?1U
                :static_cast<unsigned>(current)+configuration_.waveRequests[wave];
            if(target>INT32_MAX)return false;
            // A wave that shares a fixed-one source owns the existing lease
            // instead of multiplying it. Its clear threshold must not require
            // an extra admission that this capped request deliberately omits.
            minimumAdmitted[side]=counts.admitted+(target>current?1U:0U);
            if(!minimumAdmitted[side])minimumAdmitted[side]=1;
            if(target!=current && !(current
                ?replenish(staged,capability,static_cast<std::uint8_t>(target-current),bubble)
                :request(staged,capability,static_cast<std::uint8_t>(target),bubble)))return false;
        }
        service=std::move(staged);waveMinimumAdmitted_=minimumAdmitted;
        wave_=wave;phase_=WarPhase::active;
        waveClearAt_=0;waveClearPending_=false;return true;
    }
    [[nodiscard]] bool fail() noexcept {phase_=WarPhase::failed;return false;}
    population::Owner owner_{};std::uint64_t boot_{},lastNow_{},waveClearAt_{};
    Configuration configuration_{};std::array<PatrolState,kPatrols.size()> patrol_{};
    std::array<std::size_t,2> waveMinimumAdmitted_{};
    std::size_t wave_{};WarPhase phase_{WarPhase::unavailable};
    bool joinEligible_{},announcementPending_{},announcementObserved_{},waveClearPending_{};
};
} // namespace sunrise::server::runtime::activity::mercury::freeroam
