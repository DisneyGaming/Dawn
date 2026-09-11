#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include "../../../state/activity/strike_bond/frame.h"

namespace sunrise::client::hooks::bootflow::strike_bond_fire_trace {
namespace mission=state::activity::strike_bond;
struct Identity {
    std::uintptr_t character{};
    std::uint32_t self{UINT32_MAX},entity{UINT32_MAX};
    friend bool operator==(const Identity&,const Identity&)=default;
};
template<class T,std::size_t N> T field(const std::array<std::byte,N>& bytes,std::size_t offset) noexcept {
    T out{};std::memcpy(&out,bytes.data()+offset,sizeof out);return out;
}
inline bool admitted(const mission::BossRequest& r) noexcept {
    return r.frame.enabled && !r.frame.finished && !r.frame.bossDead && r.owner.valid() && r.enemy.valid()
        && r.enemy.run==r.owner.run && r.enemy.generation==r.owner.value
        && r.enemy.source==3 && r.enemy.registry==mission::kBossActor.registry;
}
// This is observation only. Validate a salted component, its actor/source lease,
// and the native entity->character-reference path. A resource tag alone is not
// sufficient. Do not retain a raw pointer for use in another native callback.
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t input,bool controller,
                                const mission::BossRequest& request,Identity& out) noexcept {
    if(!admitted(request)) return false;
    std::array<std::byte,0x30> header{},again{};std::uintptr_t resolved{};
    if(!read.copy(input,header)
        || field<std::uint32_t>(header,0)!=(controller?0x80F66F56U:0x80F459A3U)
        || field<std::uint32_t>(header,4)!=(controller?0x80806832U:0x80803A00U)) return false;
    const auto self=field<std::uint32_t>(header,0x24),entity=field<std::uint32_t>(header,0x2C);
    if(self==UINT32_MAX || entity==UINT32_MAX || !read.resolve(self,resolved) || resolved!=input) return false;
    std::uintptr_t table{},source{},sourceBase{},definition{},entities{},reference{},character{};
    std::uint32_t stride{},value{},sourceHandle{},refHandle{},charHandle{},entityFlags{},definitionHandle{};
    std::int64_t sourceOffset{},definitionOffset{};
    if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride) || stride<0x70 || stride>0x100000) return false;
    const auto actor=table+static_cast<std::uintptr_t>(request.enemy.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x48,value) || value!=request.enemy.actor || !read.value(actor+0x4C,value) || value!=entity
        || !read.value(actor+0x38,sourceHandle) || sourceHandle!=request.enemy.owner
        || !read.value(actor+0x40,sourceOffset) || sourceOffset<0 || sourceOffset>0x1000000
        || !read.resolve(sourceHandle,sourceBase)) return false;
    source=sourceBase+static_cast<std::uintptr_t>(sourceOffset);
    if(!read.value(source,definitionHandle) || definitionHandle!=0x80F54740U
        || !read.value(source+4,value) || value!=0x8080948FU
        || !read.value(source+0x1FC,value) || value!=request.enemy.generation
        || !read.value(source+0x244,value) || value!=request.enemy.generation
        || !read.value(source+8,definitionOffset) || definitionOffset<0 || definitionOffset>0x1000000
        || !read.resolve(definitionHandle,definition)) return false;
    definition+=static_cast<std::uintptr_t>(definitionOffset);
    std::uint8_t type{};std::uint16_t slot{};
    if(!read.value(definition+0x30,value) || value!=request.enemy.registry
        || !read.value(definition+0x34,type) || type!=1 || !read.value(definition+0x36,slot) || slot!=3) return false;
    if(controller && (!read.value(input+0xC0,value) || value!=request.enemy.actor)) return false;
    if(!read.value(image+0x1F93428,entities) || !read.value(image+0x1F93430,stride) || stride<0x9C || stride>0x100000) return false;
    const auto row=entities+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
    if(!read.value(row+4,entityFlags) || (entityFlags&4U)
        || !read.value(row+0x98,refHandle) || !read.resolve(refHandle,reference)
        || !read.value(reference+4,value) || value!=0x80803A38U
        || !read.value(reference+0x24,value) || value!=refHandle
        || !read.value(reference+0x2C,value) || value!=entity
        || !read.value(reference+0x60,charHandle) || !read.resolve(charHandle,character)) return false;
    std::array<std::byte,0x30> actual{};
    if(!read.copy(character,actual) || field<std::uint32_t>(actual,0)!=0x80F459A3U
        || field<std::uint32_t>(actual,4)!=0x80803A00U || field<std::uint32_t>(actual,0x24)!=charHandle
        || field<std::uint32_t>(actual,0x2C)!=entity || (!controller && character!=input)) return false;
    std::int64_t offsetAgain{};std::array<std::byte,0x30> actualAgain{};
    // Revalidate the callback and ownership after traversing relocatable handles.
    if(!read.copy(input,again) || again!=header || !read.resolve(self,resolved) || resolved!=input
        || !read.resolve(charHandle,resolved) || resolved!=character
        || !read.copy(character,actualAgain) || actualAgain!=actual
        || !read.resolve(refHandle,resolved) || resolved!=reference
        || !read.value(reference+0x60,value) || value!=charHandle
        || !read.value(row+0x98,value) || value!=refHandle
        || !read.value(row+4,value) || (value&4U)
        || !read.value(actor+0x48,value) || value!=request.enemy.actor
        || !read.value(actor+0x4C,value) || value!=entity
        || !read.value(actor+0x38,value) || value!=sourceHandle
        || !read.value(actor+0x40,offsetAgain) || offsetAgain!=sourceOffset
        || !read.resolve(sourceHandle,resolved) || resolved!=sourceBase
        || !read.value(source,value) || value!=definitionHandle
        || !read.value(source+8,offsetAgain) || offsetAgain!=definitionOffset
        || !read.value(source+0x1FC,value) || value!=request.enemy.generation
        || !read.value(source+0x244,value) || value!=request.enemy.generation) return false;
    if(controller && (!read.value(input+0xC0,value) || value!=request.enemy.actor)) return false;
    out={character,charHandle,entity};return true;
}
struct Suppression {std::uint32_t ticks{};std::uint8_t active{UINT8_MAX};};
template<class Read> bool suppression(Read& read,const Identity& identity,Suppression& out) noexcept {
    std::array<std::byte,5> bytes{};
    if(!read.copy(identity.character+0xAC0,bytes)) return false;
    out={field<std::uint32_t>(bytes,0),field<std::uint8_t>(bytes,4)};return true;
}
}
