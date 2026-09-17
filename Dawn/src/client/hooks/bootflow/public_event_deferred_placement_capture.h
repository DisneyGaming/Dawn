#pragma once
#include "../../../server/runtime/activity/public_event_deferred_placement_feedback.h"
#include "../../../server/runtime/activity/public_event_deferred_placement_bridge.h"
#include "adventure_cue_native_identity.h"

namespace dawn::client::hooks::bootflow::public_event_deferred_placement_capture {
namespace feedback=server::runtime::activity::public_event::deferred_placement;
using Identity=adventure_cue_native_identity::Identity;
using Binding=server::runtime::activity::public_event::deferred_bridge::Binding;
struct Context final {
    Binding binding{};Identity self{};
    std::uintptr_t component{},definitionAddress{},visualAddress{},authorityAddress{};
    std::uint32_t authority{UINT32_MAX};
    std::array<std::byte,feedback::kComponentBytes> before{};
    std::array<std::byte,0x98> definition{};
    std::array<std::byte,144> visual{};
    std::array<std::byte,0x70> authorityObject{};
    std::array<std::byte,feedback::kAuthorityBytes> authorityBody{};
};
enum class Result:std::uint8_t {accepted,binding,component,self,definition,visual,authority,body,identity};
template<class Reader,class Resolver,class Self,class Body>
[[nodiscard]] bool identity(const Context& c,Reader&& read,Resolver&& resolve,Self&& self,Body&& body) noexcept {
    Identity current{};std::uintptr_t address{};
    std::array<std::byte,16> header{};std::array<std::byte,4> handle{};
    std::array<std::byte,0x98> definition{};std::array<std::byte,144> visual{};
    std::array<std::byte,0x70> authority{};std::array<std::byte,feedback::kAuthorityBytes> authorityBody{};
    return read(c.component,std::span(header)) && std::equal(header.begin(),header.end(),c.before.begin())
        && self(c.component,current) && current==c.self
        && read(c.component+0x170,std::span(handle)) && feedback::field<std::uint32_t>(handle,0)==c.authority
        && resolve(c.binding.ticket.definition,c.binding.ticket.definitionOffset,address) && address==c.definitionAddress
        && read(address,std::span(definition)) && definition==c.definition
        && read(c.visualAddress,std::span(visual)) && visual==c.visual
        && resolve(c.authority,0,address) && address==c.authorityAddress
        && read(address,std::span(authority)) && authority==c.authorityObject
        && body(address,std::span(authorityBody)) && authorityBody==c.authorityBody;
}
// Reads only. Self must use the full current common-pool identity; Body must use
// the qualified native authority-body layout. Neither is inferred from +160.
template<class Reader,class Resolver,class Self,class Body>
[[nodiscard]] Result begin(const Binding& binding,std::uintptr_t component,Reader&& read,Resolver&& resolve,
    Self&& self,Body&& body,Context& out) noexcept {
    out={};Context c{};
    if(!binding.epoch || !feedback::valid(binding.ticket))return Result::binding;
    const auto& t=binding.ticket;
    if(component<0x10000 || component>UINTPTR_MAX-feedback::kComponentBytes
        || !read(component,std::span(c.before)))return Result::component;
    if(feedback::field<std::uint32_t>(c.before,0)!=t.definition || feedback::field<std::uint32_t>(c.before,4)!=0x80809928
        || feedback::field<std::int64_t>(c.before,8)!=t.definitionOffset)return Result::component;
    if(!self(component,c.self) || c.self.componentLink!=feedback::field<std::uint32_t>(c.before,0x20)
        || c.self.source.member==UINT32_MAX || c.self.source.offset<0 || c.self.source.offset>=0x2000000)return Result::self;
    if(!resolve(t.definition,t.definitionOffset,c.definitionAddress) || c.definitionAddress<0x10000
        || c.definitionAddress>UINTPTR_MAX-0x2000000 || !read(c.definitionAddress,std::span(c.definition)))return Result::definition;
    const auto& d=c.definition;
    if(feedback::field<std::uint32_t>(d,0)!=t.definition || feedback::field<std::uint32_t>(d,4)!=0x80809927
        || feedback::field<std::uint32_t>(d,0x30)!=t.registry || feedback::field<std::uint8_t>(d,0x34)!=4
        || feedback::field<std::uint16_t>(d,0x36)!=t.slot || feedback::field<std::uint32_t>(d,0x38)!=t.bubble
        || feedback::field<std::uint64_t>(d,0x58)!=1 || feedback::field<std::uint8_t>(d,0x94)!=1)return Result::definition;
    const auto relative=feedback::field<std::int64_t>(d,0x60);
    if(relative<0 || relative>0x1FFF00)return Result::visual;
    const auto header=c.definitionAddress+0x60+static_cast<std::uintptr_t>(relative);
    std::array<std::byte,16> array{};
    if(!read(header,std::span(array)) || feedback::field<std::uint64_t>(array,0)!=1
        || feedback::field<std::uint32_t>(array,8)!=0x808099D8)return Result::visual;
    c.visualAddress=header+16;
    if(!read(c.visualAddress,std::span(c.visual)) || feedback::field<std::uint32_t>(c.visual,0)!=t.entityDefinition
        || feedback::field<std::uint64_t>(c.visual,0x70)!=t.pointGuid)return Result::visual;
    c.authority=feedback::field<std::uint32_t>(c.before,0x170);
    if(c.authority==UINT32_MAX || !resolve(c.authority,0,c.authorityAddress)
        || !read(c.authorityAddress,std::span(c.authorityObject)))return Result::authority;
    const auto& a=c.authorityObject;
    if(feedback::field<std::uint32_t>(a,0)!=t.registry || feedback::field<std::uint8_t>(a,4)!=4
        || feedback::field<std::uint16_t>(a,6)!=t.slot || feedback::field<std::uint32_t>(a,0xC)!=0x8080992F
        || feedback::field<std::uint8_t>(a,0x18)!=0 || feedback::field<std::uint32_t>(a,0x68)!=t.bubble)return Result::authority;
    if(!body(c.authorityAddress,std::span(c.authorityBody)) || !feedback::authored_body(c.authorityBody,t.generation)
        || !std::equal(c.authorityBody.begin(),c.authorityBody.end(),c.before.begin()+0x180))return Result::body;
    c.binding=binding;c.component=component;
    if(!identity(c,read,resolve,self,body))return Result::identity;
    out=c;return Result::accepted;
}
// Brackets the unchanged native creator. Weak must validate the full salt and
// current live child, and read its exact pool row. A successful factory return
// alone cannot populate an Observation. The enclosing mailbox rechecks epoch.
template<class Reader,class Resolver,class Self,class Body,class Weak>
[[nodiscard]] bool finish(const Context& c,std::uintptr_t component,bool created,std::uint64_t sequence,
    Reader&& read,Resolver&& resolve,Self&& self,Body&& body,Weak&& weak,feedback::Observation& out) noexcept {
    out={};if(!c.binding.epoch || !created || component!=c.component || !sequence || !identity(c,read,resolve,self,body))return false;
    std::array<std::byte,feedback::kComponentBytes> after{},fresh{};
    std::array<std::byte,0x98> entity{},freshEntity{};std::uint32_t child=UINT32_MAX,freshChild=UINT32_MAX;
    if(!read(component,std::span(after)) || !weak(std::span(after),child,entity)
        || !identity(c,read,resolve,self,body) || !read(component,std::span(fresh)) || fresh!=after
        || !weak(std::span(fresh),freshChild,freshEntity) || child!=freshChild || entity!=freshEntity)return false;
    const feedback::Capture capture{c.binding.ticket,{c.self.source.member,c.self.componentLink,c.self.source.offset},sequence,
        feedback::kProducerRva,child,c.before,after,c.authorityObject,c.authorityBody,c.definition,c.visual,entity,true,true,true,created};
    return feedback::qualify(c.binding.ticket,capture,out)==feedback::Result::accepted;
}
// Refreshes only an independently accepted creation. It never synthesizes a
// creation receipt from a later body, matching generation or visible geometry.
template<class Reader,class Resolver,class Self,class Body,class Weak>
[[nodiscard]] bool retained(const Context& c,const feedback::Observation& creation,Reader&& read,Resolver&& resolve,
    Self&& self,Body&& body,Weak&& weak,std::array<std::byte,0x98>& entity) noexcept {
    entity={};std::uint32_t child=UINT32_MAX;
    if(c.binding.ticket!=creation.ticket || c.self.source.member!=creation.source.member
        || c.self.source.offset!=creation.source.offset || c.self.componentLink!=creation.source.componentLink
        || !identity(c,read,resolve,self,body) || feedback::field<std::uint64_t>(c.before,0x440)!=creation.weakChild
        || feedback::field<std::uint32_t>(c.before,0x2F0)!=creation.ticket.generation
        || !weak(std::span(c.before),child,entity) || child!=creation.child)return false;
    return feedback::field<std::uint64_t>(entity,0x90)==creation.ticket.pointGuid;
}
} // namespace dawn::client::hooks::bootflow::public_event_deferred_placement_capture
