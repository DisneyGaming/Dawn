#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include "state/activity/omega_first_lair_runtime.h"
#include "state/activity/runtime.h"
#include "core/logging/log.h"

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
int main() {
    const fight::Boss boss{42,1,2,3,7,1,0,0};
    auto frame=fight::authority(42,7,true);CHECK(frame.generation==7);
    fight::observe_initial_summon(boss);
    frame=fight::authority(42,7,false);CHECK(frame.loose[3]==3 && frame.loose[4]==3);
    fight::observe_initial_idle(boss);
    CHECK(fight::status(42).action==fight::Action::none); // Callback records a fact only.
    CHECK(!fight::claim_action(boss,fight::Action::summonLeft));
    CHECK(fight::publication_due(GetTickCount64()+1000));
    CHECK(fight::status(42).action==fight::Action::summonLeft); // Publication owner drives the definition.
    for(const auto action:{fight::Action::summonLeft,fight::Action::summonRight}) {
        CHECK(fight::claim_action(boss,action));CHECK(!fight::claim_action(boss,action));
        fight::observe_summon(boss,action,false);
        fight::observe_summon(boss,action,true);
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
    CHECK(fight::claim_action(boss,fight::Action::depart));
    CHECK(fight::observe_arrival(42,1));
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
    fight::observe_initial_idle(boss);CHECK(fight::status(42).action==fight::Action::summonLeft);
    static_cast<void>(fight::authority(42,7,true));
    CHECK(fight::claim_action(boss,fight::Action::summonLeft));
    fight::observe_summon(boss,fight::Action::summonLeft,false);
    fight::observe_summon(boss,fight::Action::summonLeft,true);
    CHECK(fight::status(42).action==fight::Action::summonRight);
    fight::reset();fake::run=43;static_cast<void>(fight::authority(43,8,true));
    fight::observe_initial_idle(boss);CHECK(!fight::status(43).boss.valid());
    CHECK(fake::failures==0);
    std::printf("PASS: %u production combat checks; owner publication, native claims, salted deaths, mode latch, reset and concurrent intake\n",checks);
}
