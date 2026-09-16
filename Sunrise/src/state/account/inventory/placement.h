#pragma once
#include "../account_state.h"
#include "../../build_data/runtime.h"

namespace sunrise::state::account::inventory {
// Available room for one profile definition, including existing partial stacks.
// Non-action currencies have one aggregate row; shaders/mods own stack identities.
inline std::int64_t profile_room(const AccountState& a,std::uint32_t hash) noexcept {
    build_data::items::Definition definition{};build_data::items::details::Definition detail{};
    build_data::inventory::buckets::Descriptor bucket{};
    if(!build_data::find_item_definition_hash(hash,definition)
        || !build_data::find_configured_item_detail(definition.definitionIndex,detail)
        || detail.definitionHash!=hash || detail.bucketId!=definition.bucketId || detail.maxStackSize<=0
        || detail.instancedDefinitionState!=build_data::items::details::InstancedDefinitionState::stackable
        || !build_data::find_inventory_bucket_descriptor(definition.bucketId,bucket)
        || bucket.arraySelector!=build_data::inventory::buckets::ArraySelector::profile) {return -1;}
    std::size_t occupied{};std::int64_t room{};bool found{};
    for(std::size_t i=0;i<a.profileItemCount;++i) {
        const auto& item=a.profileItems[i];build_data::items::Definition other{};
        if(!build_data::find_item_definition_hash(item.definitionHash,other)) {return -1;}
        occupied+=other.bucketId==definition.bucketId;
        if(item.definitionHash!=hash) {continue;}
        if(item.quantity<=0 || item.quantity>detail.maxStackSize) {return -1;}
        found=true;room+=detail.maxStackSize-item.quantity;
    }
    if(a.profileItemCount<a.profileItems.size() && occupied<bucket.slotCount
        && (!found || build_data::is_profile_action_source(definition.definitionIndex,definition.bucketId))) {
        room+=detail.maxStackSize;
    }
    return room;
}
// Ordinary items occupy their authored bucket. Recovery rows belong to the
// selected character even when the original bucket is account-wide.
inline bool has_room(const CharacterState& c,std::uint8_t id) noexcept {
    build_data::inventory::buckets::Descriptor bucket{};
    if(!build_data::find_inventory_bucket_descriptor(id,bucket)
        || bucket.arraySelector!=build_data::inventory::buckets::ArraySelector::character) {return false;}
    std::size_t used{};
    const auto count=[&](const Item& item) {
        if(item.postmaster) {used+=id==kPostmasterBucket;return true;}
        build_data::items::Definition definition{};
        if(!build_data::find_item_definition_hash(item.definitionHash,definition)) {return false;}
        used+=definition.bucketId==id;return true;
    };
    for(const auto& item:c.equipment.slots) if(item && !count(*item)) {return false;}
    for(std::size_t i=0;i<c.inventory.count;++i) if(!count(c.inventory.values[i])) {return false;}
    return used<bucket.slotCount;
}
inline void erase(CharacterState& c,std::size_t at) noexcept {
    for(auto i=at+1;i<c.inventory.count;++i) {c.inventory.values[i-1]=c.inventory.values[i];}
    c.inventory.values[--c.inventory.count]={};
}
// Caller stages the entire character and publishes the returned removal in the
// same transaction. Locking or mutating a stored item does not make it younger.
inline bool insert(CharacterState& c,Item item,std::uint64_t& removed) noexcept {
    removed=0;build_data::items::Definition definition{};
    if(!valid(item) || !build_data::find_item_definition_hash(item.definitionHash,definition)) {return false;}
    item.postmaster|=!has_room(c,definition.bucketId);
    auto oldest=c.inventory.count;
    if(item.postmaster) {
        build_data::inventory::buckets::Descriptor bucket{};
        if(!build_data::find_inventory_bucket_descriptor(kPostmasterBucket,bucket)
            || bucket.arraySelector!=build_data::inventory::buckets::ArraySelector::character || !bucket.slotCount) {return false;}
        std::size_t count{};
        for(std::size_t i=0;i<c.inventory.count;++i) if(c.inventory.values[i].postmaster) {
            if(oldest==c.inventory.count) {oldest=i;}++count;
        }
        if(count>bucket.slotCount) {return false;}
        if(count<bucket.slotCount) {oldest=c.inventory.count;}
    }
    if(c.inventory.count>=c.inventory.values.size() && oldest==c.inventory.count) {return false;}
    if(oldest<c.inventory.count) {removed=c.inventory.values[oldest].instanceSoid;erase(c,oldest);}
    c.inventory.values[c.inventory.count++]=item;return true;
}
}
