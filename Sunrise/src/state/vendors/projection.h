#pragma once
#include "../account/account_state.h"
#include <algorithm>

namespace sunrise::state::vendors {
inline constexpr std::pair<std::uint16_t,std::uint32_t> tokenCounters[]{
    {987,685157383U},{988,685157381U},{1006,3899548068U},{970,183980811U},
    {995,1505278293U},{972,2959556799U},{986,1270564331U},{1001,2270228604U},
    {599,259267040U},
    {974,3825769808U},{975,1305274547U},{976,3756389242U},{978,2640973641U},{979,950899352U},{980,478751073U},{982,494493680U},{983,2014411539U},{984,461171930U},{997,3201839676U},{998,3487922223U},{999,2949414982U},{2234,3022799524U},{2235,49145143U},{2236,2386485406U},{3004,3135658658U},{3005,31293053U},{3006,685095924U}};
template<class Object> void project_inventory(const AccountState& account,const CharacterState& character,Object& object) noexcept {
    const auto value=[&](std::uint16_t slot,std::int32_t value) {
        for(std::size_t i=0;i<object.unlockValueCount;++i) if(object.unlockValues[i].slot==slot) {object.unlockValues[i].value=value;return;}
        if(object.unlockValueCount<object.unlockValues.size()) {object.unlockValues[object.unlockValueCount++]={static_cast<std::int16_t>(slot),0,value};}
    };
    const auto flag=[&](std::uint16_t slot,bool set) {
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
    flag(239,character.characterClass==CharacterClass::hunter);
    flag(264,character.characterClass==CharacterClass::titan);
    flag(271,character.characterClass==CharacterClass::warlock);
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
    // Hawthorne's clan prerequisite and Saint's access prerequisite have no
    // stored native bank. The server has no clan/Trials-access provider yet.
    // Publish explicit false (1), rather than unset (0), so the local context
    // cannot advertise eligibility the purchase service will reject. An explicit
    // authoritative Family-5 grant still has priority over this default.
    for(const auto slot:{144,1182}) {
        if(object.unlockFlagCount<object.unlockFlags.size()) {
            object.unlockFlags[object.unlockFlagCount++]={static_cast<std::int16_t>(slot),1,0};
        }
    }
}
}
