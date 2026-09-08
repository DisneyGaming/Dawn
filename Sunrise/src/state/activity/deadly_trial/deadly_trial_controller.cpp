#include "controller.h"
#include "ending_audio.h"
namespace sunrise::state::activity::deadly_trial {
namespace {
bool same(const coo::CommandSpec& a,const coo::CommandSpec& b) noexcept {
    return a.operation==b.operation && a.asset==b.asset && a.argument==b.argument && a.wait==b.wait;
}
}
bool valid_document(const coo::script::Views& views) noexcept {
    if(!views.valid || views.missionId!="deadly_trial" || views.profileId!=kProfile.id
        || views.graphs.size()!=3 || views.mission.modules.size()!=1
        || views.mission.modules[0].asset!=kModule || views.mission.modules[0].id!=1) { return false; }
    const auto* root=views.role("mission");
    if(!root || root->definition.steps.data()!=views.mission.sequence.steps.data()
        || root->domain!="composition" || root->commands.size()!=2) { return false; }
    for(const auto domain:{std::string_view{"opening"},std::string_view{"ending"}}) {
        const auto* graph=views.role(domain);
        const auto contract=domain=="opening"?std::span<const ContractStep>{kOpeningContract}:std::span<const ContractStep>{kEndingContract};
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


bool contains(const Volume& v,Point p) noexcept {
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.x<v.min.x||p.x>v.max.x||p.y<v.min.y||p.y>v.max.y||p.z<v.min.z||p.z>v.max.z||v.vertices.size()<3) { return false; }
    bool inside{};
    for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto a=v.vertices[j],b=v.vertices[i];const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,px=double(p.x)-a.x,py=double(p.y)-a.y;
        if(dx==0 && dy==0) { continue; }
        if(std::abs(dx*py-dy*px)<0.00001 && px*dx+py*dy>=0 && px*dx+py*dy<=dx*dx+dy*dy) { return true; }
        if((a.y>p.y)!=(b.y>p.y) && p.x<dx*py/dy+a.x) { inside=!inside; }
    }return inside;
}
void Controller::reset() noexcept {
    executor_.cancel(*this);composition_.reset();views_=nullptr;run_=now_=0;started_=landingSeen_=pikeMounted_=false;
    lifecycle_.reset();objectives_={};population_={};dialogue_={};submitted_.reset();voiceEnds_={};seen_.reset();binding_={};scene_={};frame_={};sceneAudioStarted_=false;sceneAudioEnds_=0;
}
bool Controller::select(const coo::script::Views& views,std::uint64_t run) noexcept {
    if(!run || !valid_document(views)) { reset();return false; }
    if(run_==run) { return views_==&views; }
    reset();if(!lifecycle_.begin(run)) { return false; }
    views_=&views;run_=run;frame_.spawnGeneration=lifecycle_.owner().value;return true;
}
void Controller::position(std::uint64_t run,Point point) noexcept {
    if(!views_ || run!=run_) { return; }
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(contains(kVolumes[i],point) && kVolumes[i].registry==0x3A62993FU && kVolumes[i].slot==2) { landingSeen_=true; }
    }
    if(!landingSeen_) { return; }
    for(std::size_t i=0;i<std::size(kVolumes);++i) { if(contains(kVolumes[i],point)) { seen_.set(i); } }
}
bool Controller::mounted(const PikeMount& receipt) noexcept {
    if(!receipt.valid() || receipt.owner!=coo::Generation{run_,frame_.spawnGeneration}
        || !frame_.enabled || frame_.finished || !landingSeen_ || frame_.pikes!=1 || pikeMounted_) { return false; }
    pikeMounted_=true;return true;
}
bool Controller::entered(coo::Asset a) const noexcept {
    const auto observed=[this](std::uint32_t registry,std::uint16_t slot) noexcept {
        for(std::size_t i=0;i<std::size(kVolumes);++i) {
            if(kVolumes[i].registry==registry && kVolumes[i].slot==slot) { return seen_[i]; }
        }return false;
    };
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(kVolumes[i].registry!=a.registry || kVolumes[i].tag!=a.definition || kVolumes[i].slot!=a.slot) { continue; }
        if(seen_[i]) { return true; }
        // Retained authored downstream arrivals may release missed road filters.
        // These observations never satisfy population/death or dialogue receipts.
        const bool overpass=observed(kAlleysA,131)||observed(kAlleysA,132)
            ||observed(kAlleysA,130)||observed(0x38C174BBU,4)||observed(0xE4BA2E54U,1);
        if(a.registry==0x38C174BBU && a.slot==2) { return pikeMounted_||observed(kAlleysA,214)||observed(kAlleysA,229)||overpass; }
        if(a.registry==kAlleysA && a.slot==214) { return observed(kAlleysA,229)||overpass; }
        if(a.registry==kAlleysA && (a.slot==229 || a.slot==131)) { return overpass; }
        // A later authored arrival releases earlier post-Walker travel filters.
        // Kill gates and dialogue submission still use their own receipts.
        const bool tower=observed(0xC91BDFF0U,1)||observed(0xBC1C972BU,1)||observed(0xBC1C972BU,2);
        const bool tunnel=observed(kAlleysB,223)||tower;
        // The paired road directive was reached in the live run while its
        // narrow dialogue filter was missed. It still requires real travel.
        if(a.registry==0x4DDB8242U && a.slot==2) { return observed(0x4DDB8242U,1)||observed(kAlleysB,157)||tunnel; }
        if(a.registry==kAlleysB && a.slot==157) { return tunnel; }
        if(a.registry==kAlleysB && a.slot==223) { return tower; }
        if(a.registry==0xBC1C972BU && a.slot==2) { return tower; }
        return false;
    }return false;
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    if(!views_ || run!=run_ || !started_ || !dialogue_.submitted(views_->dialogue,bank,row,generation,now,frame_,frame_.revision)) { return false; }
    submitted_.set(row);voiceEnds_[row]=dialogue_.voice_until();return true;
}
void Controller::enable(std::uint32_t cohort) noexcept {
    if(cohort<1 || cohort>9) { return; }frame_.cohorts|=1U<<cohort;
    for(std::size_t i=0;i<kSpawns.size();++i) { const auto& s=kSpawns[i];if(s.cohort!=cohort) { continue; }
        population_.policy(i,{true,coo::EnemyIntent::combat,s.tactical.registry,s.tactical.slot,s.tactical.row});population_.enable(i);
    }
}
bool Controller::ready(std::uint32_t cohort) const noexcept {
    if(cohort<1 || cohort>9 || frame_.populationFault) { return false; }
    for(std::size_t i=0;i<kSpawns.size();++i) { if(kSpawns[i].cohort==cohort && !population_.ready(i,kSpawns[i].count)) { return false; } }return true;
}
bool Controller::cleared(std::uint32_t cohort) const noexcept {
    if(cohort<1 || cohort>9 || frame_.populationFault) { return false; }
    for(std::size_t i=0;i<kSpawns.size();++i) { if(kSpawns[i].cohort==cohort && !population_.cleared(i,kSpawns[i].count)) { return false; } }return true;
}
bool Controller::admitted(const EnemyReceipt& receipt) noexcept {
    if(!frame_.enabled || frame_.populationFault) { return false; }
    const auto result=population_.admit(kSpawns,receipt,run_,frame_.spawnGeneration);
    if(result==coo::Admission::overflow) { frame_.populationFault=true; }return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    if(!frame_.enabled || !population_.died(r,run_,frame_.spawnGeneration)) { return false; }
    // The final exterior encounter activates on the authenticated Walker death.
    // Its spawn intent cannot depend on later road filters or dialogue delivery.
    if(const auto* source=spawn(r.registry,r.source);source && source->cohort==4 && cleared(4)) { enable(8); }
    return true;
}
bool Controller::bind(const InteractionBinding& b) noexcept {
    if(!frame_.enabled || !frame_.reviveEnabled || !b.valid() || b.owner!=request().owner || frame_.interacted) { return false; }
    if(binding_.valid()) { return binding_==b; }binding_=b;return true;
}
bool Controller::interact(const InteractionBinding& b,std::int32_t requested,std::int32_t before,std::int32_t after,bool active) noexcept {
    if(!frame_.enabled || !frame_.reviveEnabled || frame_.interacted || !binding_.valid() || b!=binding_
        || before<0 || requested<=before || after!=requested || !active) { return false; }
    frame_.interacted=true;return true;
}
bool Controller::bind_scene(const SceneReceipt& receipt) noexcept {
    if(!frame_.enabled || frame_.finished || !frame_.interacted || !frame_.sceneGeneration
        || !receipt.valid() || receipt.run!=run_ || receipt.generation!=frame_.sceneGeneration) { return false; }
    if(scene_.valid() && scene_!=receipt) { return false; }
    scene_=receipt;frame_.sceneBound=true;return true;
}
bool Controller::scene(const SceneReceipt& receipt,bool completed) noexcept {
    if(!frame_.enabled || frame_.finished || !frame_.sceneBound || !frame_.sceneGeneration || !receipt.valid() || receipt.run!=run_ || receipt.generation!=frame_.sceneGeneration) { return false; }
    if(!scene_.valid() || (completed && !frame_.sceneStarted)) { return false; }
    if(scene_!=receipt) { return false; }frame_.sceneStarted=true;
    if(completed) { frame_.sceneComplete=true; }return true;
}
bool Controller::scene_audio(const SceneReceipt& receipt,float elapsed,std::uint64_t now) noexcept {
    if(!frame_.enabled || frame_.finished || !frame_.sceneStarted || sceneAudioStarted_
        || !receipt.valid() || scene_!=receipt || !valid_revival_audio_cue(elapsed)
        || now<now_ || now>UINT64_MAX-kRevivalAudioEndMs) { return false; }
    sceneAudioStarted_=true;sceneAudioEnds_=now+revival_audio_remaining(elapsed);return true;
}
bool Controller::publish(const coo::Command& c) noexcept {
    if(!views_ || c.token.run!=run_ || c.schema!=coo::Schema::otherMissions) { return false; }
    bool registered{};for(const auto& s:graph()->definition.steps) for(const auto& x:s.commands) { registered|=same(x,c.spec); }
    if(!registered) { return false; }const auto& s=c.spec;
    switch(s.operation) {
    case coo::Operation::objective: dialogue_.objective(views_->dialogue,s.argument,frame_,frame_.revision);{ coo::MarkerTarget marker{};for(const auto& m:kNativeMarkers) { if(m.event==s.argument) { marker=m.target; } }objectives_.set(s.argument,marker); }break;
    case coo::Operation::dialogue: dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(s.argument),now_,0,0,frame_.revision);break;
    case coo::Operation::population: enable(s.argument);break;
    case coo::Operation::device:
        if(s.asset==kBarrier) { if(!cleared(4)) { return false; }frame_.barrierOpen=true; }
        else if(s.asset==kRevive) { if(!cleared(8)||!cleared(9)) { return false; }frame_.reviveEnabled=true; }
        else { frame_.pikes=static_cast<std::uint8_t>(s.argument); }break;
    case coo::Operation::scene:
        if(!frame_.interacted || !submitted_[7] || now_<voiceEnds_[7]) { return false; }
        frame_.sceneGeneration=frame_.spawnGeneration;break;
    case coo::Operation::complete:
        if(!frame_.sceneComplete || !sceneAudioStarted_ || now_<sceneAudioEnds_ || !lifecycle_.complete(lifecycle_.owner())) { return false; }
        objectives_.clear();frame_.finished=true;break;
    default:break;
    }return true;
}
coo::StallDetail Controller::missing(const coo::CommandSpec& s) const noexcept {
    using coo::Missing;if(s.wait==coo::Wait::requested) { return {}; }
    if(s.operation==coo::Operation::population) {
        for(std::size_t i=0;i<kSpawns.size();++i) { const auto& p=kSpawns[i];if(p.cohort!=s.argument) { continue; }
            auto d=population_.missing(i,p.count,true);if(d.missing!=Missing::none) { d.asset={p.registry,p.definition,1,p.source};return d; }
        }return {};
    }
    if(s.operation==coo::Operation::dialogue) { return {submitted_[s.argument]?Missing::none:Missing::dialogue,s.asset,s.argument}; }
    if(s.asset==kScene && s.argument==3) { return {sceneAudioStarted_?(now_>=sceneAudioEnds_?Missing::none:Missing::timer):Missing::eventOrigin,s.asset}; }
    if(s.operation==coo::Operation::eventAfter) { return {submitted_[s.argument]?(now_>=voiceEnds_[s.argument]?Missing::none:Missing::timer):Missing::eventOrigin,s.asset}; }
    if(s.asset==kScene) { return {(s.argument==2?frame_.sceneComplete:frame_.sceneStarted)?Missing::none:Missing::sceneBinding,s.asset}; }
    if(s.asset==kRevive) { return {frame_.interacted?Missing::none:binding_.valid()?Missing::observation:Missing::controller,s.asset}; }
    return {entered(s.asset)?Missing::none:Missing::observation,s.asset};
}
void Controller::update_module(std::uint32_t id,const coo::MissionInput& in,Frame& out) noexcept {
    if(id!=1 || !views_ || in.run!=run_) { return; }now_=in.now;
    const auto& g=*graph();if(!started_) { started_=executor_.start(g.definition,run_); }if(!started_) { return; }
    executor_.update(*this);
    for(const auto& b:g.commands) {
        const auto& s=g.definition.steps[b.step].commands[b.command];const auto state=executor_.step_state(b.step);
        if(state.phase!=coo::StepPhase::active || !state.commands[b.command].requested) { continue; }
        const coo::Token token{run_,executor_.diagnostics().incarnation,b.step,b.command};
        if(coo::is_observation(s.operation) && missing(s).missing==coo::Missing::none) { static_cast<void>(executor_.enqueue({token,coo::Milestone::observed})); }
        if(s.operation==coo::Operation::dialogue && submitted_[s.argument]) { static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady})); }
        if(s.operation==coo::Operation::population && s.wait==coo::Wait::completed) {
            if(ready(s.argument)) { static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady})); }
            if(cleared(s.argument)) { static_cast<void>(executor_.enqueue({token,coo::Milestone::completed})); }
        }
        if(s.operation==coo::Operation::scene && frame_.sceneStarted) { static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady})); }
        if(frame_.populationFault) { static_cast<void>(executor_.enqueue({token,coo::Milestone::failed})); }
    }
    executor_.update(*this);dialogue_.advance(views_->dialogue,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    frame_.enabled=executor_.diagnostics().phase!=coo::Phase::failed;
    if(executor_.diagnostics().phase==coo::Phase::complete && frame_.section==0) {
        frame_.section=1;executor_.cancel(*this);started_=executor_.start(graph()->definition,run_);
        if(started_) { executor_.update(*this); }
    }
    frame_.checked=frame_.finished;frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();out=frame_;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool readyValue) noexcept {
    if(!views_ || run!=run_ || !readyValue) { return {}; }return composition_.update(views_->mission,{run,now,408,false,true},*this);
}
}
