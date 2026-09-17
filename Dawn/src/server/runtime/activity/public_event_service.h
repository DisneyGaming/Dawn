#pragma once
#include "registry_admission.h"
#include "../../../state/activity/coo/native_services.h"
#include "../../../state/activity/lifecycle_generation.h"

namespace dawn::server::runtime::activity::public_event {
namespace coo = state::activity::coo;

enum class Stage : std::uint8_t { rally, encounter, completion };
enum class Phase : std::uint8_t { idle, rallyPending, rallyReady, active, completing, complete, retiring, retired, failed };
enum class Result : std::uint8_t { accepted, stale, duplicate, invalid, unavailable, wrongPhase };
struct Lease final {
    state::activity::ActivityInstanceKey owner{};
    std::uint64_t boot{}, revision{}, event{};
    [[nodiscard]] bool valid() const noexcept { return owner && boot && revision && event; }
    friend bool operator==(const Lease&, const Lease&) = default;
};
struct HeroicBinding final { coo::Asset asset{}; std::uint32_t condition{}; };
struct Definition final {
    std::string_view id;
    std::span<const registry::Definition> registries;
    const coo::Definition* rally{};
    const coo::Definition* encounter{};
    const coo::Definition* completion{};
    const coo::Definition* heroicCompletion{};
    HeroicBinding heroic{};
};
struct Receipt final { Lease lease{}; Stage stage{}; coo::Asset asset{}; coo::Event event{}; };
struct HeroicReceipt final { Lease lease{}; coo::Asset asset{}; std::uint32_t condition{}; };
struct StartRequest final { Lease lease{}; std::uint64_t request{}; };
// Values come from the authoritative activity owner, not a client assertion.
struct Eligibility final { std::uint8_t bubble{}; bool registriesAdmitted{}, resourcesReserved{}; };
// Only a native adapter with a proven retirement barrier may supply this.
// Death, source consumption, network release and owner departure are insufficient.
struct Retirement final {
    Lease lease{};
    bool sourcesQuiesced{}, placementsRetired{}, observationsDrained{};
    std::size_t residentActors{};
};
struct Ports {
    virtual bool publish(const Lease&, Stage, const coo::Command&) noexcept = 0;
    // Requests cancellation; never asserts native retirement.
    virtual void cancel(const Lease&, Stage, const coo::Command&) noexcept = 0;
    virtual ~Ports() = default;
};
struct Diagnostics final {
    Phase phase{};
    Lease lease{};
    std::uint64_t lastStartRequest{}, rejected{};
    bool encounterAvailable{}, heroicAvailable{}, heroic{};
    std::array<coo::Diagnostics,3> graphs{};
};

// The shared UE executor owns sequencing. This wrapper adds independent event
// lifetime, full activity/boot/revision ownership, optional completion branches,
// and explicit retirement. It never ends the containing free-roam activity.
// The owner serializes all methods; definition storage must remain immutable.
class Service final {
public:
    [[nodiscard]] static bool asset_valid(const Definition& definition, coo::Asset asset) noexcept {
        unsigned matches{};
        for (const auto& registry : definition.registries) if (registry.key == asset.registry)
            for (const auto& slot : registry.slots)
                if (slot.index == asset.slot && slot.type == asset.type && slot.descriptorTag == asset.definition) ++matches;
        return matches == 1;
    }
    [[nodiscard]] static bool graph_valid(const Definition& definition, const coo::Definition* graph,
                                          bool requiresNativeTerminal) noexcept {
        if (!graph || !coo::Executor::valid(*graph)) return false;
        if (definition.rally && graph->schema!=definition.rally->schema) return false;
        bool nativeTerminal{};
        for (std::size_t i=0; i<graph->steps.size(); ++i) {
            bool terminal=true;
            for (std::size_t j=i+1; j<graph->steps.size(); ++j)
                if (graph->steps[j].dependencies & (std::uint32_t{1} << i)) terminal=false;
            bool nativeInStep{};
            for (const auto& command : graph->steps[i].commands) {
                if (!asset_valid(definition, command.asset)) return false;
                nativeInStep |= command.wait == coo::Wait::completed || command.wait == coo::Wait::observed;
            }
            if (terminal) {
                if (requiresNativeTerminal && !nativeInStep) return false;
                nativeTerminal |= nativeInStep;
            }
        }
        return !requiresNativeTerminal || nativeTerminal;
    }
    [[nodiscard]] static bool valid(const Definition& definition) noexcept {
        if (definition.id.empty() || definition.registries.empty() || !definition.rally) return false;
        const auto& first=definition.registries.front();
        for (std::size_t i=0; i<definition.registries.size(); ++i) {
            const auto& row=definition.registries[i];
            if (!registry::valid(row) || row.activity!=first.activity || row.scenario!=first.scenario
                || row.bubble!=first.bubble || row.bubbleHash!=first.bubbleHash) return false;
            for (std::size_t j=0; j<i; ++j) if (definition.registries[j].key==row.key) return false;
        }
        if (!graph_valid(definition, definition.rally, false)) return false;
        // Rally readiness cannot be established just by accepted publication.
        for (const auto& step : definition.rally->steps) for (const auto& command : step.commands)
            if (command.wait==coo::Wait::requested) return false;
        if (definition.encounter && !graph_valid(definition, definition.encounter, true)) return false;
        if (definition.completion && (!definition.encounter || !graph_valid(definition, definition.completion, true))) return false;
        if (definition.heroic.condition && (!definition.encounter || !asset_valid(definition, definition.heroic.asset))) return false;
        if (definition.heroicCompletion && (!definition.heroic.condition || !graph_valid(definition, definition.heroicCompletion, true))) return false;
        return true;
    }
    [[nodiscard]] bool begin(Lease lease, const Definition& definition) noexcept {
        // Do not make a retired instance reusable by recycling its old lease.
        // The owner creates a new Service only after an actual retirement barrier.
        if (phase_!=Phase::idle || !lease.valid() || !valid(definition)) return false;
        definition_=&definition; lease_=lease;
        if (!executors_[0].start(*definition.rally, lease.event)) return false;
        phase_=Phase::rallyPending; return true;
    }
    [[nodiscard]] Result start(StartRequest request, Eligibility eligibility) noexcept {
        if (request.lease!=lease_ || !lease_.valid()) return reject(Result::stale);
        if (!request.request) return reject(Result::invalid);
        if (request.request<=lastStart_) return reject(Result::duplicate);
        if (!definition_->encounter) return reject(Result::unavailable);
        if (phase_!=Phase::rallyReady) return reject(Result::wrongPhase);
        if (!eligibility.registriesAdmitted || !eligibility.resourcesReserved
            || eligibility.bubble!=definition_->registries.front().bubble) return reject(Result::invalid);
        if (!executors_[1].start(*definition_->encounter, lease_.event)) return reject(Result::invalid);
        lastStart_=request.request; phase_=Phase::active; return Result::accepted;
    }
    [[nodiscard]] Result observe(const Receipt& receipt) noexcept {
        const auto index=static_cast<std::size_t>(receipt.stage);
        if (receipt.lease!=lease_ || !lease_.valid()) return reject(Result::stale);
        if (index>=executors_.size() || phase_==Phase::retiring || phase_==Phase::retired || phase_==Phase::failed)
            return reject(Result::wrongPhase);
        const auto* graph=graph_for(receipt.stage);
        const auto& token=receipt.event.token;
        if (!graph || token.step>=graph->steps.size() || token.command>=graph->steps[token.step].commands.size()
            || graph->steps[token.step].commands[token.command].asset!=receipt.asset) return reject(Result::invalid);
        return executors_[index].enqueue(receipt.event)?Result::accepted:reject(Result::stale);
    }
    [[nodiscard]] Result heroic(const HeroicReceipt& receipt) noexcept {
        if (receipt.lease!=lease_ || !lease_.valid()) return reject(Result::stale);
        if (!definition_->heroic.condition) return reject(Result::unavailable);
        if (phase_!=Phase::active) return reject(Result::wrongPhase);
        if (receipt.asset!=definition_->heroic.asset || receipt.condition!=definition_->heroic.condition)
            return reject(Result::invalid);
        if (heroic_) return Result::duplicate;
        heroic_=true; return Result::accepted;
    }
    void update(Ports& ports) noexcept {
        if (phase_==Phase::rallyPending) {
            run(Stage::rally, ports);
            if (executors_[0].diagnostics().phase==coo::Phase::complete) phase_=Phase::rallyReady;
        } else if (phase_==Phase::active) {
            run(Stage::encounter, ports);
            if (executors_[1].diagnostics().phase==coo::Phase::complete) {
                const auto* completion=graph_for(Stage::completion);
                if (!completion) phase_=Phase::complete;
                else if (executors_[2].start(*completion, lease_.event)) phase_=Phase::completing;
                else phase_=Phase::failed;
            }
        } else if (phase_==Phase::completing) {
            run(Stage::completion, ports);
            if (executors_[2].diagnostics().phase==coo::Phase::complete) phase_=Phase::complete;
        }
        if (phase_==Phase::failed) cancel_commands(ports);
    }
    [[nodiscard]] Result retire(Lease lease, Ports& ports) noexcept {
        if (lease!=lease_ || !lease_.valid()) return reject(Result::stale);
        if (phase_==Phase::retiring || phase_==Phase::retired) return Result::duplicate;
        cancel_commands(ports); phase_=Phase::retiring; return Result::accepted;
    }
    [[nodiscard]] Result retired(const Retirement& receipt) noexcept {
        if (receipt.lease!=lease_ || !lease_.valid()) return reject(Result::stale);
        if (phase_!=Phase::retiring) return reject(Result::wrongPhase);
        if (!receipt.sourcesQuiesced || !receipt.placementsRetired || !receipt.observationsDrained
            || receipt.residentActors) return reject(Result::invalid);
        phase_=Phase::retired; return Result::accepted;
    }
    [[nodiscard]] coo::Token token(Stage stage, std::string_view receipt) const noexcept {
        const auto index=static_cast<std::size_t>(stage);
        return index<executors_.size()?executors_[index].token(receipt):coo::Token{};
    }
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        Diagnostics result{phase_,lease_,lastStart_,rejected_,definition_ && definition_->encounter,
            definition_ && definition_->heroic.condition,heroic_,{}};
        for (std::size_t i=0; i<executors_.size(); ++i) result.graphs[i]=executors_[i].diagnostics();
        return result;
    }
private:
    struct Driver final : coo::NativeServices<Driver> {
        Service& owner; Ports& ports; Stage stage;
        Driver(Service& o, Ports& p, Stage s) : owner(o), ports(p), stage(s) {}
        [[nodiscard]] coo::ServiceContext context() const noexcept {
            const auto d=owner.executors_[static_cast<std::size_t>(stage)].diagnostics();
            return {*owner.graph_for(stage),d.run,d.incarnation};
        }
        [[nodiscard]] bool request(const coo::Command& command) noexcept { return ports.publish(owner.lease_,stage,command); }
        void retire(const coo::Command& command) noexcept { ports.cancel(owner.lease_,stage,command); }
    };
    [[nodiscard]] const coo::Definition* graph_for(Stage stage) const noexcept {
        if (!definition_) return nullptr;
        switch (stage) {
        case Stage::rally: return definition_->rally;
        case Stage::encounter: return definition_->encounter;
        case Stage::completion: return heroic_ && definition_->heroicCompletion?definition_->heroicCompletion:definition_->completion;
        }
        return nullptr;
    }
    void run(Stage stage, Ports& ports) noexcept {
        auto& executor=executors_[static_cast<std::size_t>(stage)];
        Driver driver(*this,ports,stage); executor.update(driver);
        if (executor.diagnostics().phase==coo::Phase::failed) phase_=Phase::failed;
    }
    void cancel_commands(Ports& ports) noexcept {
        for (std::size_t i=executors_.size(); i-->0;) if (graph_for(static_cast<Stage>(i))) {
            Driver driver(*this,ports,static_cast<Stage>(i)); executors_[i].cancel(driver);
        }
    }
    [[nodiscard]] Result reject(Result result) noexcept { ++rejected_; return result; }
    const Definition* definition_{};
    Lease lease_{};
    std::array<coo::Executor,3> executors_{};
    Phase phase_{};
    std::uint64_t lastStart_{},rejected_{};
    bool heroic_{};
};
} // namespace dawn::server::runtime::activity::public_event
