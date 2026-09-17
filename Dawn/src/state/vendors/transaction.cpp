#include "transaction.h"
#include "conditions.h"
#include "bounties.h"
#include "../persistence/persistence.h"
#include "quest_state.h"
#include "../runtime/runtime.h"
#include "../account/inventory/placement.h"
#include "../runtime/state_account_transaction_helpers.h"
#include "../runtime/storage/internal.h"
#include "../build_data/runtime.h"
#include "../unlocks/unlocks_runtime.h"
#include "../../core/logging/log.h"
#include <algorithm>
#include <cstdio>
#include <limits>

namespace dawn::state::vendors {
namespace {
namespace bd=build_data;
namespace catalog=bd::vendors::services;
namespace detail=runtime::detail;
namespace inv=account::inventory;
namespace buckets=bd::inventory::buckets;
namespace f4=middleware::datagen::family4::loadout;
struct ItemInfo {bd::items::Definition item;bd::items::details::Definition detail;buckets::Descriptor bucket;};
// Every service stages the same optimistic account snapshot and selected character.
std::shared_ptr<TransactionData> begin_transaction() noexcept {
    auto p=std::make_shared<TransactionData>();
    p->before=account_snapshot();p->after=p->before;
    if(!account::valid(p->before) || !detail::valid_profile_inventory(p->before)) return {};
    for(p->character=0;p->character<p->before.characterCount;++p->character)
        if(p->before.characters[p->character].selected) return p;
    return {};
}
bool valid_transaction(const TransactionData& p) noexcept {
    f4::ResolvedLoadout loadout{};
    return account::valid(p.after) && detail::valid_profile_inventory(p.after)
        && f4::resolve(p.after,p.character,loadout);
}
void apply_transaction(AccountState& current,const TransactionData& p) noexcept {
    // Other characters may have changed since preparation. Only copy owned rows.
    current.characters[p.character]=p.after.characters[p.character];
    current.profileItems=p.after.profileItems;current.profileItemCount=p.after.profileItemCount;
    current.vendorProgress=p.after.vendorProgress;current.vendorUnlocks=p.after.vendorUnlocks;
}
bool insert(TransactionData& p,inv::Item item) noexcept {
    std::uint64_t removed{};
    if(!inv::insert(p.after.characters[p.character],item,removed)) return false;
    if(removed) p.removed.push_back(removed);
    p.changed.push_back(item.instanceSoid);return true;
}
bool info(std::uint16_t index,ItemInfo& out) noexcept {
    return bd::find_item_definition_index(index,out.item)
        && bd::find_configured_item_detail(index,out.detail)
        && out.detail.definitionHash==out.item.definitionHash && out.detail.bucketId==out.item.bucketId
        && out.detail.maxStackSize>0 && bd::find_inventory_bucket_descriptor(out.item.bucketId,out.bucket);
}
bool held(const CharacterState& c,std::uint32_t hash) noexcept {
    for(std::size_t i=0;i<c.inventory.count;++i) if(c.inventory.values[i].definitionHash==hash) {return true;}
    return false;
}
bool fresh_identity(const TransactionData& p,bool profile,std::uint64_t& soid) noexcept {
    const auto allocate=profile?detail::next_profile_item_instance_soid:detail::next_item_instance_soid;
    std::uint64_t before{},after{};
    if(!allocate(p.before,before) || !allocate(p.after,after)) {return false;}
    soid=(std::max)(before,after);
    // A consumed item remains reserved until its deletion has been published.
    while(detail::account_owns_soid(p.before,soid) || detail::account_owns_soid(p.after,soid)) {
        if(soid==UINT64_MAX) {return false;}++soid;
    }
    return soid!=UINT64_MAX;
}
// Debits and credits use the same rows, identity namespace and native stack caps.
bool profile(TransactionData& p,const ItemInfo& item,std::int32_t amount) noexcept {
    auto& a=p.after;auto at=a.profileItemCount;std::int32_t serial{};
    if(item.bucket.arraySelector!=buckets::ArraySelector::profile
        || item.detail.instancedDefinitionState!=bd::items::details::InstancedDefinitionState::stackable) {return false;}
    for(std::size_t i=0;i<a.profileItemCount;++i) {
        serial=(std::max)(serial,a.profileItems[i].mutationSerial);
        if(a.profileItems[i].definitionHash==item.item.definitionHash) {
            if(at!=a.profileItemCount) {return false;}at=i;
        }
    }
    const bool append=at==a.profileItemCount;
    const auto old=append?0:a.profileItems[at].quantity;
    const auto quantity=static_cast<std::int64_t>(old)+amount;
    if(quantity<0 || serial==INT32_MAX) {return false;}
    std::size_t occupied{};
    for(std::size_t i=0;i<a.profileItemCount;++i) {
        bd::items::Definition definition{};
        if(!bd::find_item_definition_hash(a.profileItems[i].definitionHash,definition)) {return false;}
        occupied+=definition.bucketId==item.item.bucketId;
    }
    const auto maximum=append && (at>=a.profileItems.size() || occupied>=item.bucket.slotCount)?0:item.detail.maxStackSize;
    const auto retained=(std::min)(quantity,static_cast<std::int64_t>(maximum));
    auto excess=quantity-retained;
    if(excess) {
        if(amount<=0) {return false;}
        auto& c=a.characters[p.character];
        while(excess>0) {
            if(c.nextInventorySerial>=INT32_MAX) {return false;}
            inv::Item mail{};
            if(!fresh_identity(p,false,mail.instanceSoid)) {return false;}
            mail.definitionHash=item.item.definitionHash;mail.quantity=static_cast<std::int32_t>((std::min)(excess,static_cast<std::int64_t>(item.detail.maxStackSize)));
            mail.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);mail.postmaster=true;
            if(!insert(p,mail)) {return false;}excess-=mail.quantity;
        }
        if(retained==old) {return true;}
    }
    if(!retained) {
        if(append) {return false;}
        if(a.profileItems[at].instanceSoid) {p.removed.push_back(a.profileItems[at].instanceSoid);}
        for(auto i=at+1;i<a.profileItemCount;++i) {a.profileItems[i-1]=a.profileItems[i];}
        a.profileItems[--a.profileItemCount]={};return true;
    }
    auto& row=a.profileItems[at];
    if(append) {
        row={};row.definitionHash=item.item.definitionHash;
        if(bd::is_profile_action_source(item.item.definitionIndex,item.item.bucketId)
            && !fresh_identity(p,true,row.instanceSoid)) {return false;}
        ++a.profileItemCount;
    }
    row.quantity=static_cast<std::int32_t>(retained);row.mutationSerial=serial+1;
    if(row.instanceSoid) {p.changed.push_back(row.instanceSoid);}return true;
}
bool grant(TransactionData& p,std::uint16_t index,std::int32_t amount,std::int32_t power=0) noexcept {
    ItemInfo item{};
    if(amount<=0 || amount>10000 || !info(index,item)) {return false;}
    if(item.bucket.arraySelector==buckets::ArraySelector::profile) {return profile(p,item,amount);}
    auto& a=p.after;auto& c=a.characters[p.character];
    if(item.bucket.arraySelector!=buckets::ArraySelector::character || amount>8
        || (item.bucket.bucketId==40 && held(c,item.item.definitionHash))) {return false;}
    for(int n=0;n<amount;++n) {
        if(c.nextInventorySerial>=INT32_MAX) {return false;}
        inv::Item row{};
        if(!fresh_identity(p,false,row.instanceSoid)) {return false;}
        row.definitionHash=item.item.definitionHash;row.quantity=1;
        row.level=item.bucket.equipmentSlot>=0 || item.bucket.bucketId==31?(power>0?power:detail::acquisition_level(c)):0;
        row.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);
        row.sockets.policy=inv::SocketPolicy::nativeDefaults;
        if(!insert(p,row)) {return false;}
    }
    return true;
}
bool credit(TransactionData& p,std::uint32_t hash,std::int32_t amount) noexcept {
    bd::items::Definition item{};
    return bd::find_item_definition_hash(hash,item) && grant(p,item.definitionIndex,amount);
}
bool recycle(TransactionData& p,const catalog::Offer& offer) noexcept {
    if(offer.costs.size()!=1 || offer.costs[0].quantity!=5) {return false;}
    ItemInfo source{};if(!info(offer.costs[0].item,source)) {return false;}
    if(offer.vendor==19 && offer.category==4 && source.item.bucketId==14) {
        // Preserve the comparison branch's server payout policy, while taking
        // the actual shader and price from the installed sale, not a copied list.
        return credit(p,3159615086U,250) && credit(p,1022552290U,5);
    }
    if(offer.vendor==195 && offer.category==25) {
        constexpr std::uint32_t synths[]{0xEB520CB8U,0xD3C0580EU,0x64D3519AU,0x350ABF36U};
        return std::find(std::begin(synths),std::end(synths),source.item.definitionHash)!=std::end(synths)
            && credit(p,3159615086U,100);
    }
    return false;
}
bool debit(TransactionData& p,const catalog::Cost& cost) noexcept {
    if(!cost.quantity) {return true;}
    ItemInfo item{};if(cost.quantity<0 || !info(cost.item,item)) {return false;}
    if(item.bucket.arraySelector==buckets::ArraySelector::profile) {return profile(p,item,-cost.quantity);}
    auto& c=p.after.characters[p.character];auto left=cost.quantity;
    for(std::size_t i=0;i<c.inventory.count && left>0;) {
        auto& row=c.inventory.values[i];
        if(row.postmaster || row.definitionHash!=item.item.definitionHash || (row.flags&inv::kLockedItemFlag)) {++i;continue;}
        const auto count=(std::min)(left,row.quantity);left-=count;row.quantity-=count;
        if(row.quantity) {
            if(c.nextInventorySerial>=INT32_MAX) {return false;}
            row.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);p.changed.push_back(row.instanceSoid);++i;
        } else {
            p.removed.push_back(row.instanceSoid);
            inv::erase(c,i);
        }
    }
    return !left;
}
bool gates(const catalog::Offer& offer,const AccountState& a,std::size_t ci,const Family5State& f) noexcept {
    for(const auto& program:offer.conditions) {
        std::int32_t result{};if(!condition(program,a,ci,f,result) || !result) {return false;}
    }
    return true;
}
bool reward(TransactionData& p,std::uint16_t vendor,const Family5State& f,bool equipment=true) noexcept {
    std::vector<std::uint16_t> pool;if(!catalog::reward_items(vendor,pool)) {return false;}
    std::vector<catalog::Offer> eligible;
    for(std::size_t i=0;i<pool.size();++i) {
        catalog::Offer row{};ItemInfo item{};
        if(!catalog::offer(vendor,static_cast<std::uint16_t>(i),row) || !info(row.item,item)
            || (equipment && (item.bucket.arraySelector!=buckets::ArraySelector::character || item.bucket.equipmentSlot<0))
            || !gates(row,p.after,p.character,f)) {continue;}
        if(std::none_of(eligible.begin(),eligible.end(),[&](const auto& e){return e.item==row.item;})) {eligible.push_back(std::move(row));}
    }
    if(eligible.empty()) {return false;}
    // Stable for an optimistic retry, varied by the next committed acquisition.
    auto seed=p.after.characters[p.character].soid^p.after.characters[p.character].nextInventorySerial;
    seed^=seed>>30;seed*=UINT64_C(0xBF58476D1CE4E5B9);seed^=seed>>27;seed*=UINT64_C(0x94D049BB133111EB);seed^=seed>>31;
    const auto& chosen=eligible[seed%eligible.size()];
    return grant(p,chosen.item,chosen.quantity);
}
Progress* progress(TransactionData& p,const Faction& f) noexcept {
    auto& bank=f.scope?p.after.characters[p.character].vendorProgress:p.after.vendorProgress;
    for(auto& row:bank) if(row.vendor==f.vendor) {return &row;}
    for(auto& row:bank) if(row.vendor==0xffff) {
        row.vendor=f.vendor;
        const auto configured=unlocks::snapshot();
        std::int32_t points{};
        if(f.scope) {
            unlocks::CharacterTable character{};
            if(unlocks::find_character(configured,p.after.characters[p.character].soid,character)
                && f.progression<character.progressions.size()) points=character.progressions[f.progression][0];
        } else if(f.progression<configured.accountProgressions.size()) {
            points=configured.accountProgressions[f.progression][0];
        }
        row.points=(std::max)(0,points);
        const auto slot=f.claimedCounter?f.claimedCounter:f.rewardCounter;
        std::int32_t value{};
        const catalog::Instruction expression{10,slot};
        if(!condition(std::span(&expression,1),p.before,p.character,{},value)) {return nullptr;}
        row.rewards=std::clamp(f.claimedCounter?value:row.points/f.rankStep-value,0,row.points/f.rankStep);
        return &row;
    }
    return nullptr;
}
bool reputation(TransactionData& p,const catalog::Offer& offer,const Faction& f) noexcept {
    std::int64_t points{};
    for(const auto cost:offer.costs) {
        bd::items::Definition item{};if(!bd::find_item_definition_index(cost.item,item)) {return false;}
        const auto multiplier=item.definitionHash==f.token?f.points:item.definitionHash==f.secondToken?f.secondPoints:item.definitionHash==f.thirdToken?f.thirdPoints:0;
        if(!multiplier) {return false;}points+=static_cast<std::int64_t>(multiplier)*cost.quantity;
    }
    if(points<=0 || points>INT32_MAX) {return false;}
    auto* entry=progress(p,f);
    if(!entry || points>INT32_MAX-entry->points) {return false;}
    entry->points+=static_cast<std::int32_t>(points);return true;
}
bool rank_available(std::uint16_t vendor,const Faction& f,const AccountState& a,std::size_t ci,const Family5State& state) noexcept {
    // Ordinary 901 claims still owe the same authored eligibility as the 904
    // reward banner, including level, faction access and seasonal prerequisites.
    for(std::uint16_t i=0;;++i) {
        catalog::Interaction row{};if(!catalog::interaction(vendor,i,row)) {return false;}
        if(row.presentation!=0xD255E388U || row.category!=f.rewardCategory
            || std::none_of(row.condition.begin(),row.condition.end(),[&](auto op){return op.opcode==10 && op.operand==f.rewardCounter;})) {continue;}
        // The alternate "Reach Level 20" banner describes a locked package;
        // its selection reply must not authorize the otherwise free reward tile.
        bool locked{};
        for(std::size_t n=2;n<row.condition.size();++n) {
            locked|=row.condition[n-2].opcode==10 && row.condition[n-2].operand==465
                && row.condition[n-1].opcode==11 && row.condition[n-1].operand==20
                && row.condition[n].opcode==15;
        }
        if(locked) {continue;}
        std::int32_t result{};
        if(!condition(row.condition,a,ci,state,result) || !result) {continue;}
        for(const auto& reply:row.replies) if(reply.selection==1
            && condition(reply.condition,a,ci,state,result) && result) {return true;}
    }
}
bool bounty(TransactionData& p,const BountyPool& pool) noexcept {
    const auto& c=p.after.characters[p.character];std::vector<std::uint32_t> available;std::size_t count{};
    for(const auto hash:pool.items) {if(held(c,hash)) {++count;}else {available.push_back(hash);}}
    if(count>=5 || available.empty()) {return false;}
    bd::items::Definition item{};
    return bd::find_item_definition_hash(available[c.nextInventorySerial%available.size()],item) && grant(p,item.definitionIndex,1);
}
bool matches(const AccountState& current,const TransactionData& p) noexcept {
    return current.primarySoid==p.before.primarySoid && current.characterCount==p.before.characterCount
        && p.character<current.characterCount && current.characters[p.character].selected
        && detail::same_character(current.characters[p.character],p.before.characters[p.character])
        && current.vendorProgress==p.before.vendorProgress
        && current.vendorUnlocks==p.before.vendorUnlocks
        && detail::same_profile_inventory(current,p.before.profileItems,p.before.profileItemCount);
}
bool known_objective(const catalog::Expression& program,const TransactionData& p,const Family5State& f,unsigned depth=0) noexcept {
    if(program.empty() || depth>32) {return false;}
    for(const auto op:program) {
        if(op.opcode==12) {
            catalog::Expression child;
            if(!catalog::pooled(static_cast<std::uint16_t>(op.operand),child) || !known_objective(child,p,f,depth+1)) {return false;}
        }
        if(op.opcode!=1 && op.opcode!=10) {continue;}
        if(op.operand>=(op.opcode==10?15500U:23500U)) {return false;}
        const auto slot=static_cast<std::uint16_t>(op.operand);std::int32_t value{};
        if(op.opcode==10 && (slot==462 || slot==465)) {continue;}
        if(saved_value(p.after,p.after.characters[p.character],op.opcode==10,slot,value)) {continue;}
        bool supplied{};
        if(op.opcode==10) {for(std::size_t i=0;i<f.valueCount;++i) {supplied|=f.values[i].slot==slot;}}
        else {for(std::size_t i=0;i<f.flagCount;++i) {supplied|=f.flags[i].slot==slot;}}
        const auto b=catalog::binding(op.opcode==10,slot);
        if(!supplied && (!b.present || b.bank>(op.opcode==10?1:2))) {return false;}
    }
    return true;
}
bool complete(const catalog::Pursuit& pursuit,const TransactionData& p,const Family5State& f) noexcept {
    if(pursuit.objectives.empty()) {return false;}bool any{};
    for(const auto index:pursuit.objectives) {
        catalog::Objective o{};std::int32_t progress{};
        const bool done=catalog::objective(index,o) && o.threshold>0 && known_objective(o.expression,p,f)
            && condition(o.expression,p.after,p.character,f,progress) && progress>=o.threshold;
        if(pursuit.requireAll && !done) {return false;}any|=done;
    }
    return any;
}
bool advance(TransactionData& p,std::uint64_t soid,const catalog::QuestStep& step,const catalog::Pursuit& pursuit) noexcept {
    auto& c=p.after.characters[p.character];auto at=c.inventory.count;
    for(std::size_t i=0;i<c.inventory.count;++i) if(c.inventory.values[i].instanceSoid==soid) {at=i;break;}
    if(at==c.inventory.count) {return false;}
    if(step.slot==12740 && step.ordinal>=5
        && (!write_value(p.after,p.character,false,753,1) || !write_value(p.after,p.character,false,1041,2))) {return false;}
    catalog::QuestStep next{};
    if(catalog::next_step(step,next)) {
        bd::items::Definition definition{};
        if(!bd::find_item_definition_index(next.item,definition) || definition.bucketId!=40
            || c.nextInventorySerial>=INT32_MAX || held(c,definition.definitionHash)) {return false;}
        auto& item=c.inventory.values[at];item.definitionHash=definition.definitionHash;item.sockets={};
        item.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);p.changed.push_back(soid);
    } else {
        if(!write_value(p.after,p.character,true,step.slot,-1,step.slot)) {return false;}
        p.removed.push_back(soid);
        inv::erase(c,at);
    }
    for(const auto index:pursuit.objectives) {
        catalog::Objective o{};if(!catalog::objective(index,o)) {return false;}
        // These are objective-local counters. Never write Power, a pooled
        // comparison or other shared inputs merely because a step completed.
        if(o.expression.size()==1 && o.expression[0].opcode==10 && o.expression[0].operand!=462
            && !write_value(p.after,p.character,true,static_cast<std::uint16_t>(o.expression[0].operand),o.threshold,step.slot)) {return false;}
    }
    return true;
}
bool hand_in(TransactionData& p,const catalog::Interaction& banner,bool& found) noexcept {
    found=false;catalog::QuestStep chosen{};catalog::Pursuit objective{};std::uint64_t soid{};
    const auto& c=p.before.characters[p.character];
    for(std::size_t i=0;i<c.inventory.count;++i) {
        bd::items::Definition item{};catalog::QuestStep step{};catalog::Pursuit progress{};
        if(c.inventory.values[i].postmaster || !bd::find_item_definition_hash(c.inventory.values[i].definitionHash,item) || item.bucketId!=40
            || !catalog::quest_step(item.definitionIndex,step) || !catalog::pursuit(step.item,progress)
            || !catalog::hand_in(progress,banner)) {continue;}
        if(found || (step.item>=15032 && step.item<=15036)) {return false;}
        // A vendor visit uses a separate counter, advanced by this accepted reply.
        for(const auto index:progress.objectives) {
            catalog::Objective o{};
            if(!catalog::objective(index,o) || o.threshold!=1 || o.expression.size()!=1 || o.expression[0].opcode!=10) {return false;}
        }
        found=true;chosen=step;objective=std::move(progress);soid=c.inventory.values[i].instanceSoid;
    }
    return !found || advance(p,soid,chosen,objective);
}
// Visit only top-level AND terms. A negative flag inside an OR, numeric
// comparison or other prerequisite must never become an acknowledgement write.
template<class Visit> bool conjuncts(std::span<const catalog::Instruction> p,Visit&& visit) noexcept {
    if(p.empty() || p.size()>256) {return false;}
    if(p.back().opcode!=4) {return visit(p);}
    std::size_t split=p.size()-1;int needed=1;
    while(split && needed) {
        const auto op=p[--split].opcode;
        if(op==1 || op==10 || op==11 || op==12) {--needed;}
        else if(op!=2 && op!=22 && op!=23 && op!=28) {
            if(op<3 || op>27) {return false;}++needed;
        }
    }
    return !needed && split && conjuncts(p.first(split),visit)
        && conjuncts(p.subspan(split,p.size()-split-1),visit);
}
bool acknowledgement(const catalog::Expression& expression,std::uint16_t& slot) noexcept {
    std::size_t count{};
    const bool valid=conjuncts(expression,[&](auto term) {
        if(term.size()!=2 || term[0].opcode!=1 || term[1].opcode!=2) {return true;}
        if(term[0].operand>=23500) {return false;}
        const auto flag=static_cast<std::uint16_t>(term[0].operand);
        const auto binding=catalog::binding(false,flag);
        if(!binding.present || binding.bank>2) {return false;}
        slot=flag;++count;return true;
    });
    return valid && count==1;
}
bool interaction_available(const catalog::Interaction& interaction,const catalog::Reply& reply,
    const Request& request,const TransactionData& p,const Family5State& f) noexcept {
    std::int32_t result{};
    if(condition(interaction.condition,p.before,p.character,f,result) && result) {return true;}
    std::uint16_t acknowledged{};
    // Native 904 is also the completion reply for a vendor vignette. Its UI can
    // supply transient context absent from investment banks (Ada flag14906).
    // Only that recovered scene context may be absent; preserve every other
    // prerequisite and save only the seen flag, never a purchase/quest reward.
    if(interaction.presentation!=0x7120FF77U || interaction.item!=0xffff || interaction.category!=0xffff
        || request.sale!=-1 || reply.selection!=0 || reply.site==0xffff
        || !acknowledgement(interaction.condition,acknowledged)) {return false;}
    return conjuncts(interaction.condition,[&](auto term) {
        if(condition(term,p.before,p.character,f,result) && result) {return true;}
        if(term.size()!=1 || term[0].opcode!=1 || term[0].operand>=23500) {return false;}
        const auto flag=static_cast<std::uint16_t>(term[0].operand);
        if(flag!=14906 || catalog::binding(false,flag).present
            || saved_value(p.before,p.before.characters[p.character],false,flag,result)) {return false;}
        for(std::size_t i=0;i<f.flagCount;++i) if(f.flags[i].slot==flag) {return false;}
        return true;
    });
}
}
bool prepare(const Request& request,Pending& pending) noexcept {
    pending={};
    if(!catalog::ready() || request.vendor>=512 || request.reply<0) {return false;}
    auto p=begin_transaction();if(!p) {return false;}
    const auto investment=investment_snapshot();catalog::Offer offer{};
    if(request.sale>=0) {
        if(request.sale>65535 || !catalog::offer(request.vendor,static_cast<std::uint16_t>(request.sale),offer)) {return false;}
    } else {offer.vendor=request.vendor;offer.quantity=1;}
    catalog::Interaction interaction{};const catalog::Reply* reply{};
    if(request.interaction>=0) {
        std::int32_t result{};
        if(request.interaction>65535 || !catalog::interaction(request.vendor,static_cast<std::uint16_t>(request.interaction),interaction)
            || static_cast<std::size_t>(request.reply)>=interaction.replies.size()) {return false;}
        reply=&interaction.replies[request.reply];
        if(!interaction_available(interaction,*reply,request,*p,investment.family5)
            || !condition(reply->condition,p->before,p->character,investment.family5,result) || !result
            || (reply->selection==0 && request.sale!=-1)
            || (reply->selection==1 && (request.sale<0 || interaction.category==0xffff || offer.category!=interaction.category))) {return false;}
    }
    if(request.sale<0 && request.interaction<0) {return false;}
    const PurchaseReceipt* receipt{};
    for(const auto& row:purchaseReceipts) if(row.vendor==request.vendor && row.sale==request.sale) {
        const catalog::Instruction flag{1,row.flag};std::int32_t claimed{};
        if(offer.item!=row.item || !condition(std::span(&flag,1),p->before,p->character,investment.family5,claimed)
            || claimed) {return false;}
        receipt=&row;break;
    }
    const auto* f=faction(request.vendor);
    const bool donation=f && (offer.item==f->tokenRewardItem
        || (f->secondRewardItem && offer.item==f->secondRewardItem));
    const bool rankClaim=f && offer.category==f->rewardCategory && offer.costs.empty();
    if(rankClaim && !rank_available(request.vendor,*f,p->before,p->character,investment.family5)) {return false;}
    if(!gates(offer,p->before,p->character,investment.family5)) {return false;}
    std::int32_t power{};
    for(const auto cost:offer.costs) {
        bd::items::Definition source{};
        if(cost.quantity>0 && bd::find_item_definition_index(cost.item,source) && source.bucketId==31) {
            const auto& c=p->before.characters[p->character];
            for(std::size_t i=0;i<c.inventory.count;++i) {
                const auto& item=c.inventory.values[i];
                if(!item.postmaster && item.definitionHash==source.definitionHash && !(item.flags&inv::kLockedItemFlag)) {power=item.level;break;}
            }
        }
    }
    for(const auto cost:offer.costs) if(!debit(*p,cost)) {return false;}
    bool turnedIn{};
    if(reply && !rankClaim && reply->site!=0xffff && !hand_in(*p,interaction,turnedIn)) {return false;}
    if(reply && !rankClaim && !turnedIn && reply->site!=0xffff) {
        std::uint16_t acknowledged{};
        if(acknowledgement(interaction.condition,acknowledged)) {
            if(!write_value(p->after,p->character,false,acknowledged,2)) {return false;}
            turnedIn=true;
        }
    }
    if(reply && reply->selection==0) {
        if(!turnedIn) {return false;}
    } else if(donation) {
        if(!reputation(*p,offer,*f)) {return false;}
    } else if(rankClaim) {
        auto* entry=progress(*p,*f);auto pool=f->rewardVendor;
        (void)catalog::loot_vendor(offer.item,pool);
        if(!entry || available(*entry,*f)<=0 || !reward(*p,pool,investment.family5)) {return false;}
        ++entry->rewards;
    } else if((request.vendor==19 && offer.category==4) || (request.vendor==195 && offer.category==25)) {
        if(!recycle(*p,offer)) {return false;}
    } else {
        const BountyPool* repeatable{};
        for(const auto& pool:bountyPools) if(pool.vendor==request.vendor && pool.item==offer.item) {repeatable=&pool;break;}
        // Campaign tiles name placeholders, while their native Pursuits contain
        // the real objective graph. The branch comparison recovered these joins.
        constexpr std::pair<std::uint32_t,std::uint32_t> substitutions[]{
            {0xBEB63647U,0x37DD26F0U},{0x6CBEA754U,0x6706D3ECU},{0x65683247U,0xF5B78E7FU}};
        bd::items::Definition sold{};
        if(!bd::find_item_definition_index(offer.item,sold)) {return false;}
        for(unsigned i=0;i<std::size(substitutions);++i) if(sold.definitionHash==substitutions[i].first) {
            if(!bd::find_item_definition_hash(substitutions[i].second,sold)) {return false;}offer.item=sold.definitionIndex;
            p->after.characters[p->character].vendorCampaigns|=static_cast<std::uint8_t>(1U<<i);
            break;
        }
        if(repeatable) {
            if(!bounty(*p,*repeatable)) {return false;}
        } else if(request.vendor==19 && offer.item==6024) {
            if(!reward(*p,197,investment.family5)) {return false;}
        } else if(!grant(*p,offer.item,offer.quantity,power)) {return false;}
    }
    if(receipt && !write_value(p->after,p->character,false,receipt->flag,2)) {return false;}
    // Backfill a legacy owned tablet only as part of a successful transaction.
    for(const auto& row:purchaseReceipts) if(receipt_earned(p->after,row)
        && !write_value(p->after,p->character,false,row.flag,2)) {return false;}
    if(p->changed.size()>16 || p->removed.size()>16 || !valid_transaction(*p)) {return false;}
    std::sort(p->changed.begin(),p->changed.end());p->changed.erase(std::unique(p->changed.begin(),p->changed.end()),p->changed.end());
    char line[180]{};std::snprintf(line,sizeof line,"ev=vendor stage=prepare result=ready vendor=%u sale=%d interaction=%d costs=%zu instances=%zu",
        request.vendor,request.sale,request.interaction,offer.costs.size(),p->changed.size());
    core::log::write(core::log::Channel::server,core::log::Level::info,line);
    pending.data=std::move(p);return true;
}
bool prepare_progress(Pending& pending) noexcept {
    pending={};if(!catalog::ready()) {return false;}
    auto p=begin_transaction();if(!p) {return false;}
    const auto investment=investment_snapshot();const auto& c=p->before.characters[p->character];
    for(std::size_t i=0;i<c.inventory.count;++i) {
        bd::items::Definition item{};catalog::QuestStep step{};catalog::Pursuit progress{};
        if(c.inventory.values[i].postmaster || !bd::find_item_definition_hash(c.inventory.values[i].definitionHash,item) || item.bucketId!=40
            || (item.definitionIndex>=15032 && item.definitionIndex<=15036)
            || !catalog::quest_step(item.definitionIndex,step) || !catalog::pursuit(step.item,progress)
            || !progress.globalQuest || catalog::manual(progress) || !complete(progress,*p,investment.family5)) {continue;}
        if(!advance(*p,c.inventory.values[i].instanceSoid,step,progress)) {return false;}
        if(!valid_transaction(*p)) {return false;}
        pending.data=std::move(p);return true;
    }
    return false;
}
bool prepare_decryption(std::uint64_t soid,std::uint16_t index,Pending& pending,bool postmaster) noexcept {
    pending={};if(!soid) {return false;}
    std::uint16_t pool{};
    if(!catalog::ready() || !catalog::loot_vendor(index,pool)) {return false;}
    auto p=begin_transaction();if(!p) {return false;}
    auto& c=p->after.characters[p->character];auto at=c.inventory.count;
    bd::items::Definition item{};if(!bd::find_item_definition_index(index,item)) {return false;}
    for(std::size_t i=0;i<c.inventory.count;++i) if(c.inventory.values[i].instanceSoid==soid
        && c.inventory.values[i].definitionHash==item.definitionHash) {at=i;break;}
    if(at==c.inventory.count || c.inventory.values[at].postmaster!=postmaster || c.inventory.values[at].quantity!=1 || (c.inventory.values[at].flags&inv::kLockedItemFlag)) {return false;}
    const auto power=c.inventory.values[at].level;
    inv::erase(c,at);p->removed.push_back(soid);
    if(!reward(*p,pool,investment_snapshot().family5,false)) {return false;}
    // Gear Up counts successful Prime decryptions, in the same transaction as
    // consuming the engram. Its native objective is character-local value 13081.
    bd::items::Definition gearUp{};
    if((index==6017 || index==6018) && bd::find_item_definition_index(15285,gearUp)
        && held(c,gearUp.definitionHash)) {
        catalog::Objective objective{};std::int32_t count{};
        if(!catalog::objective(8326,objective) || objective.threshold!=2
            || objective.expression.size()!=1 || objective.expression[0].opcode!=10
            || objective.expression[0].operand!=13081
            || !condition(objective.expression,p->after,p->character,investment_snapshot().family5,count)
            || !write_value(p->after,p->character,true,13081,(std::min)(1,(std::max)(0,count))+1,13079)) {return false;}
    }
    if(c.inventory.count && power>0) {
        auto& awarded=c.inventory.values[c.inventory.count-1];ItemInfo awardedInfo{};bd::items::Definition definition{};
        if(std::find(p->changed.begin(),p->changed.end(),awarded.instanceSoid)!=p->changed.end()
            && bd::find_item_definition_hash(awarded.definitionHash,definition) && info(definition.definitionIndex,awardedInfo)
            && awardedInfo.bucket.equipmentSlot>=0) {awarded.level=power;}
    }
    if(!valid_transaction(*p)) {return false;}
    pending.data=std::move(p);return true;
}
bool prepare_recovery(std::uint64_t soid,std::uint16_t index,std::int32_t quantity,Pending& pending) noexcept {
    pending={};ItemInfo item{};
    if(!soid || !info(index,item)) {return false;}
    auto p=begin_transaction();if(!p) {return false;}
    auto& c=p->after.characters[p->character];auto at=c.inventory.count;
    for(std::size_t i=0;i<c.inventory.count;++i) if(c.inventory.values[i].instanceSoid==soid) {at=i;break;}
    if(at==c.inventory.count || !c.inventory.values[at].postmaster
        || c.inventory.values[at].definitionHash!=item.item.definitionHash || c.nextInventorySerial>=INT32_MAX) {return false;}
    auto& stored=c.inventory.values[at];
    if(quantity==-1) {quantity=stored.quantity;}
    if(quantity<=0 || quantity>stored.quantity) {return false;}
    if(item.bucket.arraySelector==buckets::ArraySelector::profile) {
        std::int32_t available=item.detail.maxStackSize;std::size_t occupied{};bool exists{};
        for(std::size_t i=0;i<p->after.profileItemCount;++i) {
            const auto& row=p->after.profileItems[i];bd::items::Definition definition{};
            if(!bd::find_item_definition_hash(row.definitionHash,definition)) {return false;}
            occupied+=definition.bucketId==item.item.bucketId;
            if(row.definitionHash==item.item.definitionHash) {available-=row.quantity;exists=true;}
        }
        if(quantity>available || (!exists && (occupied>=item.bucket.slotCount || p->after.profileItemCount>=p->after.profileItems.size())) || !profile(*p,item,quantity)) {return false;}
        stored.quantity-=quantity;
        if(!stored.quantity) {p->removed.push_back(soid);inv::erase(c,at);}
        else {stored.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);p->changed.push_back(soid);}
    } else {
        if(item.bucket.arraySelector!=buckets::ArraySelector::character || quantity!=stored.quantity
            || !inv::has_room(c,item.item.bucketId)) {return false;}
        stored.postmaster=false;stored.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);p->changed.push_back(soid);
    }
    if(!valid_transaction(*p)) {return false;}
    pending.data=std::move(p);return true;
}
// Opcode 402 requests one unit. A mailed material stack keeps its resident
// identity until its last unit, which uses the ordinary dismantle/reward path.
bool prepare_postmaster_discard(std::uint64_t soid,std::uint16_t index,Pending& pending) noexcept {
    pending={};ItemInfo item{};
    if(!soid || !info(index,item) || item.detail.instancedDefinitionState!=bd::items::details::InstancedDefinitionState::stackable
        || item.item.bucketId<8) return false;
    auto p=begin_transaction();if(!p) return false;
    auto& c=p->after.characters[p->character];
    for(std::size_t i=0;i<c.inventory.count;++i) {
        auto& stored=c.inventory.values[i];if(stored.instanceSoid!=soid) continue;
        if(!stored.postmaster || stored.quantity<=1 || stored.definitionHash!=item.item.definitionHash
            || (stored.flags&inv::kLockedItemFlag) || c.nextInventorySerial>=INT32_MAX) return false;
        --stored.quantity;stored.mutationSerial=static_cast<std::int32_t>(c.nextInventorySerial++);p->changed.push_back(soid);
        f4::ResolvedLoadout loadout{};
        if(!account::valid(p->after) || !f4::resolve(p->after,p->character,loadout)) return false;
        pending.data=std::move(p);return true;
    }
    return false;
}
bool preview(const Pending& pending,AccountState& after) noexcept {
    if(!pending.data) {return false;}const auto current=account_snapshot();const auto& p=*pending.data;
    if(!matches(current,p)) {return false;}
    after=current;apply_transaction(after,p);return true;
}
bool commit(Pending& pending) noexcept {
    if(!pending.data) {return false;}const auto p=std::move(pending.data);
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& current=runtime::storage::g_state.account;bool ok=matches(current,*p);
    if(ok) {
        auto candidate=std::make_unique<AccountState>(current);
        apply_transaction(*candidate,*p);
        ok=state::persistence::commit_account(current,*candidate);if(ok) {current=*candidate;}
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);return ok;
}
}
