#pragma once
#include "omega_mission_catalog.h"
#include <algorithm>

namespace sunrise::state::activity::omega::mission {
inline constexpr std::uint32_t kInvalid = UINT32_MAX;
struct Boss {
    std::uint64_t run{};
    std::uint32_t generation{}, actor{kInvalid}, character{kInvalid}, biped{kInvalid},
        entity{kInvalid}, member{kInvalid}, revision{};
    bool operator==(const Boss&) const = default;
};
struct Token { Boss boss{}; std::uint32_t epoch{}; bool operator==(const Token&) const = default; };
enum class Action : std::uint8_t { none, left, right, startCycle, depart, deletion, shield, recover, finalDeath, ending };
enum class Phase : std::uint8_t { dormant, summon, clearance, departure, arrival, deletion, rescue, route, carrying, shield, eye, recovery, death, ending, finished };
enum class Animation : std::uint8_t { started, finished, deletionHold, eyeExposed, departureFinished };
enum class Health : std::uint8_t { eyeCrossed, bodyCheckpoint, dead };
struct Command { Token token{}; Action action{}; std::uint8_t wave{}, island{}, cycle{}; bool claimed{}; };
struct ActorReceipt {
    std::uint64_t run{};
    std::uint32_t generation{}, registry{}, source{kInvalid}, actor{kInvalid}, entity{kInvalid}, character{kInvalid}, health{kInvalid};
    std::uint16_t slot{};
    std::uint8_t category{};
    bool operator==(const ActorReceipt&) const = default;
};
struct ChargeReceipt {
    Token token{};
    std::uint32_t source{kInvalid}, generation{}, item{kInvalid}, player{kInvalid}, sink{kInvalid};
    std::uint32_t registry{};
    std::uint16_t sourceSlot{}, sinkSlot{};
    bool operator==(const ChargeReceipt&) const = default;
};
struct SceneCommand {
    Token token{};
    std::uint32_t generation{};
    std::uint16_t slot{};
    bool stop{};
    std::uint8_t eventCount{};
    std::array<std::uint32_t,4> events{};
};
struct Snapshot {
    Command command{};
    Phase phase{};
    std::array<std::array<std::uint8_t,2>,kSources.size()> requested{};
    std::array<SceneCommand,9> scenes{};
    std::uint32_t revision{}, generation{};
    std::uint8_t cannonPrepared{}, transitPrepared{}, cannons{}, rescueStartedMask{}, rescueReadyMask{};
    bool restriction{}, chargeEnabled{}, chargeDunked{}, chargePlatform{}, eyePlatform{}, finalArrival{}, chargePickedUp{};
    bool nativeAnimationFinished{}, nativeBodyCheckpoint{}, nativeBossDead{}, endingStarted{};
    bool dpsFrontCreated{}, dpsBackCreated{}, nativeReturnLauncher{};
};

// Documented reconstruction. Every transition consumes a qualified native
// receipt; clocks, missing objects, and transmitted requests are not receipts.
class State final {
public:
    bool bind(const Boss& boss) noexcept {
        if(!valid(boss)) return false;
        if(s_.phase!=Phase::dormant && s_.command.token.boss.run==boss.run)
            return s_.command.token.boss==boss;
        if(s_.command.token.boss.run && boss.run<=s_.command.token.boss.run) return false;
        *this=State{};
        s_.command.token.boss=boss; s_.generation=boss.generation;
        summon(0,0,0,Action::left);
        return true;
    }
    const Snapshot& snapshot() const noexcept { return s_; }
    bool current(const Token& token) const noexcept { return token==s_.command.token && token.epoch!=0; }
    bool claim(const Token& token,Action action) noexcept {
        if(!current(token) || action!=s_.command.action || action==Action::none || s_.command.claimed) return false;
        if(action==Action::shield && !s_.eyePlatform) return false;
        s_.command.claimed=true; return true;
    }
    bool animation(const Token& token,Animation event) noexcept {
        if(!current(token) || !s_.command.claimed) return false;
        if(s_.phase==Phase::summon) {
            if(event==Animation::started && !waveStarted_) {
                waveStarted_=true;
                for(std::size_t i=0;i<kSources.size();++i)
                    if(kSources[i].wave==s_.command.wave) s_.requested[i]=kSources[i].requested;
                changed(); return true;
            }
            if(event!=Animation::finished || !waveStarted_ || s_.nativeAnimationFinished) return false;
            s_.nativeAnimationFinished=true;
            // Initial batches may overlap. The escape cohort never gates on deaths.
            if(s_.command.wave==0) summon(1,0,0,Action::right);
            else if(s_.command.wave==11) schedule(Action::depart,Phase::departure);
            else { s_.phase=Phase::clearance; changed(); advance_clearance(); }
            return true;
        }
        if(s_.phase==Phase::departure && event==Animation::departureFinished) {
            departed_=true;
            if(s_.command.island==0 && (s_.cannonPrepared&7)==7) s_.cannons|=7;
            if(s_.command.island==3 && (s_.cannonPrepared&8)) s_.cannons|=8;
            s_.phase=Phase::arrival; changed(); advance_arrival(); return true;
        }
        if(s_.phase==Phase::deletion) {
            if(event==Animation::started && !deletionStarted_) {
                deletionStarted_=true; open_scene(rescue_slot()); changed(); return true;
            }
            if(event==Animation::deletionHold && deletionStarted_ && !deletionHeld_) {
                deletionHeld_=true; s_.phase=Phase::rescue; changed(); advance_rescue(); return true;
            }
        }
        if(s_.phase==Phase::shield && event==Animation::eyeExposed) {
            if(s_.command.cycle==1) event_scene(9,0xECF4AD0CU);
            s_.phase=Phase::eye; changed();
            if(eyeCrossed_) health(token,Health::eyeCrossed);
            return true;
        }
        if(s_.phase==Phase::recovery && event==Animation::finished && !s_.nativeAnimationFinished) {
            s_.nativeAnimationFinished=true; changed(); advance_recovery(); return true;
        }
        if(s_.phase==Phase::death && event==Animation::finished && !s_.nativeAnimationFinished) {
            s_.nativeAnimationFinished=true; changed(); advance_death(); return true;
        }
        return false;
    }
    bool admit(const ActorReceipt& receipt) noexcept {
        const auto index=source_index(receipt.registry,receipt.slot);
        if(index==kSources.size() || !valid_actor(receipt) || receipt.category>=kSources[index].categories
            || admitted_[index][receipt.category]>=s_.requested[index][receipt.category]) return false;
        for(const auto& entry:actors_) if(entry.used && entry.receipt.actor==receipt.actor) return false;
        for(auto& entry:actors_) if(!entry.used) {
            entry={receipt,true,false}; ++admitted_[index][receipt.category]; changed(); return true;
        }
        return false;
    }
    bool death(const ActorReceipt& receipt) noexcept {
        if(!valid_actor(receipt)) return false;
        for(auto& entry:actors_) if(entry.used && entry.receipt==receipt) {
            if(entry.dead) return false;
            entry.dead=true; ++dead_[source_index(receipt.registry,receipt.slot)][receipt.category];
            changed(); advance_clearance(); return true;
        }
        return false;
    }
    bool arrival(const Token& token,std::uint8_t island,std::uint32_t player) noexcept {
        if(!current(token) || !handle(player) || !s_.command.claimed
            || (s_.phase!=Phase::departure && s_.phase!=Phase::arrival)) return false;
        const auto expected=s_.command.island<4 ? s_.command.island+1 : 5;
        if(island!=expected || arrived_) return false;
        arrived_=true; player_=player;
        if(island==5) s_.finalArrival=true;
        changed(); advance_arrival(); return true;
    }
    bool prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index,bool transit) noexcept {
        if(run!=s_.command.token.boss.run || generation!=s_.generation || index>=(transit?7:4)) return false;
        auto& mask=transit?s_.transitPrepared:s_.cannonPrepared;
        const auto bit=static_cast<std::uint8_t>(1U<<index);
        if(mask&bit) return false;
        mask|=bit;
        if(!transit && departed_) {
            if((mask&7)==7) s_.cannons|=7;
            if(s_.command.island>=3 && (mask&8)) s_.cannons|=8;
        }
        changed(); advance_arrival(); advance_rescue(); return true;
    }
    bool rescue_started(const Token& token,std::uint16_t slot) noexcept {
        const auto* scene=find_scene(slot);
        if(!scene || scene->stop || scene->token!=token || slot!=rescue_slot()
            || (s_.phase!=Phase::deletion && s_.phase!=Phase::rescue)) return false;
        const auto bit=static_cast<std::uint8_t>(1U<<(s_.command.cycle-1));
        if(s_.rescueStartedMask&bit) return false;
        s_.rescueStartedMask|=bit; changed(); return true;
    }
    bool rescue_ready(const Token& token,std::uint16_t slot) noexcept {
        const auto* scene=find_scene(slot);
        if(!scene || scene->stop || scene->token!=token || slot!=rescue_slot() || rescueReady_
            || (s_.phase!=Phase::deletion && s_.phase!=Phase::rescue)) return false;
        rescueReady_=true; s_.rescueReadyMask|=static_cast<std::uint8_t>(1U<<(s_.command.cycle-1)); changed(); advance_rescue(); return true;
    }
    bool route_arrival(const Token& token,bool eye,std::uint32_t player) noexcept {
        if(!current(token) || !handle(player)) return false;
        if(eye) {
            if(s_.phase!=Phase::shield || !s_.chargeDunked || s_.command.claimed || player!=charge_.player) return false;
        } else if(s_.phase!=Phase::route && s_.phase!=Phase::carrying) return false;
        auto& arrived=eye?s_.eyePlatform:s_.chargePlatform;
        if(arrived || (held_ && charge_.player!=player)) return false;
        arrived=true;
        if(!eye && s_.command.cycle<3) event_scene(s_.command.cycle==1?82:67,0x97C48FC6);
        changed(); return true;
    }
    bool pickup(const ChargeReceipt& receipt) noexcept {
        if(!current(receipt.token) || s_.phase!=Phase::route || !s_.chargeEnabled || !valid_charge(receipt)) return false;
        charge_=receipt; held_=true; s_.chargePickedUp=true; s_.phase=Phase::carrying;
        if(s_.command.cycle<3) event_scene(s_.command.cycle==1?83:66,0x0E57C0FA);
        changed(); return true;
    }
    bool drop(const ChargeReceipt& receipt) noexcept {
        if(s_.phase!=Phase::carrying || !held_ || charge_!=receipt || !current(receipt.token)) return false;
        held_=false; s_.phase=Phase::route; changed(); return true;
    }
    bool dunk(const ChargeReceipt& receipt) noexcept {
        if(s_.phase!=Phase::carrying || !held_ || charge_!=receipt || !current(receipt.token)) return false;
        held_=false; s_.chargeEnabled=false; s_.chargeDunked=true;
        event_scene(rescue_slot(),s_.command.cycle==3?0x14A9A975:0x1E9C04B7);
        if(s_.command.cycle<3) stop_scene(s_.command.cycle==1?83:66);
        schedule(Action::shield,Phase::shield); return true;
    }
    bool health(const Token& token,Health event) noexcept {
        if(!current(token)) return false;
        // The native downward crossing can occur during eye opening. Retain
        // that qualified receipt, but do not start recovery before exposure.
        if(event==Health::eyeCrossed && s_.phase==Phase::shield && s_.command.claimed) {
            if(eyeCrossed_) return false;
            eyeCrossed_=true; changed(); return true;
        }
        if(event==Health::eyeCrossed && s_.phase==Phase::eye) {
            if(s_.command.cycle==3) {
                event_scene(46,0x505750FE); schedule(Action::finalDeath,Phase::death);
            } else schedule(Action::recover,Phase::recovery);
            return true;
        }
        if(event==Health::bodyCheckpoint && s_.phase==Phase::recovery && !s_.nativeBodyCheckpoint) {
            s_.nativeBodyCheckpoint=true; changed(); advance_recovery(); return true;
        }
        if(event==Health::dead && s_.phase==Phase::death && !s_.nativeBossDead) {
            s_.nativeBossDead=true; s_.restriction=false; changed(); advance_death(); return true;
        }
        return false;
    }
    bool ending(const Token& token,bool finished) noexcept {
        if(!current(token) || s_.phase!=Phase::ending || !s_.command.claimed) return false;
        if(!finished) {
            if(s_.endingStarted) return false;
            s_.endingStarted=true; for(auto& scene:s_.scenes) if(scene.generation) scene.stop=true;
        } else { if(!s_.endingStarted) return false; s_.phase=Phase::finished; }
        changed(); return true;
    }
    bool eye_object(const Token& token,std::uint16_t slot,std::uint32_t generation,
                    std::uint32_t source,std::uint32_t entity) noexcept {
        if(!current(token) || s_.command.cycle<1 || s_.command.cycle>2
            || !handle(source) || !handle(entity) || s_.generation>0x7FFFFFFAU) return false;
        const auto cycle=s_.command.cycle;
        if(slot==30) {
            if(s_.phase!=Phase::recovery || !s_.command.claimed || s_.nativeReturnLauncher
                || generation!=s_.generation+2U*cycle-1U || entity==returnEntity_) return false;
            returnEntity_=entity;s_.nativeReturnLauncher=true;changed();advance_recovery();return true;
        }
        if(s_.phase!=Phase::shield || !s_.chargeDunked) return false;
        if(slot==27 && generation==s_.generation+1 && !s_.dpsFrontCreated) {
            s_.dpsFrontCreated=true;changed();return true;
        }
        if(slot==28 && generation==s_.generation+2U*cycle-1U && !s_.dpsBackCreated && entity!=backEntity_) {
            backEntity_=entity;s_.dpsBackCreated=true;changed();return true;
        }
        return false;
    }
    unsigned deaths(unsigned wave) const noexcept {
        unsigned total{};
        for(std::size_t i=0;i<kSources.size();++i) if(kSources[i].wave==wave) total+=dead_[i][0]+dead_[i][1];
        return total;
    }
private:
    Snapshot s_{};
    struct Ledger { ActorReceipt receipt{}; bool used{},dead{}; };
    std::array<Ledger,256> actors_{};
    std::array<std::array<std::uint8_t,2>,kSources.size()> admitted_{},dead_{};
    bool waveStarted_{},departed_{},arrived_{},deletionStarted_{},deletionHeld_{},rescueReady_{},held_{};
    bool eyeCrossed_{};
    std::uint32_t player_{kInvalid};
    ChargeReceipt charge_{};
    std::uint32_t returnEntity_{kInvalid},backEntity_{kInvalid};
    static bool handle(std::uint32_t value) noexcept { return value!=0 && value!=kInvalid; }
    static bool valid(const Boss& b) noexcept {
        return b.run && b.run!=UINT64_MAX && b.generation && b.generation<=0x7FFFFFFFU
            && handle(b.actor) && handle(b.character) && handle(b.biped) && handle(b.entity) && handle(b.member)
            && b.character!=b.biped;
    }
    bool valid_actor(const ActorReceipt& r) const noexcept {
        return r.run==s_.command.token.boss.run && r.generation==s_.generation
            && handle(r.source) && handle(r.actor) && handle(r.entity) && handle(r.character) && handle(r.health);
    }
    bool valid_charge(const ChargeReceipt& r) const noexcept {
        const auto cycle=s_.command.cycle;
        constexpr std::array<std::uint32_t,3> registries{0x0040BF06,0x0040BF05,0x0040BF03};
        constexpr std::array<std::uint16_t,3> sources{18,1,0},sinks{20,3,2};
        return cycle>=1 && cycle<=3 && r.registry==registries[cycle-1]
            && r.sourceSlot==sources[cycle-1] && r.sinkSlot==sinks[cycle-1] && r.generation==s_.generation
            && handle(r.source) && handle(r.item) && handle(r.player) && handle(r.sink);
    }
    void changed() noexcept { ++s_.revision; }
    void schedule(Action action,Phase phase) noexcept {
        ++s_.command.token.epoch; s_.command.action=action; s_.command.claimed=false; s_.phase=phase;
        s_.nativeAnimationFinished=false; s_.nativeBodyCheckpoint=false;
        s_.nativeReturnLauncher=false;changed();
    }
    void summon(std::uint8_t wave,std::uint8_t island,std::uint8_t cycle,Action action) noexcept {
        s_.command.wave=wave; s_.command.island=island; s_.command.cycle=cycle; waveStarted_=false;
        schedule(action,Phase::summon);
    }
    bool cleared(unsigned wave) const noexcept { return wave<15 && deaths(wave)==kWaveTotals[wave]; }
    void advance_clearance() noexcept {
        if(s_.phase!=Phase::clearance || !s_.nativeAnimationFinished || !cleared(s_.command.wave)) return;
        const auto wave=s_.command.wave;
        if(wave==1 && !cleared(0)) return;
        if(wave<=4) { departed_=arrived_=false; schedule(Action::depart,Phase::departure); }
        else if(wave==5 || wave==8 || wave==12) summon(static_cast<std::uint8_t>(wave+1),4,s_.command.cycle,Action::left);
        else if(wave==6 || wave==9 || wave==13) summon(static_cast<std::uint8_t>(wave+1),4,s_.command.cycle,Action::right);
        else {
            deletionStarted_=deletionHeld_=rescueReady_=false;
            schedule(Action::deletion,Phase::deletion);
        }
    }
    void advance_arrival() noexcept {
        if(s_.phase!=Phase::arrival || !departed_ || !arrived_) return;
        const auto island=s_.command.island;
        if(island<3 && (s_.cannonPrepared&7)==7) summon(static_cast<std::uint8_t>(island+2),static_cast<std::uint8_t>(island+1),0,island==1?Action::right:Action::left);
        else if(island==3 && (s_.cannonPrepared&8)) { s_.restriction=true; summon(5,4,1,Action::startCycle); }
        else if(island==4 && (s_.transitPrepared&0x40)) {
            s_.restriction=true; reset_cycle(); summon(12,4,3,Action::startCycle);
        }
    }
    std::uint16_t rescue_slot() const noexcept {
        return s_.command.cycle==1?9:s_.command.cycle==2?27:46;
    }
    SceneCommand* find_scene(std::uint16_t slot) noexcept {
        for(auto& scene:s_.scenes) if(scene.generation && scene.slot==slot) return &scene;
        return nullptr;
    }
    void open_scene(std::uint16_t slot) noexcept {
        if(find_scene(slot)) return;
        for(auto& scene:s_.scenes) if(!scene.generation) {
            scene={s_.command.token,s_.generation,slot,false,0,{}}; return;
        }
    }
    void event_scene(std::uint16_t slot,std::uint32_t event) noexcept {
        auto* scene=find_scene(slot); if(!scene) return;
        for(unsigned i=0;i<scene->eventCount;++i) if(scene->events[i]==event) return;
        if(scene->eventCount<scene->events.size()) scene->events[scene->eventCount++]=event;
    }
    void stop_scene(std::uint16_t slot) noexcept { if(auto* scene=find_scene(slot)) scene->stop=true; }
    void advance_rescue() noexcept {
        if(s_.phase!=Phase::rescue || !deletionHeld_ || !rescueReady_) return;
        const auto cycle=s_.command.cycle;
        const auto mask=cycle==1?7U:cycle==2?0x38U:0U;
        if((s_.transitPrepared&mask)!=mask) return;
        if(cycle<3) for(auto slot:cycle==1?std::array<std::uint16_t,3>{81,82,83}:std::array<std::uint16_t,3>{68,67,66}) open_scene(slot);
        s_.phase=Phase::route; s_.chargeEnabled=true; changed();
    }
    void reset_cycle() noexcept {
        s_.chargeEnabled=s_.chargeDunked=s_.chargePlatform=s_.eyePlatform=s_.chargePickedUp=false;
        eyeCrossed_=false;
        s_.dpsFrontCreated=s_.dpsBackCreated=false;
        charge_={};
        deletionStarted_=deletionHeld_=rescueReady_=held_=false;
    }
    void advance_recovery() noexcept {
        if(s_.phase!=Phase::recovery || !s_.nativeAnimationFinished || !s_.nativeBodyCheckpoint || !s_.nativeReturnLauncher) return;
        if(s_.command.cycle==1) { reset_cycle(); summon(8,4,2,Action::startCycle); }
        else { s_.restriction=false; departed_=arrived_=false; summon(11,4,2,Action::left); }
    }
    void advance_death() noexcept {
        if(s_.phase==Phase::death && s_.nativeAnimationFinished && s_.nativeBossDead) schedule(Action::ending,Phase::ending);
    }
};
} // namespace sunrise::state::activity::omega::mission
