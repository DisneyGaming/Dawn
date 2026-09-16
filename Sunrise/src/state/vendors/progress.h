#pragma once
#include <array>
#include <cstdint>
#include "../build_data/vendors/service_catalog.h"

namespace sunrise::state::vendors {
struct Progress {
    std::uint16_t vendor{0xffff};
    std::int32_t points{},rewards{}; // Lifetime reputation and packages already claimed.
    bool operator==(const Progress&) const = default;
};
using ProgressBank=std::array<Progress,16>;
struct Faction {
    std::uint16_t vendor,progression,rewardVendor,rewardCategory,tokenRewardItem;
    std::uint8_t scope;
    std::int32_t rankStep;
    std::uint32_t token,secondToken;
    std::int32_t points,secondPoints;
    std::uint16_t turnInFlag{};
    std::uint16_t secondRewardItem{};
    std::uint32_t thirdToken{};
    std::int32_t thirdPoints{};
    std::uint16_t rewardCounter{},claimedCounter{};
};
// Installed 86657 progression joins and rank thresholds. Token conversion rates
// are server reward policy; prices and quantities come from the installed sale.
inline constexpr Faction factions[]{
    {11,58,78,2,1222,1,2000,1505278293U,0,100,0,2364,0,0,0,915},
    {16,62,37,3,1228,1,2000,3899548068U,0,100,0,2374,0,0,0,927},
    {18,49,36,10,1211,1,2000,183980811U,0,100,0,2339,0,0,0,888},
    {20,50,62,1,1212,0,2000,2959556799U,0,100,0,0,0,0,0,891},
    {21,54,63,1,1219,0,2000,1270564331U,0,100,0,0,0,0,0,903},
    {22,55,44,8,1220,1,3000,685157383U,685157381U,30,25,2357,0,0,0,906},
    {24,60,64,1,1225,0,2000,2270228604U,0,100,0,0,0,0,0,921},
    {392,46,146,12,568,0,2000,259267040U,0,100,0,0,0,0,0,592,595},
    {467,74,197,10,12462,0,5000,4237793825U,0,10,0,0,12463,0,0,7091},
    {5,51,39,4,1213,1,2750,3825769808U,3756389242U,100,250,2343,1214,1305274547U,50,894},
    {6,52,40,5,1215,1,2750,2640973641U,478751073U,100,250,2347,1216,950899352U,50,897},
    {10,53,41,4,1217,1,2750,494493680U,461171930U,100,250,2351,1218,2014411539U,50,900},
    {14,59,42,3,1223,1,2750,3201839676U,2949414982U,100,250,2366,1224,3487922223U,50,918},
    {66,63,76,2,2928,1,2750,3022799524U,2386485406U,100,250,3851,2930,49145143U,50,2232},
    {125,64,146,2,4018,1,2750,3135658658U,685095924U,100,250,4800,4019,31293053U,50,2996},
};
inline const Faction* faction(std::uint16_t vendor) noexcept {
    // Werner and Benedict share Leviathan progression 58 and pending-package
    // counter 915. Keep one bank so either vendor can accept tokens/claim ranks.
    if(vendor==324) vendor=11;
    for(const auto& f:factions) if(f.vendor==vendor) {return &f;}return nullptr;
}
inline std::int32_t available(const Progress& p,const Faction& f) noexcept {
    return p.points/f.rankStep-p.rewards;
}
inline bool counter(const Progress& p,const Faction& f,std::uint16_t slot,std::int32_t& value) noexcept {
    if(slot==f.rewardCounter) {value=f.claimedCounter?p.points/f.rankStep:available(p,f);return true;}
    if(f.claimedCounter && slot==f.claimedCounter) {value=p.rewards;return true;}return false;
}
template<class Object> void project(const ProgressBank& bank,Object& object) noexcept {
    for(const auto& p:bank) {
        const auto* f=faction(p.vendor);if(!f) {continue;}
        for(auto& entry:object.progressions) if(entry.definitionIndex==f->progression) {
            // The native level walk reads lane zero. Preserve other lanes: their
            // meaning is not established by the package's progression definition.
            entry.values[0]=p.points;
        }
        // Authored rank interactions read numeric unlock counters, not the other
        // progression lanes. Saint compares earned ranks with a claimed counter.
        const auto slot=f->claimedCounter?f->claimedCounter:f->rewardCounter;
        const auto b=build_data::vendors::services::binding(true,slot);
        if(b.present && b.bank==f->scope && b.row<object.objectiveValues.size()) {
            object.objectiveValues[b.row]=f->claimedCounter?p.rewards:available(p,*f);
        }
    }
}
}
