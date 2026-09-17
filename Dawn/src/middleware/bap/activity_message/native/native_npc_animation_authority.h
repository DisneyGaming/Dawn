#pragma once
#include "retained_authority_scope.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::middleware::bap::activity_message::native::npc_animation {
inline constexpr std::uint32_t kSchema=0x80809586U;
inline constexpr std::uint32_t kRuntimeClass=0x80809583U;
inline constexpr std::uint32_t kEmptyHash=0x811C9DC5U;
inline constexpr std::size_t kBodyBits=101;

// Native 80809589 sequence tuple. completion is the queried event/state key,
// not a second clip. Empty completion keeps the completion query unsatisfied.
struct Control final {
    std::uint32_t sequence{kEmptyHash},completion{kEmptyHash};
    std::uint32_t counter{};
    friend constexpr bool operator==(Control,Control) noexcept=default;
};
[[nodiscard]] constexpr bool valid(Control value) noexcept {
    return value.counter<=0x7FFFFFFFU
        && (value.sequence!=kEmptyHash ? value.counter!=0 : value.completion==kEmptyHash);
}
// 80809588 has a three-bit count, capacity four (80809587). Each 8080958A
// element is two unconditional u32s: sequence and event. A0F4A1 publishes
// these as native 5E requests in group AFB11A12; they are retained state.
struct Event final {std::uint32_t sequence{},event{};};
[[nodiscard]] constexpr std::size_t body_bits(std::size_t events=0) noexcept {return kBodyBits+64U*events;}
template<class Writer> [[nodiscard]] bool write(Writer& writer,Control value,std::span<const Event> events={}) noexcept {
    if(!valid(value) || events.size()>4) return false;
    for(std::size_t i=0;i<events.size();++i) {
        if(events[i].sequence==kEmptyHash || events[i].event==kEmptyHash) return false;
        for(std::size_t j=0;j<i;++j)
            if(events[i].sequence==events[j].sequence && events[i].event==events[j].event) return false;
    }
    const auto start=writer.bit_count();
    if(!writer.write(1,1) || !writer.write(value.sequence,32)
        || !writer.write(value.completion,32) || !writer.write(value.counter+0x80000000U,32)
        || !writer.write(1,1) || !writer.write(events.size(),3)) return false;
    for(const auto& event:events)
        if(!writer.write(event.sequence,32) || !writer.write(event.event,32)) return false;
    return writer.bit_count()-start==body_bits(events.size());
}
struct Request final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    Control control{};
};
struct Batch final { std::array<Request,16> entries{};std::size_t count{}; };
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t registry,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=42 || batch.count>batch.entries.size()) return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].registry==registry && batch.entries[i].slot==slot) return &batch.entries[i];
    return nullptr;
}
template<class Roster> [[nodiscard]] bool valid(const Batch& batch,const Roster& roster,
    std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size()) return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& value=batch.entries[i];
        if(!value.registry || value.registry==UINT32_MAX || value.registry==kEmptyHash
            || value.slot>32767 || value.bubble>63 || !authority_scope::valid(roster,value.registry,value.bubble,region) || !valid(value.control)) return false;
        for(std::size_t j=0;j<i;++j)
            if(batch.entries[j].registry==value.registry && batch.entries[j].slot==value.slot) return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& group=roster.groups[g];if(group.key!=value.registry) continue;
            ++groups;
            if(group.slotTypes.size()!=group.slotIndices.size() || group.slotFlags.size()!=group.slotIndices.size()) return false;
            for(std::size_t s=0;s<group.slotIndices.size();++s) if(group.slotIndices[s]==value.slot) {
                if(group.slotTypes[s]!=42 || group.slotFlags[s]!=2) return false;
                ++slots;
            }
        }
        if(groups!=1 || slots!=1) return false;
    }
    return true;
}
} // namespace dawn::middleware::bap::activity_message::native::npc_animation
