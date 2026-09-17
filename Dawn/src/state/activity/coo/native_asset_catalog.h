#pragma once
#include "executor.h"
#include <span>
#include <string_view>

namespace dawn::state::activity::coo::native_catalog {
struct AssetBinding {
    Asset asset;
    std::uint32_t offset,component,sense,authority;
    std::string_view name;
};
struct Group {
    std::uint32_t key,tag,hint;
    std::uint8_t bubble;
    bool topLevel;
    std::span<const AssetBinding> slots;
};
// Wire schemas belong to the native component type. Authored registries retain
// their own definition, offset, slot order and name; unsupported types fail at build time.
consteval AssetBinding bind(Asset asset,std::uint32_t offset,std::string_view name) {
    switch(asset.type) {
    case 1:return {asset,offset,0x80809A3BU,0x80807ECCU,0x80807EC9U,name};
    case 2:return {asset,offset,0x8080834EU,0x80807DA2U,0x80807DA1U,name};
    case 3:return {asset,offset,0x80808348U,0x80807F04U,0x80807F0CU,name};
    case 4:return {asset,offset,0x80809927U,0x8080992EU,0x8080992FU,name};
    case 5:return {asset,offset,0x80804F01U,0xFFFFFFFFU,0x80804F04U,name};
    case 6:return {asset,offset,0x80804F06U,0xFFFFFFFFU,0x80804F08U,name};
    case 11:return {asset,offset,0x80804E8EU,0xFFFFFFFFU,0x80804F58U,name};
    case 13:return {asset,offset,0x80804F2DU,0x80804F2FU,0x80804F30U,name};
    case 16:return {asset,offset,0x808099F1U,0xFFFFFFFFU,0x808099F9U,name};
    case 17:return {asset,offset,0x80809915U,0xFFFFFFFFU,0x8080991AU,name};
    case 18:return {asset,offset,0x80809917U,0xFFFFFFFFU,0x80809919U,name};
    case 19:return {asset,offset,0x80809525U,0xFFFFFFFFU,0x80809527U,name};
    case 23:return {asset,offset,0x80804F45U,0x80804F47U,0x80804F48U,name};
    case 26:return {asset,offset,0x8080953FU,0x8080954AU,0x8080954BU,name};
    case 30:return {asset,offset,0x8080952FU,0x80809531U,0x80809532U,name};
    case 31:return {asset,offset,0x80809522U,0xFFFFFFFFU,0x80809524U,name};
    case 34:return {asset,offset,0x80809568U,0xFFFFFFFFU,0x8080956AU,name};
    case 35:return {asset,offset,0x808099BDU,0xFFFFFFFFU,0x808099BFU,name};
    case 39:return {asset,offset,0x80804EE2U,0x80804EE4U,0x80804EE5U,name};
    case 41:return {asset,offset,0x80804EE6U,0x80804EE8U,0x80804EE9U,name};
    case 42:return {asset,offset,0x80809583U,0xFFFFFFFFU,0x80809586U,name};
    case 43:return {asset,offset,0x80806382U,0x8080626AU,0x8080626BU,name};
    case 53:return {asset,offset,0x80804F4BU,0xFFFFFFFFU,0x80804F77U,name};
    case 57:return {asset,offset,0x808094D7U,0xFFFFFFFFU,0xFFFFFFFFU,name};
    case 66:return {asset,offset,0x808094CFU,0xFFFFFFFFU,0xFFFFFFFFU,name};
    case 68:return {asset,offset,0x80804F53U,0xFFFFFFFFU,0x80804F67U,name};
    case 70:return {asset,offset,0x808094EEU,0x808094F0U,0x808094F1U,name};
    default:throw "Unsupported native asset type";
    }
}
constexpr std::uint8_t slot_flags(const AssetBinding& binding) noexcept {
    return (binding.sense!=UINT32_MAX?1U:0U)|(binding.authority!=UINT32_MAX?2U:0U);
}
}
