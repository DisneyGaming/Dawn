#pragma once
#include "coo_native_components.h"
#include <bit>
#include "omega_enemy_native_health.h"
#include "../../../state/activity/coo/population_service.h"
namespace sunrise::client::hooks::bootflow::coo_native {
// Native allocator 34F790 clears the slot's bit at head+8 -> +10 on
// retirement. Count allocated slots, including non-mission actors. Unknown
// metadata is reported as unknown; no fixed budget or heap patch is assumed.
template<class Read> state::activity::coo::PopulationCapacity capacity(Read& read,std::uintptr_t pool) noexcept {
    std::uintptr_t head{},bitmapOwner{},bitmap{},again{};std::uint32_t maximum{};std::uint16_t slots{};
    if(!read.value(pool,head) || !read.value(pool+0x14,maximum) || !maximum || maximum>256
        || !read.value(head+0x1C,slots) || slots!=maximum || !read.value(head+8,bitmapOwner)
        || !read.value(bitmapOwner+0x10,bitmap)) { return {}; }
    std::array<std::uint32_t,8> words{},after{};const auto count=(maximum+31U)/32U;
    if(!read.copy(bitmap,std::as_writable_bytes(std::span(words).first(count)))) { return {}; }
    std::uint32_t used{};
    for(std::uint32_t i=0;i<count;++i) {
        const auto remaining=maximum-i*32U;const auto mask=remaining<32?((1U<<remaining)-1U):UINT32_MAX;
        used+=static_cast<std::uint32_t>(std::popcount(words[i]&mask));
    }
    if(!read.value(pool,again) || again!=head || !read.copy(bitmap,std::as_writable_bytes(std::span(after).first(count))) || after!=words) { return {}; }
    return {true,used,maximum};
}
// Same regular/boss actor table. No AI commands, damage writes, forced movement,
// or inference from velocity. Applied source assignment is read from the native
// requested scope +180 / authored row +234 and applied scope +5F0 / row +5FC.
// Native4E2A90 copies the authored assignment.4E2C40 independently commits the
// selected index at +600; that index need not equal the authored row.
template<class Read,class Receipt>
state::activity::coo::EnemyReadiness enemy(Read& read,std::uintptr_t image,const Receipt& receipt) noexcept {
    state::activity::coo::EnemyReadiness out{};
    if(!receipt.valid()) { return out; }
    std::uintptr_t table{},actor{},source{},parent{},entities{},row{},character{},health{};
    std::uint32_t stride{},self{},entity{},parentHandle{},sourceHandle{},kind{},generation{},committed{},again{},bundle{},flags{};
    std::int64_t sourceOffset{};
    if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride) || stride<0x70 || stride>0x100000) { return out; }
    actor=table+static_cast<std::uintptr_t>(receipt.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x48,self) || self!=receipt.actor || !read.value(actor+0x4C,entity) || entity==UINT32_MAX
        || !read.value(actor+0x38,sourceHandle) || sourceHandle!=receipt.owner || !read.value(actor+0x40,sourceOffset)
        || sourceOffset<0 || sourceOffset>0x1000000 || !read.resolve(sourceHandle,source)) { return out; }
    source+=static_cast<std::uintptr_t>(sourceOffset);
    if(!read.value(source+4,kind) || kind!=0x8080948FU || !read.value(source+0x1FC,generation) || generation!=receipt.generation
        || !read.value(source+0x244,committed) || committed!=generation) { return out; }
    std::uint32_t definitionHandle{},registry{};std::uint8_t sourceType{};std::uint16_t sourceSlot{};
    std::int64_t definitionOffset{};std::uintptr_t definition{};
    if(!read.value(source,definitionHandle) || !read.value(source+8,definitionOffset)
        || definitionOffset<0 || definitionOffset>0x1000000 || !read.resolve(definitionHandle,definition)) { return out; }
    definition+=static_cast<std::uintptr_t>(definitionOffset);
    if(!read.value(definition+0x30,registry) || registry!=receipt.registry || !read.value(definition+0x34,sourceType) || sourceType!=1
        || !read.value(definition+0x36,sourceSlot) || sourceSlot!=receipt.source) { return out; }
    out.created=true;
    std::uint32_t aiEntity{},aiSelf{},aiActor{};
    if(read.value(actor+0x50,parentHandle) && parentHandle!=UINT32_MAX && read.resolve(parentHandle,parent)
        && read.value(parent+4,kind) && kind==0x808082ECU && read.value(parent+0x24,aiSelf) && aiSelf==parentHandle
        && read.value(parent+0x2C,aiEntity) && aiEntity==entity && read.value(parent+0x1470,aiActor) && aiActor==receipt.actor) { out.ai=true; }
    std::uint8_t type{},appliedType{};std::uint16_t appliedSlot{};
    std::int32_t authored{-1},appliedRow{-1},selected{-1};std::uint32_t appliedRegistry{},group{UINT32_MAX};
    if(read.value(source+0x180,out.tacticalRegistry) && read.value(source+0x184,type) && type==3
        && read.value(source+0x186,out.tacticalSlot) && read.value(source+0x234,authored) && authored>=0 && authored<24
        && read.value(source+0x5F0,appliedRegistry) && appliedRegistry==out.tacticalRegistry
        && read.value(source+0x5F4,appliedType) && appliedType==type
        && read.value(source+0x5F6,appliedSlot) && appliedSlot==out.tacticalSlot
        && read.value(source+0x5FC,appliedRow) && appliedRow==authored
        && read.value(source+0x600,selected) && selected>=0 && read.value(source+0x5E0,group) && group!=UINT32_MAX) {
        out.tactical=true;out.tacticalRow=static_cast<std::int8_t>(authored);
    }
    if(read.value(image+0x1F93428,entities) && read.value(image+0x1F93430,stride) && stride>=0x50 && stride<=0x100000) {
        row=entities+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
        if(read.value(row+4,flags) && !(flags&4U) && read.value(row+0x4C,bundle)
            // Live Vex resources contain 432-453 reflected rows. Keep the
            // bounded iterator and its identity checks, but admit actor-sized tables.
            && component<Read,1024>(read,bundle,entity,0x80806832U,character)) {
            std::uint32_t characterActor{},healthKind{},healthSelf{},healthEntity{},healthRuntime{};std::int64_t healthOffset{};
            if(read.value(character+0xC0,characterActor) && characterActor==receipt.actor
                && read.value(character+0x2E8,out.healthHandle) && read.value(character+0x2EC,healthKind) && read.value(character+0x2F0,healthOffset)
                && read.resolve(out.healthHandle,health) && read.value(health+4,healthRuntime) && read.value(health+0x24,healthSelf) && read.value(health+0x2C,healthEntity)) {
                out.health=omega_enemy_native_health::identity(out.healthHandle,healthKind,healthOffset,healthRuntime,healthSelf,entity,healthEntity);
            }
        }
    }
    // Reject observations spanning retirement, a source reset or an actor rebind.
    std::uintptr_t resolved{};std::int64_t offsetAgain{};
    if(!read.value(actor+0x48,again) || again!=receipt.actor || !read.value(actor+0x4C,again) || again!=entity
        || !read.value(actor+0x38,again) || again!=sourceHandle || !read.value(actor+0x40,offsetAgain) || offsetAgain!=sourceOffset
        || !read.resolve(sourceHandle,resolved) || resolved+static_cast<std::uintptr_t>(sourceOffset)!=source
        || !read.value(source+0x1FC,again) || again!=generation || !read.value(source+0x244,again) || again!=generation
        || !read.value(source,again) || again!=definitionHandle || !read.value(source+8,offsetAgain) || offsetAgain!=definitionOffset) { return {}; }
    if(out.health && (!read.resolve(out.healthHandle,resolved) || resolved!=health
        || !read.value(character+0x2E8,again) || again!=out.healthHandle || !read.value(character+0xC0,again) || again!=receipt.actor
        || !read.value(health+0x24,again) || again!=out.healthHandle || !read.value(health+0x2C,again) || again!=entity)) { return {}; }
    if(out.ai && (!read.value(actor+0x50,again) || again!=parentHandle || !read.resolve(parentHandle,resolved) || resolved!=parent
        || !read.value(parent+0x1470,again) || again!=receipt.actor)) { return {}; }
    std::uint8_t typeAgain{};std::uint16_t slotAgain{};std::int32_t rowAgain{};
    if(out.tactical && (!read.value(source+0x5E0,again) || again!=group
        || !read.value(source+0x180,again) || again!=out.tacticalRegistry
        || !read.value(source+0x184,typeAgain) || typeAgain!=type
        || !read.value(source+0x186,slotAgain) || slotAgain!=out.tacticalSlot
        || !read.value(source+0x234,rowAgain) || rowAgain!=authored
        || !read.value(source+0x5F0,again) || again!=appliedRegistry
        || !read.value(source+0x5F4,typeAgain) || typeAgain!=appliedType
        || !read.value(source+0x5F6,slotAgain) || slotAgain!=appliedSlot
        || !read.value(source+0x5FC,rowAgain) || rowAgain!=appliedRow
        || !read.value(source+0x600,rowAgain) || rowAgain!=selected)) { return {}; }
    return out;
}
}
