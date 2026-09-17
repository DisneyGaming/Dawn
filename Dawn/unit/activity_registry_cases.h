#pragma once
#include "server/runtime/activity/mercury_registries.h"

void activity_registry_cases() {
    namespace r=dawn::server::runtime::activity::registry;
    namespace m=dawn::server::runtime::activity::mercury;
    using A=r::Admission;
    struct Storage {
        std::array<r::catalog::RosterGroup,m::kRegistries.size()> rosterGroups{};
        std::array<r::wire::BubbleSubBlock,3> rosterSubBlocks{};
        std::array<std::array<std::uint32_t,m::kRegistries.size()>,3> rosterSubBlockKeys{};
    };
    auto storage=std::make_unique<Storage>();
    r::wire::Roster roster{};
    r::catalog::Definition layout{};
    constexpr std::string_view name="mercury_freeroam";
    std::copy(name.begin(),name.end(),layout.name.begin());
    layout.nameLength=static_cast<std::uint8_t>(name.size());
    layout.tag=0x80F4696A;layout.bubbleCount=16;layout.bubbleHashes[15]=0xA83A9175;
    auto registryRows=std::make_unique<std::array<r::catalog::RosterGroup,m::kRegistries.size()>>();
    for(std::size_t i=0;i<registryRows->size();++i) {
        const auto& definition=m::kRegistries[i];
        CHECK(r::valid(definition));
        constexpr std::uint64_t mask=std::uint64_t{1}<<15;
        using dawn::state::activity::coo::registry::required;
        CHECK(required(definition,layout.tag,definition.objectTag,definition.key,mask));
        CHECK(!required(definition,layout.tag+1,definition.objectTag,definition.key,mask));
        CHECK(!required(definition,layout.tag,definition.objectTag+1,definition.key,mask));
        CHECK(!required(definition,layout.tag,definition.objectTag,definition.key+1,mask));
        CHECK(!required(definition,layout.tag,definition.objectTag,definition.key,0));
        CHECK(!required(definition,layout.tag,definition.objectTag,definition.key,mask|1));
        auto& row=(*registryRows)[i];
        row.registryKey=definition.key;row.objectTag=definition.objectTag;
        row.slotCount=static_cast<std::uint16_t>(definition.slots.size());
        for(std::size_t j=0;j<definition.slots.size();++j) {
            // Reverse order to exercise real slot identity, not array ordinal.
            const auto& slot=definition.slots[definition.slots.size()-1-j];
            row.slotIndices[j]=slot.index;row.slotTypes[j]=slot.type;row.slotFlags[j]=slot.flags();
            row.componentClasses[j]=slot.componentClass;row.senseSchemas[j]=slot.senseSchema;
            row.authSchemas[j]=slot.authSchema;row.descriptorTags[j]=slot.descriptorTag;
        }
    }
    auto find=[&](std::uint32_t key,r::catalog::RosterGroup& row) noexcept {
        for(const auto& candidate:*registryRows) {
            if(candidate.registryKey==key) { row=candidate;return true; }
        }
        return false;
    };
    auto admit=[&](const r::Definition& definition) { return r::admit(layout,*storage,roster,definition,find); };
    for(const auto& definition:m::kRegistries) { CHECK(admit(definition)==A::added); }
    CHECK(roster.groupCount==m::kRegistries.size());CHECK(roster.bubbleSubBlocks.size()==1);
    CHECK(roster.bubbleSubBlocks[0].bubble==15);CHECK(roster.bubbleSubBlocks[0].keys.size()==m::kRegistries.size());
    for(const auto& definition:m::kRegistries) { CHECK(admit(definition)==A::present); }
    // Existing wire descriptors are insufficient if the source cache disagrees.
    for(std::size_t i=0;i<registryRows->size();++i) {
        auto& row=(*registryRows)[i];
        for(std::size_t j=0;j<row.slotCount;++j) {
            const auto expectRejected=[&] {
                CHECK(admit(m::kRegistries[i])==A::schemaMismatch);
                CHECK(roster.groupCount==m::kRegistries.size());CHECK(roster.bubbleSubBlocks[0].keys.size()==m::kRegistries.size());
            };
            row.authSchemas[j]^=1;expectRejected();row.authSchemas[j]^=1;
            row.senseSchemas[j]^=1;expectRejected();row.senseSchemas[j]^=1;
            row.componentClasses[j]^=1;expectRejected();row.componentClasses[j]^=1;
            row.descriptorTags[j]^=1;expectRejected();row.descriptorTags[j]^=1;
            row.slotFlags[j]^=1;expectRejected();row.slotFlags[j]^=1;
        }
    }
    auto bad=m::kRegistries[0];bad.scenario^=1;CHECK(admit(bad)==A::unrelated);
    bad=m::kRegistries[0];bad.bubbleHash^=1;CHECK(admit(bad)==A::missingLayout);
    bad=m::kRegistries[0];bad.bubble=16;CHECK(admit(bad)==A::missingLayout);
    bad=m::kRegistries[0];bad.bubble=64;CHECK(admit(bad)==A::invalid);
    auto duplicateSlots=m::kPond;duplicateSlots[1]=duplicateSlots[0];
    bad=m::kRegistries[0];bad.slots=duplicateSlots;CHECK(admit(bad)==A::invalid);
    const auto originalKeys=storage->rosterSubBlockKeys[0];
    storage->rosterSubBlockKeys[0][0]=m::kRegistries[1].key;
    CHECK(admit(m::kRegistries[1])==A::conflict);
    storage->rosterSubBlockKeys[0]=originalKeys;
    storage->rosterSubBlocks[0].bubble=14;CHECK(admit(m::kRegistries[0])==A::conflict);
    storage->rosterSubBlocks[0].bubble=15;
    std::array<std::uint8_t,m::kRegistries.size()> presence{};presence.fill(1);presence[0]=0;storage->rosterSubBlocks[0].presence=presence;
    CHECK(admit(m::kRegistries[0])==A::conflict);
    presence[0]=1;CHECK(admit(m::kRegistries[0])==A::present);
    storage->rosterSubBlocks[0].presence={};
    // New admission cannot erase another service's explicit native removal state.
    roster={};std::array<std::uint32_t,1> externalKeys{123};std::array<std::uint8_t,1> removed{0};
    std::array<r::wire::BubbleSubBlock,1> externalBlocks{{{15,externalKeys,removed}}};
    roster.bubbleSubBlocks=externalBlocks;
    CHECK(admit(m::kRegistries[0])==A::conflict);CHECK(roster.groupCount==0);
    // External sub-block storage survives appending a distinct bubble.
    externalBlocks[0].bubble=14;
    CHECK(admit(m::kRegistries[0])==A::added);
    CHECK(roster.bubbleSubBlocks.size()==2);
    CHECK(roster.bubbleSubBlocks[0].presence[0]==0);
    CHECK(roster.bubbleSubBlocks[0].keys[0]==123);
    CHECK(roster.bubbleSubBlocks[1].keys[0]==m::kRegistries[0].key);
    // Capacity failure leaves both the roster and its external spans untouched.
    roster={};std::array<std::uint32_t,m::kRegistries.size()> fullKeys{};
    for(std::size_t i=0;i<fullKeys.size();++i) fullKeys[i]=static_cast<std::uint32_t>(i+1);
    externalBlocks[0]={15,fullKeys};roster.bubbleSubBlocks=externalBlocks;
    CHECK(admit(m::kRegistries[0])==A::noCapacity);CHECK(roster.groupCount==0);
    CHECK(roster.bubbleSubBlocks.data()==externalBlocks.data());CHECK(fullKeys.back()==fullKeys.size());
    roster={};roster.groupCount=roster.groups.size()+1;
    CHECK(admit(m::kRegistries[0])==A::invalid);
    roster={};const auto saved=(*registryRows)[0].registryKey;(*registryRows)[0].registryKey=123;
    CHECK(admit(m::kRegistries[0])==A::missingGroup);CHECK(roster.groupCount==0);(*registryRows)[0].registryKey=saved;
}
