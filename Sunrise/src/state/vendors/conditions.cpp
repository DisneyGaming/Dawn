#include "conditions.h"
#include "projection.h"
#include "quest_state.h"
#include "../unlocks/unlocks_runtime.h"
#include "../equipment/light/resolution/configured_equipment_light_resolver.h"
#include <bit>
#include <algorithm>
#include <limits>

namespace sunrise::state::vendors {
namespace {
namespace catalog=build_data::vendors::services;
std::int32_t quantity(const AccountState& a,const CharacterState& c,std::uint32_t hash) noexcept {
    std::int64_t count{};
    for(std::size_t i=0;i<a.profileItemCount;++i) if(a.profileItems[i].definitionHash==hash) {count+=a.profileItems[i].quantity;}
    for(std::size_t i=0;i<c.inventory.count;++i) if(!c.inventory.values[i].postmaster && c.inventory.values[i].definitionHash==hash) {count+=c.inventory.values[i].quantity;}
    return static_cast<std::int32_t>((std::min)(count,static_cast<std::int64_t>(INT32_MAX)));
}
std::int32_t leaf(bool value,std::uint16_t slot,const AccountState& account,
    std::size_t ci,const Family5State& overrides,const unlocks::ScopedTable& table,
    const unlocks::CharacterTable* characterUnlocks) noexcept {
    const auto& c=account.characters[ci];
    if(!value && slot==239) {return c.characterClass==CharacterClass::hunter;}
    if(!value && slot==264) {return c.characterClass==CharacterClass::titan;}
    if(!value && slot==271) {return c.characterClass==CharacterClass::warlock;}
    for(const auto& f:factions) if(!value && f.turnInFlag && slot==f.turnInFlag) {
        return quantity(account,c,f.token)>0 || (f.secondToken && quantity(account,c,f.secondToken)>0) || (f.thirdToken && quantity(account,c,f.thirdToken)>0);
    }
    if(value) {
        for(const auto& f:factions) {
            const auto& bank=f.scope?c.vendorProgress:account.vendorProgress;
            for(const auto& p:bank) if(p.vendor==f.vendor) {
                std::int32_t result{};if(counter(p,f,slot,result)) {return result;}break;
            }
        }
        std::int32_t tracking{};if(quest_value(account,c,slot,tracking)) {return tracking;}
        if(saved_value(account,c,true,slot,tracking)) {return tracking;}
        if(slot==462) {
            equipment::light::Evaluation power{};
            return equipment::light::resolution::resolve(account,ci,power)?power.average:0;
        }
        // These kind-zero slots are the installed vendor price/turn-in counts.
        for(const auto [s,h]:tokenCounters) if(slot==s) {return quantity(account,c,h);}
        constexpr std::uint16_t campaigns[]{12578,12609,12624};
        for(unsigned i=0;i<3;++i) if(slot==campaigns[i]) {return (c.vendorCampaigns>>i)&1U;}
        if(slot==465) {return c.level;}
        for(std::size_t i=0;i<overrides.valueCount;++i) if(overrides.values[i].slot==slot) {return overrides.values[i].value;}
    } else {
        for(const auto& receipt:purchaseReceipts)
            if(receipt.flag==slot && receipt_earned(account,receipt)) {return 1;}
        std::int32_t saved{};if(saved_value(account,c,false,slot,saved)) {return saved==2;}
        if(quest_flag(c,slot)) {return 1;}
        for(std::size_t i=0;i<overrides.flagCount;++i) if(overrides.flags[i].slot==slot) {return overrides.flags[i].value==2;}
        for(const auto [access,available]:accessFlags) if(access==slot) {return available;}
        // Native inventory evaluation applies 808077AB flag supplies. Quest and
        // Passage ownership must make the same predicates true on the server.
        for(std::size_t i=0;i<c.inventory.count;++i) {
            const auto& item=c.inventory.values[i];build_data::items::Definition definition{};
            if(!item.postmaster && item.quantity>0 && build_data::find_item_definition_hash(item.definitionHash,definition)
                && catalog::item_flag(definition.definitionIndex,slot)) {return 1;}
        }
    }
    const auto b=catalog::binding(value,slot);if(!b.present) {return 0;}
    if(value) {
        if(b.bank==0 && b.row<table.objectiveValues.size()) {return table.objectiveValues[b.row];}
        if(b.bank==1 && characterUnlocks && b.row<characterUnlocks->objectValues.size()) {return characterUnlocks->objectValues[b.row];}
    } else {
        if(b.bank==0 && b.row<table.accountFlags.size()) {return table.accountFlags[b.row]==2;}
        if(b.bank==1 && b.row<table.profileFlags.size()) {return table.profileFlags[b.row]==2;}
        if(b.bank==2 && characterUnlocks && b.row<characterUnlocks->objectFlags.size()) {return characterUnlocks->objectFlags[b.row]==2;}
        if(b.bank==4 && characterUnlocks && b.row<characterUnlocks->flags.size()) {return characterUnlocks->flags[b.row]==2;}
    }
    return 0;
}
bool evaluate(std::span<const catalog::Instruction> program,const AccountState& a,std::size_t ci,
    const Family5State& overrides,const unlocks::ScopedTable& table,
    const unlocks::CharacterTable* characterUnlocks,std::int32_t& result,unsigned depth) noexcept {
    if(depth>32 || ci>=a.characterCount || program.size()>256) {return false;}
    if(program.empty()) {result=1;return true;}
    std::array<std::uint32_t,256> stack{};std::size_t count{};
    for(const auto i:program) {
        if(i.opcode==1 || i.opcode==10 || i.opcode==11 || i.opcode==12) {
            if(count==stack.size()) {return false;}
            auto value=i.operand;
            if(i.opcode==1 || i.opcode==10) {value=static_cast<std::uint32_t>(leaf(i.opcode==10,static_cast<std::uint16_t>(i.operand),a,ci,overrides,table,characterUnlocks));}
            if(i.opcode==12) {
                catalog::Expression pooled;std::int32_t v{};
                if(!catalog::pooled(static_cast<std::uint16_t>(i.operand),pooled) || !evaluate(pooled,a,ci,overrides,table,characterUnlocks,v,depth+1)) {return false;}
                value=static_cast<std::uint32_t>(v);
            }
            stack[count++]=value;continue;
        }
        if(!count) {return false;}
        if(i.opcode==2) {stack[count-1]=!stack[count-1];continue;}
        if(i.opcode==22) {stack[count-1]=0U-stack[count-1];continue;}
        if(i.opcode==28) {stack[count-1]=~stack[count-1];continue;}
        if(i.opcode==23) {
            const auto v=stack[count-1];std::uint32_t h=0x811C9DC5U;
            for(int shift=24;shift>=0;shift-=8) {h=(h^((v>>shift)&255))*0x1000193U;}
            stack[count-1]=h;continue;
        }
        if(count<2) {return false;}
        const auto right=stack[--count];auto& left=stack[count-1];
        const auto x=std::bit_cast<std::int32_t>(left),y=std::bit_cast<std::int32_t>(right);
        switch(i.opcode) {
        case 3:left=left || right;break;
        case 4:left=left && right;break;
        case 5:left=!(left || right);break;
        case 6:case 9:left=left!=right;break;
        case 7:left=!(left && right);break;
        case 8:left=left==right;break;
        case 13:left=x>y;break;case 14:left=x>=y;break;case 15:left=x<y;break;case 16:left=x<=y;break;
        case 17:left+=right;break;case 18:left-=right;break;case 19:left*=right;break;
        case 20:case 21:
            if(!y || (x==INT32_MIN && y==-1)) {return false;}
            left=static_cast<std::uint32_t>(i.opcode==20?x/y:x%y);break;
        case 24:
            for(int shift=24;shift>=0;shift-=8) {left=(left^((right>>shift)&255U))*0x1000193U;}
            break;
        case 25:left&=right;break;case 26:left|=right;break;case 27:left^=right;break;
        default:return false;
        }
    }
    if(count!=1) {return false;}result=std::bit_cast<std::int32_t>(stack[0]);return true;
}
}
bool condition(std::span<const catalog::Instruction> p,const AccountState& a,std::size_t ci,
    const Family5State& f,std::int32_t& out) noexcept {
    out=0;
    if(ci>=a.characterCount) return false;
    // One coherent durable view for the entire expression, including pooled calls.
    const auto table=unlocks::snapshot();
    unlocks::CharacterTable character{};
    const bool found=unlocks::find_character(table,a.characters[ci].soid,character);
    return evaluate(p,a,ci,f,table,found?&character:nullptr,out,0);
}
}
