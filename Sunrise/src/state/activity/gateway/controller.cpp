#include "controller.h"
#include "ai_bindings.h"
namespace sunrise::state::activity::gateway {
namespace {
bool same(const coo::CommandSpec& a,const coo::CommandSpec& b) noexcept {
    return a.operation==b.operation && a.asset==b.asset && a.argument==b.argument && a.wait==b.wait;
}
}
bool valid_document(const coo::script::Views& views) noexcept {
    if(!views.valid || views.missionId!="gateway" || views.profileId!=kProfile.id
        || views.graphs.size()!=3 || views.mission.modules.size()!=1
        || views.mission.modules[0].asset!=kModule || views.mission.modules[0].id!=1) { return false; }
    const auto* root=views.role("mission");
    if(!root || root->definition.steps.data()!=views.mission.sequence.steps.data()
        || root->domain!="composition" || root->commands.size()!=2) { return false; }
    for(const auto domain:{std::string_view{"opening"},std::string_view{"ending"}}) {
        const auto* graph=views.role(domain);
        const auto contract=domain=="opening"?std::span<const ContractStep>{kContract}:std::span<const ContractStep>{kEndingContract};
        if(!graph || graph->domain!=domain || graph->definition.steps.size()!=contract.size()) { return false; }
        std::size_t commandCount{};
        for(std::size_t i=0;i<contract.size();++i) {
            const auto& expected=contract[i];const auto& step=graph->definition.steps[i];
            if(step.dependencies!=expected.dependencies || step.commands.size()!=expected.commands.size()) { return false; }
            commandCount+=expected.commands.size();
            for(std::size_t n=0;n<expected.commands.size();++n) {
                unsigned found{};
                for(const auto& command:graph->commands) { found+=command.step==i && command.command==n && command.capability==expected.commands[n]; }
                if(found!=1) { return false; }
            }
        }
        if(graph->commands.size()!=commandCount) { return false; }
    }
    for(const auto& capability:kCapabilities) {
        const auto* graph=capability.domain=="composition"?root:views.role(capability.domain);
        if(!graph) { return false; }
        unsigned found{};
        for(const auto& command:graph->commands) {
            if(command.capability==capability.id) {
                const auto& spec=graph->definition.steps[command.step].commands[command.command];
                if(!same(spec,capability.spec) && !(capability.argumentMaximum && spec.operation==coo::Operation::eventAfter
                    && spec.asset==capability.spec.asset && spec.wait==capability.spec.wait && spec.argument>0 && spec.argument<=capability.argumentMaximum)) { return false; }
                ++found;
            }
        }
        if(found!=1) { return false; }
    }
    return true;
}

void Controller::reset() noexcept {
    executor_.cancel(*this); composition_.reset(); views_=nullptr;run_=0;now_=0;
    started_=false;landingSeen_=false;dialogueSubmitted_.reset();dialogue_={};frame_={};seen_.reset();population_={};prepared_=0;returnContact_=false;destructible_={};scene_={};returnCue_.reset();objects_={};objectives_={};lifecycle_.reset();vanceRequested_=false;ascentDelay_=finishDelay_=0;
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
        || !returnCue_.bind(lifecycle_.owner())) { reset();return false; }
    for(const auto& binding:views.role("ending")->commands) {
        const auto delay=views.role("ending")->definition.steps[binding.step].commands[binding.command].argument;
        if(binding.capability=="vance.ascent_cue") { ascentDelay_=delay; }
        if(binding.capability=="vance.ending_cue") { finishDelay_=delay; }
    }
    if(finishDelay_<ascentDelay_) { reset();return false; }frame_.services=true;return true;
}
void Controller::position(std::uint64_t run,Point point) noexcept {
    if(run!=run_ || !views_) { return; }
    // Only the verified opening can start this profile. A far spawn cannot arm it.
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(kVolumes[i].registry==kLanding.registry && kVolumes[i].slot==kLanding.slot
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
    // Row 5 includes Ghost before Vance. Only the authenticated native submission
    // starts this clock; queueing the exchange must not advance the encounter.
    if(row==5) { static_cast<void>(returnCue_.mark(lifecycle_.owner(),0,now)); }
    // Turn with the actual first line, even if earlier dialogue delayed its dispatch.
    if(row==10 && frame_.lighthouseOpen && entered({0xBA0B27A0U,0x80F46DCDU,60,11})) {
        static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::greetingSubmitted,now));project_services();
    }
    return true;
}
bool Controller::publish(const coo::Command& command) noexcept {
    if(command.token.run!=run_ || command.schema!=coo::Schema::otherMissions) { return false; }
    bool registered{};
    for(const auto& step:views_->role(frame_.section==0?"opening":"ending")->definition.steps) {
        for(const auto& spec:step.commands) { registered|=same(spec,command.spec); }
    }
    if(!registered) { return false; }
    const auto& spec=command.spec;
    if(spec.operation==coo::Operation::objective) {
        dialogue_.objective(views_->dialogue,spec.argument,frame_,frame_.revision);
        coo::MarkerTarget marker{};for(const auto& binding:views_->markers) { if(binding.event==spec.argument) { marker=binding.target; } }
        objectives_.set(spec.argument,marker);
    } else if(spec.operation==coo::Operation::dialogue) {
        dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(spec.argument),now_,0,0,frame_.revision);
    }
    else if(spec.operation==coo::Operation::eventAfter && spec.asset==kDialogueAsset) { frame_.returnCuePending=true; }
    else if(spec.operation==coo::Operation::population) { enable(spec.argument); }
    else if(spec.operation==coo::Operation::mechanic && spec.argument==10) { enable(0);frame_.marchers=true; }
    else if(spec.operation==coo::Operation::scene) {
        if(!frame_.lighthouseOpen || !frame_.vanceEntered || !entered({0xBA0B27A0U,0x80F46DCDU,60,13})) { return false; }
        vanceRequested_=true;
    }
    else if(spec.operation==coo::Operation::mechanic) {
        if(spec.argument==20) { destructible_.expose();frame_.moduleVulnerable=true; }
        else if(spec.argument==21 && frame_.moduleDestroyed) { frame_.lighthouseOpen=true; }
        else { return false; }
    }
    else if(spec.operation==coo::Operation::complete) {
        if(spec.argument!=6 || !scene_.seen(coo::SceneSignal::conversationFinished) || frame_.lighthouseChannels!=3
            || !lifecycle_.complete(lifecycle_.owner())) { return false; }
        objectives_.clear();frame_.finished=true;
    }
    else if(spec.operation==coo::Operation::device) {
        if(spec.asset.registry==0xBA0B27A0U && spec.asset.type==23 && spec.asset.slot<=1) {
            if(!scene_.after(coo::SceneSignal::conversationStarted,now_,ascentDelay_)) { return false; }
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
    if(source && source->cohort==9) { returnContact_=true; }
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

coo::StallDetail Controller::missing(const coo::CommandSpec& spec) const noexcept {
    using coo::Missing;
    if(spec.wait==coo::Wait::requested) { return {}; }
    if(spec.operation==coo::Operation::population) {
        for(std::size_t i=0;i<kSpawns.size();++i) {
            const auto& source=kSpawns[i];if(source.cohort!=spec.argument) { continue; }
            auto detail=population_.missing(i,source.count,true);
            if(detail.missing!=Missing::none) { detail.asset={source.registry,source.definition,1,source.source};return detail; }
        }return {};
    }
    if(spec.operation==coo::Operation::eventAfter) {
        if(spec.asset==kDialogueAsset) {
            if(!returnCue_.seen(0)) { return {Missing::eventOrigin,spec.asset}; }
            return returnCue_.elapsed(0,now_,spec.argument)?coo::StallDetail{}:coo::StallDetail{Missing::timer,spec.asset,spec.argument};
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
    // Retain the second event only after the native turn and the greeting audio finish.
    if(dialogueSubmitted_[10] && frame_.lighthouseOpen && entered({0xBA0B27A0U,0x80F46DCDU,60,11})) { static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::greetingSubmitted,now_)); }
    if(vanceRequested_) { static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::approached,now_)); }
    if(dialogueSubmitted_[9] && dialogueSubmitted_[10] && now_>=dialogue_.voice_until()) { static_cast<void>(scene_.signal(scene_.owner(),coo::SceneSignal::prerollFinished,now_)); }
    scene_.update(now_,finishDelay_);project_services();
    const auto& graph=*views_->role(frame_.section==0?"opening":"ending");
    if(!started_) { started_=executor_.start(graph.definition,run_); }
    if(!started_) { return; }
    executor_.update(*this);
    for(const auto& binding:graph.commands) {
        const auto& spec=graph.definition.steps[binding.step].commands[binding.command];
        const auto state=executor_.step_state(binding.step);
        if(state.phase!=coo::StepPhase::active || !state.commands[binding.command].requested) { continue; }
        const coo::Token token{run_,executor_.diagnostics().incarnation,binding.step,binding.command};
        // Forward passage also releases the shelf reinforcement trigger if its smaller
        // volume was missed. Route observations never create enemy-death receipts.
        const bool shelfExit=spec.operation==coo::Operation::observation && spec.argument==3U
            && entered({0x85742F3EU,0x80F470E5U,60,375});
        const bool vanceCue=spec.operation==coo::Operation::eventAfter;
        const bool cueElapsed=spec.asset==kDialogueAsset?returnCue_.elapsed(0,now_,spec.argument)
            :scene_.after(coo::SceneSignal::conversationStarted,now_,spec.argument);
        const bool vanceApproach=spec.operation==coo::Operation::observation
            && spec.asset.registry==0xBA0B27A0U && spec.asset.slot==13;
        const bool observed=vanceApproach?(frame_.vanceTurned && entered(spec.asset)):vanceCue?cueElapsed:spec.argument==513U?frame_.moduleDestroyed
            :spec.argument==512U?(returnContact_ || entered(spec.asset))
            :(shelfExit || entered(spec.asset) || (spec.argument==256U?cleared(3)&&cleared(4):cleared(spec.argument)));
        if(coo::is_observation(spec.operation) && observed) {
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
    // The final opening step contains only a requested objective. Retire that
    // publication now so the return graph can share the cue's outgoing frame.
    if(frame_.section==0 && executor_.step_state(std::size(kContract)-1).phase==coo::StepPhase::active) { executor_.update(*this); }
    dialogue_.advance(views_->dialogue,publicationGeneration_-1U,now_,false,frame_,frame_.revision);
    frame_.enabled=executor_.diagnostics().phase!=coo::Phase::failed;
    if(executor_.diagnostics().phase==coo::Phase::complete && frame_.section==0) {
        frame_.openingChecked=true;frame_.section=1;
        // Outbound Lighthouse visits cannot trigger the return encounter.
        seen_.reset();executor_.cancel(*this);
        // Publish return cohorts and their objective in this same cue frame.
        // Deferring section startup to the next normal packet adds up to five seconds.
        started_=executor_.start(views_->role("ending")->definition,run_);
        if(started_) { executor_.update(*this); }
        frame_.returnCuePending=false;
    }
    frame_.checked=frame_.finished && frame_.conversationStarted;
    frame_.preparedMask=prepared_;
    project_services();output=frame_;
    output.cannons=frame_.cannons && (prepared_&3)==3;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!views_ || run!=run_ || !ready) { return {}; }
    return composition_.update(views_->mission,{run,now,kRegion,false,true},*this);
}
} // namespace sunrise::state::activity::gateway
