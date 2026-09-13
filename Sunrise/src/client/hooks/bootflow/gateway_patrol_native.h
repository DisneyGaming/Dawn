#pragma once
#include "../../../state/activity/gateway/patrol_loop.h"
#include <cstring>

namespace sunrise::client::hooks::bootflow::gateway_patrol {
// A false native result permits an untouched output buffer. Use the retained
// command for holding actors and publish only an actual patrol change.
template<class Read,class Redirect> bool publish_point(Read& read,std::uintptr_t context,
    void* command,bool changed,Redirect redirect) noexcept {
    std::uintptr_t actor{},previous{};
    if(!read.value(context+8,actor)) return changed;
    if(!changed && (!read.value(context+0x18,previous) || !previous)) return changed;
    alignas(16) std::array<std::byte,0xB0> candidate{},before{};
    if(!read.value(changed?reinterpret_cast<std::uintptr_t>(command):previous,candidate)) return changed;
    before=candidate;
    if(!redirect(actor,reinterpret_cast<std::uintptr_t>(candidate.data())) || candidate==before) return changed;
    std::memcpy(command,candidate.data(),candidate.size());
    return true;
}
// Re-resolve the source at the native movement producer. Never use an actor
// index alone: all three salted identities, source definition and generations
// must still belong to the currently admitted Gateway marcher.
template<class Read> bool owns(Read& read,std::uintptr_t image,std::uintptr_t actor,
    const state::activity::gateway::EnemyReceipt& owner) noexcept {
    using namespace state::activity::gateway;
    if(!owner.valid() || patrol::index(owner.registry,owner.source)==patrol::kActors) return false;
    std::uintptr_t table{},source{},definition{},parent{},entities{};
    std::uint32_t stride{},self{},sourceHandle{},kind{},generation{},committed{},entity{},parentHandle{},aiActor{},aiEntity{},registry{},flags{};
    std::int64_t offset{},definitionOffset{};std::uint32_t definitionHandle{};
    std::uint8_t type{};std::uint16_t slot{};
    if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000
        || actor!=table+std::uintptr_t(owner.actor&0x1FFFU)*stride
        || !read.value(actor+0x48,self) || self!=owner.actor
        || !read.value(actor+0x38,sourceHandle) || sourceHandle!=owner.owner
        || !read.value(actor+0x40,offset) || offset<0 || offset>0x1000000
        || !read.resolve(sourceHandle,source)) return false;
    source+=static_cast<std::uintptr_t>(offset);
    if(!read.value(source+4,kind) || kind!=0x8080948FU
        || !read.value(source+0x1FC,generation) || generation!=owner.generation
        || !read.value(source+0x244,committed) || committed!=generation
        || !read.value(source,definitionHandle) || !read.value(source+8,definitionOffset)
        || definitionOffset<0 || definitionOffset>0x1000000 || !read.resolve(definitionHandle,definition)) return false;
    definition+=static_cast<std::uintptr_t>(definitionOffset);
    if(!read.value(definition+0x30,registry) || registry!=owner.registry
        || !read.value(definition+0x34,type) || type!=1
        || !read.value(definition+0x36,slot) || slot!=owner.source
        || !read.value(actor+0x4C,entity) || entity==UINT32_MAX
        || !read.value(actor+0x50,parentHandle) || parentHandle==UINT32_MAX
        || !read.resolve(parentHandle,parent) || !read.value(parent+4,kind) || kind!=0x808082ECU
        || !read.value(parent+0x24,self) || self!=parentHandle
        || !read.value(parent+0x2C,aiEntity) || aiEntity!=entity
        || !read.value(parent+0x1470,aiActor) || aiActor!=owner.actor
        || !read.value(image+0x1F93428,entities) || !read.value(image+0x1F93430,stride)
        || stride<0x50 || stride>0x100000) return false;
    const auto row=entities+std::uintptr_t(entity&0x1FFFU)*stride;
    return read.value(row+0xC,self) && self==entity && read.value(row+4,flags) && !(flags&4U);
}
}
