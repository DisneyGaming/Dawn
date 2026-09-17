#pragma once
#include "../../../server/runtime/activity/public_event_participant_bridge.h"
#include "adventure_cue_native_identity.h"

namespace dawn::client::hooks::bootflow::public_event_participant_capture {
namespace bridge=server::runtime::activity::public_event::participant_bridge;
namespace feedback=bridge::feedback;
using Identity=adventure_cue_native_identity::Identity;
struct Context final {
    bridge::Binding binding{};
    Identity self{};
    std::uintptr_t component{},packet{},packetBody{};
    std::uint32_t authority{UINT32_MAX};
    std::array<std::byte,16> packetHeader{};
    std::array<std::byte,0x60> definition{};
    std::array<std::byte,0x70> authorityObject{};
    std::array<std::byte,feedback::kComponentBytes> prior{};
    feedback::wire::Decoded incoming{};
    feedback::LocalIdentity local{};
};
enum class Result:std::uint8_t {accepted,binding,component,definition,self,authority,packet,identity};
template<class Reader,class Resolver,class Self>
[[nodiscard]] bool identity(const Context& context,Reader&& read,Resolver&& resolve,Self&& self) noexcept {
    std::array<std::byte,16> header{};
    std::array<std::byte,0x60> definition{};
    std::array<std::byte,0x70> authority{};
    std::array<std::byte,4> handle{};
    std::uintptr_t address{};Identity observed{};
    return read(context.component,std::span(header))
        && std::equal(header.begin(),header.end(),context.prior.begin())
        && self(context.component,observed) && observed==context.self
        && read(context.component+0x170,std::span(handle)) && feedback::field<std::uint32_t>(handle,0)==context.authority
        && resolve(context.authority,0,address) && read(address,std::span(authority)) && authority==context.authorityObject
        && resolve(context.binding.ticket.definition,context.binding.ticket.definitionOffset,address)
        && read(address,std::span(definition)) && definition==context.definition;
}
template<class Reader,class Resolver,class Self>
[[nodiscard]] Result begin(const bridge::Binding& binding,std::uintptr_t component,std::uintptr_t packet,
    Reader&& read,Resolver&& resolve,Self&& self,Context& out) noexcept {
    out={};Context captured{};
    if(!binding.epoch || !feedback::valid(binding.ticket))return Result::binding;
    const auto& ticket=binding.ticket;
    if(component<0x10000 || component>UINTPTR_MAX-feedback::kComponentBytes
        || !read(component,std::span(captured.prior)))return Result::component;
    if(feedback::field<std::uint32_t>(captured.prior,0)!=ticket.definition
        || feedback::field<std::uint32_t>(captured.prior,4)!=0x80804F4A
        || feedback::field<std::int64_t>(captured.prior,8)!=ticket.definitionOffset)return Result::component;
    std::uintptr_t address{};
    if(!resolve(ticket.definition,ticket.definitionOffset,address) || !read(address,std::span(captured.definition)))return Result::definition;
    const auto& definition=captured.definition;
    if(feedback::field<std::uint32_t>(definition,0x30)!=ticket.request.registry
        || feedback::field<std::uint8_t>(definition,0x34)!=71
        || feedback::field<std::uint16_t>(definition,0x36)!=ticket.request.slot
        || feedback::field<std::uint32_t>(definition,0x38)!=ticket.nativeScope
        || feedback::field<std::uint32_t>(definition,0x48)!=0x80804F57)return Result::definition;
    if(!self(component,captured.self) || captured.self.componentLink!=feedback::field<std::uint32_t>(captured.prior,0x20)
        || captured.self.source.member==UINT32_MAX || captured.self.source.offset<0
        || captured.self.source.offset>=0x2000000)return Result::self;
    captured.authority=feedback::field<std::uint32_t>(captured.prior,0x170);
    if(captured.authority==UINT32_MAX || !resolve(captured.authority,0,address)
        || !read(address,std::span(captured.authorityObject)))return Result::authority;
    const auto& authority=captured.authorityObject;
    if(feedback::field<std::uint32_t>(authority,0)!=ticket.request.registry
        || feedback::field<std::uint8_t>(authority,4)!=71
        || feedback::field<std::uint16_t>(authority,6)!=ticket.request.slot
        || feedback::field<std::uint32_t>(authority,0xC)!=0x80804F57
        || feedback::field<std::uint8_t>(authority,0x18)!=0
        || feedback::field<std::uint32_t>(authority,0x68)!=ticket.nativeScope)return Result::authority;
    if(!read(packet,std::span(captured.packetHeader)))return Result::packet;
    captured.packetBody=feedback::field<std::uintptr_t>(captured.packetHeader,8);
    if(!read(captured.packetBody,std::span(captured.incoming))
        || !feedback::wire::matches_fields(captured.incoming,ticket.request))return Result::packet;
    captured.binding=binding;captured.component=component;captured.packet=packet;
    if(!identity(captured,read,resolve,self))return Result::identity;
    out=captured;return Result::accepted;
}
// Called only after the unchanged originalBF5AA0 callback. Two stable snapshots
// and exact native identity bracket qualification; no native function is called.
template<class Reader,class Resolver,class Self>
[[nodiscard]] bool finish(const Context& context,std::uintptr_t component,std::uint64_t sequence,
    Reader&& read,Resolver&& resolve,Self&& self,const feedback::LocalIdentity& localAfter,feedback::Observation& out) noexcept {
    out={};
    if(!context.binding.epoch || component!=context.component || !sequence || !identity(context,read,resolve,self))return false;
    std::array<std::byte,feedback::kComponentBytes> after{},fresh{};
    std::array<std::byte,16> packet{};feedback::wire::Decoded incoming{};
    if(!read(component,std::span(after)) || !read(context.packet,std::span(packet)) || packet!=context.packetHeader
        || !read(context.packetBody,std::span(incoming)) || incoming!=context.incoming
        || !identity(context,read,resolve,self) || !read(component,std::span(fresh)) || fresh!=after)return false;
    const feedback::Capture capture{context.binding.ticket,context.self.source,sequence,feedback::kProducerRva,
        context.prior,after,incoming,context.authorityObject,context.local,localAfter,true};
    return feedback::qualify(context.binding.ticket,capture,out)==feedback::Result::accepted;
}
}
