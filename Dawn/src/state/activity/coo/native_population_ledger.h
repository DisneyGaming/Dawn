#pragma once
#include "executor.h"
#include <algorithm>
#include <limits>
#include <type_traits>

namespace dawn::state::activity::coo {

struct PopulationOwner final {
    std::uint64_t activity{}, run{}, incarnation{};
    Asset source{};
    std::uint32_t generation{};
    [[nodiscard]] bool valid() const noexcept {
        return activity && run && incarnation && generation && generation<=0x7FFFFFFFU
            && source.registry && source.registry!=UINT32_MAX && source.registry!=0x811C9DC5U
            && source.definition && source.definition!=UINT32_MAX && source.definition!=0x811C9DC5U;
    }
    friend bool operator==(const PopulationOwner&,const PopulationOwner&)=default;
};
struct PopulationActor final {
    PopulationOwner owner{};
    // Salted native handles. The native adapter must qualify them before intake.
    std::uint32_t actor{UINT32_MAX}, entity{UINT32_MAX};
    // Monotonic mailbox creation receipt for this native birth. Zero retains
    // the legacy conservative identity: its handles may never be reused in one
    // generation.
    std::uint64_t birthNonce{};
    [[nodiscard]] bool valid() const noexcept { return owner.valid() && actor!=UINT32_MAX && entity!=UINT32_MAX; }
    friend bool operator==(const PopulationActor&,const PopulationActor&)=default;
};
enum class PopulationIntake : std::uint8_t { accepted, duplicate, unrelated, unknown, conflict, overflow, closed };
enum class PopulationPhase : std::uint8_t { idle, active, retiring, retired };
struct PopulationCounts final {
    std::size_t admitted{}, alive{}, dead{}, resident{};
    bool sourceRetired{}, failed{};
};

// Value-only accounting owned by the activity server. No AI, movement, spawn
// timer, packet encoding, or client pointer belongs here. Native source requests
// can produce authored groups, so this ledger does NOT assume one request is one
// actor. The capability's verified policy decides when to request replacements.
template<std::size_t Capacity>
class NativePopulationLedger final {
    static_assert(Capacity>0 && Capacity<=256);
    static constexpr std::size_t kMaximumCategories=8;
public:
    [[nodiscard]] bool begin(PopulationOwner owner) noexcept {
        if(!owner.valid() || failed_ || (phase_!=PopulationPhase::idle && phase_!=PopulationPhase::retired)) { return false; }
        if(phase_!=PopulationPhase::idle) {
            const bool sameLifetime=owner.activity==owner_.activity && owner.run==owner_.run
                && owner.incarnation==owner_.incarnation && owner.source==owner_.source;
            if(sameLifetime ? owner.generation<=owner_.generation : owner.incarnation<=owner_.incarnation)return false;
        }
        owner_=owner;actors_={};used_=0;tombstones_={};tombstoneUsed_=0;
        admitted_=dead_=0;admittedByCategory_={};deadByCategory_={};
        retiredNonceFloor_=0;legacyHistoryLost_=false;
        sourceRetired_=false;phase_=PopulationPhase::active;return true;
    }
    [[nodiscard]] PopulationIntake admitted(PopulationActor actor,std::uint8_t category=UINT8_MAX) noexcept {
        if(category>=kMaximumCategories && category!=UINT8_MAX)return PopulationIntake::conflict;
        if(!matches(actor)) { return PopulationIntake::unrelated; }
        for(std::size_t i=0;i<used_;++i) {
            if(actors_[i].identity==actor) {
                if(actors_[i].category!=category) {failed_=true;return PopulationIntake::conflict;}
                return PopulationIntake::duplicate;
            }
            if(actor.birthNonce && actors_[i].identity.birthNonce==actor.birthNonce) {
                failed_=true;return PopulationIntake::conflict;
            }
            if((actors_[i].identity.actor==actor.actor || actors_[i].identity.entity==actor.entity)
                && !reusable(actors_[i],actor)) {
                failed_=true;return PopulationIntake::conflict;
            }
        }
        for(std::size_t i=0;i<tombstoneUsed_;++i) {
            if(tombstones_[i].identity==actor) {
                if(tombstones_[i].category!=category) {failed_=true;return PopulationIntake::conflict;}
                return PopulationIntake::duplicate;
            }
            if(actor.birthNonce && tombstones_[i].identity.birthNonce==actor.birthNonce) {
                failed_=true;return PopulationIntake::conflict;
            }
            if((tombstones_[i].identity.actor==actor.actor || tombstones_[i].identity.entity==actor.entity)
                && !reusable(tombstones_[i],actor)) {
                failed_=true;return PopulationIntake::conflict;
            }
        }
        // An evicted completed birth can no longer be compared structurally.
        // Its monotonic creation receipt (or the loss of a legacy zero receipt)
        // keeps a stale replay fail-closed.
        if((actor.birthNonce && actor.birthNonce<=retiredNonceFloor_)
            || (!actor.birthNonce && legacyHistoryLost_)) {failed_=true;return PopulationIntake::conflict;}
        if(sourceRetired_ || phase_==PopulationPhase::retired) {
            // New births after the native source's quiescence barrier disprove
            // its contract; fail visibly rather than silently lose ownership.
            failed_=true;return PopulationIntake::closed;
        }
        if(used_==Capacity)compact();
        // Compaction can evict a newer completed receipt and advance the replay
        // floor. Revalidate an out-of-order admission against that new floor.
        if(stale(actor)) {failed_=true;return PopulationIntake::conflict;}
        if(used_==Capacity || admitted_==(std::numeric_limits<std::size_t>::max)()) {
            failed_=true;return PopulationIntake::overflow;
        }
        actors_[used_].identity=actor;actors_[used_++].category=category;++admitted_;
        if(category<kMaximumCategories)++admittedByCategory_[category];return PopulationIntake::accepted;
    }
    [[nodiscard]] PopulationIntake died(PopulationActor actor) noexcept {
        if(!matches(actor)) { return PopulationIntake::unrelated; }
        auto* item=find(actor);if(!item) {
            if(tombstone(actor))return PopulationIntake::duplicate;
            if(nonce_conflict(actor) || stale(actor)){failed_=true;return PopulationIntake::conflict;}
            return PopulationIntake::unknown;
        }
        if(item->dead) { return PopulationIntake::duplicate; }
        if(item->retired) { return PopulationIntake::closed; }
        if(dead_==(std::numeric_limits<std::size_t>::max)()) {failed_=true;return PopulationIntake::overflow;}
        item->dead=true;++dead_;if(item->category<kMaximumCategories)++deadByCategory_[item->category];return PopulationIntake::accepted;
    }
    [[nodiscard]] PopulationIntake actor_retired(PopulationActor actor) noexcept {
        if(!matches(actor)) { return PopulationIntake::unrelated; }
        auto* item=find(actor);if(!item) {
            if(tombstone(actor))return PopulationIntake::duplicate;
            if(nonce_conflict(actor) || stale(actor)){failed_=true;return PopulationIntake::conflict;}
            return PopulationIntake::unknown;
        }
        if(item->retired) { return PopulationIntake::duplicate; }
        item->retired=true;settle_retirement();return PopulationIntake::accepted;
    }
    // The adapter witnessed destruction of the former source and authenticated
    // a replacement in the same generation. All old actors must have retired.
    // Keep real deaths; forget only survivors removed by streaming, not kills.
    [[nodiscard]] bool source_recreated(PopulationOwner owner,
        std::array<std::uint8_t,kMaximumCategories>& streamedSurvivors) noexcept {
        if(owner!=owner_ || phase_!=PopulationPhase::active || sourceRetired_ || failed_)return false;
        for(std::size_t i=0;i<used_;++i)if(!actors_[i].retired)return false;
        streamedSurvivors={};
        for(std::size_t i=0;i<used_;++i)if(!actors_[i].dead) {
            if(actors_[i].category>=kMaximumCategories
                || streamedSurvivors[actors_[i].category]==UINT8_MAX)return false;
            ++streamedSurvivors[actors_[i].category];
        }
        std::size_t kept{};
        for(std::size_t i=0;i<used_;++i)if(actors_[i].dead)actors_[kept++]=actors_[i];else {
            --admitted_;if(actors_[i].category<kMaximumCategories)--admittedByCategory_[actors_[i].category];
        }
        for(std::size_t i=kept;i<used_;++i)actors_[i]={};
        used_=kept;return true;
    }
    [[nodiscard]] bool source_recreated(PopulationOwner owner) noexcept {
        if(owner!=owner_ || phase_!=PopulationPhase::active || sourceRetired_ || failed_)return false;
        for(std::size_t i=0;i<used_;++i)if(!actors_[i].retired)return false;
        // Legacy/non-authorizing callers may not have exact member-category
        // evidence. Forget streamed survivors while retaining real deaths, but
        // never translate unknown categories into replacement quota.
        std::size_t kept{};
        for(std::size_t i=0;i<used_;++i)if(actors_[i].dead)actors_[kept++]=actors_[i];else {
            --admitted_;if(actors_[i].category<kMaximumCategories)--admittedByCategory_[actors_[i].category];
        }
        for(std::size_t i=kept;i<used_;++i)actors_[i]={};
        used_=kept;return true;
    }
    // Recurring sources keep the same native object and advance only their
    // generation. The bridge calls this after it has atomically excluded old
    // queued and provisional births. This is a lease rollover, not a native
    // source-stop receipt, so sourceRetired is deliberately not asserted.
    [[nodiscard]] bool renew(PopulationOwner prior,PopulationOwner next) noexcept {
        if(prior!=owner_ || phase_!=PopulationPhase::active || sourceRetired_ || failed_ || !next.valid()
            || next.activity!=prior.activity || next.run!=prior.run || next.incarnation!=prior.incarnation
            || next.source!=prior.source || prior.generation==0x7FFFFFFFU
            || next.generation!=prior.generation+1 || !admitted_) return false;
        for(std::size_t i=0;i<used_;++i) if(!actors_[i].dead || !actors_[i].retired) return false;
        owner_=next;actors_={};used_=0;tombstones_={};tombstoneUsed_=0;
        admitted_=dead_=0;admittedByCategory_={};deadByCategory_={};
        retiredNonceFloor_=0;legacyHistoryLost_=false;
        sourceRetired_=false;phase_=PopulationPhase::active;return true;
    }
    // Call only after the server/native service has accepted a retirement request.
    // In-flight births still belong to this source and must be accounted for.
    [[nodiscard]] bool retiring(PopulationOwner owner) noexcept {
        if(owner!=owner_ || phase_!=PopulationPhase::active) { return false; }
        phase_=PopulationPhase::retiring;return true;
    }
    // Qualified native barrier: source stopped, births quiesced, preceding actor
    // observations drained. The adapter must prove this; elapsed time cannot.
    [[nodiscard]] PopulationIntake source_retired(PopulationOwner owner) noexcept {
        if(owner!=owner_ || phase_==PopulationPhase::idle) { return PopulationIntake::unrelated; }
        if(sourceRetired_) { return PopulationIntake::duplicate; }
        if(phase_!=PopulationPhase::retiring) { return PopulationIntake::unrelated; }
        sourceRetired_=true;settle_retirement();return PopulationIntake::accepted;
    }
    [[nodiscard]] PopulationCounts counts() const noexcept {
        PopulationCounts result{admitted_,0,dead_,0,sourceRetired_,failed_};
        for(std::size_t i=0;i<used_;++i) {
            result.resident+=actors_[i].retired?0U:1U;
            result.alive+=!actors_[i].dead && !actors_[i].retired?1U:0U;
        }
        return result;
    }
    // Unknown categories are not redistributed between lanes. A multi-category
    // refill must account for every actor with exact admitted-template evidence.
    [[nodiscard]] PopulationCounts counts(std::uint8_t category) const noexcept {
        if(category>=kMaximumCategories)return PopulationCounts{0,0,0,0,sourceRetired_,true};
        PopulationCounts result{admittedByCategory_[category],0,deadByCategory_[category],0,
            sourceRetired_,failed_};
        for(std::size_t i=0;i<used_;++i)if(actors_[i].category==category) {
            result.resident+=actors_[i].retired?0U:1U;
            result.alive+=!actors_[i].dead && !actors_[i].retired?1U:0U;
        }
        return result;
    }
    [[nodiscard]] PopulationPhase phase() const noexcept { return phase_; }
    [[nodiscard]] PopulationOwner owner() const noexcept { return owner_; }
    [[nodiscard]] bool sole_live(PopulationActor& output) const noexcept {
        output={};bool found{};
        for(std::size_t i=0;i<used_;++i)if(!actors_[i].dead && !actors_[i].retired) {
            if(found)return false;output=actors_[i].identity;found=true;
        }
        return found;
    }
    [[nodiscard]] bool live_actor(std::uint32_t actor) const noexcept {
        if(actor==UINT32_MAX)return false;
        for(std::size_t i=0;i<used_;++i)if(actors_[i].identity.actor==actor
            && !actors_[i].dead && !actors_[i].retired)return true;
        return false;
    }
    [[nodiscard]] bool live_entity(std::uint32_t entity) const noexcept {
        if(entity==UINT32_MAX)return false;
        for(std::size_t i=0;i<used_;++i)if(actors_[i].identity.entity==entity
            && !actors_[i].dead && !actors_[i].retired)return true;
        return false;
    }
    [[nodiscard]] bool current_entity(std::uint32_t entity) const noexcept {
        if(entity==UINT32_MAX)return false;
        for(std::size_t i=0;i<used_;++i)if(actors_[i].identity.entity==entity && !actors_[i].retired)return true;
        return false;
    }
private:
    struct Actor final { PopulationActor identity{}; bool dead{},retired{};std::uint8_t category{UINT8_MAX}; };
    [[nodiscard]] static bool reusable(const Actor& previous,const PopulationActor& next) noexcept {
        return previous.dead && previous.retired && previous.identity.birthNonce
            && next.birthNonce && previous.identity.birthNonce!=next.birthNonce;
    }
    [[nodiscard]] bool matches(const PopulationActor& actor) const noexcept {
        return phase_!=PopulationPhase::idle && actor.valid() && actor.owner==owner_;
    }
    [[nodiscard]] Actor* find(const PopulationActor& actor) noexcept {
        for(std::size_t i=0;i<used_;++i) { if(actors_[i].identity==actor) { return &actors_[i]; } }return nullptr;
    }
    [[nodiscard]] bool tombstone(const PopulationActor& actor) const noexcept {
        for(std::size_t i=0;i<tombstoneUsed_;++i)if(tombstones_[i].identity==actor)return true;
        return false;
    }
    [[nodiscard]] bool stale(const PopulationActor& actor) const noexcept {
        return (actor.birthNonce && actor.birthNonce<=retiredNonceFloor_)
            || (!actor.birthNonce && legacyHistoryLost_);
    }
    [[nodiscard]] bool nonce_conflict(const PopulationActor& actor) const noexcept {
        if(!actor.birthNonce)return false;
        for(std::size_t i=0;i<used_;++i)
            if(actors_[i].identity.birthNonce==actor.birthNonce)return true;
        for(std::size_t i=0;i<tombstoneUsed_;++i)
            if(tombstones_[i].identity.birthNonce==actor.birthNonce)return true;
        return false;
    }
    void remember(Actor actor) noexcept {
        if(tombstoneUsed_==Capacity) {
            const auto evicted=tombstones_[0].identity.birthNonce;
            if(evicted)retiredNonceFloor_=(std::max)(retiredNonceFloor_,evicted);
            else legacyHistoryLost_=true;
            for(std::size_t i=1;i<tombstoneUsed_;++i)tombstones_[i-1]=tombstones_[i];
            --tombstoneUsed_;
        }
        tombstones_[tombstoneUsed_++]=actor;
    }
    void compact() noexcept {
        std::size_t kept{};
        for(std::size_t i=0;i<used_;++i) {
            if(actors_[i].dead && actors_[i].retired)remember(actors_[i]);
            else actors_[kept++]=actors_[i];
        }
        for(std::size_t i=kept;i<used_;++i)actors_[i]={};
        used_=kept;
    }
    void settle_retirement() noexcept {
        if(failed_ || phase_!=PopulationPhase::retiring || !sourceRetired_) { return; }
        for(std::size_t i=0;i<used_;++i) { if(!actors_[i].retired) { return; } }
        phase_=PopulationPhase::retired;
    }
    PopulationOwner owner_{};
    std::array<Actor,Capacity> actors_{};
    std::array<Actor,Capacity> tombstones_{};
    std::size_t used_{};
    std::size_t tombstoneUsed_{},admitted_{},dead_{};
    std::array<std::size_t,kMaximumCategories> admittedByCategory_{},deadByCategory_{};
    std::uint64_t retiredNonceFloor_{};
    PopulationPhase phase_{PopulationPhase::idle};
    bool sourceRetired_{},failed_{},legacyHistoryLost_{};
};
static_assert(std::is_trivially_copyable_v<NativePopulationLedger<64>>);
} // namespace dawn::state::activity::coo
