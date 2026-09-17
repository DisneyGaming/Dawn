#include "lost_sector_rewards.h"
#include "lost_sector_reward_claim_slots.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <mutex>

#include "../../../../core/logging/log.h"
#include "../../../runtime/activity/lost_sector_reward_items.h"
#include "../../../../state/runtime/runtime.h"
#include "../../../runtime/activity/native_activity_runtime.h"
#include "../../../runtime/activity/lost_sector_reward_policy.h"
#include "../queuez/queuez_outcome_staging.h"
#include "../queuez/queuez_state_validation.h"

namespace sunrise::server::bap::encrypted::lost_sector_rewards {
namespace {
namespace rewards=server::runtime::activity::lost_sector::reward_items;
struct Claim final {
    state::activity::ActivityInstanceKey activity{};
    std::uint64_t account{},character{},boot{},seed{},retryAt{};
    std::uint32_t registry{},generation{};
    std::uint16_t slot{},sector{UINT16_MAX};
    std::uint8_t bubble{};
    bool occupied{},delivered{};
};
std::array<Claim,32> claims{};
std::mutex claimsMutex;
constexpr std::uint64_t kRetryMilliseconds=1000;

void report(const char* stage,const Claim& claim) noexcept {
    std::array<char,320> line{};
    const auto size=std::snprintf(line.data(),line.size(),
        "ev=lost_sector_chest stage=%s owner=%016llX incarnation=%llu sector=%u "
        "boot=%016llX registry=%08X slot=%u bubble=%u generation=%u account=%016llX character=%016llX",
        stage,claim.activity.sessionId,claim.activity.incarnation.value,claim.sector,
        claim.boot,claim.registry,claim.slot,claim.bubble,claim.generation,claim.account,claim.character);
    if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(
        core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
}
}

void receive(const Session& session,state::activity::ActivityInstanceKey owner,
    std::uint32_t bubble,std::uint32_t registry,std::uint8_t type,std::uint16_t slot,
    const middleware::bap::activity_message::object_sense::Output& output) noexcept {
    if(!session.authenticated || !lifecycle::activity_binding_is_current(session)
        || !session.activity.lineage || session.activity.lineage.source!=owner
        || session.activity.lineage.bound!=session.activity.instance || !state::activity::contains(owner)
        || output.generation<=0)return;
    const auto generation=static_cast<std::uint32_t>(output.generation);
    const auto ticket=server::runtime::activity::native_activity::lost_sector_reward(
        owner,registry,slot,generation);
    if(!server::runtime::activity::lost_sector::reward::accepted_use(
            ticket,bubble,registry,type,slot,output))return;
    const auto account=state::account_snapshot();
    const state::CharacterState* character{};
    for(std::size_t i=0;i<account.characterCount;++i)
        if(account.characters[i].selected) {character=&account.characters[i];break;}
    if(!account.primarySoid || !character)return;
    std::lock_guard claimsLock(claimsMutex);
    const auto claimSlot=detail::select_claim_slot(claims,
        [](const Claim& claim) noexcept {return claim.occupied;},
        [](const Claim& claim) noexcept {
            return claim.delivered && !server::runtime::activity::native_activity::lost_sector_reward_current(
                claim.activity,claim.registry,claim.slot,claim.generation);
        },
        [&](const Claim& claim) noexcept {
            return claim.activity==owner && claim.sector==ticket.sector
            && claim.boot==ticket.boot && claim.generation==generation
            && claim.account==account.primarySoid;
        });
    if(claimSlot==detail::kNoClaimSlot || claimSlot==detail::kDuplicateClaimSlot)return;
    auto& available=claims[claimSlot];
    const auto seed=owner.sessionId^(owner.incarnation.value<<32)^ticket.boot^registry
        ^(std::uint64_t(generation)<<16)^character->soid;
    available={owner,account.primarySoid,character->soid,ticket.boot,seed,0,registry,generation,slot,
        ticket.sector,ticket.bubble,true,false};
    report("queued",available);
}

bool consume(Session& session,Scratch& scratch,std::span<std::byte> response,
    std::size_t& written,bool& touchesScratch) noexcept {
    if(!session.authenticated || !session.queuez.family4Active
        || !lifecycle::authentication_key_is_current(session) || !queuez::valid(session.queuez))return false;
    std::lock_guard claimsLock(claimsMutex);
    const auto now=GetTickCount64();
    for(auto& claim:claims) {
        if(!claim.occupied || claim.delivered || now<claim.retryAt
            || claim.account!=session.queuez.family4RootSoid)continue;
        const auto account=state::account_snapshot();
        const state::CharacterState* character{};
        for(std::size_t i=0;i<account.characterCount;++i)
            if(account.characters[i].selected && account.characters[i].soid==claim.character) {
                character=&account.characters[i];break;
            }
        if(account.primarySoid!=claim.account || !character)continue;
        bool resident{};
        for(std::size_t i=0;i<session.queuez.family4ResidentCount;++i)
            if(session.queuez.family4Residents[i].objectSoid==claim.character) {resident=true;break;}
        if(!resident)continue;
        const auto reward=rewards::world(claim.seed,static_cast<std::uint8_t>(character->characterClass));
        if(!reward.item)return false;
        claim.retryAt=now+kRetryMilliseconds;
        touchesScratch=true;std::size_t size{};auto nonce=session.sendNonce;
        auto outcome=std::make_unique<ServiceOutcome>();
        auto& transaction=outcome->transaction.emplace<ItemAcquisitionTransaction>();
        if(!state::prepare_item_acquisition(reward.collectible,reward.item,transaction.pending,
                state::AcquisitionSource::missionReward,500)
            || transaction.pending.accountSoid!=claim.account
            || transaction.pending.characterSoid!=claim.character
            || !queuez::stage_item_acquisition(session.queuez,transaction.pending.accountSoid,
                transaction.pending.characterSoid,transaction.pending.acquiredInstanceSoid,
                transaction.pending.profileChanged,transaction.update,transaction.pending.removedInstanceSoid)) {
            report("retry_prepare",claim);continue;
        }
        queuez::StagedPublication publication{};
        if(!queuez::stage_service_outcome(scratch,session.queuez,*outcome,state::bap().sessionKey,
                nonce,scratch.framed,size,publication)
            || !publication.hasState || !size || size>response.size()
            || !state::commit_item_acquisition(transaction.pending)) {
            report("retry_publish",claim);continue;
        }
        std::copy_n(scratch.framed.begin(),size,response.begin());written=size;
        session.sendNonce=nonce;session.queuez=publication.after;session.accountMutationPublished=true;
        claim.delivered=true;claim.retryAt=0;report("granted",claim);return true;
    }
    return false;
}

} // namespace sunrise::server::bap::encrypted::lost_sector_rewards
