#pragma once
#include "../../../middleware/bap/activity_message/native/lost_sector_shield_authority.h"
#include "native_activity_definition.h"
#include <memory>

namespace sunrise::server::runtime::activity {
struct NativeActivityFrame final {
    population::wire::Batch populations{};
    placement::wire::Batch placements{};
    middleware::bap::activity_message::native::lost_sector_shield::Batch lostSectorShields{};
    npc_animation::wire::Batch animations{};
    adventure::OpeningFrame opening{};
    activity_clock::Publication clock{};
    forest_generator::wire::Batch generators{};
    cue_presentation::wire::Batch cues{};
    world_device::wire::Batch devices{};
    public_event::engagement_feedback::wire::Batch engagements{};
    adventure::dialogue_feedback::wire::Batch dialogues{};
    public_event::sequence::wire::Batch sequences{};
    public_event::participant_feedback::wire::Batch eventParticipants{};
    music::wire::Batch music{};
};
// One authoritative activity incarnation. The document outlives both executors
// and their commands. Graph completion leaves persistent services active; it
// does not end free roam or manufacture a native completion receipt.
class PersistentActivity final {
public:
    using Document=coo::script::MissionDocument;
    [[nodiscard]] static const NativeAction* action(const NativeActivityDefinition& definition,
        const coo::CommandSpec& spec) noexcept {
        const NativeAction* found{};
        for(const auto& candidate:definition.actions) {
            const auto& expected=candidate.command;
            if(expected.operation==spec.operation && expected.asset==spec.asset
                && expected.argument==spec.argument && expected.wait==spec.wait) {
                if(found) return nullptr;found=&candidate;
            }
        }
        return found;
    }
    [[nodiscard]] static bool valid(const NativeActivityDefinition& definition,const Document& document) noexcept {
        const auto& views=document.views();
        if(!definition.profile || definition.bubble>63 || definition.placements.size()>32 || !views.valid
            || views.missionId!=definition.activity || views.profileId!=definition.profile->id
            || views.mission.modules.size()!=1 || !views.mission.observations.empty()
            || views.mission.modules[0].asset!=definition.persistentModule.asset
            || views.mission.modules[0].id!=definition.persistentModule.id) return false;
        const auto* graph=views.role("persistent");
        if(!graph || graph->domain!="nativeActivity" || graph->definition.schema!=definition.profile->schema
            || !coo::MissionRuntime::valid(views.mission)) return false;
        std::array<ambient_population::InitialPolicy,32> initial{};std::size_t initialCount{};
        if(!ambient_population::configure_initial(definition,document,definition.ambientInitial,initial,initialCount)) return false;
        if(!public_event::RallyRuntime::valid(definition,document))return false;
        if(!public_event::InitialRuntime::valid(definition,document))return false;
        if(!adventure::OpeningRuntime::valid(definition,document))return false;
        if(!native_capture::Runtime::valid(definition,document))return false;
        if(!definition.directives.empty()) {
            if(definition.directiveSource.type!=68 || !definition.directiveSource.definition)return false;
            cue_presentation::Service probe;
            if(!probe.begin({1,{1}},1,definition.directives))return false;
            for(const auto& item:definition.directives) {
                if(item.presentation.registry!=definition.directiveSource.registry
                    || item.presentation.slot!=definition.directiveSource.slot
                    || (item.requiredEvidence&~3U)
                    || ((item.requiredEvidence&1U) && definition.captures.empty())
                    || ((item.requiredEvidence&2U) && definition.generators.empty())
                    || (item.timer!=cue_presentation::TimerOperation::absent && definition.clockFrequencyParameter.empty()))return false;
            }
        }
        if(definition.devices.size()>world_device::wire::kCapacity)return false;
        for(const auto& capability:definition.devices) {
            if(!world_device::valid(capability) || capability.registry->bubble!=definition.bubble)return false;
            bool registered{};
            for(const auto& item:definition.registries)if(&item==capability.registry)registered=true;
            if(!registered)return false;
        }
        if(definition.generators.size()>4)return false;
        for(const auto& capability:definition.generators) {
            if(!forest_generator::valid(capability) || capability.registry->bubble!=definition.bubble)return false;
            bool registered{};
            for(const auto& item:definition.registries)if(&item==capability.registry)registered=true;
            if(!registered || (!capability.seedParameter.empty() && !views.parameter(capability.seedParameter)))return false;
            for(const auto& names:capability.anchorParameters)for(const auto name:{names.column,names.height})
                if(!name.empty()) {
                    const auto* value=views.parameter(name);
                    if(!value || value->value>127)return false;
                }
        }
        if(!definition.clockFrequencyParameter.empty()) {
            const auto* frequency=views.parameter(definition.clockFrequencyParameter);
            if(!frequency || !frequency->value || frequency->value>1000 || definition.registries.empty())return false;
        }
        if(definition.occupancyWaits.size()>32)return false;
        for(const auto& binding:definition.occupancyWaits) {
            if(!occupancy_wait::valid(binding) || binding.registry->bubble!=definition.bubble)return false;
            bool registered{};
            for(const auto& item:definition.registries)if(&item==binding.registry)registered=true;
            if(!registered)return false;
        }
        if(definition.animations.size()>16) return false;
        for(const auto& capability:definition.animations) {
            if(!npc_animation::valid(capability) || capability.registry->bubble!=definition.bubble) return false;
            // The controller must belong to the same admitted registry profile.
            bool registered{};
            for(const auto& registry:definition.registries)
                if(&registry==capability.registry) registered=true;
            if(!registered) return false;
        }
        std::array<coo::Asset,32> seen{};std::array<bool,4> seenCaptures{};std::size_t used{};
        for(const auto& step:graph->definition.steps) for(const auto& spec:step.commands) {
            const auto* route=action(definition,spec);
            if(!route || used==seen.size()) return false;
            if(spec.operation==coo::Operation::observation) {
                if(spec.wait!=coo::Wait::observed || route->capability>=definition.occupancyWaits.size()
                    || !route->countParameter.empty() || spec.argument!=1)return false;
                const auto& binding=definition.occupancyWaits[route->capability];
                if(spec.asset.registry!=binding.registry->key || spec.asset.slot!=binding.slot || spec.asset.type!=30)return false;
                bool descriptor{};
                for(const auto& slot:binding.registry->slots)if(slot.index==binding.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor)return false;
            } else if(spec.operation==coo::Operation::mechanic && spec.asset.type==4) {
                if(spec.wait!=coo::Wait::completed || route->capability>=definition.captures.size()
                    || seenCaptures[route->capability] || spec.argument!=1 || !route->countParameter.empty())return false;
                seenCaptures[route->capability]=true;
                const auto& binding=definition.captures[route->capability];
                const auto& placement=definition.placements[binding.placement];
                if(spec.asset.registry!=placement.registry->key || spec.asset.slot!=placement.slot)return false;
                bool descriptor{};for(const auto& slot:placement.registry->slots)if(slot.index==placement.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor)return false;
            } else if(spec.wait!=coo::Wait::requested)return false;
            for(std::size_t i=0;i<used;++i) if(seen[i]==spec.asset
                && !(spec.operation==coo::Operation::mechanic && spec.asset.type==4)
                && spec.operation!=coo::Operation::objective
                && !(spec.operation==coo::Operation::device && spec.asset.type==23)) return false;
            seen[used++]=spec.asset;
            if(spec.operation==coo::Operation::device && spec.asset.type==4) {
                if(route->capability>=definition.placements.size() || !route->countParameter.empty()) return false;
                const auto& cap=definition.placements[route->capability];placement::wire::Batch output;
                if(!placement::project({&cap,1},definition.bubble,output) || output.count!=1
                    || spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot || spec.asset.type!=4) return false;
                bool descriptor{};
                for(const auto& slot:cap.registry->slots) if(slot.index==cap.slot) descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor) return false;
            } else if(spec.operation==coo::Operation::device && spec.asset.type==23) {
                if(route->capability>=definition.devices.size() || !route->countParameter.empty())return false;
                const auto& capability=definition.devices[route->capability];
                if(spec.asset.registry!=capability.registry->key || spec.asset.slot!=capability.slot)return false;
                bool descriptor{},supported{};
                for(const auto& slot:capability.registry->slots)if(slot.index==capability.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                for(const auto& candidate:capability.actions)if(candidate.id==spec.argument)supported=true;
                if(!descriptor || !supported)return false;
            } else if(spec.operation==coo::Operation::population) {
                if(route->capability>=definition.populations.size()) return false;
                const auto& cap=definition.populations[route->capability];
                const auto* count=views.parameter(route->countParameter);
                if(!population::valid(cap) || cap.registry->bubble!=definition.bubble || !count || !count->value || count->value>63
                    || spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot || spec.asset.type!=1) return false;
                bool descriptor{};
                for(const auto& slot:cap.registry->slots) if(slot.index==cap.slot) descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor) return false;
            } else if(spec.operation==coo::Operation::objective) {
                if(spec.asset!=definition.directiveSource || route->capability>=definition.directives.size()
                    || spec.argument!=definition.directives[route->capability].id)return false;
                if(!route->countParameter.empty()) {
                    const auto* duration=views.parameter(route->countParameter);
                    if(!duration || !duration->value || duration->value>3600000
                        || definition.directives[route->capability].timer!=cue_presentation::TimerOperation::start)return false;
                }
            } else if(spec.operation==coo::Operation::mechanic && spec.asset.type==37) {
                if(route->capability>=definition.generators.size() || !route->countParameter.empty())return false;
                const auto& cap=definition.generators[route->capability];
                if(spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot)return false;
                bool descriptor{},supported{};
                for(const auto& slot:cap.registry->slots)if(slot.index==cap.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                for(const auto& candidate:cap.actions)if(candidate.id==spec.argument)supported=true;
                if(!descriptor || !supported)return false;
            } else if(spec.operation==coo::Operation::mechanic && spec.asset.type==42) {
                if(route->capability>=definition.animations.size() || !route->countParameter.empty()) return false;
                const auto& cap=definition.animations[route->capability];
                if(spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot) return false;
                bool descriptor{},action{};
                for(const auto& slot:cap.registry->slots)
                    if(slot.index==cap.slot) descriptor=slot.descriptorTag==spec.asset.definition;
                for(const auto& candidate:cap.actions)
                    if(candidate.id==spec.argument && candidate.retailVerified) action=true;
                if(!descriptor || !action) return false;
            } else if(spec.operation!=coo::Operation::observation
                && !(spec.operation==coo::Operation::mechanic && spec.asset.type==4))return false;
        }
        return true;
    }
    [[nodiscard]] bool begin(population::Owner owner,const NativeActivityDefinition& definition,
        std::shared_ptr<const Document> document,std::uint64_t boot) noexcept {
        if(document_ || !document || !valid(definition,*document)) return false;
        population::Service population;
        npc_animation::Service animation;
        ambient_population::InitialActivation<> ambient;
        public_event::RallyRuntime rally;
        public_event::InitialRuntime publicInitial;
        adventure::OpeningRuntime opening;
        occupancy_wait::Service occupancy;
        native_capture::Runtime capture;
        forest_generator::Service generator;
        world_device::Service device;
        cue_presentation::Service presentation;
        std::array<cue_presentation::Action,8> directives{};
        if(definition.directives.size()>directives.size())return false;
        std::copy(definition.directives.begin(),definition.directives.end(),directives.begin());
        for(const auto& route:definition.actions)if(route.command.operation==coo::Operation::objective && !route.countParameter.empty()) {
            const auto* duration=document->views().parameter(route.countParameter);
            if(route.capability>=definition.directives.size() || !duration
                || !activity_clock::wire::from_milliseconds(duration->value,directives[route.capability].durationTicks))return false;
        }
        std::array<std::uint32_t,4> seeds{};
        std::array<forest_generator::AnchorConfiguration,4> anchors{};
        for(std::size_t i=0;i<definition.generators.size();++i) {
            const auto name=definition.generators[i].seedParameter;
            if(!name.empty())seeds[i]=document->views().parameter(name)->value;
            for(std::size_t j=0;j<definition.generators[i].anchorParameters.size();++j) {
                const auto& names=definition.generators[i].anchorParameters[j];
                if(!names.column.empty())anchors[i][j].column=document->views().parameter(names.column)->value;
                if(!names.height.empty())anchors[i][j].height=document->views().parameter(names.height)->value;
            }
        }
        std::array<ambient_population::InitialPolicy,32> initial{};std::size_t initialCount{};
        if(!population.begin(owner,definition.populations,boot)
            || (!definition.animations.empty() && !animation.begin(owner,boot,definition.animations))
            || !ambient_population::configure_initial(definition,*document,definition.ambientInitial,initial,initialCount)
            || !rally.begin(owner,boot,definition,*document)
            || !publicInitial.begin(owner,boot,definition,*document)
            || !opening.begin(owner,boot,definition,*document)
            || !occupancy.begin(owner,boot,definition.occupancyWaits)
            || !capture.begin(definition,*document)
            || !generator.begin(owner,boot,definition.generators,std::span(seeds).first(definition.generators.size()),
                std::span(anchors).first(definition.generators.size()))
            || !device.begin(owner,boot,definition.devices)
            || (!definition.directives.empty() && !presentation.begin(owner,boot,std::span(directives).first(definition.directives.size())))
            || (initialCount && !ambient.begin(owner,boot,std::span(initial).first(initialCount)))) return false;
        population_=std::move(population);animation_=std::move(animation);ambient_=std::move(ambient);rally_=std::move(rally);
        opening_=std::move(opening);publicInitial_=std::move(publicInitial);
        occupancy_=std::move(occupancy);
        capture_=std::move(capture);
        generator_=std::move(generator);
        device_=std::move(device);
        presentation_=std::move(presentation);
        definition_=&definition;document_=std::move(document);return true;
    }
    [[nodiscard]] NativeActivityFrame update(std::uint32_t bubble,bool arrived,
        const adventure_start::wire::Request& selected={},bool openingAdmissionReady=true,
        activity_clock::Publication clock={},public_event::participant_feedback::LocalIdentity local={}) noexcept {
        if(!document_)return {};
        if(clock && (clock.domain.owner!=population_.owner() || clock.domain.boot!=population_.boot()
            || clock.domain.bubble!=definition_->bubble || definition_->registries.empty()
            || clock.domain.scenario!=definition_->registries.front().scenario))return {};
        NativeActivityFrame frame{};
        capture_.clock(clock);
        clock_=clock;
        if(arrived && bubble==definition_->bubble) {
            Ports ports(*this);
            const coo::MissionInput input{population_.owner().incarnation.value,0,static_cast<int>(bubble*8U),true,true};
            frame=composition_.update<NativeActivityFrame>(document_->views().mission,input,ports);
        } else if(definition_->retainRosterOrdinals) {
            // A region selection is not activity retirement. Publishing empty
            // bodies here resets native source generations and placement state
            // even when their roster descriptors and owners are still live.
            if(!project_authority(frame))return {};
        }
        frame.opening=opening_.update(bubble,arrived && openingAdmissionReady,selected);frame.clock=clock;
        if(!publicInitial_.update(bubble,arrived,openingAdmissionReady,selected,population_,frame.placements,frame.cues,frame.engagements,rally_.used(),clock,local))return {};
        if(!publicInitial_.append_participants(frame.eventParticipants))return {};
        if(!publicInitial_.append_dialogues(frame.dialogues))return {};
        if(!publicInitial_.append_sequences(frame.sequences))return {};
        if(!publicInitial_.append_music(frame.music))return {};
        if(!publicInitial_.append_world(frame.placements,frame.devices))return {};
        return frame;
    }
    // Named-point-dependent sources are owned by the finite initial gate. A
    // development command must not bypass its parameter, occupancy or readiness.
    [[nodiscard]] population::Result request_population(const population::Command& command,std::uint32_t bubble) noexcept {
        if(!definition_ || publicInitial_.owns(command.registry,command.slot))return population::Result::unsupported;
        for(const auto& binding:definition_->ambientInitial) {
            if(!binding.namedDependency || binding.capability>=definition_->populations.size())continue;
            const auto& capability=definition_->populations[binding.capability];
            if(capability.registry->key==command.registry && capability.slot==command.slot)
                return population::Result::unsupported;
        }
        return population_.request(command,bubble);
    }
    [[nodiscard]] population::Service& population() noexcept {return population_;}
    [[nodiscard]] const population::Service& population() const noexcept {return population_;}
    [[nodiscard]] npc_animation::Service& animation() noexcept {return animation_;}
    [[nodiscard]] const npc_animation::Service& animation() const noexcept {return animation_;}
    [[nodiscard]] ambient_population::InitialActivation<>& ambient() noexcept {return ambient_;}
    [[nodiscard]] const ambient_population::InitialActivation<>& ambient() const noexcept {return ambient_;}
    [[nodiscard]] const public_event::RallyRuntime& rally() const noexcept {return rally_;}
    [[nodiscard]] public_event::InitialRuntime& public_initial() noexcept {return publicInitial_;}
    [[nodiscard]] const public_event::InitialRuntime& public_initial() const noexcept {return publicInitial_;}
    [[nodiscard]] const adventure::OpeningRuntime& opening() const noexcept {return opening_;}
    [[nodiscard]] const NativeActivityDefinition* definition() const noexcept {return definition_;}
    [[nodiscard]] const native_capture::Runtime& capture() const noexcept {return capture_;}
    [[nodiscard]] const forest_generator::Service& generator() const noexcept {return generator_;}
    [[nodiscard]] const world_device::Service& device() const noexcept {return device_;}
    [[nodiscard]] const cue_presentation::Service& presentation() const noexcept {return presentation_;}
    [[nodiscard]] bool observe_occupancy(population::Owner owner,std::uint64_t boot,std::uint32_t bubble,
        const ambient_population::sense::SenseObject& object) noexcept {
        return occupancy_.observe(owner,boot,bubble,object,[this](coo::Event event) noexcept {
            return executor_.enqueue(event);
        });
    }
    /** Pure projection of requests already accepted by this activity's script. */
    [[nodiscard]] placement::wire::Batch placements() const noexcept {
        placement::wire::Batch output{};
        if(!definition_) return output;
        std::array<placement::Capability,32> active{};std::size_t count{};
        for(std::size_t i=0;i<definition_->placements.size();++i)
            if(placements_[i]) active[count++]=definition_->placements[i];
        static_cast<void>(placement::project(std::span(active).first(count),definition_->bubble,output));
        if(!rally_.append(definition_->bubble,output))return {};
        if(!capture_.append(output) || !publicInitial_.append(output))return {};
        adventure_start::restrict_banners(definition_->startRoutes,output);
        return output;
    }
    [[nodiscard]] std::uint64_t fingerprint() const noexcept {return document_?document_->fingerprint():0;}
    [[nodiscard]] coo::Diagnostics diagnostics() const noexcept {return executor_.diagnostics();}
private:
    // Project accepted owner-scoped requests without running the graph, starting
    // a population, or consuming a regional trigger in a different bubble.
    [[nodiscard]] bool project_authority(NativeActivityFrame& frame) const noexcept {
        frame.populations=population_.project(definition_->bubble);
        frame.animations=animation_.project(definition_->bubble);
        frame.generators=generator_.project(definition_->bubble);
        frame.devices=device_.project(definition_->bubble);
        frame.cues=presentation_.project();
        frame.placements={};
        std::array<placement::Capability,32> active{};std::size_t count{};
        for(std::size_t i=0;i<definition_->placements.size();++i)
            if(placements_[i])active[count++]=definition_->placements[i];
        return placement::project(std::span(active).first(count),definition_->bubble,frame.placements)
            && rally_.append(definition_->bubble,frame.placements) && capture_.append(frame.placements);
    }
    struct Driver final:coo::Services {
        PersistentActivity& owner;explicit Driver(PersistentActivity& value):owner(value){}
        bool publish(const coo::Command& command) noexcept override {
            const auto* route=action(*owner.definition_,command.spec);if(!route) return false;
            if(command.spec.operation==coo::Operation::observation)
                return owner.occupancy_.arm(route->capability,command.token);
            if(command.spec.operation==coo::Operation::device && command.spec.asset.type==23) {
                auto& device=owner.device_;
                if(device.last_request()==UINT64_MAX)return false;
                const auto result=device.request({device.owner(),device.boot(),device.revision(),
                    device.last_request()+1,command.spec.asset.registry,command.spec.argument,command.spec.asset.slot},
                    owner.definition_->bubble);
                return result==world_device::Result::accepted || result==world_device::Result::unchanged;
            }
            if(command.spec.operation==coo::Operation::device) {
                owner.placements_[route->capability]=true;return true;
            }
            if(command.spec.operation==coo::Operation::mechanic && command.spec.asset.type==4)
                return owner.capture_.start(route->capability,command.token);
            if(command.spec.operation==coo::Operation::objective) {
                auto& presentation=owner.presentation_;
                if(presentation.last_request()==UINT64_MAX)return false;
                std::uint32_t evidence{};
                bool captured=owner.capture_.size()!=0;
                for(std::size_t i=0;i<owner.capture_.size();++i)captured&=owner.capture_.state(i).completed;
                if(captured)evidence|=1U;
                const auto generators=owner.generator_.project(owner.definition_->bubble);
                bool requested=generators.count!=0 && generators.count==owner.definition_->generators.size();
                for(std::size_t i=0;i<generators.count;++i)requested&=generators.entries[i].state.primary.enabled;
                if(requested)evidence|=2U;
                const auto result=presentation.request({presentation.owner(),presentation.boot(),presentation.revision(),
                    presentation.last_request()+1,command.spec.argument,evidence,
                    owner.clock_?owner.clock_.elapsedTicks:UINT64_MAX});
                return result==cue_presentation::Result::accepted || result==cue_presentation::Result::unchanged;
            }
            if(command.spec.operation==coo::Operation::mechanic && command.spec.asset.type==37) {
                auto& generator=owner.generator_;
                if(generator.last_request()==UINT64_MAX)return false;
                const auto result=generator.request({generator.owner(),generator.boot(),generator.revision(),
                    generator.last_request()+1,command.spec.asset.registry,command.spec.argument,command.spec.asset.slot},
                    owner.definition_->bubble);
                return result==forest_generator::Result::accepted || result==forest_generator::Result::unchanged;
            }
            if(command.spec.operation==coo::Operation::mechanic && command.spec.asset.type==42) {
                auto& animation=owner.animation_;
                if(animation.last_request()==UINT64_MAX) return false;
                const npc_animation::Command request{animation.owner(),animation.boot(),animation.revision(),
                    animation.last_request()+1,command.spec.asset.registry,command.spec.argument,
                    command.spec.asset.slot,false,false};
                return animation.request(request,owner.definition_->bubble)==npc_animation::Result::accepted;
            }
            if(command.spec.operation!=coo::Operation::population) return false;
            const auto* count=owner.document_->views().parameter(route->countParameter);if(!count) return false;
            auto& source=owner.population_;
            if(source.last_request()==UINT64_MAX) return false;
            const auto result=source.request({source.owner(),source.revision(),source.last_request()+1,
                command.spec.asset.registry,command.spec.asset.slot,static_cast<std::uint8_t>(count->value),source.boot()},owner.definition_->bubble);
            return result==population::Result::accepted || result==population::Result::unchanged;
        }
        // Failure cancels future graph requests. Keep already-published native
        // leases until activity teardown; cancellation is not actor retirement.
        void cancel(const coo::Command& command) noexcept override {owner.occupancy_.cancel(command.token);}
    };
    struct Ports final:coo::MissionPorts<NativeActivityFrame> {
        PersistentActivity& owner;explicit Ports(PersistentActivity& value):owner(value){}
        void update_module(std::uint32_t module,const coo::MissionInput& input,NativeActivityFrame& frame) noexcept override {
            if(module!=owner.definition_->persistentModule.id) return;
            if(!owner.started_) {
                owner.started_=owner.executor_.start(owner.document_->views().role("persistent")->definition,input.run);
                if(!owner.started_) return;
            }
            Driver driver(owner);
            if(!owner.capture_.poll([&](coo::Event event) noexcept {return owner.executor_.enqueue(event);})) {
                owner.executor_.cancel(driver);return;
            }
            owner.executor_.update(driver);
            for(std::size_t i=0;i<owner.ambient_.size();++i)
                if(owner.ambient_.state(i)==ambient_population::InitialState::eligible)
                    static_cast<void>(owner.ambient_.activate(i,owner.population_,owner.definition_->bubble));
            if(!owner.rally_.update(owner.definition_->bubble,frame.placements)
                || !owner.project_authority(frame))frame={};
        }
        std::uint32_t observations(std::uint64_t,const NativeActivityFrame&) noexcept override {return 0;}
    };
    const NativeActivityDefinition* definition_{};
    std::shared_ptr<const Document> document_;
    population::Service population_;
    npc_animation::Service animation_;
    ambient_population::InitialActivation<> ambient_;
    public_event::RallyRuntime rally_;
    public_event::InitialRuntime publicInitial_;
    adventure::OpeningRuntime opening_;
    occupancy_wait::Service occupancy_;
    native_capture::Runtime capture_;
    forest_generator::Service generator_;
    world_device::Service device_;
    cue_presentation::Service presentation_;
    activity_clock::Publication clock_{};
    coo::MissionRuntime composition_;
    coo::Executor executor_;
    std::array<bool,32> placements_{};
    bool started_{};
};
}
