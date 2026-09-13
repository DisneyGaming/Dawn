#include "state/activity/nightfall/rules.h"
#include "state/runtime/runtime.h"
#include "client/activity/nightfall_player.h"
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
namespace nf = sunrise::state::activity::nightfall;
namespace strikes = sunrise::state::activity::strikes;
unsigned checks{};
void check(bool value, const char* label) {
    ++checks; if (!value) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
int main() {
    check(nf::rewards::amount(strikes::Difficulty::standard)==0
        && nf::rewards::amount(strikes::Difficulty::adept)==1000
        && nf::rewards::amount(strikes::Difficulty::master)==2500
        && nf::rewards::amount(strikes::Difficulty::grandmaster)==5000,
        "Dawn completion bonus is exact per tier");
    check(nf::rewards::account_matches(77,77) && !nf::rewards::account_matches(0,0)
        && !nf::rewards::account_matches(77,78),
        "capped and non-capped delivery require exact nonzero account owner");
    nf::leave();
    const auto revision = nf::power_revision();
    nf::Options custom{}; custom.startingRevives=1; custom.reviveMinutes=10; custom.powerDelta=1;
    nf::arm(835,custom);
    const auto selected=nf::selection();
    check(selected.activity==835 && selected.options.powerDelta==30,"GM minimum power disadvantage");
    check(selected.options.lockedEquipment && selected.options.limitedRevives && selected.options.extinguish
        && selected.options.disableInfiniteAmmo,"GM core modifiers cannot be removed");
    check(nf::movement_blocked() && nf::infinite_ammo_blocked() && nf::equipment_locked(),"GM overrides saved cheats");
    check(nf::power_revision()==revision+2 && (nf::power_revision() & 1U)==0,
        "cap edge publishes one complete native refresh version");
    custom.startingRevives=4; custom.powerDelta=50;
    nf::enter(100,835,10);
    check(nf::options().startingRevives==1 && nf::options().powerDelta==30,"launch copied immutable options");
    nf::observe_local_player(100,10,true,1000);
    check(nf::progress().deaths==0 && !nf::failed(10),"spawn or initial dead body cannot fail a run");
    nf::observe_local_player(100,10,false,2000);
    nf::observe_local_player(99,10,true,2100);
    nf::observe_local_player(100,9,true,2100);
    check(!nf::failed(10),"stale player receipts rejected");
    nf::enemy_defeated(9,true,true);
    nf::enemy_defeated(10,false,false);
    nf::enemy_defeated(10,false,true);
    check(nf::progress().score==5100 && nf::progress().champions==0,"real accepted kills and bosses score separately");
    nf::enter(101,835,10);
    check(nf::progress().kills==2 && nf::options().startingRevives==1,"region session change preserves run and custom modifiers");
    check(nf::power_revision()==revision+2,"unchanged world polls do not flood refreshes");
    nf::observe_local_player(100,10,true,2200);
    check(!nf::failed(10),"old region session cannot fail current owner");
    nf::observe_local_player(101,10,true,2300);
    nf::observe_local_player(101,10,true,2400);
    check(nf::failed(10) && nf::progress().deaths==1 && nf::progress().finishedAt==2300,"solo GM death fails exactly once");
    nf::complete(10,2500); nf::enemy_defeated(10,true,true);
    check(nf::progress().outcome==nf::Outcome::failed && nf::progress().kills==2,"failure cannot award completion or later kills");
    nf::leave();
    check(!nf::movement_blocked() && !nf::equipment_locked() && !nf::infinite_ammo_blocked(),"orbit restores saved features");
    check(nf::power_revision()==revision+4 && (nf::power_revision() & 1U)==0,
        "orbit refresh restores one complete uncapped version");
    check(nf::progress().outcome==nf::Outcome::failed,"last result survives orbit");
    for (const auto& variant : strikes::kVariants) {
        nf::arm(static_cast<std::int16_t>(variant.activity),nf::defaults(variant.difficulty));
        nf::enter(200,static_cast<std::int16_t>(variant.activity),20);
        const bool gm=variant.difficulty==strikes::Difficulty::grandmaster;
        check(nf::movement_blocked()==gm,"movement restriction belongs only to both GM identities");
        check(nf::infinite_ammo_blocked()==gm,"default ammo restriction belongs only to both GM identities");
        nf::leave();
    }
    auto budget=nf::defaults(strikes::Difficulty::adept); budget.limitedRevives=true;
    budget.startingRevives=1; budget.reviveMinutes=5;
    nf::arm(830,budget); nf::enter(300,830,30);
    nf::observe_local_player(300,30,false,1000);
    nf::observe_local_player(300,30,true,2000);
    check(!nf::failed(30) && nf::progress().revives==0,"one death spends one optional solo revive");
    nf::observe_local_player(300,30,true,2100);
    check(nf::progress().deaths==1,"repeated dead samples cannot spend again");
    nf::observe_local_player(300,30,false,3000);
    nf::enemy_defeated(30,true);
    check(nf::progress().revives==1,"authenticated champion credit replenishes budget");
    nf::observe_local_player(300,30,false,301000);
    nf::enemy_defeated(30,true);
    check(nf::progress().revives==0 && nf::progress().revivesExpired,"expired budget cannot be replenished");
    nf::observe_local_player(300,30,true,302000);
    check(nf::failed(30),"death with expired budget fails");
    nf::leave(); nf::arm(808,{}); nf::enter(400,808,40);
    nf::observe_local_player(400,40,false,1000); nf::enemy_defeated(40);
    nf::complete(40,5000); nf::complete(40,6000);
    nf::observe_local_player(400,40,true,7000);
    check(nf::progress().outcome==nf::Outcome::completed && nf::progress().finishedAt==5000
        && nf::progress().deaths==0,"completed result immutable");
    nf::rewards::Ticket reward{};
    check(nf::rewards::claim(400,reward) && reward.run==40 && reward.quantity==1000,
        "Adept completion queues exact Dawn Glimmer bonus");
    auto tampered=reward; ++tampered.quantity;
    check(!nf::rewards::finish(tampered,1000),"tampered reward ticket cannot acknowledge debt");
    nf::rewards::release(tampered);
    nf::rewards::Ticket stillClaimed{};
    check(!nf::rewards::claim(400,stillClaimed),"tampered ticket cannot release exact claim");
    nf::rewards::release(reward);
    check(nf::rewards::claim(400,reward) && nf::rewards::finish(reward,1000),
        "released exact reward retries and completes once");
    check(nf::rewards::offer(400,40,strikes::Difficulty::adept)
        && !nf::rewards::claim(400,reward),"delivered run is idempotent");
    sunrise::state::PendingProfileItemAcquisition malformed{};
    malformed.rewardGrant=true;malformed.expectedItemCount=2;malformed.afterItemCount=2;
    malformed.profileIndex=0;malformed.beforeItems[1]={0,2,10,1};
    malformed.afterItems=malformed.beforeItems;
    check(sunrise::state::profile_currency_grant_after_image_exact(malformed),
        "reward image accepts preserved non-target rows");
    malformed.afterItems[1].quantity=11;
    check(!sunrise::state::profile_currency_grant_after_image_exact(malformed),
        "reward image rejects unrelated profile mutation");
    check(sunrise::state::profile_currency_credit(100,250000,5000)==5000
        && sunrise::state::profile_currency_credit(249000,250000,5000)==1000
        && sunrise::state::profile_currency_credit(250000,250000,5000)==0,
        "reward credit saturates at installed currency cap");
    nf::leave();
    std::future<void> arming;
    {
        const nf::EquipmentMutation mutation;
        check(mutation.allowed(),"equipment transaction allowed in orbit");
        arming=std::async(std::launch::async,[]{nf::arm(813,nf::defaults(strikes::Difficulty::grandmaster));});
        check(arming.wait_for(std::chrono::milliseconds{10})==std::future_status::timeout,"launch waits for in-progress equipment commit");
    }
    arming.get();
    {
        const nf::EquipmentMutation mutation;
        check(!mutation.allowed(),"prepared equipment commit rejected after GM arms");
    }
    nf::leave(); nf::arm(835,{}); nf::enter(500,298,50);
    check(!nf::active() && !nf::movement_blocked(),"unexpected campaign destination clears GM constraints");
    sunrise::client::activity::nightfall_player::Cursor cursor{};
    nf::arm(813,{});nf::enter(600,813,60);cursor.bind(60,600);
    check(!cursor.accepts(true),"new native player lease cannot begin with a death");
    check(cursor.accepts(false),"native alive sample qualifies same-session Ghost death");
    nf::observe_local_player(600,60,false,1234);
    cursor.previous.entity=77;cursor.previous.health={21,45};
    cursor.bind(60,600);
    check(cursor.liveQualified && cursor.previous.entity==77,"same-session Ghost retains qualified body");
    nf::enter(601,813,60);cursor.bind(60,601);
    check(!cursor.liveQualified && cursor.previous.entity==UINT32_MAX && !cursor.accepts(true),
        "region transition discards old body and ignores unqualified dead body");
    check(cursor.accepts(false),"new region qualifies after native living player");
    nf::observe_local_player(601,60,false,5678);
    check(nf::progress().startedAt==1234,"region qualification preserves the run timer");
    check(cursor.accepts(true),"qualified new-region death can fail GM");
    nf::observe_local_player(601,60,true,6789);
    check(nf::failed(60) && nf::progress().deaths==1,"new-region qualified death fails exactly once");
    cursor.bind(61,601);
    check(!cursor.liveQualified && cursor.previous.entity==UINT32_MAX,"new run clears same-session health lease");
    nf::rewards::clear();
    for(std::uint64_t index=0;index<16;++index)
        check(nf::rewards::offer(700+index,700+index,strikes::Difficulty::adept),"reward mailbox fill");
    nf::leave();nf::arm(808,{});nf::enter(900,808,900);nf::complete(900,8000);
    check(nf::progress().outcome==nf::Outcome::completed && !nf::rewards::claim(900,reward),
        "full mailbox retains completed terminal without untracked grant");
    check(nf::rewards::claim(700,reward) && nf::rewards::finish(reward,1000),
        "one earlier delivery frees mailbox row");
    nf::complete(900,9000);
    check(nf::progress().finishedAt==8000 && nf::rewards::claim(900,reward),
        "repeated completed publication retries debt without changing terminal time");
    nf::leave();
    std::cout << "nightfall rules PASS " << checks << '\n';
}
