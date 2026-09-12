#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include "state/activity/omega_first_lair_runtime.h"
#include "state/activity/runtime.h"
#include "core/logging/log.h"
#include "fixtures/omega_archive_arm_control.h"
#include "state/activity/omega_boss_intro_action.h"
#include "client/hooks/bootflow/omega_boss_health_identity.h"

namespace activity=sunrise::state::activity;
namespace fight=activity::omega_first_lair;
namespace fake {
std::atomic_uint64_t run{42};
std::atomic_bool seed{true},quiesced{false};
std::atomic<activity::WorldPhase> phase{activity::WorldPhase::arrived};
std::atomic_uint failures{},graphs{};
}
namespace sunrise::state::activity {
std::uint64_t mission_run_generation() noexcept { return fake::run; }
bool mission_seed_armed() noexcept { return fake::seed; }
bool omega_authority_quiesced() noexcept { return fake::quiesced; }
WorldPhase world_phase() noexcept { return fake::phase; }
}
namespace sunrise::core::log {
void write(Channel,Level,std::string_view line) noexcept {
    if(line.starts_with("ev=coo_combat")) { ++fake::graphs; }
    if(line.find("failed=1")!=line.npos) { ++fake::failures; }
}
}
static unsigned checks{};
#define CHECK(value) do { ++checks;if(!(value)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#value);std::exit(1); } } while(false)
namespace arm=activity::omega_archive_arm;
using omega_archive_arm_fixture::ControlFixture;
using omega_archive_arm_fixture::arm_owner;
using omega_archive_arm_fixture::put;
void start_intro(const fight::Boss& boss) {
    auto request=arm_owner(boss);request.revision=0;
    fight::observe_initial_idle(boss);CHECK(!fight::status(boss.run).boss.valid());
    CHECK(fight::request_intro(request));CHECK(!fight::request_intro(request));
    CHECK(fight::authority(boss.run,boss.generation).intro.revision==1);
    CHECK(!fight::observe_intro_control(request));
    auto applied=request;applied.revision=1;auto stale=applied;++stale.fullBody;
    CHECK(!fight::observe_intro_control(stale));CHECK(fight::observe_intro_control(applied));
    CHECK(!fight::observe_intro_control(applied));CHECK(fight::status(boss.run).intro.applied);
}
void arm_cycle(const fight::Boss& boss,fight::Action action) {
    const auto owner=arm_owner(boss);
    CHECK(!fight::claim_action(boss,action));
    CHECK(fight::prepare_arm(owner));CHECK(!fight::prepare_arm(owner));
    auto state=fight::status(boss.run);const auto high=state.arm.control;
    CHECK(high.high && high.right==(action==fight::Action::summonRight) && state.arm.pending);
    CHECK(fight::authority(boss.run,boss.generation).arm.revision==high.revision);
    // A native start without the committed control receipt cannot release adds.
    fight::observe_summon(boss,action,false);CHECK(fight::status(boss.run).phase==state.phase);
    CHECK(!fight::release_arm(owner,true));
    const std::array<float,4> zero{},one{1,1,1,1};
    const auto left=high.right?zero:one;const auto right=high.right?one:zero;
    ControlFixture native(boss,high);CHECK(native.receipt().applied);
    auto stale=owner;++stale.generation;
    CHECK(!fight::observe_arm_control(stale,native.receipt(),left,right));
    stale=owner;++stale.fullBody;CHECK(!fight::observe_arm_control(stale,native.receipt(),left,right));
    stale=owner;++stale.actionEpoch;CHECK(!fight::observe_arm_control(stale,native.receipt(),left,right));
    CHECK(!fight::observe_arm_control(owner,native.receipt(),zero,zero));
    CHECK(fight::observe_arm_control(owner,native.receipt(),left,right));
    CHECK(!fight::observe_arm_control(owner,native.receipt(),left,right));
    CHECK(!fight::release_arm(owner,true)); // Applied scalar alone is not playback.
    fight::observe_summon(boss,action,false);
    state=fight::status(boss.run);CHECK(state.phase==(high.right?fight::Phase::rightPlaying:fight::Phase::leftPlaying));
    fight::observe_summon(boss,action,true);CHECK(fight::status(boss.run).phase==state.phase);
    CHECK(fight::release_arm(owner,true));CHECK(!fight::release_arm(owner,true));
    state=fight::status(boss.run);CHECK(state.arm.pending && !state.arm.control.high);
    CHECK(state.arm.control.revision==high.revision+1);
    CHECK(fight::authority(boss.run,boss.generation).arm.revision==high.revision+1);
    CHECK(!fight::observe_arm_control(owner,native.receipt(),zero,zero));
    native=ControlFixture(boss,state.arm.control);
    CHECK(!fight::observe_arm_control(owner,native.receipt(),left,right));
    CHECK(fight::observe_arm_control(owner,native.receipt(),zero,zero));
    CHECK(!fight::status(boss.run).arm.pending);
}
void intro_native_guards() {
    namespace intro=activity::omega_archive_intro;
    auto queue=activity::omega_presentation::boss_intro_action();
    CHECK(intro::matches_queue(queue,0));CHECK(!intro::matches_queue(queue,1));
    for(const auto offset:{0U,8U,0x18U,0x1CU,0x20U,0x24U,0x28U,0x2AU,0x2BU,0x2CU,0x2DU}) {
        auto other=queue;other[offset]^=std::byte{1};CHECK(!intro::matches_queue(other,0));
    }
    intro::Ledger ledger;arm::Owner owner{42,1,2,3,7,0,77,0,0};
    CHECK(ledger.request(owner));CHECK(ledger.cancel());CHECK(!ledger.cancel());
    owner.revision=1;CHECK(!ledger.acknowledge(owner));
    ledger={};owner.revision=0;CHECK(ledger.request(owner));owner.revision=1;
    CHECK(ledger.acknowledge(owner));CHECK(!ledger.cancel()); // Never retire an established Boss identity.
    const auto incarnation=ledger.status().incarnation;CHECK(incarnation==1);
    owner.island=4; // First crown command has the same action epoch as the intro.
    for(const auto sequence:{0x65D2379EU,0x65D2379DU,0x65D2379BU}) {
        CHECK(ledger.next(owner,sequence));CHECK(!ledger.next(owner,sequence));
        auto native=owner;native.revision=ledger.status().program.revision;
        CHECK(!ledger.acknowledge(owner));CHECK(ledger.acknowledge(native));
        CHECK(ledger.status().owner.revision==incarnation && ledger.status().incarnation==incarnation);
        CHECK(ledger.status().program.revision>incarnation);
        ++owner.actionEpoch;
    }
}
void movement_program_guards() {
    namespace program=activity::omega_archive_intro;
    program::Ledger ledger;auto owner=arm_owner(fight::Boss{42,1,2,3,7,0,0,0});
    CHECK(ledger.request(owner));owner.revision=1;CHECK(ledger.acknowledge(owner));
    for(std::uint8_t island=0;island<4;++island) {
        owner.island=island;CHECK(ledger.depart(owner));CHECK(!ledger.depart(owner));
        CHECK(ledger.status().program.departure==island);
        CHECK(ledger.status().program.sequence==0xCBFDCA32U);
        auto native=owner;native.revision=ledger.status().program.revision;
        CHECK(!ledger.acknowledge(owner));CHECK(ledger.acknowledge(native));CHECK(ledger.status().incarnation==1);
        CHECK(!ledger.depart(owner));
        auto queue=activity::omega_presentation::boss_intro_action();
        put(queue,0x18,std::uint32_t{0x1F992208});put(queue,0x1C,std::uint32_t{0xCBFDCA32});
        put(queue,0x24,std::uint32_t{0x95FB2E01});queue[0x28]=std::byte{48};put(queue,0x2A,std::int16_t{55});queue[0x2D]=std::byte{island};
        CHECK(program::matches_queue(queue,0,ledger.status().program));
        for(const auto offset:{0x18U,0x1CU,0x20U,0x24U,0x28U,0x2AU,0x2BU,0x2CU,0x2DU}) {
            auto changed=queue;changed[offset]^=std::byte{1};CHECK(!program::matches_queue(changed,0,ledger.status().program));
        }
    }
    owner.island=4;CHECK(ledger.next(owner,0x65D2379C));
    auto native=owner;native.revision=ledger.status().program.revision;CHECK(ledger.acknowledge(native));
    for(const auto sequence:{0x65D2379EU,0x65D2379DU}) {
        ++owner.actionEpoch;CHECK(ledger.next(owner,sequence));native=owner;native.revision=ledger.status().program.revision;CHECK(ledger.acknowledge(native));
    }
    ++owner.actionEpoch;CHECK(ledger.depart(owner));native=owner;native.revision=ledger.status().program.revision;
    CHECK(ledger.acknowledge(native));CHECK(ledger.status().program.departure==4);CHECK(!ledger.depart(owner));
    ++owner.actionEpoch;CHECK(ledger.next(owner,0x65D2379B));native=owner;native.revision=ledger.status().program.revision;
    CHECK(ledger.acknowledge(native));CHECK(ledger.status().incarnation==1 && ledger.status().program.revision==10);
}
void health_program_identity_guards() {
    namespace health=sunrise::client::hooks::bootflow::omega_boss_health;
    const fight::Boss boss{42,1,2,3,7,1,4,9};
    const health::Reference ref{0x1234,0x8080834E,0};
    activity::omega_archive_intro::Status program{arm_owner(boss),{7,5,true,0x65D2379B},true,1};
    std::array<std::byte,health::kMemberBytes> member{};std::array<std::byte,0x108> auth{};
    put(member,0,std::uint32_t{0x80F4756D});put(member,4,std::uint32_t{0x80807D9D});
    put(member,8,std::uint64_t{0xB58});put(member,0x48,ref);put(member,0x180,boss.generation);
    put(member,0x190,program.program.revision);put(member,0x21C,boss.actor);
    put(auth,0,boss.generation);put(auth,0x100,program.program.revision);auth[6]=std::byte{1};
    CHECK(!health::member_identity(member,boss,ref));
    CHECK(health::member_program_identity(member,auth,boss,ref,program));
    // Mechanic action epochs advance without replacing the physical actor.
    ++program.owner.actionEpoch;CHECK(health::member_program_identity(member,auth,boss,ref,program));
    for(const auto offset:{0U,4U,8U,0x48U,0x4CU,0x50U,0x180U,0x190U,0x1D4U,0x21CU}) {
        auto changed=member;changed[offset]^=std::byte{1};
        CHECK(!health::member_program_identity(changed,auth,boss,ref,program));
    }
    for(const auto offset:{0U,6U,0x100U}) {
        auto changed=auth;changed[offset]^=std::byte{1};
        CHECK(!health::member_program_identity(member,changed,boss,ref,program));
    }
    auto changed=program;changed.applied=false;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;changed.program.play=false;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.program.revision;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.incarnation;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.owner.run;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.owner.character;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.owner.entity;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.owner.generation;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
    changed=program;++changed.owner.revision;CHECK(!health::member_program_identity(member,auth,boss,ref,changed));
}
void native_control_guards() {
    const fight::Boss boss{42,1,2,3,7,1,0,0};
    const arm::Control control{7,1,false,true};
    const ControlFixture original(boss,control);CHECK(original.receipt().applied);
    for(const auto offset:{0U,4U,8U,0x180U,0x190U,0x1D4U,0xA90U,0xA94U,0xAB0U}) {
        auto fixture=original;fixture.member[offset]^=std::byte{1};CHECK(!fixture.receipt().applied);
    }
    for(const auto offset:{0U,6U,0x4CU,0x50U,0x51U,0x54U,0x58U,0x6CU,0x70U,0x74U,0x76U,0x78U,0x7CU,0x80U,0x88U,0x100U}) {
        auto fixture=original;fixture.auth[offset]^=std::byte{1};CHECK(!fixture.receipt().applied);
    }
    CHECK(!arm::inspect(std::span(original.member).first(0xAB3),original.auth).applied);
    CHECK(!arm::inspect(original.member,std::span(original.auth).first(0x107)).applied);
    ControlFixture defaults(boss,{});CHECK(defaults.receipt().applied && defaults.receipt().revision==0);
    put(defaults.auth,0x7C,std::uint32_t{2});CHECK(!defaults.receipt().applied);
    arm::Ledger ledger;auto owner=arm_owner(boss);CHECK(ledger.prepare(owner,false));
    CHECK(ledger.release(owner,false));CHECK(!ledger.release(owner,false));
    CHECK(!ledger.prepare(owner,true)); // Cancellation must be acknowledged first.
    ControlFixture low(boss,ledger.status().control);const std::array<float,4> zero{};
    CHECK(ledger.acknowledge(owner,low.receipt(),zero,zero));
    ++owner.run;CHECK(!ledger.prepare(owner,true));--owner.run;
    ++owner.generation;CHECK(!ledger.prepare(owner,true));
}
int main() {
    native_control_guards();intro_native_guards();health_program_identity_guards();movement_program_guards();
    const fight::Boss boss{42,1,2,3,7,1,0,0};
    auto frame=fight::authority(42,7,true);CHECK(frame.generation==7);
    start_intro(boss);
    fight::observe_initial_summon(boss);
    frame=fight::authority(42,7,false);CHECK(frame.loose[3]==3 && frame.loose[4]==3);
    fight::observe_initial_idle(boss);
    CHECK(fight::status(42).action==fight::Action::none); // Callback records a fact only.
    CHECK(!fight::claim_action(boss,fight::Action::summonLeft));
    CHECK(fight::publication_due(GetTickCount64()+1000));
    CHECK(fight::status(42).action==fight::Action::summonLeft); // Publication owner drives the definition.
    for(const auto action:{fight::Action::summonLeft,fight::Action::summonRight}) {
        arm_cycle(boss,action);
        static_cast<void>(fight::authority(42,7,false)); // The initial selected mode remains latched.
    }
    CHECK(fight::status(42).action==fight::Action::none);
    std::vector<fight::ActorReceipt> actors;
    for(const auto& group:fight::kGroups) {
        for(unsigned i=0;i<group.count;++i) {
            const auto id=100U+static_cast<unsigned>(actors.size());
            const fight::ActorReceipt actor{42,id,id+1000,7,group.source,group.registry};
            CHECK(fight::observe_admission(actor));CHECK(!fight::observe_admission(actor));actors.push_back(actor);
        }
    }
    CHECK(actors.size()==21);
    for(auto actor:actors) {
        auto stale=actor;++stale.sourceHandle;CHECK(!fight::observe_death(stale));
        CHECK(fight::observe_death(actor));CHECK(!fight::observe_death(actor));
    }
    CHECK(fight::status(42).action==fight::Action::none);
    CHECK(fight::publication_due(GetTickCount64()+1000));
    CHECK(fight::status(42).action==fight::Action::depart);
    CHECK(!fight::claim_action(boss,fight::Action::depart));
    const auto movementOwner=arm_owner(boss);
    CHECK(fight::request_boss_movement(movementOwner));CHECK(!fight::request_boss_movement(movementOwner));
    CHECK(fight::authority(42,7,true).intro.departure==0);
    CHECK(fight::observe_arrival(42,1));
    fight::observe_departure(boss,true,true);CHECK(fight::status(42).island==0);
    auto movementNative=movementOwner;movementNative.revision=fight::status(42).intro.program.revision;
    CHECK(!fight::observe_intro_control(movementOwner));CHECK(fight::observe_intro_control(movementNative));
    fight::observe_departure(boss,true,false);CHECK(fight::status(42).island==0);
    fight::observe_departure(boss,true,true);CHECK(fight::status(42).island==0);
    frame=fight::authority(42,7,true);CHECK(fight::status(42).island==1);
    CHECK(fight::status(42).action==fight::Action::summonLeft);
    CHECK(!fight::claim_action(boss,fight::Action::summonLeft));
    fake::quiesced=true;CHECK(!fight::status(42).enabled);CHECK(!fight::observe_arrival(42,2));
    fake::quiesced=false;
    fake::phase=activity::WorldPhase::idle;CHECK(!fight::status(42).enabled);fake::phase=activity::WorldPhase::arrived;
    std::thread intake([] {
        for(unsigned i=0;i<1000;++i) { static_cast<void>(fight::observe_arrival(41,2));static_cast<void>(fight::status(42)); }
    });
    for(unsigned i=0;i<1000;++i) { static_cast<void>(fight::authority(42,7,true));static_cast<void>(fight::publication_due(GetTickCount64())); }
    intake.join();CHECK(fake::failures==0);CHECK(fake::graphs>0);
    // Reset removes the old actor and section. A separately selected legacy run
    // keeps its synchronous receipt contract despite a later setting flip.
    fight::reset();CHECK(!fight::status(42).enabled);
    static_cast<void>(fight::authority(42,7,false));
    start_intro(boss);
    fight::observe_initial_idle(boss);CHECK(fight::status(42).action==fight::Action::summonLeft);
    static_cast<void>(fight::authority(42,7,true));
    arm_cycle(boss,fight::Action::summonLeft);
    CHECK(fight::status(42).action==fight::Action::summonRight);
    fight::reset();fake::run=43;static_cast<void>(fight::authority(43,8,true));
    fight::observe_initial_idle(boss);CHECK(!fight::status(43).boss.valid());
    CHECK(fake::failures==0);
    const fight::Boss nextBoss{43,1,2,3,8,1,0,0};
    auto pending=arm_owner(nextBoss);pending.revision=0;
    CHECK(fight::request_intro(pending));fight::invalidate(43);
    CHECK(fight::authority(43,8,true).intro.revision==2);
    CHECK(!fight::authority(43,8,true).intro.play);
    pending.revision=1;CHECK(!fight::observe_intro_control(pending));
    fight::reset();CHECK(!fight::status(43).enabled);
    std::printf("PASS: %u production combat checks; owner publication, native claims, salted deaths, mode latch, reset and concurrent intake\n",checks);
}
