#include "controller.h"
#include "ai_bindings.h"
namespace sunrise::state::activity::gateway {
bool valid_document(const coo::script::Views& views) noexcept {
    if(!views.valid || views.missionId!="gateway" || views.profileId!=kProfile.id || views.phases.empty()
        || views.mission.modules.size()!=1 || views.mission.modules[0].asset!=kModule
        || views.mission.modules[0].id!=1) { return false; }
    return coo::script::authorized(views,kProfile);
}

void Controller::reset() noexcept {
    forestXReached_=hydraDefeated_=false;
    executor_.cancel(*this); composition_.reset(); views_=nullptr;run_=0;now_=0;
    started_=false;landingSeen_=false;dialogueSubmitted_.reset();dialogue_={};frame_={};seen_.reset();population_={};prepared_=0;deathCohorts_.reset();destructible_={};scene_={};dialogueClock_.reset();objects_={};objectives_={};lifecycle_.reset();vanceRequested_=greetingRequested_=false;voiceEnds_={};
}
bool Controller::select(const coo::script::Views& views,std::uint64_t run) noexcept {
    if(run==0 || !valid_document(views)) { reset();return false; }
    if(run_==run) { return views_==&views; }
    reset();
    // Reserve a second generation for the module after its inactive preparation.
    // Native 9F2F30 requires requested > committed, even when the entity is gone.
    // Lighthouse position revisions are signed 16-bit values retained by native devices.
    if(!lifecycle_.begin(run)) { return false; }
    publicationGeneration_=lifecycle_.owner().value;views_=&views;run_=run;frame_.spawnGeneration=publicationGeneration_;
    if(!objects_.begin(lifecycle_.owner(),kEndingObjects) || !scene_.preload(run,publicationGeneration_,kSceneEvents)
        || !dialogueClock_.bind(lifecycle_.owner())) { reset();return false; }
    frame_.services=true;return true;
}
void Controller::position(std::uint64_t run,Point point) noexcept {
    if(run!=run_ || !views_) { return; }
    if(std::isfinite(point.x) && point.x>=266.F) forestXReached_=true;
    if(!views_->observationStart) { landingSeen_=true; }
    for(std::size_t i=0;i<std::size(kVolumes) && !landingSeen_;++i) {
        const auto& asset=views_->observationStart->asset;
        if(kVolumes[i].registry==asset.registry && kVolumes[i].slot==asset.slot
            && contains(kVolumes[i],point)) { landingSeen_=true; }
    }
    if(!landingSeen_) { return; }
    for(std::size_t i=0;i<std::size(kVolumes);++i) { if(contains(kVolumes[i],point)) { seen_.set(i); } }
}
bool Controller::entered(coo::Asset asset) const noexcept {
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(kVolumes[i].registry==asset.registry && kVolumes[i].slot==asset.slot) { return seen_[i]; }
    }
    return false;
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,
    std::uint32_t generation,std::uint64_t now) noexcept {
    if(!views_ || run!=run_ || !started_) { return false; }
    if(!dialogue_.submitted(views_->dialogue,bank,row,generation,now,frame_,frame_.revision)) { return false; }
    dialogueSubmitted_.set(row);
    static_cast<void>(dialogueClock_.mark(lifecycle_.owner(),row,now));
    voiceEnds_[row]=dialogue_.voice_until();
    return true;
}
bool Controller::publish(const coo::Command& command) noexcept {
    if(!views_ || !graph() || !coo::script::valid_token(graph()->definition,executor_,command)) { return false; }
    const auto& spec=command.spec;
    if(spec.operation==coo::Operation::objective) {
        dialogue_.objective(views_->dialogue,spec.argument,frame_,frame_.revision);
        coo::MarkerTarget marker{};for(const auto& binding:views_->markers) { if(binding.event==spec.argument) { marker=binding.target; } }
        objectives_.set(spec.argument,marker);
    } else if(spec.operation==coo::Operation::dialogue) {
        dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(spec.argument),now_,0,0,frame_.revision);
    }
    else if(spec.operation==coo::Operation::eventAfter && spec.asset.definition==kDialogueAsset.definition) { frame_.returnCuePending=true; }
    else if(spec.operation==coo::Operation::population) { enable(spec.argument); }
    else if(spec.operation==coo::Operation::mechanic && spec.argument==10) { enable(0);frame_.marchers=true; }
    else if(spec.operation==coo::Operation::scene) {
        vanceRequested_=true;
    }
    else if(spec.operation==coo::Operation::mechanic) {
        if(spec.asset==kVanceScene) { greetingRequested_=true; }
        else if(spec.argument==30) { seen_.reset();frame_.returnCuePending=false; }
        else if(spec.argument==20) { destructible_.expose();frame_.moduleVulnerable=true; }
        else if(spec.argument==21) { frame_.lighthouseOpen=true; }
        else { return false; }
    }
    else if(spec.operation==coo::Operation::complete) {
        if(!lifecycle_.complete(lifecycle_.owner())) { return false; }
        objectives_.clear();frame_.finished=true;
    }
    else if(spec.operation==coo::Operation::device) {
        if(spec.asset.registry==0xBA0B27A0U && spec.asset.type==23 && spec.asset.slot<=1) {
            frame_.lighthouseChannels|=static_cast<std::uint8_t>(1U<<spec.asset.slot);
        } else if(spec.argument==1) { frame_.cannons=true; } else if(spec.argument==2) { frame_.finalCannon=true; }
    }
    return true;
}
void Controller::enable(std::uint32_t cohort) noexcept {
    if(cohort>kLastCohort) { return; }
    frame_.cohorts|=1U<<cohort;
    for(std::size_t i=0;i<kSpawns.size();++i) { if(kSpawns[i].cohort==cohort) {
        const auto task=tactical_group(kSpawns[i]);population_.policy(i,{true,coo::EnemyIntent::combat,task.registry,task.slot,task.row});population_.enable(i);
    } }
}
bool Controller::ready(std::uint32_t cohort) const noexcept {
    if(cohort==0 || cohort>kLastCohort || frame_.populationFault) { return false; }
    for(std::size_t i=0;i<kSpawns.size();++i) {
        if(kSpawns[i].cohort==cohort && !population_.ready(i,kSpawns[i].count)) { return false; }
    }
    return true;
}
bool Controller::cleared(std::uint32_t cohort) const noexcept {
    if(cohort==0 || cohort>kLastCohort || frame_.populationFault) { return false; }
    for(std::size_t i=0;i<kSpawns.size();++i) {
        if(kSpawns[i].cohort==cohort && !population_.cleared(i,kSpawns[i].count)) { return false; }
    }
    return true;
}
bool Controller::admitted(const EnemyReceipt& receipt) noexcept {
    if(!views_ || !frame_.enabled || frame_.populationFault) { return false; }
    const auto result=population_.admit(kSpawns,receipt,run_,publicationGeneration_);
    if(result==coo::Admission::overflow) { frame_.populationFault=true; }
    return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& receipt) noexcept {
    if(!views_ || !frame_.enabled || !population_.died(receipt,run_,publicationGeneration_)) { return false; }
    const auto* source=spawn(receipt.registry,receipt.source);
    if(receipt.registry==kMainlandRegistry && receipt.source==49) hydraDefeated_=true;
    if(source && source->cohort<deathCohorts_.size()) { deathCohorts_.set(source->cohort); }
    return true;
}
bool Controller::prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index) noexcept {
    if(!views_ || !frame_.enabled || run!=run_ || generation!=publicationGeneration_ || index>6 || (prepared_&(1U<<index))) { return false; }
    prepared_|=static_cast<std::uint8_t>(1U<<index);
    for(std::size_t i=0;i<std::size(kObjectPreparation);++i) { if(kObjectPreparation[i]==index) { static_cast<void>(objects_.prepared(lifecycle_.owner(),i)); } }
    return true;
}
bool Controller::module(const ModuleReceipt& receipt,bool dead) noexcept {
    if(!views_ || !frame_.enabled || frame_.moduleDestroyed || (prepared_&(1U<<5))==0 || !receipt.valid() || receipt.run!=run_ || receipt.generation!=publicationGeneration_+1U) { return false; }
    if(dead) {
        if(!destructible_.destroyed(receipt)) { return false; }
        frame_.moduleDestroyed=true;project_services();return true;
    }
    return destructible_.bind(receipt);
}
bool Controller::object(std::size_t index,const coo::ObjectReceipt& receipt,bool applied,float position,std::int16_t revision) noexcept {
    if(!views_ || !frame_.enabled) { return false; }
    const bool changed=objects_.observe(index,receipt,applied,position,revision);project_services();return changed;
}
bool Controller::scene(const SceneReceipt& receipt,bool completed) noexcept {
    if(!views_ || !frame_.enabled || !receipt.valid() || receipt.run!=run_ || receipt.generation!=frame_.sceneGeneration) { return false; }
    if(completed) {
        if(!frame_.vanceConversation || !scene_.signal(receipt,coo::SceneSignal::sceneFinished,now_)) { return false; }
    } else if(!scene_.bind(receipt)) { return false; }
    project_services();return true;
}
bool Controller::vance(const SceneReceipt& receipt,VanceMilestone milestone,std::uint64_t now) noexcept {
    if((milestone!=VanceMilestone::turned && milestone!=VanceMilestone::conversationStarted) || !views_ || !frame_.enabled || !frame_.vanceEntered || receipt!=scene_.owner()) { return false; }
    if(milestone==VanceMilestone::conversationStarted && (!frame_.vanceTurned || !frame_.vanceConversation)) { return false; }
    const bool accepted=scene_.signal(receipt,milestone==VanceMilestone::turned?coo::SceneSignal::animationReady:coo::SceneSignal::conversationStarted,now);
    project_services();return accepted;
}
void Controller::project_services() noexcept {
    frame_.pendingServices=false;destructible_.project(objects_,kModuleLinks);
    for(std::size_t i=0;i<frame_.objects.size();++i) { frame_.objects[i]=objects_.state(i);frame_.pendingServices|=frame_.objects[i].phase!=coo::ObjectPhase::ready && frame_.objects[i].phase!=coo::ObjectPhase::retired; }
    frame_.sceneStarted=scene_.owner().valid();frame_.sceneComplete=scene_.seen(coo::SceneSignal::sceneFinished);
    frame_.vanceEntered=scene_.event_count()>=1;frame_.vanceConversation=scene_.event_count()>=2;
    frame_.vanceTurned=scene_.seen(coo::SceneSignal::animationReady);frame_.conversationStarted=scene_.seen(coo::SceneSignal::conversationStarted);
    frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();
}

bool Controller::observed(const coo::CommandSpec& spec) const noexcept {
    if(spec.asset==kModule && spec.argument==1024) return forestXReached_;
    if(spec.asset==kModule && spec.argument==1025) return hydraDefeated_;
    if(views_ && views_->condition(spec)) { return views_->evaluate(spec,[this](const auto& native) noexcept { return observed(native); }); }
    if(spec.operation==coo::Operation::eventAfter) {
        if(spec.asset.definition==kDialogueAsset.definition) { return dialogueClock_.elapsed(spec.asset.slot,now_,spec.argument); }
        return scene_.after(coo::SceneSignal::conversationStarted,now_,spec.argument);
    }
    if(spec.asset==kModule) {
        return spec.argument>=256 ? spec.argument-256<deathCohorts_.size() && deathCohorts_[spec.argument-256] : cleared(spec.argument);
    }
    if(spec.asset==kDialogueAsset) { return spec.argument<16 && dialogueSubmitted_[spec.argument] && now_>=voiceEnds_[spec.argument]; }
    if(spec.asset==kVanceScene) { return scene_.seen(coo::SceneSignal::animationReady); }
    if(spec.asset==kEndingObjects[1].source) { return frame_.moduleDestroyed; }
    return entered(spec.asset);
}
coo::StallDetail Controller::missing(const coo::CommandSpec& spec) const noexcept {
    using coo::Missing;
    if(spec.wait==coo::Wait::requested || (coo::is_observation(spec.operation) && observed(spec))) { return {}; }
    if(views_ && views_->condition(spec)) { return {Missing::observation,spec.asset}; }
    if(spec.operation==coo::Operation::population) {
        for(std::size_t i=0;i<kSpawns.size();++i) {
            const auto& source=kSpawns[i];if(source.cohort!=spec.argument) { continue; }
            auto detail=population_.missing(i,source.count,true);
            if(detail.missing!=Missing::none) { detail.asset={source.registry,source.definition,1,source.source};return detail; }
        }return {};
    }
    if(spec.operation==coo::Operation::eventAfter) {
        if(spec.asset.definition==kDialogueAsset.definition) {
            if(!dialogueClock_.seen(spec.asset.slot)) { return {Missing::eventOrigin,spec.asset}; }
            return dialogueClock_.elapsed(spec.asset.slot,now_,spec.argument)?coo::StallDetail{}:coo::StallDetail{Missing::timer,spec.asset,spec.argument};
        }
        if(!frame_.conversationStarted) { return {Missing::eventOrigin,spec.asset}; }
        return scene_.after(coo::SceneSignal::conversationStarted,now_,spec.argument)?coo::StallDetail{}:coo::StallDetail{Missing::timer,spec.asset,spec.argument};
    }
    if(spec.operation==coo::Operation::scene) {
        return frame_.conversationStarted?coo::StallDetail{}:coo::StallDetail{!frame_.sceneStarted?Missing::sceneBinding:scene_.pending_signals()?Missing::sceneEvent:Missing::conversation,spec.asset,scene_.pending_signals()};
    }
    if(spec.operation==coo::Operation::dialogue) {
        return spec.argument<16 && dialogueSubmitted_[spec.argument]?coo::StallDetail{}:coo::StallDetail{Missing::dialogue,spec.asset,spec.argument};
    }
    if(spec.asset==kEndingObjects[1].source && !frame_.moduleDestroyed) {
        for(std::size_t i=0;i<3;++i) {
            const auto phase=objects_.state(i).phase;
            if(phase!=coo::ObjectPhase::ready && phase!=coo::ObjectPhase::retired) {
                return {phase==coo::ObjectPhase::prepare?Missing::preparation:phase==coo::ObjectPhase::create?Missing::object:phase==coo::ObjectPhase::bind?Missing::controller:Missing::device,kEndingObjects[i].source};
            }
        }
        return {Missing::death,spec.asset};
    }
    if(spec.operation==coo::Operation::device) { return {(prepared_&3)==3?Missing::none:Missing::preparation,spec.asset,3,static_cast<std::uint32_t>(prepared_&3)}; }
    return {entered(spec.asset)?Missing::none:Missing::observation,spec.asset};
}
void Controller::update_module(std::uint32_t id,const coo::MissionInput& input,Frame& output) noexcept {
    if(id!=1 || !views_ || input.run!=run_) { return; }
    now_=input.now;
    // Start the native cast/idle on world arrival, keeping this owner through the ending.
    frame_.sceneGeneration=scene_.generation();
    if(greetingRequested_) { static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::greetingSubmitted,now_)); }
    if(vanceRequested_) {
        static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::approached,now_));
        static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::prerollFinished,now_));
    }
    project_services();
    const auto& currentGraph=*graph();
    if(!started_) { started_=executor_.start(currentGraph.definition,run_); }
    if(!started_) { return; }
    executor_.update(*this);
    for(const auto& binding:currentGraph.commands) {
        const auto& spec=currentGraph.definition.steps[binding.step].commands[binding.command];
        const auto state=executor_.step_state(binding.step);
        if(state.phase!=coo::StepPhase::active || !state.commands[binding.command].requested) { continue; }
        const coo::Token token{run_,executor_.diagnostics().incarnation,binding.step,binding.command};
        if(coo::is_observation(spec.operation) && observed(spec)) {
            static_cast<void>(executor_.enqueue({token,coo::Milestone::observed}));
        } else if(spec.operation==coo::Operation::dialogue && spec.argument<16 && dialogueSubmitted_[spec.argument]) {
            static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady}));
        } else if(spec.operation==coo::Operation::population && spec.wait==coo::Wait::completed) {
            if(ready(spec.argument)) { static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady})); }
            if(cleared(spec.argument)) { static_cast<void>(executor_.enqueue({token,coo::Milestone::completed})); }
        } else if(spec.operation==coo::Operation::device && spec.wait==coo::Wait::nativeReady && (prepared_&3)==3) {
            static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady}));
        }
        if(spec.operation==coo::Operation::scene) {
            if(frame_.conversationStarted) { static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady})); }
        }
        if(frame_.populationFault) { static_cast<void>(executor_.enqueue({token,coo::Milestone::failed})); }
    }
    executor_.update(*this);
    // Drain requested publications and advance the authored phase list in the same frame.
    executor_.update(*this);
    dialogue_.advance(views_->dialogue,publicationGeneration_-1U,now_,false,frame_,frame_.revision);
    frame_.enabled=executor_.diagnostics().phase!=coo::Phase::failed;
    if(executor_.diagnostics().phase==coo::Phase::complete && std::size_t(frame_.section)+1<views_->phases.size()) {
        frame_.openingChecked=true;++frame_.section;executor_.cancel(*this);
        started_=executor_.start(graph()->definition,run_);
        if(started_) { executor_.update(*this); }
    }
    frame_.checked=frame_.finished;
    frame_.preparedMask=prepared_;
    project_services();output=frame_;
    output.cannons=frame_.cannons && (prepared_&3)==3;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!views_ || run!=run_ || !ready) { return {}; }
    return composition_.update(views_->mission,{run,now,kRegion,false,true},*this);
}
} // namespace sunrise::state::activity::gateway
