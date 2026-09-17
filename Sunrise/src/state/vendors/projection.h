#pragma once
#include "../account/account_state.h"
#include <algorithm>

namespace sunrise::state::vendors {
// Sunrise has no clan membership or Trials schedule provider. Clan services
// remain unavailable; Saint's ordinary inventory is open, with its authored
// level, ownership and reward requirements still evaluated separately.
inline constexpr std::pair<std::uint16_t,bool> accessFlags[]{{std::uint16_t{144},false},{std::uint16_t{1182},true}};
inline constexpr std::pair<std::uint16_t,std::uint32_t> tokenCounters[]{
    {std::uint16_t{987},685157383U},{std::uint16_t{988},685157381U},{std::uint16_t{1006},3899548068U},{std::uint16_t{970},183980811U},
    {std::uint16_t{995},1505278293U},{std::uint16_t{972},2959556799U},{std::uint16_t{986},1270564331U},{std::uint16_t{1001},2270228604U},
    {std::uint16_t{599},259267040U},
    {std::uint16_t{974},3825769808U},{std::uint16_t{975},1305274547U},{std::uint16_t{976},3756389242U},{std::uint16_t{978},2640973641U},{std::uint16_t{979},950899352U},{std::uint16_t{980},478751073U},{std::uint16_t{982},494493680U},{std::uint16_t{983},2014411539U},{std::uint16_t{984},461171930U},{std::uint16_t{997},3201839676U},{std::uint16_t{998},3487922223U},{std::uint16_t{999},2949414982U},{std::uint16_t{2234},3022799524U},{std::uint16_t{2235},49145143U},{std::uint16_t{2236},2386485406U},{std::uint16_t{3004},3135658658U},{std::uint16_t{3005},31293053U},{std::uint16_t{3006},685095924U}};
template<class Object> void project_inventory(const AccountState& account,const CharacterState& character,Object& object) noexcept {
    const auto value=[&](std::uint16_t slot,std::int32_t value) {
        for(std::size_t i=0;i<object.unlockValueCount;++i) if(object.unlockValues[i].slot==slot) {object.unlockValues[i].value=value;return;}
        if(object.unlockValueCount<object.unlockValues.size()) {object.unlockValues[object.unlockValueCount++]={static_cast<std::int16_t>(slot),0,value};}
    };
    const auto flag=[&](std::uint16_t slot,bool set) {
        // Use the native acquired bank when mapped. Reserve the twenty override
        // rows for unmapped vendor, New Light, and Festival requirement flags.
        const auto binding=build_data::vendors::services::binding(false,slot);
        if(binding.present && binding.bank==2 && binding.row<object.acquiredFlags.size()) {
            object.acquiredFlags[binding.row]=static_cast<typename decltype(object.acquiredFlags)::value_type>(set?2:0);
            return;
        }
        if(object.unlockFlagCount<object.unlockFlags.size()) {object.unlockFlags[object.unlockFlagCount++]={static_cast<std::int16_t>(slot),static_cast<std::uint8_t>(set?2:0),0};}
    };
    for(const auto [slot,hash]:tokenCounters) {
        std::int64_t quantity{};
        for(std::size_t i=0;i<account.profileItemCount;++i) if(account.profileItems[i].definitionHash==hash) {quantity+=account.profileItems[i].quantity;}
        // Unheld currencies use native zero; reserve the bounded override bank
        // for owned stacks and leave room for quest/level values.
        if(quantity) {value(slot,static_cast<std::int32_t>((std::min)(quantity,static_cast<std::int64_t>(INT32_MAX))));}
    }
    value(465,character.level);
    constexpr std::uint16_t campaigns[]{12578,12609,12624};
    for(unsigned i=0;i<3;++i) {value(campaigns[i],(character.vendorCampaigns>>i)&1U);}
    // Class requirements are derived from the native character identity already
    // encoded in Family 4. Do not spend three override rows duplicating it: ten
    // turn-in flags, two service flags, four New Light flags and four Festival
    // flags exactly fill the native twenty-row bank.
    // The installed exchange interactions test these availability flags as well
    // as the amount band on the sale. Offer a turn-in while its currency is held.
    for(const auto& f:factions) if(f.turnInFlag) {
        bool held{};
        for(std::size_t i=0;i<account.profileItemCount;++i) {
            const auto& item=account.profileItems[i];
            held|=item.quantity>0 && (item.definitionHash==f.token || (f.secondToken && item.definitionHash==f.secondToken) || (f.thirdToken && item.definitionHash==f.thirdToken));
        }
        flag(f.turnInFlag,held);
    }
    // Explicit false is 1, not the unset value 0. Family-5 overrides retain
    // priority over these defaults, as they do in the purchase evaluator.
    for(const auto [slot,available]:accessFlags) {
        if(object.unlockFlagCount<object.unlockFlags.size()) {
            object.unlockFlags[object.unlockFlagCount++]={static_cast<std::int16_t>(slot),static_cast<std::uint8_t>(available?2:1),0};
        }
    }
}
}
