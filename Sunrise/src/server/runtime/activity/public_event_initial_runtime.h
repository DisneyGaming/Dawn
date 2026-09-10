#pragma once
#include "public_event_opening_runtime.h"
#include "public_event_engagement_runtime.h"
#include "public_event_deferred_placement_bridge.h"
#include "placement_service.h"
#include "ambient_population_registry.h"
#include "adventure_start_plan.h"
#include "public_event_dialogue_runtime.h"
#include "public_event_sequence_runtime.h"
#include "public_event_clock.h"
#include "world_object_runtime.h"
#include "public_event_key_runtime.h"
#include "public_event_directive_runtime.h"
#include "public_event_incoming_runtime.h"
#include "music_runtime.h"
namespace sunrise::server::runtime::activity::public_event {
struct InitialDefinition final {
    const OpeningDefinition* opening{};
    const opening_coo::Definition* prerequisites{};
    std::span<const deferred_placement::Ticket> gates{};
    engagement_feedback::Ticket engagement{};
    std::string_view enableParameter{};
    // Optional next encounter: its authored cue must replace the completed
    // opening on the same native directive; its own source readiness is new.
    const OpeningDefinition* following{};
    const dialogue::Definition* incoming{};
    const sequence::Definition* intro{};
    // Reconstructed presentation timings are profile parameters, separate
    // from authored assets and from native completion/death receipts.
    std::string_view introDelayParameter{},voiceDelayParameter{},openingDurationParameter{};
    const world_object::Definition* world{};
    std::uint32_t worldOpeningScene{};
    const keys::Definition* keys{};
    const directive::Definition* travel{};
    std::array<std::uint32_t,3> transitScenes{};
    std::string_view travelDurationParameter{},rotationDelayParameter{},activationDelayParameter{};
    participant_feedback::Ticket participant{};
    std::string_view incomingCueDelayParameter{},incomingDurationParameter{};
    const dialogue::Definition* joinedVoice{};
    const music::Definition* musicDefinition{};
    std::uint32_t openingMusic{};
};
// Finite diagnostic policy. Gate creation, attached interfaces, participant
// collection and accepted native presentation are distinct UE prerequisites.
// The optional opening quota uses native deaths; scheduling and native source
// renewal remain separate policies.
class InitialRuntime final {
public:
    template<class Definition,class Document>
    [[nodiscard]] static bool valid(const Definition& activity,const Document& document) noexcept {
        if(activity.publicEventInitials.size()>1)return false;
        for(const auto& d:activity.publicEventInitials) {
            const auto* enabled=document.views().parameter(d.enableParameter);
            if(!enabled || enabled->value>1 || !d.opening || !OpeningRuntime::valid(*d.opening)
                || !d.prerequisites || !opening_coo::Executor::valid(*d.prerequisites)
                || d.prerequisites->steps.size()!=2 || d.gates.empty() || d.gates.size()>4
                || d.prerequisites->steps[0].dependencies || d.prerequisites->steps[1].dependencies!=1
                || d.prerequisites->steps[0].commands.size()!=d.gates.size()
                || d.prerequisites->steps[1].commands.size()!=1
                || !ambient_population::optional_binding(activity,d.opening->registry,d.enableParameter))return false;
            unsigned policy{};for(const auto& p:activity.profile->parameters)
                if(p.id==d.enableParameter && p.minimum==0 && p.maximum==1 && p.defaultValue==0 && !p.liveEditable)++policy;
            if(policy!=1)return false;
            if(d.incoming && (!dialogue::Runtime::valid(*d.incoming) || d.incoming->registry!=d.opening->registry))return false;
            if(d.joinedVoice && (!d.incoming || !d.participant.definition || !dialogue::Runtime::valid(*d.joinedVoice)
                || d.joinedVoice->registry!=d.incoming->registry || d.joinedVoice->slot!=d.incoming->slot))return false;
            if(d.participant.definition) {
                if(!d.intro || d.participant.request.registry!=d.opening->registry->key
                    || d.participant.request.scope!=activity.bubble)return false;
                unsigned matches{};
                for(const auto& s:d.opening->registry->slots)
                    if(s.index==d.participant.request.slot && s.type==71 && s.descriptorTag==d.participant.definition
                        && s.authSchema==0x80804F57)++matches;
                if(matches!=1)return false;
                for(const auto name:{d.incomingCueDelayParameter,d.incomingDurationParameter}) {
                    const auto* p=document.views().parameter(name);
                    if(name.empty() || !p || p->value>3600000)return false;
                }
                if(!document.views().parameter(d.incomingDurationParameter)->value)return false;
            }
            if(d.world) {
                if(!world_object::Runtime::valid(*d.world) || !d.worldOpeningScene)return false;
                for(const auto& p:d.world->placements)
                    if(!ambient_population::optional_binding(activity,p.capability.registry,d.enableParameter))return false;
            }
            if(d.musicDefinition) {
                if(!d.world || !music::Runtime::valid(*d.musicDefinition)
                    || d.musicDefinition->registry!=d.opening->registry || !d.openingMusic)return false;
                bool found{};for(auto key:d.musicDefinition->candidates)if(key==d.openingMusic)found=true;
                if(!found)return false;
            }
            if(d.keys && (!d.following || !keys::Runtime::valid(*d.keys)))return false;
            if(d.travel) {
                if(!d.keys || !d.world || !d.intro || !directive::Runtime::valid(*d.travel))return false;
                for(const auto name:{d.travelDurationParameter,d.rotationDelayParameter,d.activationDelayParameter}) {
                    const auto* p=document.views().parameter(name);if(name.empty() || !p || p->value>3600000)return false;
                }
                if(!document.views().parameter(d.travelDurationParameter)->value
                    || document.views().parameter(d.activationDelayParameter)->value<document.views().parameter(d.rotationDelayParameter)->value)return false;
            }
            if(d.intro) {
                if(!sequence::Runtime::valid(*d.intro) || d.intro->registry!=d.opening->registry
                    || activity.clockFrequencyParameter.empty())return false;
                for(const auto name:{d.introDelayParameter,d.voiceDelayParameter,d.openingDurationParameter}) {
                    const auto* p=document.views().parameter(name);
                    if(name.empty() || !p || p->value>3600000)return false;
                }
                if(document.views().parameter(d.voiceDelayParameter)->value<document.views().parameter(d.introDelayParameter)->value
                    || !document.views().parameter(d.openingDurationParameter)->value)return false;
            }
            if(d.following) {
                if(!d.opening->defeatGoal || !OpeningRuntime::valid(*d.following)
                    || d.following->registry!=d.opening->registry)return false;
                auto prior=d.opening->cue;auto next=d.following->cue;
                prior.owner=next.owner={1,{1}};
                prior.boot=next.boot=prior.definitionRevision=next.definitionRevision=prior.selectionRevision=next.selectionRevision=1;
                if(!opening_bridge::Mailbox::can_advance(prior,next))return false;
                for(const auto& source:d.following->sources) {
                    unsigned matches{};for(const auto& cap:activity.populations)
                        if(cap.registry==source.registry && cap.slot==source.slot && cap.rule==source.rule
                            && cap.tactical.registry==source.tactical.registry && cap.tactical.slot==source.tactical.slot
                            && cap.tactical.row==source.tactical.row && cap.hasRule==source.hasRule)++matches;
                    if(matches!=1)return false;
                    for(const auto& old:d.opening->sources)if(old.slot==source.slot)return false;
                }
            }
            auto e=d.engagement;e.owner={1,{1}};e.boot=e.definitionRevision=e.selectionRevision=e.event=1;
            if(!engagement_feedback::valid(e) || e.request.collection!=engagement_feedback::wire::Collection::activePlayers
                || e.request.registry!=d.opening->registry->key || e.request.scope!=activity.bubble
                || d.opening->cue.request.readiness!=opening_cue::wire::Reference{e.request.registry,70,e.request.slot})return false;
            unsigned engagementSlots{};for(const auto& slot:d.opening->registry->slots)
                if(slot.index==e.request.slot && slot.type==70 && slot.descriptorTag==e.definition
                    && slot.authSchema==0x808094F1 && slot.senseSchema==engagement_sense::kSchema)++engagementSlots;
            if(engagementSlots!=1)return false;
            const auto& command=d.prerequisites->steps[1].commands[0];
            if(command.operation!=opening_coo::Operation::mechanic || command.wait!=opening_coo::Wait::nativeReady
                || command.argument!=1 || command.asset!=opening_coo::Asset{e.request.registry,e.definition,70,e.request.slot})return false;
            for(std::size_t i=0;i<d.gates.size();++i) {
                auto t=d.gates[i];t.owner={1,{1}};t.boot=t.definitionRevision=t.selectionRevision=t.event=1;
                const auto& c=d.prerequisites->steps[0].commands[i];
                if(!deferred_placement::valid(t) || !deferred_placement::tag(t.pointComponent) || t.pointInterfaceOffset<=0 || t.pointInterfaceOffset>=0x2000000
                    || t.registry!=e.request.registry || t.bubble!=e.request.scope || t.generation!=1
                    || c.operation!=opening_coo::Operation::device || c.wait!=opening_coo::Wait::nativeReady || c.argument!=1
                    || c.asset!=opening_coo::Asset{t.registry,t.definition,4,t.slot})return false;
                unsigned matches{};for(const auto& s:d.opening->registry->slots)
                    if(s.index==t.slot && s.type==4 && s.descriptorTag==t.definition)++matches;
                if(matches!=1)return false;
                for(std::size_t j=0;j<i;++j)if(d.gates[j].slot==t.slot)return false;
            }
            for(const auto& source:d.opening->sources) {
                unsigned matches{};for(const auto& cap:activity.populations)
                    if(cap.registry==source.registry && cap.slot==source.slot && cap.rule==source.rule)++matches;
                if(matches!=1)return false;
            }
        }
        return true;
    }
    template<class Definition,class Document>
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,const Definition& activity,const Document& document) noexcept {
        if(owner_ || !owner || !boot || !valid(activity,document))return false;
        owner_=owner;boot_=boot;revision_=document.fingerprint();
        if(!activity.publicEventInitials.empty()) {
            definition_=&activity.publicEventInitials.front();enabled_=document.views().parameter(definition_->enableParameter)->value==1;
            if(definition_->intro) {
                introDelay_=document.views().parameter(definition_->introDelayParameter)->value;
                voiceDelay_=document.views().parameter(definition_->voiceDelayParameter)->value;
                openingDuration_=document.views().parameter(definition_->openingDurationParameter)->value;
            }
            if(definition_->travel) {
                travelDuration_=document.views().parameter(definition_->travelDurationParameter)->value;
                rotationDelay_=document.views().parameter(definition_->rotationDelayParameter)->value;
                activationDelay_=document.views().parameter(definition_->activationDelayParameter)->value;
            }
            if(definition_->participant.definition) {
                incomingCueDelay_=document.views().parameter(definition_->incomingCueDelayParameter)->value;
                incomingDuration_=document.views().parameter(definition_->incomingDurationParameter)->value;
            }
        }
        return true;
    }
    [[nodiscard]] bool owns(std::uint32_t registry,std::uint16_t slot) const noexcept {
        if(definition_)for(const auto& source:definition_->opening->sources)
            if(source.registry->key==registry && source.slot==slot)return true;
        if(definition_ && definition_->following)for(const auto& source:definition_->following->sources)
            if(source.registry->key==registry && source.slot==slot)return true;
        return false;
    }
    [[nodiscard]] bool update(std::uint32_t bubble,bool arrived,bool admitted,const adventure_start::wire::Request& selected,
        population::Service& population,placement::wire::Batch& placements,opening_cue::wire::Batch& cues,
        engagement_feedback::wire::Batch& engagements,bool startRequested,activity_clock::Publication clock={},
        participant_feedback::LocalIdentity local={}) noexcept {
        if(!enabled_)return true;
        if(population.owner()!=owner_ || population.boot()!=boot_)return false;
        const bool eligible=arrived && admitted && bubble==definition_->opening->registry->bubble;
        if(!started_ && !failed_) {
            // The temporary diagnostic begins only after qualified rally use.
            // This input gates startup, never retained authority after startup.
            if(!startRequested || !eligible || !selected.revision || selected.selection.activityIndex!=definition_->opening->cue.activity
                || selected.selection.sourceActivityIndex!=selected.selection.activityIndex)return true;
            if(definition_->participant.definition && !participant_feedback::valid(local))return true;
            selection_=selected.revision;event_=owner_.incarnation.value;
            if(definition_->intro && (!same_clock(clock) || !clock_.observe(clock)))return true;
            for(std::size_t i=0;i<definition_->gates.size();++i){tickets_[i]=definition_->gates[i];auto& t=tickets_[i];
                t.owner=owner_;t.boot=boot_;t.definitionRevision=revision_;t.selectionRevision=selection_;t.event=event_;}
            auto ticket=definition_->engagement;ticket.owner=owner_;ticket.boot=boot_;ticket.definitionRevision=revision_;
            ticket.selectionRevision=selection_;ticket.event=event_;
            bool bound{};
            if(definition_->participant.definition) {
                IncomingDefinition incoming{};incoming.participant=definition_->participant;
                auto& p=incoming.participant;p.owner=owner_;p.boot=boot_;p.definitionRevision=revision_;
                p.selectionRevision=selection_;p.event=event_;p.request.entity=local.entity;p.request.identifierEncoding=local.encoding;
                incoming.empty=ticket;incoming.empty.request.collection=engagement_feedback::wire::Collection::none;
                incoming.cue=definition_->opening->cue;auto& c=incoming.cue;
                c.owner=owner_;c.boot=boot_;c.definitionRevision=revision_;c.selectionRevision=selection_;c.incoming=true;
                c.request.publicEvent={p.request.registry,71,p.request.slot};
                c.request.hasProgress=true;c.request.progress={0,static_cast<std::int32_t>(definition_->opening->defeatGoal)};
                bound=clock_.start_phase(1,incomingDuration_) && clock_.project(c.request) && incomingUi_.begin(incoming);
            } else bound=engagement_.begin(ticket,definition_->opening->cue.activity);
            if(!bound || !executor_.start(*definition_->prerequisites,event_))failed_=true;
            else started_=true;
        }
        if(started_ && (selected.revision!=selection_ || selected.selection.activityIndex!=definition_->opening->cue.activity
            || selected.selection.sourceActivityIndex!=selected.selection.activityIndex))conflicting_=true;
        // Eligibility gates graph updates and mailbox intake, never withdrawal
        // of authority already adopted by this same world owner.
        if(started_ && !failed_ && !conflicting_ && eligible) {
            if(definition_->intro) {
                if(!same_clock(clock) || !clock_.observe(clock))return project(placements,cues,engagements);
                const sequence::Context context{owner_,boot_,revision_,selection_,event_,clock_.now(),selected.selection.activityIndex,bubble,arrived,admitted};
                if(!introStarted_ && clock_.after(introDelay_)) {
                    introStarted_=intro_.begin(*definition_->intro,context);
                    if(!introStarted_)return fail();
                }
                if(introStarted_ && !intro_.update(context))return fail();
            }
            if(definition_->participant.definition && clock_.after(incomingCueDelay_)) {
                const EngagementContext context{owner_,boot_,revision_,selection_,event_,selected.selection.activityIndex,bubble,arrived,admitted};
                // Rally is the explicitly enabled development shortcut. Native
                // presentation still passes through incoming before joining.
                if(startRequested && clock_.after(voiceDelay_) && !incomingUi_.request_join(context))return fail();
                incomingFrame_=incomingUi_.update(context);
                if(incomingFrame_.failed || incomingFrame_.stale)return fail();
                if(incomingFrame_.joined && !joinedClock_) {
                    if(!clock_.start_phase(2,openingDuration_))return fail();joinedClock_=true;
                }
            }
            if(definition_->world && (!definition_->intro || clock_.after(voiceDelay_))) {
                const world_object::Context context{owner_,boot_,revision_,selection_,event_,selected.selection.activityIndex,bubble,arrived,admitted};
                if(!worldStarted_) {
                    if(!world_.begin(*definition_->world,context) || !world_.request(definition_->worldOpeningScene,context))return fail();
                    worldStarted_=true;
                }
                if(!world_.update(context))return fail();
                if(definition_->musicDefinition) {
                    const music::Context audio{owner_,boot_,revision_,selection_,event_,selected.selection.activityIndex,bubble,arrived,admitted};
                    if(!musicStarted_) {
                        if(!music_.begin(*definition_->musicDefinition,audio)
                            || !music_.request(definition_->openingMusic,1,audio))return fail();
                        musicStarted_=true;
                    }
                    if(!music_.update(audio))return fail();
                }
            }
            if(definition_->incoming && !incomingFailed_ && (!definition_->intro || clock_.after(voiceDelay_))) {
                const dialogue::Context context{owner_,boot_,revision_,selection_,event_,selected.selection.activityIndex,bubble,arrived,admitted};
                if(!incomingStarted_) {
                    incomingStarted_=incoming_.begin(*definition_->incoming,context);
                    incomingFailed_=!incomingStarted_;
                }
                if(incomingStarted_ && !incoming_.update(context))incomingFailed_=true;
                if(definition_->joinedVoice && !joinedVoiceStarted_ && incomingFrame_.joined && incoming_.submitted()
                    && clock_.after(voiceDelay_+definition_->incoming->conversation.durationMs)) {
                    joinedVoiceStarted_=incoming_.advance(*definition_->joinedVoice,context);
                    if(!joinedVoiceStarted_ || !incoming_.update(context))incomingFailed_=true;
                }
            }
            if(!definition_->intro || clock_.after(voiceDelay_))static_cast<void>(advance(bubble,arrived,admitted,selected,population));
        }
        return project(placements,cues,engagements);
    }
    [[nodiscard]] bool append_dialogues(dialogue::feedback::wire::Batch& output) const noexcept {
        return incoming_.append(output);
    }
    [[nodiscard]] bool append_sequences(sequence::wire::Batch& output) const noexcept {return intro_.append(output);}
    [[nodiscard]] bool append_music(music::wire::Batch& output) const noexcept {
        return !musicStarted_ || music_.append(output);
    }
    [[nodiscard]] bool append_participants(participant_feedback::wire::Batch& output) const noexcept {
        if(!incomingFrame_.publishParticipant)return true;
        if(output.count>=output.entries.size())return false;
        const auto& p=incomingFrame_.participant;
        for(std::size_t i=0;i<output.count;++i)
            if(output.entries[i].registry==p.registry && output.entries[i].slot==p.slot)return false;
        output.entries[output.count++]=p;return true;
    }
    [[nodiscard]] bool append_world(placement::wire::Batch& placements,world_device::wire::Batch& devices) const noexcept {
        auto p=placements;auto d=devices;
        if((worldStarted_ && !world_.append(p,d)) || !keys_.append(p,d))return false;
        placements=p;devices=d;return true;
    }

    [[nodiscard]] bool append(placement::wire::Batch& output) const noexcept {
        if(!started_)return true;
        auto result=output;
        for(std::size_t i=0;i<definition_->gates.size();++i)if(requested_[i]) {
            const auto& t=tickets_[i];if(result.count==result.entries.size())return false;
            for(std::size_t j=0;j<result.count;++j)if(result.entries[j].registry==t.registry && result.entries[j].slot==t.slot)return false;
            placement::wire::Request request{t.registry,t.slot,t.bubble};request.generation=t.generation;
            result.entries[result.count++]=request;
        }
        output=result;return true;
    }
    [[nodiscard]] bool observe(std::uint32_t registry,std::uint16_t slot,std::uint32_t bubble,std::uint32_t schema,const engagement_sense::Output& sense) noexcept {
        if(definition_ && definition_->participant.definition) {
            const auto& t=incomingUi_.engagement_ticket();
            return started_ && !failed_ && !conflicting_ && t.request.registry==registry && t.request.slot==slot
                && incomingUi_.observe(t,bubble,schema,sense);
        }
        const auto& t=engagement_.ticket();return started_ && !failed_ && !conflicting_ && t.request.registry==registry && t.request.slot==slot
            && engagement_.observe(t,bubble,schema,sense);
    }
    [[nodiscard]] bool observe_accepted(const opening_native::Event& event,opening_coo::PopulationIntake intake) noexcept {
        if(!openingStarted_ || failed_ || conflicting_)return false;
        if(opening_.observe_accepted(event,intake))return true;
        return followingStarted_ && following_.observe_accepted(event,intake);
    }
    [[nodiscard]] std::uint32_t state() const noexcept {
        std::uint32_t bits=enabled_?1U:0U;for(std::size_t i=0;i<4;++i){if(requested_[i])bits|=2U<<i;if(ready_[i])bits|=32U<<i;}
        if(engagementRequested_)bits|=512;if(engagementReady_)bits|=1024;if(openingFrame_.requested)bits|=2048;
        if(openingFrame_.nativeReady)bits|=4096;if(openingFrame_.combatRequested)bits|=8192;if(openingFrame_.combatReady)bits|=16384;
        if(conflicting_)bits|=32768;if(failed_)bits|=65536;
        if(opening_.defeat_goal_completed())bits|=131072;
        if(followingFrame_.requested)bits|=262144;if(followingFrame_.nativeReady)bits|=524288;
        if(followingFrame_.combatRequested)bits|=1048576;if(followingFrame_.combatReady)bits|=2097152;
        if(incoming_.requested())bits|=4194304;if(incoming_.submitted())bits|=8388608;if(incomingFailed_)bits|=16777216;return bits;
    }
private:
    [[nodiscard]] bool same_clock(const activity_clock::Publication& clock) const noexcept {
        return clock.domain.owner==owner_ && clock.domain.boot==boot_
            && clock.domain.scenario==definition_->opening->registry->scenario
            && clock.domain.bubble==definition_->opening->registry->bubble;
    }
    bool advance(std::uint32_t bubble,bool arrived,bool admitted,const adventure_start::wire::Request& selected,
        population::Service& population) noexcept {
        for(std::size_t i=0;i<definition_->gates.size();++i)if(requested_[i] && !ready_[i]) {
            const auto state=deferred_bridge::lookup(tickets_[i].definition);
            if(state.binding.ticket==tickets_[i] && state.created && state.ready) {
                if(!executor_.enqueue({tokens_[i],opening_coo::Milestone::nativeReady}))return fail();ready_[i]=true;
            }
        }
        if(engagementRequested_) {
            const auto state=definition_->participant.definition?EngagementFrame{}:
                engagement_.update({owner_,boot_,revision_,selection_,event_,definition_->opening->cue.activity,bubble,arrived,admitted});
            if(state.failed || state.stale)return fail();
            if((definition_->participant.definition?incomingFrame_.readyForWorld:state.readyForCue) && !engagementReady_) {
                if(!executor_.enqueue({engagementToken_,opening_coo::Milestone::nativeReady}))return fail();engagementReady_=true;
            }
        }
        Driver driver(*this);executor_.update(driver);
        if(executor_.diagnostics().phase==opening_coo::Phase::failed)return fail();
        if(executor_.diagnostics().phase==opening_coo::Phase::complete && !openingStarted_) {
            openingDefinition_=*definition_->opening;
            if(definition_->participant.definition)openingDefinition_.cue=incomingUi_.definition().cue;
            else if(definition_->intro) {
                if(!clock_.start_phase(1,openingDuration_) || !clock_.project(openingDefinition_.cue.request))return fail();
            }
            if(!opening_.begin(openingDefinition_,owner_,boot_,revision_,selection_,event_,population))return fail();openingStarted_=true;
        }
        if(openingStarted_)openingFrame_=opening_.update(bubble,arrived,admitted,selected.selection.activityIndex,selected.revision);
        if(definition_->participant.definition && openingFrame_.requested && !openingFrame_.nativeReady && incomingUi_.cue_receipt()) {
            if(!opening_.adopt_cue_receipt(*incomingUi_.cue_receipt()))return fail();
            openingFrame_=opening_.update(bubble,arrived,admitted,selected.selection.activityIndex,selected.revision);
        }
        if(openingFrame_.failed)return fail();
        if(definition_->following && opening_.defeat_goal_completed()) {
            if(!followingStarted_) {
                followingDefinition_=*definition_->following;
                followingDefinition_.cue.request.publicEvent=opening_.ticket().request.publicEvent;
                if(definition_->intro && !clock_.project(followingDefinition_.cue.request))return fail();
                if(!following_.begin(followingDefinition_,owner_,boot_,revision_,selection_,event_,population,&opening_.ticket()))return fail();
                followingStarted_=true;
            }
            followingFrame_=following_.update(bubble,arrived,admitted,selected.selection.activityIndex,selected.revision);
            if(followingFrame_.failed)return fail();
            if(definition_->keys && followingFrame_.nativeReady) {
                const world_object::Context context{owner_,boot_,revision_,selection_,event_,selected.selection.activityIndex,bubble,arrived,admitted};
                if(!keysStarted_) {
                    if(!keys_.begin(*definition_->keys,context))return fail();keysStarted_=true;
                }
                if(!keys_.update(context,{following_.defeated(0),following_.defeated(1)}))return fail();
                followingFrame_.cue.progress.current=static_cast<std::int32_t>(keys_.deposited());
                if(definition_->travel && keys_.complete()) {
                    if(!travelStarted_) {
                        opening_cue::wire::Request timed{};
                        if(!clock_.start_phase(definition_->participant.definition?3:2,travelDuration_) || !clock_.project(timed)
                            || !travel_.begin(*definition_->travel,context,following_.ticket(),timed.timer)
                            || !world_.request(definition_->transitScenes[0],context))return fail();
                        transitStart_=clock_.now();travelStarted_=true;
                    }
                    if(!travel_.update(context))return fail();
                    if(world_.requested()) {
                        std::uint64_t rotation{},activation{};
                        if(!activity_clock::wire::from_milliseconds(rotationDelay_,rotation)
                            || !activity_clock::wire::from_milliseconds(activationDelay_,activation))return fail();
                        if(world_.scene()==definition_->transitScenes[0] && clock_.now()-transitStart_>=rotation) {
                            if(!world_.request(definition_->transitScenes[1],context))return fail();
                        } else if(world_.scene()==definition_->transitScenes[1] && travel_.ready() && clock_.now()-transitStart_>=activation) {
                            if(!world_.request(definition_->transitScenes[2],context))return fail();
                        }
                    }
                }
            }
        }
        if(engagementRequested_ && !definition_->participant.definition) {
            const auto state=engagement_.update({owner_,boot_,revision_,selection_,event_,definition_->opening->cue.activity,bubble,arrived,admitted});
            if(state.failed || state.stale)return fail();
        }
        return true;
    }
    // A stopped graph cannot withdraw previously adopted native authority.
    // These projections keep their original immutable tickets until world retirement.
    // Capacity/slot failures leave all three caller batches unchanged.
    bool project(placement::wire::Batch& placements,opening_cue::wire::Batch& cues,engagement_feedback::wire::Batch& engagements) noexcept {
        auto nextPlacements=placements;auto nextCues=cues;auto nextEngagements=engagements;
        if(nextPlacements.count>nextPlacements.entries.size() || nextCues.count>nextCues.entries.size()
            || nextEngagements.count>nextEngagements.entries.size() || !append(nextPlacements))return fail();
        const auto* authority=incomingFrame_.publishEngagement?&incomingFrame_.engagement:engagement_.retained_authority();
        if(authority) {
            if(nextEngagements.count==nextEngagements.entries.size())return fail();
            for(std::size_t i=0;i<nextEngagements.count;++i)
                if(nextEngagements.entries[i].registry==authority->registry && nextEngagements.entries[i].slot==authority->slot)return fail();
            nextEngagements.entries[nextEngagements.count++]=*authority;
        }
        auto active=followingFrame_.requested?followingFrame_:openingFrame_;
        if(!active.requested && incomingFrame_.publishCue){active.requested=true;active.cue=incomingFrame_.cue;}
        // Joining updates the timer in the existing native entry, not its
        // immutable insertion receipt. Later objectives retain that deadline.
        if(active.requested && joinedClock_ && !clock_.project(active.cue))return fail();
        if(travel_.requested()){active.requested=true;active.cue=travel_.ticket().request;}
        if(active.requested) {
            if(nextCues.count==nextCues.entries.size())return fail();
            for(std::size_t i=0;i<nextCues.count;++i)if(nextCues.entries[i].registry==active.cue.registry && nextCues.entries[i].slot==active.cue.slot)return fail();
            nextCues.entries[nextCues.count++]=active.cue;
        }
        placements=nextPlacements;cues=nextCues;engagements=nextEngagements;return true;
    }
    bool fail() noexcept {failed_=true;return false;}
    struct Driver final:opening_coo::Services {
        InitialRuntime& owner;explicit Driver(InitialRuntime& value):owner(value){}
        bool publish(const opening_coo::Command& c) noexcept override {
            if(c.spec.asset.type==70){if(owner.engagementRequested_)return false;owner.engagementRequested_=true;owner.engagementToken_=c.token;return true;}
            for(std::size_t i=0;i<owner.definition_->gates.size();++i)if(c.spec.asset.slot==owner.tickets_[i].slot) {
                if(owner.requested_[i] || !deferred_bridge::bind(owner.tickets_[i]))return false;
                owner.requested_[i]=true;owner.tokens_[i]=c.token;return true;
            }return false;
        }
        void cancel(const opening_coo::Command&) noexcept override {}
    };
    const InitialDefinition* definition_{};population::Owner owner_{};std::uint64_t boot_{},revision_{},selection_{},event_{};
    std::array<deferred_placement::Ticket,4> tickets_{};std::array<opening_coo::Token,4> tokens_{};
    std::array<bool,4> requested_{},ready_{};opening_coo::Executor executor_;opening_coo::Token engagementToken_{};
    EngagementRuntime engagement_;OpeningRuntime opening_;OpeningFrame openingFrame_{};
    OpeningRuntime following_;OpeningFrame followingFrame_{};bool followingStarted_{};
    dialogue::Runtime incoming_;bool incomingStarted_{},incomingFailed_{},joinedVoiceStarted_{};
    sequence::Runtime intro_;Clock clock_;bool introStarted_{};
    world_object::Runtime world_;bool worldStarted_{};
    music::Runtime music_;bool musicStarted_{};
    keys::Runtime keys_;bool keysStarted_{};
    directive::Runtime travel_;bool travelStarted_{};
    IncomingRuntime incomingUi_;IncomingFrame incomingFrame_{};bool joinedClock_{};
    std::uint64_t incomingCueDelay_{},incomingDuration_{};
    std::uint64_t travelDuration_{},rotationDelay_{},activationDelay_{},transitStart_{};
    OpeningDefinition openingDefinition_{},followingDefinition_{};
    std::uint64_t introDelay_{},voiceDelay_{},openingDuration_{};
    bool enabled_{},started_{},failed_{},conflicting_{},engagementRequested_{},engagementReady_{},openingStarted_{};
};
}
