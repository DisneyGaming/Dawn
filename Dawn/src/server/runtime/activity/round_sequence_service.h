#pragma once
#include "public_event_sequence_runtime.h"
#include "timed_round_service.h"

namespace dawn::server::runtime::activity::round_sequence {
namespace sequence=public_event::sequence;
struct Binding final {
    const sequence::Definition* definition{};
    std::uint16_t phases{};
};
[[nodiscard]] constexpr std::uint16_t phase_mask(timed_round::Phase phase) noexcept {
    return static_cast<std::uint16_t>(1U<<static_cast<unsigned>(phase));
}
[[nodiscard]] inline bool valid(std::span<const Binding> bindings) noexcept {
    if(bindings.size()>sequence::wire::kCapacity)return false;
    for(std::size_t i=0;i<bindings.size();++i) {
        const auto& b=bindings[i];
        if(!b.definition || !b.phases || !sequence::Runtime::valid(*b.definition))return false;
        for(std::size_t j=0;j<i;++j)
            if(bindings[j].definition->registry->key==b.definition->registry->key
                && bindings[j].definition->slot==b.definition->slot)return false;
    }
    return true;
}
// Optional presentation of authored sequences at qualified round transitions.
// Native entities own their effects/timing; sequence publication never claims
// a teleport arrival, enemy death, or any other gameplay receipt.
class Service final {
public:
    [[nodiscard]] bool update(std::span<const Binding> bindings,sequence::Context context,
        std::uint64_t round,timed_round::Phase phase) noexcept {
        if(!valid(bindings))return false;
        if(!round || !context.arrived || !context.admitted)return true;
        if(owner_ && (owner_!=context.owner || boot_!=context.boot))return false;
        owner_=context.owner;boot_=context.boot;
        for(std::size_t i=0;i<bindings.size();++i) {
            if(!(bindings[i].phases&phase_mask(phase)) || rounds_[i]==round)continue;
            if(rounds_[i]>round)return false;
            context.event=round;
            context.generation=static_cast<std::uint8_t>(1+(round-1)%254);
            sequence::Runtime next;
            if(!next.begin(*bindings[i].definition,context) || !next.update(context))return false;
            runtimes_[i]=next;rounds_[i]=round;
        }
        return true;
    }
    [[nodiscard]] bool append(sequence::wire::Batch& output) const noexcept {
        for(const auto& runtime:runtimes_)if(!runtime.append(output))return false;
        return true;
    }
private:
    sequence::Owner owner_{};std::uint64_t boot_{};
    std::array<sequence::Runtime,sequence::wire::kCapacity> runtimes_{};
    std::array<std::uint64_t,sequence::wire::kCapacity> rounds_{};
};
}
