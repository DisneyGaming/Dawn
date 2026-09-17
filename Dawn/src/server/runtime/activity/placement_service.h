#pragma once
#include "registry_admission.h"
#include "../../../middleware/bap/activity_message/native/placement_authority.h"

namespace dawn::server::runtime::activity::placement {
namespace wire=middleware::bap::activity_message::native::placement;
namespace interaction=middleware::bap::activity_message::native::interaction;
struct Capability final {
    const registry::Definition* registry{};std::uint16_t slot{};
    std::uint32_t generation{};
    interaction::Mode interactionMode{interaction::Mode::unchanged};
};
// Invoked only after the owning activity has admitted the exact registry.
// Persistent placements keep the native activation tuple stable across refreshes.
[[nodiscard]] inline bool project(std::span<const Capability> definitions,std::uint32_t bubble,
    wire::Batch& output) noexcept {
    output={};wire::Batch result{};
    for(const auto& capability:definitions) {
        if(!capability.registry || !registry::valid(*capability.registry)
            || !wire::valid_generation(capability.generation)
            || !interaction::valid(capability.interactionMode)) return false;
        const auto& definition=*capability.registry;
        unsigned matches{};
        for(const auto& slot:definition.slots)
            if(slot.index==capability.slot && slot.type==4 && slot.componentClass==0x80809927
                && slot.senseSchema==0x8080992E && slot.authSchema==0x8080992F) ++matches;
        if(matches!=1) return false;
        if(definition.bubble!=bubble) continue;
        for(std::size_t i=0;i<result.count;++i)
            if(result.entries[i].registry==definition.key && result.entries[i].slot==capability.slot) return false;
        if(result.count==result.entries.size()) return false;
        auto& request=result.entries[result.count++];
        request.registry=definition.key;request.slot=capability.slot;request.bubble=definition.bubble;
        request.generation=capability.generation;request.interactionMode=capability.interactionMode;
    }
    output=result;return true;
}
}
