#pragma once
#include <Windows.h>
#include <algorithm>
#include <memory>
#include "../../internal.h"
#include "../../queuez/queuez_state_validation.h"
#include "../../queuez/queuez_outcome_staging.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../core/logging/log.h"
#include "newlight_quest.h"

namespace dawn::server::bap::encrypted::push::activity::quest_progress {
inline bool consume(Session& session,Scratch& scratch,std::span<std::byte> response,
    std::size_t& written,bool& touchesScratch) noexcept {
    if(!session.authenticated || !session.queuez.family4Active || session.activity.joinedForeignSession
        || !lifecycle::authentication_key_is_current(session) || !queuez::valid(session.queuez)) {return false;}
    const auto now=GetTickCount64();
    if(now<session.questProgressDueTick) {return false;}session.questProgressDueTick=now+500;
    auto outcome=std::make_unique<ServiceOutcome>();auto& tx=outcome->transaction.emplace<VendorServiceTransaction>();
    if(!state::vendors::prepare_progress(tx.pending)
        || !queuez::stage_vendor_transaction(session.queuez,tx.pending,tx.update)) {return false;}
    touchesScratch=true;std::size_t size{};auto nonce=session.sendNonce;queuez::StagedPublication publication{};
    if(!queuez::stage_service_outcome(scratch,session.queuez,*outcome,state::bap().sessionKey,nonce,scratch.framed,size,publication)
        || !publication.hasState || !size || size>response.size() || !state::vendors::commit(tx.pending)) {return false;}
    std::copy_n(scratch.framed.begin(),size,response.begin());written=size;
    session.sendNonce=nonce;session.queuez=publication.after;session.accountMutationPublished=true;
    core::log::write(core::log::Channel::server,core::log::Level::info,"ev=quest stage=advance result=published");
    return true;
}
}
