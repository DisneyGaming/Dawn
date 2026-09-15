#pragma once
#include <algorithm>
#include "../account/account_state.h"
#include "../build_data/runtime.h"
#include "../build_data/vendors/service_catalog.h"

namespace sunrise::state::vendors {
inline bool saved_value(const AccountState& account,const CharacterState& character,bool numeric,
    std::uint16_t slot,std::int32_t& value) noexcept {
    return lookup(numeric?character.vendorUnlocks.values:character.vendorUnlocks.flags,slot,value)
        || lookup(numeric?account.vendorUnlocks.values:account.vendorUnlocks.flags,slot,value);
}
inline bool write_value(AccountState& account,std::size_t ci,bool numeric,std::uint16_t slot,
    std::int32_t value,std::uint16_t questSlot=0xffff) noexcept {
    if(ci>=account.characterCount || slot>=(numeric?15500:23500) || (!numeric && value!=1 && value!=2)) {return false;}
    const auto b=build_data::vendors::services::binding(numeric,slot);
    const auto q=build_data::vendors::services::binding(true,questSlot);
    if((b.present && b.bank>(numeric?1:2)) || (!numeric && !b.present)) {return false;}
    const bool shared=b.present?(numeric?b.bank==0:b.bank<2):(q.present && q.bank==0);
    auto& target=shared?account.vendorUnlocks:account.characters[ci].vendorUnlocks;
    return store(numeric?target.values:target.flags,slot,value);
}
// The owned step is authoritative across purchases, abandonment and saved-game
// restore. Publish its authored tracking value in the existing native bank.
template<class F> void visit_quests(const CharacterState& character,F&& visit) noexcept {
    if(character.inventory.count>character.inventory.values.size()) {return;}
    for(std::size_t i=0;i<character.inventory.count;++i) {
        const auto& item=character.inventory.values[i];build_data::items::Definition definition{};
        build_data::vendors::services::QuestStep step{};
        if(!item.postmaster && item.quantity>0 && build_data::find_item_definition_hash(item.definitionHash,definition)
            && build_data::vendors::services::quest_step(definition.definitionIndex,step)) {visit(step);}
    }
}
inline void select_quest(const CharacterState& character,std::uint16_t slot,
    build_data::vendors::services::QuestStep& selected,bool& found) noexcept {
    visit_quests(character,[&](auto step) {
        if(step.slot==slot && (!found || step.ordinal>selected.ordinal)) {selected=step;found=true;}
    });
}
inline bool quest_value(const AccountState& account,const CharacterState& character,
    std::uint16_t slot,std::int32_t& value) noexcept {
    // A completed chain has no held step, but its terminal tracking value must
    // survive restart and must not advertise its initial acquisition again.
    if(saved_value(account,character,true,slot,value) && value==-1) {return true;}
    const auto binding=build_data::vendors::services::binding(true,slot);bool found{};
    build_data::vendors::services::QuestStep selected{};
    if(binding.present && binding.bank==0) {
        for(std::size_t i=0;i<account.characterCount;++i) {select_quest(account.characters[i],slot,selected,found);}
    } else {select_quest(character,slot,selected,found);}
    if(found) {value=selected.value;}
    return found;
}
inline bool quest_flag(const CharacterState& character,std::uint16_t slot) noexcept {
    bool found{};
    visit_quests(character,[&](auto step) {
        build_data::vendors::services::Pursuit p{};
        if(found || !build_data::vendors::services::pursuit(step.item,p)) {return;}
        for(const auto index:p.objectives) {
            build_data::vendors::services::Objective o{};
            if(build_data::vendors::services::objective(index,o)
                && std::find(o.flags.begin(),o.flags.end(),slot)!=o.flags.end()) {found=true;break;}
        }
    });
    return found;
}
template<class Object> void project_account_quests(const AccountState& account,Object& object) noexcept {
    for(const auto v:account.vendorUnlocks.values) {
        const auto b=build_data::vendors::services::binding(true,v.slot);
        if(b.present && b.bank==0 && b.row<object.objectiveValues.size()) {object.objectiveValues[b.row]=v.value;}
    }
    for(const auto f:account.vendorUnlocks.flags) {
        const auto b=build_data::vendors::services::binding(false,f.slot);
        if(b.present && b.bank==0 && b.row<object.acquiredFlags.size()) {object.acquiredFlags[b.row]=static_cast<std::uint8_t>(f.value);}
        if(b.present && b.bank==1 && b.row<object.profileUnlockFlags.size()) {object.profileUnlockFlags[b.row]=static_cast<std::uint8_t>(f.value);}
    }
    for(std::size_t i=0;i<account.characterCount;++i) visit_quests(account.characters[i],[&](auto step) {
        const auto binding=build_data::vendors::services::binding(true,step.slot);std::int32_t value{};
        if(binding.present && binding.bank==0 && binding.row<object.objectiveValues.size()
            && quest_value(account,account.characters[i],step.slot,value)) {object.objectiveValues[binding.row]=value;}
    });
}
template<class Object> void project_quests(const CharacterState& character,std::uint8_t bank,Object& object) noexcept {
    for(const auto v:character.vendorUnlocks.values) {
        const auto b=build_data::vendors::services::binding(true,v.slot);
        if(b.present && b.bank==bank && b.row<object.objectiveValues.size()) {object.objectiveValues[b.row]=v.value;}
    }
    for(const auto f:character.vendorUnlocks.flags) {
        const auto b=build_data::vendors::services::binding(false,f.slot);
        if(bank==1 && b.present && b.bank==2 && b.row<object.acquiredFlags.size()) {
            object.acquiredFlags[b.row]=static_cast<typename decltype(object.acquiredFlags)::value_type>(f.value);
        }
    }
    visit_quests(character,[&](auto step) {
        const auto binding=build_data::vendors::services::binding(true,step.slot);
        if(binding.present && binding.bank==bank && binding.row<object.objectiveValues.size()) {
            bool found=true;select_quest(character,step.slot,step,found);
            object.objectiveValues[binding.row]=step.value;
        }
    });
}
template<class Object> bool project_active_progress(const AccountState& a,const CharacterState& c,Object& object) noexcept {
    bool ok=true;
    visit_quests(c,[&](auto step) {
        build_data::vendors::services::Pursuit p{};
        if(!build_data::vendors::services::pursuit(step.item,p)) {return;}
        for(const auto index:p.objectives) {
            build_data::vendors::services::Objective o{};
            if(!build_data::vendors::services::objective(index,o)) {continue;}
            for(const auto op:o.expression) {
                if(op.opcode!=10 || op.operand>=15500 || build_data::vendors::services::binding(true,static_cast<std::uint16_t>(op.operand)).present) {continue;}
                std::int32_t value{};
                if(!saved_value(a,c,true,static_cast<std::uint16_t>(op.operand),value)) {continue;}
                std::size_t at{};
                for(;at<object.unlockValueCount;++at) if(object.unlockValues[at].slot==op.operand) {break;}
                if(at>=object.unlockValues.size()) {ok=false;continue;}
                if(at==object.unlockValueCount) {++object.unlockValueCount;}
                object.unlockValues[at]={static_cast<std::int16_t>(op.operand),0,value};
            }
        }
    });return ok;
}
}
