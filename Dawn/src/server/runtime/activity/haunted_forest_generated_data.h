#pragma once

#include <array>
#include <cstdint>

#include "../../../state/activity/native_population_events.h"

namespace dawn::server::runtime::activity::haunted_forest::mode {
namespace native_population = dawn::state::activity::native_population;

inline constexpr std::uint32_t kGeneratedResourceTag=0x8150A904U;
inline constexpr std::uint32_t kGeneratedWorkerDefinitionTag=0x80C59636U;
inline constexpr std::uint32_t kGeneratedWorkerDefinitionOffset=0x11038U;
inline constexpr std::uint32_t kGeneratedWorkerRuntimeTag=0x80C59636U;
inline constexpr std::uint32_t kGeneratedWorkerRuntimeOffset=0xE0U;
inline constexpr std::uint32_t kNativeArchetypeType=0xB10F785DU;
inline constexpr std::uint32_t kMinorArchetypeValue=0xD09AD130U;
inline constexpr std::uint32_t kMajorArchetypeValue=0x4A3554B4U;
inline constexpr std::uint32_t kMinibossArchetypeValue=0x9A3D2CCBU;
inline constexpr std::uint32_t kUltraArchetypeValue=0x357DB49DU;
inline constexpr std::uint32_t kNamedEliteCompletionGroup=0x574459CEU;

inline constexpr std::array<native_population::PaletteDefinition,19> kGeneratedPalettes{{
    {0x8150A2A1U,32848U,72608U,481U},
    {0x8150A2EEU,34480U,73728U,481U},
    {0x8150A343U,32720U,71824U,481U},
    {0x8150A3A1U,34336U,73536U,481U},
    {0x8150A404U,35408U,74752U,481U},
    {0x8150A442U,33136U,72192U,481U},
    {0x8150A49DU,41584U,80912U,505U},
    {0x8150A4F6U,33840U,73136U,481U},
    {0x8150A54EU,40048U,79168U,504U},
    {0x8150A59CU,39136U,78384U,505U},
    {0x8150A612U,42512U,82240U,481U},
    {0x8150A64CU,40480U,79824U,505U},
    {0x8150A78EU,5664U,7584U,10U},
    {0x8150A7B4U,5120U,6816U,10U},
    {0x8150A7FEU,5872U,7840U,10U},
    {0x8150A85EU,7440U,9792U,10U},
    {0x8150A893U,6160U,8080U,10U},
    {0x8150A8B6U,5968U,7664U,10U},
    {0x8150A902U,7680U,10752U,20U},
}};

enum class Rank : std::uint8_t { minor, major, miniboss, ambiguous, nonRanked };
struct RankedPrefab final {
    std::uint32_t tag{};
    Rank rank{Rank::nonRanked};
    bool hasEliteCompletionGroup{};
};

inline constexpr std::array<RankedPrefab,163> kRankedPrefabs{{
    {0x80C5964EU,Rank::nonRanked,false},
    {0x80F44C55U,Rank::minor,false},
    {0x80F44C57U,Rank::major,true},
    {0x80F44C5EU,Rank::major,true},
    {0x80F44C61U,Rank::major,false},
    {0x80F44C65U,Rank::major,false},
    {0x80F44C67U,Rank::miniboss,true},
    {0x80F44C6BU,Rank::miniboss,true},
    {0x80F44C6FU,Rank::miniboss,true},
    {0x80F44C72U,Rank::major,true},
    {0x80F44C79U,Rank::miniboss,true},
    {0x80F44C7CU,Rank::minor,false},
    {0x80F44C80U,Rank::major,true},
    {0x80F44C85U,Rank::miniboss,true},
    {0x80F44C88U,Rank::ambiguous,false},
    {0x80F44C8BU,Rank::major,true},
    {0x80F44C8EU,Rank::major,true},
    {0x80F44C93U,Rank::major,true},
    {0x80F44C96U,Rank::miniboss,true},
    {0x80F44C9AU,Rank::miniboss,true},
    {0x80F44C9FU,Rank::miniboss,true},
    {0x80F44CA4U,Rank::miniboss,true},
    {0x80F44CA7U,Rank::minor,true},
    {0x80F44CAAU,Rank::minor,true},
    {0x80F44CADU,Rank::minor,false},
    {0x80F44CB0U,Rank::minor,false},
    {0x80F44CB3U,Rank::major,true},
    {0x80F44CB8U,Rank::major,true},
    {0x80F44CBBU,Rank::miniboss,true},
    {0x80F44CBEU,Rank::miniboss,true},
    {0x80F44CC1U,Rank::miniboss,true},
    {0x80F44CC3U,Rank::ambiguous,false},
    {0x80F44CC7U,Rank::major,true},
    {0x80F44CCEU,Rank::miniboss,true},
    {0x80F44CD2U,Rank::major,true},
    {0x80F44CDBU,Rank::miniboss,true},
    {0x80F44CDDU,Rank::major,true},
    {0x80F44CE2U,Rank::major,false},
    {0x80F44CE7U,Rank::major,false},
    {0x80F44CEAU,Rank::miniboss,true},
    {0x80F44CEFU,Rank::major,false},
    {0x80F44CF2U,Rank::minor,false},
    {0x80F44CF4U,Rank::major,true},
    {0x80F44CF9U,Rank::major,false},
    {0x80F44CFCU,Rank::miniboss,true},
    {0x80F44CFFU,Rank::minor,false},
    {0x80F44D03U,Rank::major,true},
    {0x80F44D0AU,Rank::major,true},
    {0x80F44D10U,Rank::major,false},
    {0x80F44D13U,Rank::miniboss,true},
    {0x80F44D16U,Rank::minor,false},
    {0x80F44D19U,Rank::major,true},
    {0x80F44D1FU,Rank::miniboss,true},
    {0x80F44D24U,Rank::miniboss,true},
    {0x80F44D27U,Rank::minor,false},
    {0x80F44D29U,Rank::major,true},
    {0x80F44D2EU,Rank::major,true},
    {0x80F44D31U,Rank::major,false},
    {0x80F44D35U,Rank::major,false},
    {0x80F44D37U,Rank::miniboss,true},
    {0x80F44D3BU,Rank::miniboss,true},
    {0x80F44D3FU,Rank::miniboss,true},
    {0x80F44D42U,Rank::minor,false},
    {0x80F44D45U,Rank::minor,false},
    {0x80F44D48U,Rank::minor,false},
    {0x80F44D4BU,Rank::minor,false},
    {0x80F44D4EU,Rank::major,true},
    {0x80F44D51U,Rank::major,true},
    {0x80F44D54U,Rank::major,true},
    {0x80F44D57U,Rank::major,true},
    {0x80F44D5AU,Rank::miniboss,true},
    {0x80F44D5CU,Rank::miniboss,true},
    {0x80F44D61U,Rank::miniboss,true},
    {0x80F44D65U,Rank::miniboss,true},
    {0x80F44D69U,Rank::miniboss,true},
    {0x80F44D6EU,Rank::miniboss,true},
    {0x80F44D71U,Rank::minor,false},
    {0x80F44D74U,Rank::minor,false},
    {0x80F44D77U,Rank::major,true},
    {0x80F44D7CU,Rank::miniboss,true},
    {0x80F44D80U,Rank::miniboss,true},
    {0x80F44D85U,Rank::miniboss,true},
    {0x80F44D89U,Rank::miniboss,true},
    {0x80F44D8FU,Rank::major,true},
    {0x80F44D92U,Rank::major,true},
    {0x80F44D95U,Rank::miniboss,true},
    {0x80F44D99U,Rank::minor,false},
    {0x80F44D9BU,Rank::major,true},
    {0x80F44DA3U,Rank::major,false},
    {0x80F44DA6U,Rank::miniboss,true},
    {0x80F44DA9U,Rank::minor,false},
    {0x80F44DACU,Rank::major,true},
    {0x80F44DB1U,Rank::major,true},
    {0x80F44DB4U,Rank::miniboss,true},
    {0x80F44DB7U,Rank::miniboss,true},
    {0x80F44DBAU,Rank::minor,false},
    {0x80F44DBCU,Rank::major,true},
    {0x80F44DC4U,Rank::major,true},
    {0x80F44DC8U,Rank::major,false},
    {0x80F44DCAU,Rank::miniboss,true},
    {0x80F44DCEU,Rank::miniboss,true},
    {0x80F44DD2U,Rank::miniboss,true},
    {0x80F44DD6U,Rank::miniboss,true},
    {0x80F44DDAU,Rank::miniboss,true},
    {0x80F44DDDU,Rank::minor,false},
    {0x80F44DE0U,Rank::minor,false},
    {0x80F44DE3U,Rank::major,true},
    {0x80F44DE9U,Rank::miniboss,true},
    {0x80F44DECU,Rank::minor,false},
    {0x80F44DEFU,Rank::major,true},
    {0x80F44E07U,Rank::miniboss,true},
    {0x80F44E0AU,Rank::minor,false},
    {0x80F44E0CU,Rank::major,true},
    {0x80F44E12U,Rank::major,true},
    {0x80F44E18U,Rank::major,false},
    {0x80F44E1DU,Rank::major,false},
    {0x80F44E20U,Rank::miniboss,true},
    {0x80F44E23U,Rank::miniboss,true},
    {0x80F44E28U,Rank::minor,false},
    {0x80F44E2DU,Rank::minor,false},
    {0x80F44E2FU,Rank::major,true},
    {0x80F44E37U,Rank::major,false},
    {0x80F44E3AU,Rank::miniboss,true},
    {0x80F44E3DU,Rank::minor,false},
    {0x80F44E3FU,Rank::major,true},
    {0x80F44E48U,Rank::major,false},
    {0x80F44E4BU,Rank::miniboss,true},
    {0x80F44E50U,Rank::major,false},
    {0x80F44E53U,Rank::minor,false},
    {0x80F44E55U,Rank::major,true},
    {0x80F44E5AU,Rank::major,false},
    {0x80F44E5EU,Rank::miniboss,true},
    {0x80F44E62U,Rank::minor,false},
    {0x80F44E64U,Rank::major,true},
    {0x80F44E6AU,Rank::major,true},
    {0x80F44E6EU,Rank::major,false},
    {0x80F44E71U,Rank::miniboss,true},
    {0x80F44E74U,Rank::miniboss,true},
    {0x80F44E77U,Rank::minor,false},
    {0x80F44E79U,Rank::major,true},
    {0x80F44E80U,Rank::major,false},
    {0x80F44E83U,Rank::miniboss,true},
    {0x80F44E85U,Rank::minor,true},
    {0x80F44E8DU,Rank::miniboss,true},
    {0x80F44E90U,Rank::miniboss,true},
    {0x80F44FA6U,Rank::minor,false},
    {0x80F44FAAU,Rank::miniboss,true},
    {0x80F45004U,Rank::minor,false},
    {0x80F45009U,Rank::minor,false},
    {0x80F4500DU,Rank::minor,false},
    {0x80F45014U,Rank::major,false},
    {0x80F4501AU,Rank::minor,false},
    {0x80F45024U,Rank::minor,false},
    {0x80F45033U,Rank::miniboss,true},
    {0x80F4503BU,Rank::minor,false},
    {0x80F4504FU,Rank::minor,false},
    {0x80F45053U,Rank::minor,false},
    {0x80F45056U,Rank::minor,false},
    {0x80F45059U,Rank::minor,false},
    {0x80F4505BU,Rank::miniboss,true},
    {0x80F4507CU,Rank::minor,false},
    {0x80F45080U,Rank::minor,false},
    {0x80F45083U,Rank::minor,false},
}};

static_assert(kGeneratedPalettes.size()==19);
static_assert(kRankedPrefabs.size()==163);

constexpr std::size_t countRank(Rank rank) noexcept {
    std::size_t count{};
    for(const auto& prefab:kRankedPrefabs) if(prefab.rank==rank) ++count;
    return count;
}
static_assert(countRank(Rank::ambiguous)==2);
static_assert(kRankedPrefabs.size()-countRank(Rank::ambiguous)==161);
// Completion group 574459CE is represented only as named elite presence; no
// daemon or score-weight meaning is assigned by this immutable metadata.

} // namespace dawn::server::runtime::activity::haunted_forest::mode
