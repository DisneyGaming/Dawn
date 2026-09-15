#pragma once
#include "../Newlight/launchpad/native_catalog.h"
namespace sunrise::state::activity::vendors {
using newlight::launchpad::AssetBinding;
using newlight::launchpad::Group;
// Installed destination NPCs, native named performances and placement-rule joins.
inline constexpr AssetBinding kAssets[]{
    {{0xC984DDDEU,0x80BE9309U,1,0},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_vendor_fallen_conflict"},
    {{0xC984DDDEU,0x80BE6461U,42,2},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_vendor_fallen_conflict_idle"},
    {{0x05324D75U,0x80B9997AU,1,0},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_sloan_vendor"},
    {{0x05324D75U,0x80BF16E2U,42,2},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_sloan_vendor_idle"},
    {{0x36E4495DU,0x80BD6EDCU,1,0},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"ashir_squad"},
    {{0x36E4495DU,0x80BD7599U,2,1},0xB58U,0x8080834EU,0x80807DA2U,0x80807DA1U,"ashir_squad__vendor_ashir"},
    {{0x36E4495DU,0x80BD33C9U,42,3},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"pf_civilian_vendor_talus"},
    {{0x7B3D65F8U,0x80C030D0U,1,0},0x878U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_vendor"},
    {{0x7B3D65F8U,0x80C030D6U,42,2},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_vendor_idle"},
    {{0x54BD10BDU,0x80C03B26U,4,1},0x4C8U,0x80809927U,0x8080992EU,0x8080992FU,"o_penumbra_vendor"},
    {{0x0D1B60CFU,0x80F6B14FU,1,0},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_vendor_ana_bray"},
    {{0x0D1B60CFU,0x80F6B158U,42,3},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"perf_state_machine"},
    {{0x564C6ECEU,0x80F5B9CCU,1,0},0x878U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_vance"},
    {{0x564C6ECEU,0x80F5BA28U,42,2},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_vance_idle"},
    {{0x6D47E9B3U,0x80FDC330U,1,1},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_vendor_spider"},
    {{0x6D47E9B3U,0x80FDC7ACU,42,8},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_vendor_spider_idle"},
    {{0x4ECC8169U,0x80F28552U,1,1},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_dreaming_city_vendor"},
    {{0x4ECC8169U,0x80F28757U,42,4},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_dreaming_city_vendor_idle"},
    {{0x882CA5C9U,0x81572D11U,4,0},0x4C8U,0x80809927U,0x8080992EU,0x8080992FU,"o_eris_1"},
    {{0x882CA5C9U,0x81572D29U,4,8},0x4C8U,0x80809927U,0x8080992EU,0x8080992FU,"o_nightmare_forge_vendor"},
    {{0x23AB4F6AU,0x80B84546U,1,2},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_postmaster"},
    {{0x23AB4F6AU,0x80B84549U,1,3},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_hawthorne"},
    {{0x23AB4F6AU,0x80B84555U,1,14},0x728U,0x80809A3BU,0x80807ECCU,0x80807EC9U,"sq_cryptarch"},
    {{0x23AB4F6AU,0x80B848D1U,42,22},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_cryptarch_idle"},
    {{0x23AB4F6AU,0x80B848D4U,42,23},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_hawthorne_idle"},
    {{0x23AB4F6AU,0x80B848D7U,42,24},0x228U,0x80809583U,0xFFFFFFFFU,0x80809586U,"sq_postmaster_idle"},
    {{0xF18B720FU,0x815BA3D9U,4,0},0x4C8U,0x80809927U,0x8080992EU,0x8080992FU,"o_vendor"},
};
inline constexpr Group kGroups[]{
    {0xC984DDDEU,0x80BE950DU,UINT32_MAX,51,false,std::span(kAssets).subspan(0,2)},
    {0x05324D75U,0x80B9922EU,UINT32_MAX,2,false,std::span(kAssets).subspan(2,2)},
    {0x36E4495DU,0x80BD759CU,UINT32_MAX,4,false,std::span(kAssets).subspan(4,3)},
    {0x7B3D65F8U,0x80C038CDU,UINT32_MAX,3,false,std::span(kAssets).subspan(7,2)},
    {0x54BD10BDU,0x80C02ADEU,UINT32_MAX,1,false,std::span(kAssets).subspan(9,1)},
    {0x0D1B60CFU,0x80F6B15BU,UINT32_MAX,1,false,std::span(kAssets).subspan(10,2)},
    {0x564C6ECEU,0x80F5BA2BU,UINT32_MAX,15,false,std::span(kAssets).subspan(12,2)},
    {0x6D47E9B3U,0x80FDC7AFU,UINT32_MAX,21,false,std::span(kAssets).subspan(14,2)},
    {0x4ECC8169U,0x80F2875AU,UINT32_MAX,20,false,std::span(kAssets).subspan(16,2)},
    {0x882CA5C9U,0x81572D38U,UINT32_MAX,23,false,std::span(kAssets).subspan(18,2)},
    {0x23AB4F6AU,0x80B84719U,UINT32_MAX,0,false,std::span(kAssets).subspan(20,6)},
    {0xF18B720FU,0x815BA505U,UINT32_MAX,0,false,std::span(kAssets).subspan(26,1)},
};
inline constexpr std::span<const Group> groups(std::uint32_t scenario) noexcept {
    switch(scenario) {
    case 0x80B2F00AU:return std::span(kGroups).subspan(0,1);
    case 0x80B3E142U:return std::span(kGroups).subspan(1,1);
    case 0x80B56B1BU:return std::span(kGroups).subspan(2,1);
    case 0x80B43A1CU:return std::span(kGroups).subspan(3,2);
    case 0x80F6AB20U:return std::span(kGroups).subspan(5,1);
    case 0x80F4696AU:return std::span(kGroups).subspan(6,1);
    case 0x80FC9645U:return std::span(kGroups).subspan(7,1);
    case 0x80F1404DU:return std::span(kGroups).subspan(8,1);
    case 0x81503E69U:return std::span(kGroups).subspan(9,1);
    case 0x80B84013U:return std::span(kGroups).subspan(10,1);
    case 0x815BA00EU:return std::span(kGroups).subspan(11,1);
    default:return {};
    }
}
// Like the upstream open-world profiles, retain the complete native registry
// in the package catalog. The bindings above select authority, not the full
// set of sync records needed for the native registry's initialization barrier.
inline constexpr bool required(std::uint32_t scenario,std::uint32_t objectTag,
    std::uint32_t key,std::uint64_t explicitBubbleMask) noexcept {
    for(const auto& group:groups(scenario))
        if(group.tag==objectTag && group.key==key
            && explicitBubbleMask==(UINT64_C(1)<<group.bubble)) {return true;}
    return false;
}
struct Source {std::uint32_t scenario,key;std::uint16_t source,rule,idle;std::uint32_t sequence;std::uint16_t member;std::array<float,3> position;bool approach;};
inline constexpr Source kSources[]{
    {0x80B2F00AU,0xC984DDDEU,0,3,2,0x3342E2ABU,65535,{541.294921875F,92.516426086F,92.361770630F},true},
    {0x80B3E142U,0x05324D75U,0,3,2,0x22503C9BU,65535,{-193.156616211F,410.409851074F,77.446151733F},true},
    {0x80B56B1BU,0x36E4495DU,0,4,3,0xF46E1578U,1,{1378.385620117F,404.698059082F,3.850521088F},true},
    {0x80B43A1CU,0x7B3D65F8U,0,65535,2,0x5D354105U,65535,{1148.446044922F,-135.627197266F,-16.237630844F},false},
    {0x80F6AB20U,0x0D1B60CFU,0,4,3,0x69F48E99U,65535,{-333.127502441F,-365.317596436F,34.468006134F},true},
    {0x80F4696AU,0x564C6ECEU,0,65535,2,0x010B0F07U,65535,{37.833992004F,249.519378662F,311.340515137F},true},
    {0x80FC9645U,0x6D47E9B3U,1,9,8,0x681C2A3AU,65535,{105.533561707F,-297.446044922F,-48.572269440F},true},
    {0x80F1404DU,0x4ECC8169U,1,7,4,0x60A2CEBAU,65535,{68.267669678F,-216.443008423F,30.764768600F},true},
    {0x80B84013U,0x23AB4F6AU,2,32,24,0x870A7D66U,65535,{42.693180084F,-18.876859665F,5.167573929F},true},
    {0x80B84013U,0x23AB4F6AU,3,29,23,0xFD41C0E5U,65535,{15.000529289F,6.036268234F,9.361040115F},true},
    {0x80B84013U,0x23AB4F6AU,14,31,22,0x6996DACAU,65535,{-2.316748619F,-18.451322556F,4.086104393F},true},
};
}
