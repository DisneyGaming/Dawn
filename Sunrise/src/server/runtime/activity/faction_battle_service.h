#pragma once
#include "population_service.h"
#include "../../../state/activity/native_population_events.h"

namespace sunrise::server::runtime::activity::faction_battle {
namespace coo=state::activity::coo;
namespace events=state::activity::native_population;

// Qualification is trusted profile metadata, never a development command or a
// JSON switch. No installed Mercury profile currently satisfies these gates.
enum Evidence : std::uint16_t {
    identity=1, cohorts=2, nativeHostility=4, waveConditions=8,
    presentation=16, eligibility=32, termination=64
};
inline constexpr std::uint16_t kRequiredEvidence=127;
struct Cohort final {
    std::uint8_t capability{}, requests{};
    // Actor cardinality must be recovered separately from source request count.
    std::uint16_t actors{};
};
struct Wave final { std::array<Cohort,2> sides{}; };
struct Definition final {
    std::uint32_t identity{}, startIncidentHash{};
    std::uint16_t evidence{}, residentBudget{};
    std::span<const population::Capability> populations;
    std::span<const Wave> waves;
};
enum class Phase : std::uint8_t { idle, announcement, active, resolved, retiring, retired, failed };
enum class Block : std::uint8_t {
    none, qualification, stale, announcement, births, deaths, publication,
    observations, residentBudget, nativeRetirement, sourceRenewal
};
struct Token final {
    population::Owner owner{};
    std::uint64_t boot{}, run{}, revision{};
    friend bool operator==(const Token&,const Token&)=default;
};
struct Diagnostics final {
    Phase phase{};Block blocked{};std::size_t wave{};
    std::array<coo::PopulationCounts,2> sides{};
    bool announcementRequested{},announcementObserved{};
};

// An owner-thread encounter service using the existing native population
// authority path. The first supported progression contract is BOTH authored
// cohorts fully admitted and dead. Other retail conditions require a recovered
// adapter; do not select this contract for Mercury on the basis of its name.
// Definitions and capabilities must remain immutable/alive for the owner lease.
// No host timer, AI targeting, health edit, reward or source reset lives here.
class Service final {
public:
    static constexpr std::size_t kMaxWaves=8,kMaxActorsPerSource=64;
    [[nodiscard]] static bool valid(const Definition& definition) noexcept {
        if(!definition.identity || !definition.startIncidentHash
            || definition.evidence!=kRequiredEvidence || definition.waves.empty()
            || definition.waves.size()>kMaxWaves || definition.populations.empty()
            || definition.populations.size()>kMaxWaves*2 || !definition.residentBudget
            || definition.residentBudget>kMaxWaves*2*kMaxActorsPerSource) return false;
        std::array<bool,kMaxWaves*2> used{};
        for(const auto& wave:definition.waves) for(const auto& side:wave.sides) {
            if(side.capability>=definition.populations.size() || used[side.capability]
                || !side.requests || side.requests>63 || !side.actors || side.actors>kMaxActorsPerSource
                || !population::valid(definition.populations[side.capability])) return false;
            // This implementation intentionally cannot renew or reuse a source.
            used[side.capability]=true;
        }
        const auto* first=definition.populations[0].registry;
        for(const auto& cap:definition.populations) {
            if(!population::valid(cap) || cap.registry->scenario!=first->scenario
                || cap.registry->activity!=first->activity || cap.registry->bubble!=first->bubble) return false;
        }
        return true;
    }
    [[nodiscard]] bool begin(Token token,const Definition& definition) noexcept {
        if(phase_!=Phase::idle || !token.owner || !token.boot || !token.run || !token.revision
            || !valid(definition)) {blocked_=Block::qualification;return false;}
        if(!population_.begin(token.owner,definition.populations,token.boot)) return false;
        token_=token;definition_=&definition;phase_=Phase::announcement;blocked_=Block::announcement;return true;
    }
    // The server stores the operation before a middleware adapter may queue the
    // native incident. This is at most once; publication is not a HUD receipt.
    [[nodiscard]] bool request_announcement(Token token) noexcept {
        if(token!=token_ || phase_!=Phase::announcement || announcementRequested_) return false;
        announcementRequested_=true;return true;
    }
    [[nodiscard]] bool announcement_observed(Token token,std::uint32_t incidentHash) noexcept {
        if(token!=token_ || !definition_ || incidentHash!=definition_->startIncidentHash
            || phase_!=Phase::announcement || !announcementRequested_ || announcementObserved_) return false;
        announcementObserved_=true;return true;
    }
    // Caller already qualified scheduler/budget eligibility for this token at
    // the owner update boundary. Both cohort requests commit atomically.
    [[nodiscard]] bool update(Token token,std::uint32_t bubble,bool eligible) noexcept {
        if(token!=token_ || !definition_ || bubble!=definition_->populations[0].registry->bubble) return false;
        if(phase_==Phase::announcement) {
            if(!announcementObserved_ || !eligible) return false;
            return publish_wave(0,bubble);
        }
        if(phase_!=Phase::active || !eligible) return false;
        const auto& wave=definition_->waves[wave_];
        for(const auto& side:wave.sides) {
            const auto counts=ledgers_[side.capability].counts();
            if(counts.failed || counts.admitted>side.actors) {fail(Block::observations);return false;}
            if(counts.admitted!=side.actors) {blocked_=Block::births;return false;}
            if(counts.dead!=side.actors) {blocked_=Block::deaths;return false;}
        }
        if(wave_+1==definition_->waves.size()) {phase_=Phase::resolved;blocked_=Block::nativeRetirement;return true;}
        return publish_wave(wave_+1,bubble);
    }
    [[nodiscard]] events::Lease lease(std::size_t capability) const noexcept {
        if(!definition_ || capability>=definition_->populations.size() || !published_[capability]) return {};
        return {token_.owner,ledgers_[capability].owner(),definition_->populations[capability].registry->bubble};
    }
    // Use only the qualified native mailbox route, including deaths caused by
    // another combatant. Killer identity deliberately does not filter this path.
    [[nodiscard]] coo::PopulationIntake observe(Token token,const events::Event& event) noexcept {
        if(token!=token_ || !definition_ || phase_==Phase::idle || event.sourceHandle==UINT32_MAX)
            return coo::PopulationIntake::unrelated;
        for(std::size_t i=0;i<definition_->populations.size();++i) {
            if(!published_[i] || event.lease!=lease(i) || event.actor.owner!=event.lease.source) continue;
            if(bound_[i] && handles_[i]!=event.sourceHandle) {fail(Block::observations);return coo::PopulationIntake::conflict;}
            auto result=coo::PopulationIntake::unrelated;
            if(event.kind==events::Kind::admitted) result=ledgers_[i].admitted(event.actor);
            else if(event.kind==events::Kind::died) result=ledgers_[i].died(event.actor);
            else if(event.kind==events::Kind::retired) result=ledgers_[i].actor_retired(event.actor);
            if(result==coo::PopulationIntake::accepted || result==coo::PopulationIntake::duplicate) {
                handles_[i]=event.sourceHandle;bound_[i]=true;settle_retirement();
                for(const auto& wave:definition_->waves) for(const auto& side:wave.sides)
                    if(side.capability==i && ledgers_[i].counts().admitted>side.actors) fail(Block::observations);
            } else fail(Block::observations);
            return result;
        }
        return coo::PopulationIntake::unrelated;
    }
    // Called AFTER the actual native service accepts its retirement operation.
    // This does not invoke that operation and no adapter is supplied for Mercury.
    [[nodiscard]] bool retirement_requested(Token token) noexcept {
        if(token!=token_ || !definition_ || phase_==Phase::retiring || phase_==Phase::retired || phase_==Phase::idle) return false;
        for(std::size_t i=0;i<definition_->populations.size();++i)
            if(published_[i] && !ledgers_[i].retiring(ledgers_[i].owner())) return false;
        phase_=Phase::retiring;blocked_=Block::nativeRetirement;settle_retirement();return true;
    }
    [[nodiscard]] bool source_retired(Token token,const events::Lease& source) noexcept {
        if(token!=token_ || !definition_ || phase_!=Phase::retiring) return false;
        for(std::size_t i=0;i<definition_->populations.size();++i) if(published_[i] && source==lease(i)) {
            const auto result=ledgers_[i].source_retired(source.source);settle_retirement();
            return result==coo::PopulationIntake::accepted;
        }
        return false;
    }
    void observation_lost(Token token) noexcept {if(token==token_) fail(Block::observations);}
    // Retained cumulative authority is never lowered to pretend to retire actors.
    [[nodiscard]] population::wire::Batch project(std::uint32_t bubble) const noexcept {return population_.project(bubble);}
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        Diagnostics result{phase_,blocked_,wave_,{},announcementRequested_,announcementObserved_};
        if(definition_) for(std::size_t s=0;s<2;++s) result.sides[s]=ledgers_[definition_->waves[wave_].sides[s].capability].counts();
        return result;
    }
private:
    [[nodiscard]] bool publish_wave(std::size_t index,std::uint32_t bubble) noexcept {
        std::size_t resident{};
        for(std::size_t i=0;i<definition_->populations.size();++i) resident+=ledgers_[i].counts().resident;
        for(const auto& side:definition_->waves[index].sides) resident+=side.actors;
        if(resident>definition_->residentBudget) {blocked_=Block::residentBudget;return false;}
        auto staged=population_;
        std::array<coo::NativePopulationLedger<kMaxActorsPerSource>,2> births{};
        for(std::size_t s=0;s<2;++s) {
            const auto& side=definition_->waves[index].sides[s];const auto& cap=definition_->populations[side.capability];
            coo::Asset asset{cap.registry->key,0,1,cap.slot};
            for(const auto& slot:cap.registry->slots) if(slot.index==cap.slot) asset.definition=slot.descriptorTag;
            const coo::PopulationOwner owner{token_.owner.sessionId,token_.boot,token_.owner.incarnation.value,
                asset,static_cast<std::uint32_t>(token_.owner.incarnation.value)};
            if(!births[s].begin(owner) || staged.last_request()==UINT64_MAX
                || staged.request({token_.owner,staged.revision(),staged.last_request()+1,cap.registry->key,
                    cap.slot,side.requests,token_.boot},bubble)!=population::Result::accepted) {fail(Block::publication);return false;}
        }
        // The native mailbox adapter must bind lease() for BOTH sources before
        // handing project() to middleware. This method never calls native code.
        population_=staged;
        for(std::size_t s=0;s<2;++s) {const auto i=definition_->waves[index].sides[s].capability;ledgers_[i]=births[s];published_[i]=true;}
        wave_=index;phase_=Phase::active;blocked_=Block::births;return true;
    }
    void fail(Block reason) noexcept {failed_=true;phase_=Phase::failed;blocked_=reason;}
    void settle_retirement() noexcept {
        if(phase_!=Phase::retiring || failed_) return;
        for(std::size_t i=0;i<definition_->populations.size();++i)
            if(published_[i] && ledgers_[i].phase()!=coo::PopulationPhase::retired) return;
        phase_=Phase::retired;blocked_=Block::sourceRenewal;
        // Deliberately no transition back to idle. Native generation renewal is
        // not proven; re-entry belongs to a new authoritative activity owner.
    }
    Token token_{};const Definition* definition_{};population::Service population_{};
    std::array<coo::NativePopulationLedger<kMaxActorsPerSource>,kMaxWaves*2> ledgers_{};
    std::array<std::uint32_t,kMaxWaves*2> handles_{};
    std::array<bool,kMaxWaves*2> bound_{},published_{};
    std::size_t wave_{};Phase phase_{};Block blocked_{};
    bool announcementRequested_{},announcementObserved_{},failed_{};
};
} // namespace sunrise::server::runtime::activity::faction_battle
