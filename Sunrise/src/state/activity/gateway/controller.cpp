#include "controller.h"
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
                if(!same(graph->definition.steps[command.step].commands[command.command],capability.spec)) { return false; }
                ++found;
            }
        }
        if(found!=1) { return false; }
    }
    return true;
}

void Controller::reset() noexcept {
    executor_.cancel(*this); composition_.reset(); views_=nullptr;run_=0;now_=0;
    started_=false;landingSeen_=false;dialogueSubmitted_.reset();dialogue_={};frame_={};seen_.reset();population_={};prepared_=0;returnContact_=false;moduleOwner_={};sceneOwner_={};vanceRequested_=false;conversationAt_=0;
}
bool Controller::select(const coo::script::Views& views,std::uint64_t run) noexcept {
    if(run==0 || !valid_document(views)) { reset();return false; }
    if(run_==run) { return views_==&views; }
    reset();
    // Reserve a second generation for the module after its inactive preparation.
    // Native 9F2F30 requires requested > committed, even when the entity is gone.
    // Lighthouse position revisions are signed 16-bit values retained by native devices.
    if(publicationGeneration_>=32764U) { return false; }
    publicationGeneration_=publicationGeneration_?publicationGeneration_+2U:1U;views_=&views;run_=run;frame_.spawnGeneration=publicationGeneration_;return true;
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
    // Turn with the actual first line, even if earlier dialogue delayed its dispatch.
    if(row==10 && frame_.lighthouseOpen && entered({0xBA0B27A0U,0x80F46DCDU,60,11})) { frame_.vanceEntered=true; }
    return true;
}
bool Controller::publish(const coo::Command& command) noexcept {
    if(command.token.run!=run_ || command.schema!=coo::Schema::otherMissions) { return false; }
    bool registered{};
    for(const auto& cap:kCapabilities) { registered|=cap.domain==(frame_.section==0?"opening":"ending") && same(cap.spec,command.spec); }
    if(!registered) { return false; }
    const auto& spec=command.spec;
    if(spec.operation==coo::Operation::objective) {
        dialogue_.objective(views_->dialogue,spec.argument,frame_,frame_.revision);
    } else if(spec.operation==coo::Operation::dialogue) {
        dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(spec.argument),now_,0,0,frame_.revision);
    }
    else if(spec.operation==coo::Operation::population) { enable(spec.argument); }
    else if(spec.operation==coo::Operation::mechanic && spec.argument==10) { enable(0);frame_.marchers=true; }
    else if(spec.operation==coo::Operation::scene) {
        if(!frame_.lighthouseOpen || !frame_.vanceEntered || !entered({0xBA0B27A0U,0x80F46DCDU,60,13})) { return false; }
        vanceRequested_=true;
    }
    else if(spec.operation==coo::Operation::mechanic) {
        if(spec.argument==20) { frame_.moduleVulnerable=true; }
        else if(spec.argument==21 && frame_.moduleDestroyed) { frame_.lighthouseOpen=true; }
        else if(spec.argument==30 && frame_.conversationStarted && frame_.lighthouseChannels==3
            && now_>=conversationAt_ && now_-conversationAt_>=kVanceFinishMs) { frame_.finished=true; }
        else { return false; }
    }
    else if(spec.operation==coo::Operation::device) {
        if(spec.asset.registry==0xBA0B27A0U && spec.asset.type==23 && spec.asset.slot<=1) {
            if(!frame_.conversationStarted || now_<conversationAt_ || now_-conversationAt_<kVanceAscentMs) { return false; }
            frame_.lighthouseChannels|=static_cast<std::uint8_t>(1U<<spec.asset.slot);
        } else if(spec.argument==1) { frame_.cannons=true; } else if(spec.argument==2) { frame_.finalCannon=true; }
    }
    return true;
}
void Controller::enable(std::uint32_t cohort) noexcept {
    if(cohort>kLastCohort) { return; }
    frame_.cohorts|=1U<<cohort;
    for(std::size_t i=0;i<kSpawns.size();++i) { if(kSpawns[i].cohort==cohort) { population_.enable(i); } }
}
bool Controller::ready(std::uint32_t cohort) const noexcept {
    if(cohort==0 || cohort>kLastCohort || frame_.populationFault) { return false; }
    for(std::size_t i=0;i<kSpawns.size();++i) {
        if(kSpawns[i].cohort==cohort && !population_.admitted(i,kSpawns[i].count)) { return false; }
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
    prepared_|=static_cast<std::uint8_t>(1U<<index);return true;
}
bool Controller::module(const ModuleReceipt& receipt,bool dead) noexcept {
    if(!views_ || !frame_.enabled || frame_.moduleDestroyed || (prepared_&(1U<<5))==0 || !receipt.valid() || receipt.run!=run_ || receipt.generation!=publicationGeneration_+1U) { return false; }
    if(dead) {
        if(!frame_.moduleVulnerable || receipt!=moduleOwner_ || frame_.moduleDestroyed) { return false; }
        frame_.moduleDestroyed=true;return true;
    }
    if(moduleOwner_.valid()) { return false; }
    moduleOwner_=receipt;return true;
}
bool Controller::scene(const SceneReceipt& receipt,bool completed) noexcept {
    if(!views_ || !frame_.enabled || !receipt.valid() || receipt.run!=run_ || !frame_.sceneGeneration
        || receipt.generation!=frame_.sceneGeneration) { return false; }
    if(completed) {
        if(!frame_.vanceConversation || !frame_.sceneStarted || frame_.sceneComplete || receipt!=sceneOwner_) { return false; }
        frame_.sceneComplete=true;return true;
    }
    if(frame_.sceneStarted) { return false; }
    sceneOwner_=receipt;frame_.sceneStarted=true;return true;
}
bool Controller::vance(const SceneReceipt& receipt,VanceMilestone milestone,std::uint64_t now) noexcept {
    if(!views_ || !frame_.enabled || !frame_.sceneStarted || !receipt.valid()
        || receipt!=sceneOwner_ || receipt.generation!=frame_.sceneGeneration || !frame_.vanceEntered) { return false; }
    if(milestone==VanceMilestone::turned) {
        if(frame_.vanceTurned) { return false; }frame_.vanceTurned=true;return true;
    }
    if(milestone!=VanceMilestone::conversationStarted || !frame_.vanceTurned || !frame_.vanceConversation || frame_.conversationStarted) { return false; }
    frame_.conversationStarted=true;conversationAt_=now;return true;
}
void Controller::update_module(std::uint32_t id,const coo::MissionInput& input,Frame& output) noexcept {
    if(id!=1 || !views_ || input.run!=run_) { return; }
    now_=input.now;
    // Start the native cast/idle on world arrival, keeping this owner through the ending.
    frame_.sceneGeneration=publicationGeneration_;
    // Retain the second event only after the native turn and the greeting audio finish.
    if(vanceRequested_ && frame_.vanceTurned && dialogueSubmitted_[9] && dialogueSubmitted_[10] && now_>=dialogue_.voice_until()) { frame_.vanceConversation=true; }
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
        const bool vanceCue=spec.asset==kVanceScene && spec.operation==coo::Operation::observation;
        const bool vanceApproach=spec.operation==coo::Operation::observation
            && spec.asset.registry==0xBA0B27A0U && spec.asset.slot==13;
        const bool observed=vanceApproach?(frame_.vanceTurned && entered(spec.asset)):vanceCue?(frame_.conversationStarted && now_>=conversationAt_
            && now_-conversationAt_>=spec.argument):spec.argument==513U?frame_.moduleDestroyed
            :spec.argument==512U?(returnContact_ || entered(spec.asset))
            :(shelfExit || entered(spec.asset) || (spec.argument==256U?cleared(3)&&cleared(4):cleared(spec.argument)));
        if(spec.operation==coo::Operation::observation && observed) {
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
    dialogue_.advance(views_->dialogue,publicationGeneration_-1U,now_,false,frame_,frame_.revision);
    frame_.enabled=executor_.diagnostics().phase!=coo::Phase::failed;
    if(executor_.diagnostics().phase==coo::Phase::complete && frame_.section==0) {
        frame_.openingChecked=true;frame_.section=1;
        // Outbound Lighthouse visits cannot trigger the return encounter.
        seen_.reset();executor_.cancel(*this);started_=false;
    }
    frame_.checked=frame_.finished && frame_.conversationStarted;
    frame_.preparedMask=prepared_;
    output=frame_;
    output.cannons=frame_.cannons && (prepared_&3)==3;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!views_ || run!=run_ || !ready) { return {}; }
    return composition_.update(views_->mission,{run,now,kRegion,false,true},*this);
}
} // namespace sunrise::state::activity::gateway
