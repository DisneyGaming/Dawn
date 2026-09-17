#pragma once
#include "launchpad_inventory.h"
#include "../../../../../state/activity/Newlight/launchpad/tower.h"

namespace dawn::server::bap::encrypted::push::activity::newlight_quest {
inline bool consume(Session& session,Scratch& scratch,std::span<std::byte> response,
    std::size_t& written,bool& touchesScratch) noexcept {
    namespace q=state::activity::newlight::launchpad::quest;
    namespace t=state::activity::newlight::launchpad::tower;
    if(!session.authenticated || !session.queuez.family4Active || session.activity.joinedForeignSession
        || !lifecycle::authentication_key_is_current(session) || !queuez::valid(session.queuez)) {return false;}
    const auto account=state::account_snapshot();
    if(session.queuez.family4RootSoid!=account.primarySoid) {return false;}
    const state::CharacterState* character{};
    for(std::size_t i=0;i<account.characterCount;++i) if(account.characters[i].selected) {character=&account.characters[i];break;}
    if(!character) {return false;}
    const auto step=q::step(*character);const auto tower=t::state();
    const auto run=state::activity::mission_run_generation();
    const bool escaped=step==0 && run && tower.run==run && tower.origin.valid()
        && tower.phase!=t::Phase::idle && tower.phase!=t::Phase::failed;
    const bool arrived=step==1 && run && q::towerRun.load()==run
        && state::activity::world_phase()==state::activity::WorldPhase::arrived;
    if(!escaped && !arrived) {return false;}
    auto outcome=std::make_unique<ServiceOutcome>();auto& tx=outcome->transaction.emplace<NewlightQuestTransaction>();
    if(!state::prepare_newlight_quest(static_cast<std::uint8_t>(step),tx.pending)
        || !queuez::stage_newlight_quest(session.queuez,tx.pending,tx.update)) {return false;}
    touchesScratch=true;std::size_t size{};auto nonce=session.sendNonce;queuez::StagedPublication publication{};
    if(!queuez::stage_service_outcome(scratch,session.queuez,*outcome,state::bap().sessionKey,nonce,scratch.framed,size,publication)
        || !publication.hasState || !size || size>response.size()
        || !state::commit_newlight_quest(tx.pending)) {return false;}
    std::copy_n(scratch.framed.begin(),size,response.begin());written=size;
    session.sendNonce=nonce;session.queuez=publication.after;session.accountMutationPublished=true;
    core::log::write(core::log::Channel::server,core::log::Level::info,
        escaped?"ev=newlight stage=quest result=escaped_cosmodrome":"ev=newlight stage=quest result=tower_arrived");
    return true;
}
}
