#pragma once
#include <cstdint>
namespace sunrise::client::content::scenarios::campaign_shared {
// Both campaign scenarios explicitly import these encounter objects from their strike package.
// Keep the scenario, object, registry and authored bubble pinned; unrelated shared objects
// continue to use the normal package ownership rule.
struct Group { std::uint32_t scenario,object,key;std::uint8_t bubble; };
inline constexpr Group kGroups[]{
    {0x80F474E3U,0x80F551DEU,0xCC7A090DU,15},
    {0x80F474E3U,0x80F551EBU,0x75A69C5AU,15},
    {0x80F474E3U,0x80F55205U,0xF8D0DADCU,15},
    {0x80F474E3U,0x80F550B8U,0x2763EC91U,9},
    {0x80F474E3U,0x80F550C9U,0xA9350228U,9},
    {0x80F474E3U,0x80F550DAU,0x7E558786U,9},
    {0x80F474E3U,0x80F54FFEU,0x588E5FB9U,2},
    {0x80F474E3U,0x80F55018U,0xFBD01A06U,2},
    {0x80F474E3U,0x80F55029U,0x8E64DB66U,2},
    {0x80F474E3U,0x80F54E07U,0xA5F083B5U,0},
    {0x80F474E3U,0x80F54E12U,0x73CBF939U,0},
    {0x80F474E3U,0x80F54E1DU,0xC6F46FAFU,0},
    {0x80F47445U,0x80F54563U,0xC95ECB1AU,1},
    {0x80F47445U,0x80F54571U,0xE5ABAF3FU,1},
    {0x80F47445U,0x80F5460EU,0x2763EC90U,10},
    {0x80F47445U,0x80F54627U,0x45B69C3CU,10},
    {0x80F47445U,0x80F54659U,0x40BFB1C0U,15},
    {0x80F47445U,0x80F54666U,0x5F8099ABU,15},
    {0x80F47445U,0x80F54A8EU,0x2CB86C0FU,17},
    {0x80F47445U,0x80F54AA4U,0xA1F8CD1CU,17},
};
constexpr bool candidate(std::uint32_t scenario,std::uint32_t object) noexcept {
    for(const auto& row:kGroups) if(row.scenario==scenario && row.object==object) return true;
    return false;
}
constexpr bool selected(std::uint32_t scenario,std::uint32_t object,std::uint32_t key,
                        std::uint8_t registry,std::uint8_t bubble,std::uint64_t explicitMask) noexcept {
    if(registry!=2 || bubble>=64) return false;
    for(const auto& row:kGroups) if(row.scenario==scenario && row.object==object && row.key==key
        && row.bubble==bubble && (explicitMask&(std::uint64_t{1}<<bubble))) return true;
    return false;
}
}
