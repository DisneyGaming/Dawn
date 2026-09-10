#pragma once
#include "forest_generator_service.h"
#include "../../../middleware/bap/activity_message/sense_update.h"

namespace sunrise::server::runtime::activity::generator_progress {
namespace generator=forest_generator;
namespace wire=generator::wire;
namespace sense=middleware::bap::activity_message::sense_update;
namespace typed=middleware::bap::activity_message::native::forest_generator_sense;
using Owner=generator::Owner;
enum class Result : std::uint8_t {accepted,unchanged,unbound,stale,invalid,noDelta,exhausted};
struct Snapshot final {
    std::uint32_t registry{},definition{},seed{},nativeRevision{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    std::uint64_t acceptedRevision{},openedGroups{};
    std::uint32_t clearedAreas{};
    bool observed{};
};
// A native progress mirror only. It issues no gameplay request and exposes no
// inferred percentage, total area/actor count, branch or mission completion.
class Observer final {
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot) noexcept {
        if(owner_ || !owner || !boot)return false;
        owner_=owner;boot_=boot;return true;
    }
    [[nodiscard]] Result bind(const generator::Service& accepted,
        const generator::Capability& capability,std::uint32_t bubble) noexcept {
        if(!owner_ || accepted.owner()!=owner_ || accepted.boot()!=boot_)return Result::stale;
        if(!generator::valid(capability) || capability.registry->bubble!=bubble)return Result::invalid;
        const auto batch=accepted.project(bubble);
        const auto* request=wire::find(batch,capability.registry->key,37,capability.slot);
        if(!request)return Result::unbound;
        if(!active(*request))return Result::invalid;
        const auto hash=typed::recipe_hash(request->state);
        std::uint32_t definition{};
        for(const auto& slot:capability.registry->slots)if(slot.index==request->slot)definition=slot.descriptorTag;
        for(std::size_t i=0;i<count_;++i) {
            auto& b=bindings_[i];
            if(b.snapshot.registry!=request->registry || b.snapshot.slot!=request->slot)continue;
            if(b.snapshot.definition!=definition || b.snapshot.bubble!=bubble)return Result::invalid;
            if(accepted.revision()<b.snapshot.acceptedRevision)return Result::stale;
            if(b.recipeHash==hash && b.snapshot.seed==request->state.primary.seed) {
                b.snapshot.acceptedRevision=accepted.revision();return Result::unchanged;
            }
            // Retain the source's native revision floor across host requests;
            // an old packet cannot become new just because the policy changed.
            b.snapshot.seed=request->state.primary.seed;b.snapshot.acceptedRevision=accepted.revision();
            b.snapshot.openedGroups=0;b.snapshot.clearedAreas=0;b.snapshot.observed=false;
            b.recipeHash=hash;b.bodyBits=wire::body_bits(request->state)+33;return Result::accepted;
        }
        if(count_==bindings_.size())return Result::exhausted;
        bindings_[count_++]={{request->registry,definition,request->state.primary.seed,0,request->slot,
            request->bubble,accepted.revision(),0,0,false},hash,wire::body_bits(request->state)+33};
        return Result::accepted;
    }
    // owner/boot/bubble must be captured from the receiving activity context,
    // not supplied by packet fields or reacquired from a later current activity.
    [[nodiscard]] Result observe(Owner owner,std::uint64_t boot,std::uint32_t bubble,
        const generator::Service& accepted,const sense::SenseObject& object) noexcept {
        if(!owner_ || owner!=owner_ || boot!=boot_ || accepted.owner()!=owner_ || accepted.boot()!=boot_)return Result::stale;
        if(object.slotType!=37)return Result::unbound;
        Binding* binding{};
        for(std::size_t i=0;i<count_;++i)
            if(bindings_[i].snapshot.registry==object.registryKey && bindings_[i].snapshot.slot==object.slotIndex)binding=&bindings_[i];
        if(!binding)return Result::unbound;
        auto& prior=binding->snapshot;
        if(prior.bubble!=bubble || accepted.revision()<prior.acceptedRevision)return Result::stale;
        const auto batch=accepted.project(bubble);const auto* request=wire::find(batch,prior.registry,37,prior.slot);
        if(!request || !active(*request) || request->state.primary.seed!=prior.seed
            || typed::recipe_hash(request->state)!=binding->recipeHash)return Result::stale;
        if(!object.hasNativeSchema || object.nativeSchema!=typed::kSchema || object.inferredBodyWidth
            || object.revision!=object.nativeRevision)return Result::invalid;
        if(!object.hasRootDelta || !object.hasGeneratorProgress)return Result::noDelta;
        const auto& reported=object.generatorProgress;
        if(object.bodyBits!=binding->bodyBits || !reported.canonicalFlags
            || reported.primarySeed!=prior.seed || reported.reportedSeed!=prior.seed
            || reported.recipeHash!=binding->recipeHash || !reported.primaryEnabled
            || (reported.primaryOverrides&(wire::Seed|wire::Enabled))!=(wire::Seed|wire::Enabled))return Result::invalid;
        if(object.nativeRevision<=prior.nativeRevision)return Result::stale;
        prior.nativeRevision=object.nativeRevision;prior.openedGroups=reported.openedGroups;
        prior.clearedAreas=reported.clearedAreas;prior.observed=true;return Result::accepted;
    }
    [[nodiscard]] const Snapshot* find(std::uint32_t registry,std::uint16_t slot) const noexcept {
        for(std::size_t i=0;i<count_;++i)if(bindings_[i].snapshot.registry==registry && bindings_[i].snapshot.slot==slot)return &bindings_[i].snapshot;
        return nullptr;
    }
    [[nodiscard]] std::size_t size() const noexcept {return count_;}
private:
    [[nodiscard]] static bool active(const wire::Request& request) noexcept {
        return wire::valid(request.state) && request.state.primary.enabled
            && (request.state.primary.overrides&(wire::Seed|wire::Enabled))==(wire::Seed|wire::Enabled);
    }
    struct Binding final {Snapshot snapshot{};std::uint64_t recipeHash{};std::size_t bodyBits{};};
    std::array<Binding,4> bindings_{};std::size_t count_{};Owner owner_{};std::uint64_t boot_{};
};
}
