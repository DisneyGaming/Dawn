#pragma once
#include "public_event_placement_feedback.h"

namespace sunrise::server::runtime::activity::public_event::rally_use {
inline constexpr std::size_t kBytes=0x2E8;
inline constexpr std::uint32_t kProducerRva=0xF36640;
struct Binding final {
    placement_feedback::Ticket ticket{};
    std::uint64_t epoch{};
    std::uint32_t entity{UINT32_MAX};
};
struct Receipt final {
    Binding binding{};
    std::uint32_t controller{UINT32_MAX},requester{UINT32_MAX},playerEntity{UINT32_MAX};
    std::int32_t completed{};
};
// Original F36640 consumes the pending request at +2DC into +2D8 and applies
// the authored one-shot state at +2D0. Prompt evaluation/range/placement alone
// never satisfy this receipt. The caller retains the lease before that original
// call, resolves its requester, then copies the same component after return.
[[nodiscard]] inline bool qualify(const Binding& binding,std::span<const std::byte> before,
    std::span<const std::byte> after,std::uint32_t requester,std::uint32_t playerEntity,
    std::uint32_t producer,Receipt& output) noexcept {
    using placement_feedback::field;
    const auto& d=binding.ticket.definition;
    if(!binding.epoch || !binding.ticket.lease.valid() || !d.interactionDefinition || d.interactionOffset<=0
        || !d.enableInteractionAfterPlacement || binding.entity==UINT32_MAX || requester==UINT32_MAX
        || playerEntity==UINT32_MAX || producer!=kProducerRva || before.size()!=kBytes || after.size()!=kBytes)return false;
    const auto controller=field<std::uint32_t>(before,0x24);
    if(controller==UINT32_MAX || field<std::uint32_t>(before,0)!=d.interactionDefinition
        || field<std::uint32_t>(before,4)!=0x80804FB2 || field<std::int64_t>(before,8)!=d.interactionOffset
        || field<std::uint32_t>(before,0x2C)!=binding.entity || before[0x2C0]!=std::byte{}
        || before[0x2D0]!=std::byte{} || after[0x2D0]!=std::byte{1}
        || std::memcmp(before.data(),after.data(),16)!=0
        || field<std::uint32_t>(after,0x24)!=controller || field<std::uint32_t>(after,0x2C)!=binding.entity
        || std::memcmp(before.data()+0x2E0,after.data()+0x2E0,8)!=0)return false;
    const auto requested=field<std::int32_t>(before,0x2DC),consumed=field<std::int32_t>(before,0x2D8);
    if(consumed<0 || requested<=consumed || field<std::int32_t>(after,0x2D8)!=requested
        || field<std::int32_t>(after,0x2DC)!=requested)return false;
    output={binding,controller,requester,playerEntity,requested};return true;
}
}
