#pragma once
#include <Windows.h>
#include "../../internal.h"
#include "../../queuez/queuez_state_validation.h"
#include "../../queuez/queuez_outcome_staging.h"
#include "../../../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../../../state/activity/Newlight/launchpad/rewards.h"
#include "../../../../../state/runtime/runtime.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../core/logging/log.h"
#include <memory>
#include <cstdio>

namespace dawn::server::bap::encrypted::push::activity::launchpad_inventory {
namespace mission=state::activity::newlight::launchpad;
inline bool consume(Session& session,Scratch& scratch,std::span<std::byte> response,
    std::size_t& written,bool& touchesScratch) noexcept {
    const auto requested=mission::grant_request();
    if(!requested.owner.valid() || requested.pickup>=std::size(mission::kPickups) || !session.authenticated
        || session.activity.joinedForeignSession || !session.queuez.family4Active
        || !lifecycle::authentication_key_is_current(session) || !queuez::valid(session.queuez)) {return false;}
    // Activity and inventory subscribe on separate BAP connections. The
    // mission owns the accepted-use lease; this authenticated Family-4 peer
    // owns delivery to the selected character. It needs no activity binding.
    const auto account=state::account_snapshot();
    if(session.queuez.family4RootSoid!=account.primarySoid) {return false;}
    const state::CharacterState* character{};
    for(std::size_t i=0;i<account.characterCount;++i) {if(account.characters[i].selected) {character=&account.characters[i];break;}}
    if(!character) {return false;}
    const bool worldDrop=requested.pickup==3;
    const auto reward=worldDrop?mission::rewards::world(requested.seed^character->soid,static_cast<std::uint8_t>(character->characterClass))
        :mission::rewards::kTutorial[requested.pickup];
    if(!reward.item) {return false;}
    bool resident{};
    for(std::size_t i=0;i<session.queuez.family4ResidentCount;++i)
        if(session.queuez.family4Residents[i].objectSoid==character->soid) {resident=true;break;}
    if(!resident) {return false;}
    static std::uint64_t next{},run{};const auto now=GetTickCount64();
    if(run==requested.owner.run && now<next) {return false;}run=requested.owner.run;next=now+250;
    // Replaying the tutorial does not duplicate a weapon the selected Guardian already owns.
    if(!worldDrop) for(const auto& equipped:character->equipment.slots) {
        if(equipped && equipped->definitionHash==reward.item) {
            static_cast<void>(mission::observe_granted(requested,equipped->instanceSoid));return false;
        }
    }
    std::uint64_t owned{};
    if(!worldDrop) for(std::size_t i=0;i<character->inventory.count;++i) {
        const auto& item=character->inventory.values[i];if(item.definitionHash==reward.item) {owned=item.instanceSoid;break;}
    }
    touchesScratch=true;std::size_t size{};auto nonce=session.sendNonce;const auto& key=state::bap().sessionKey;
    auto outcome=std::make_unique<ServiceOutcome>();
    if(owned) {
        auto& tx=outcome->transaction.emplace<EquipmentSwapTransaction>();
        if(!state::prepare_equipment_swap(owned,tx.pending)
            || !queuez::stage_equipment_swap(session.queuez,tx.pending.characterSoid,tx.update)) {return false;}
    } else {
        auto& tx=outcome->transaction.emplace<ItemAcquisitionTransaction>();
        if(!state::prepare_item_acquisition(reward.collectible,reward.item,tx.pending,state::AcquisitionSource::missionReward,worldDrop?500:0)
            || !queuez::stage_item_acquisition(session.queuez,tx.pending.accountSoid,tx.pending.characterSoid,
                tx.pending.acquiredInstanceSoid,tx.pending.profileChanged,tx.update,tx.pending.removedInstanceSoid)) {return false;}
    }
    queuez::StagedPublication publication{};
    const auto instance=owned?owned:std::get<ItemAcquisitionTransaction>(outcome->transaction).pending.acquiredInstanceSoid;
    // Reuse the whole transaction publisher: equipment also updates resident Family-0/3
    // appearances. Every frame must fit before either inventory or mission State commits.
    if(!queuez::stage_service_outcome(scratch,session.queuez,*outcome,key,nonce,scratch.framed,size,publication)
        || !publication.hasState || !size || size>response.size()
        || !mission::commit_grant(requested,instance,worldDrop || owned!=0,outcome.get(),[](void* p) noexcept {
            auto& tx=*static_cast<ServiceOutcome*>(p);
            if(auto* equip=transaction_if<EquipmentSwapTransaction>(tx)) {return state::commit_equipment_swap(equip->pending);}
            auto* acquire=transaction_if<ItemAcquisitionTransaction>(tx);
            return acquire && state::commit_item_acquisition(acquire->pending);
        })) {return false;}
    std::copy_n(scratch.framed.begin(),size,response.begin());written=size;
    session.sendNonce=nonce;session.queuez=publication.after;session.accountMutationPublished=true;
    std::array<char,160> line{};
    std::snprintf(line.data(),line.size(),"ev=launchpad stage=weapon_grant pickup=%u action=%s connection=%u bytes=%zu",
        requested.pickup,owned?"equipped":"acquired",session.id,size);
    core::log::write(core::log::Channel::server,core::log::Level::info,line.data());
    return true;
}
}
