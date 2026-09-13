#include "state/activity/omega/omega_mission_publication.h"
#include <iostream>
#include <vector>
#include "state/activity/omega/omega_mission_state.h"
namespace m=sunrise::state::activity::omega::mission;
unsigned checks{},failures{};
void check(bool value,const char* message) { ++checks; if(!value) { ++failures; std::cerr<<message<<'\n'; } }
const m::Boss boss{1,2,0x12340001,0x23450002,0x34560003,0x45670004,0x56780005,0};
struct Run {
    m::State state;
    std::uint32_t identity{0x12342010};
    unsigned admissions{},deaths{};
    std::uint32_t previousReturnEntity{UINT32_MAX};
    explicit Run(bool prepare=true) {
        check(state.bind(boss),"bind accepted physical boss");
        check(state.bind(boss),"duplicate bind preserves state");
        if(prepare) {
            for(std::uint8_t i=0;i<4;++i) check(state.prepared(1,2,i,false),"prepare each native chase core");
            for(std::uint8_t i=0;i<7;++i) check(state.prepared(1,2,i,true),"prepare each native Crown core");
        }
    }
    m::Token claim(m::Action action) {
        auto command=state.snapshot().command;
        check(command.action==action,"documented action order");
        if(action==m::Action::left || action==m::Action::right) {
            check(state.prepare_arm(command.token),"native arm readiness schedules authority");
            check(!state.animation(command.token,m::Animation::started),"pending high authority cannot start wave");
            check(state.arm_applied(command.token,state.snapshot().arm.revision),"qualified native high control receipt");
        } else check(state.claim(command.token,action),"claim exact command");
        check(!state.claim(command.token,action),"command cannot be claimed twice");
        auto stale=command.token; ++stale.epoch;
        check(!state.animation(stale,m::Animation::started),"foreign epoch cannot start native action");
        return command.token;
    }
    std::vector<m::ActorReceipt> actors(unsigned wave) {
        std::vector<m::ActorReceipt> result;
        for(const auto& source:m::kSources) if(source.wave==wave)
            for(std::uint8_t category=0;category<source.categories;++category)
                for(unsigned i=0;i<source.requested[category];++i) {
                    const auto actor=++identity;
                    m::ActorReceipt receipt{1,2,source.registry,0x34566000U+source.slot,actor,
                        actor+0x10000000U,actor+0x20000000U,actor+0x30000000U,source.slot,category};
                    auto wrong=receipt; ++wrong.generation;
                    check(!state.admit(wrong),"wrong native source generation rejected");
                    check(state.admit(receipt),"native admitted actor enters its source ledger");
                    check(!state.admit(receipt),"same full actor cannot be counted twice");
                    result.push_back(receipt); ++admissions;
                }
        check(result.size()==m::kWaveTotals[wave],"documented wave count preserved");
        return result;
    }
    void kill(const std::vector<m::ActorReceipt>& actors) {
        for(const auto& actor:actors) {
            auto wrong=actor; ++wrong.health;
            check(!state.death(wrong),"foreign health allocation cannot kill an admitted actor");
            check(state.death(actor),"qualified native death accepted"); ++deaths;
            check(!state.death(actor),"duplicate death rejected");
        }
    }
    void wave(unsigned index,m::Action action,bool early) {
        check(state.snapshot().command.wave==index,"documented wave order");
        const bool nativeArm=action==m::Action::left || action==m::Action::right;
        const auto token=claim(action);
        check(!state.animation(token,m::Animation::finished),"finish before actual summon start rejected");
        check(state.animation(token,m::Animation::started),"native summon starts budget");
        check(!state.animation(token,m::Animation::started),"duplicate start cannot repeat requests");
        auto admitted=actors(index);
        if(early) kill(admitted);
        if(nativeArm) {
            check(state.release_arm(token),"native wrap requests low authority");
            check(!state.animation(token,m::Animation::finished),"wave waits for actual native low receipt");
            check(state.arm_applied(token,state.snapshot().arm.revision),"qualified native low receipt finishes summon");
        } else check(state.animation(token,m::Animation::finished),"native summon completion accepted");
        if(!early) kill(admitted);
    }
    void depart(std::uint8_t destination,bool earlyArrival) {
        check(state.snapshot().phase==m::Phase::departure,"clearance requests native departure");
        auto token=claim(m::Action::depart);
        check(!state.arrival(token,destination+1,0x12),"skipped island cannot fabricate intermediate stages");
        if(earlyArrival) check(state.arrival(token,destination,0x12),"actual arrival during departure is retained");
        check(state.animation(token,m::Animation::departureFinished),"native motion cleanup receipt");
        if(!earlyArrival) check(state.arrival(token,destination,0x12),"actual arrival after departure accepted");
        check(!state.arrival(token,destination,0x12),"old arrival cannot replay next stage");
    }
    void mechanic(unsigned cycle,bool earlyHealth,bool earlyRescue,unsigned launcherOrder) {
        auto token=claim(m::Action::deletion);
        const std::uint16_t scene=cycle==1?9:cycle==2?27:46;
        check(!state.rescue_ready(token,scene),"rescue needs issued native Scene");check(!state.rescue_started(token,scene),"arrival needs issued Scene");
        check(state.animation(token,m::Animation::started),"native deletion starts Scene");
        check(state.rescue_started(token,scene),"native active Scene accepts arrival");check(!state.rescue_started(token,scene),"arrival deduplicated");
        if(earlyRescue) check(state.rescue_ready(token,scene),"early Scene readiness retained");
        check(state.animation(token,m::Animation::deletionHold),"native deletion hold");
        if(!earlyRescue) check(state.rescue_ready(token,scene),"actual Scene readiness enables route");
        check(state.snapshot().phase==m::Phase::route,"route after Scene and prepared cores");
        check(!state.route_arrival(token,true,0x12),"eye platform arrival cannot imply charge pickup");
        check(state.route_arrival(token,false,0x12),"charge platform arrival accepted");
        const std::array<std::uint32_t,3> registries{0x40BF06,0x40BF05,0x40BF03};
        const std::array<std::uint16_t,3> source{18,1,0},sink{20,3,2};
        const m::ChargeReceipt charge{token,0x122,2,0x233,0x12,0x344,registries[cycle-1],source[cycle-1],sink[cycle-1]};
        check(!state.dunk(charge),"near sink without held item is not dunk");
        check(state.pickup(charge),"native held item receipt");
        check(state.drop(charge),"drop preserves unfinished route");
        check(state.snapshot().phase==m::Phase::route,"drop does not break shield");
        check(state.pickup(charge),"same native item may be picked up again");
        auto wrong=charge; ++wrong.player;
        check(!state.dunk(wrong),"another player cannot consume retained charge");
        check(state.dunk(charge),"exact native successful use breaks shield");
        const auto transportToken=state.snapshot().command.token;
        check(!state.claim(transportToken,m::Action::shield),"eye command waits for post-dunk arrival");
        check(!state.route_arrival(token,true,charge.player),"pre-dunk epoch cannot acknowledge teleport");
        check(!state.route_arrival(transportToken,true,wrong.player),"different player cannot acknowledge dunker's teleport");
        check(state.route_arrival(transportToken,true,charge.player),"same dunker reaches authored eye receiving platform");
        check(!state.route_arrival(transportToken,true,charge.player),"arrival is consumed once");
        token=claim(m::Action::shield);
        if(earlyHealth) {
            check(state.health(token,m::Health::eyeCrossed),"native crossing during eye opening retained");
            check(state.snapshot().phase==m::Phase::shield,"early crossing waits for actual exposure");
            check(!state.health(token,m::Health::eyeCrossed),"duplicate opening crossing ignored");
        }
        check(state.animation(token,m::Animation::eyeExposed),"actual native eye exposure");
        if(!earlyHealth) check(state.health(token,m::Health::eyeCrossed),"qualified eye crossing");
        token=claim(cycle==3?m::Action::finalDeath:m::Action::recover);
        const auto health=cycle==3?m::Health::dead:m::Health::bodyCheckpoint;
        if(cycle==3) {
            if(earlyHealth) check(state.health(token,health),"early native health receipt retained");
            check(state.animation(token,m::Animation::finished),"actual recovery/death animation finished");
            if(!earlyHealth) check(state.health(token,health),"native health receipt after animation accepted");
            return;
        }
        // Recovery now joins three independently qualified callbacks: animation,
        // health checkpoint and the newly created return launcher (source slot30).
        // Exercise every ordering without treating publication as creation.
        const auto generation=state.snapshot().generation+2U*cycle-1U;
        const auto launcherSource=++identity,launcherEntity=++identity;
        auto stale=token;++stale.epoch;
        check(!state.eye_object(stale,30,generation,launcherSource,launcherEntity),"foreign recovery epoch cannot acknowledge launcher");
        check(!state.eye_object(token,30,generation+1,launcherSource,launcherEntity),"wrong launcher generation cannot release recovery");
        check(!state.eye_object(token,30,generation,UINT32_MAX,launcherEntity),"missing native source cannot acknowledge launcher");
        if(previousReturnEntity!=UINT32_MAX)
            check(!state.eye_object(token,30,generation,launcherSource,previousReturnEntity),"prior cycle entity cannot acknowledge replacement launcher");
        unsigned delivered{};
        for(unsigned index=0;index<3;++index) {
            if(index==launcherOrder) {
                check(state.eye_object(token,30,generation,launcherSource,launcherEntity),"qualified native return launcher creation accepted");
                check(!state.eye_object(token,30,generation,launcherSource,launcherEntity),"return launcher receipt consumed once");
                previousReturnEntity=launcherEntity;
            } else {
                const bool first=delivered++==0;
                if(first==earlyHealth) check(state.health(token,health),"qualified native health checkpoint accepted");
                else check(state.animation(token,m::Animation::finished),"actual native recovery animation finished");
            }
            if(index<2) check(state.snapshot().phase==m::Phase::recovery,"recovery waits for every native prerequisite");
        }
        check(state.snapshot().phase==m::Phase::summon && state.snapshot().command.wave==(cycle==1?8:11),
            "all three native receipts release the next authored wave");

    }
};
void full_route(unsigned order) {
    Run run;
    // Independent documented walkthrough: 21 initial, 12 chase, 99 Crown,
    // 11 living escape actors; no clock or synthetic disappearance advances it.
    run.wave(0,m::Action::left,order&1);
    run.wave(1,m::Action::right,!(order&1));
    run.depart(1,order&2); check(run.state.snapshot().cannons==7,"first three cannons activate together");
    run.wave(2,m::Action::left,order&1); run.depart(2,!(order&2));
    run.wave(3,m::Action::right,order&1); run.depart(3,order&2);
    run.wave(4,m::Action::left,order&1); run.depart(4,!(order&2));
    check(run.state.snapshot().cannons==15 && run.state.snapshot().restriction,"Crown arrival enables fourth cannon/restriction");
    run.wave(5,m::Action::startCycle,order&1); run.wave(6,m::Action::left,order&1); run.wave(7,m::Action::right,order&1);
    run.mechanic(1,order&4,order&8,order%3);
    run.wave(8,m::Action::startCycle,order&1); run.wave(9,m::Action::left,order&1); run.wave(10,m::Action::right,order&1);
    run.mechanic(2,!(order&4),!(order&8),(order+1)%3);
    auto token=run.claim(m::Action::left);
    check(run.state.animation(token,m::Animation::started),"escape starts from native summon");
    const auto escape=run.actors(11);
    check(run.state.release_arm(token),"escape native clip wrap requests low control");
    check(run.state.arm_applied(token,run.state.snapshot().arm.revision),"living escape permits departure after native low receipt");
    check(!run.state.snapshot().restriction,"second recovery releases restriction");
    run.depart(5,order&2);
    check(run.state.snapshot().restriction,"actual final arrival restores restriction");
    run.wave(12,m::Action::startCycle,order&1); run.wave(13,m::Action::left,order&1); run.wave(14,m::Action::right,order&1);
    run.mechanic(3,order&4,order&8,0);
    token=run.claim(m::Action::ending);
    check(!run.state.ending(token,true),"missing cinematic cannot mean completion");
    check(run.state.ending(token,false),"native ending start accepted");
    check(run.state.ending(token,true),"native completion or skip finishes mission");
    check(run.state.snapshot().phase==m::Phase::finished,"full documented route finishes");
    check(run.admissions==143 && run.deaths==132,"143 admissions /132 required deaths /11 live escape");
    check(!run.state.snapshot().restriction,"final native death releases restriction");
    auto next=boss; ++next.run;
    check(run.state.bind(next),"new mission resets state");
    check(!run.state.bind(boss),"old callback cannot reset new mission");
    check(!run.state.death(escape[0]),"old encounter death cannot contaminate new run");
}
void publication() {
    m::Publication p;
    check(!p.due(0,0,1,14,100),"inactive mission does not publish");
    check(p.due(7,2,10,14,100),"first mission revision publishes immediately");
    check(p.due(7,2,10,14,101),"failed staging remains due");
    p.delivered(7,2,10,14,101);
    check(!p.due(7,2,11,15,200),"coalesce receipt bursts for 100ms");
    check(p.due(7,2,11,15,201),"new receipt does not wait five seconds");
    p.delivered(7,2,11,15,201);
    check(!p.due(7,2,11,15,10000),"unchanged mission adds no network traffic");
    check(p.due(7,2,11,16,6601),"dialogue interval expiry publishes without new gameplay receipt");
    check(p.due(8,2,1,14,202),"new run does not inherit previous delay");
}
int main() {
    publication();
    for(unsigned order=0;order<16;++order) full_route(order);
    std::cout<<(failures?"FAIL ":"PASS ")<<checks<<" mission-state checks; "<<failures<<" failures\n";
    return failures?1:0;
}
