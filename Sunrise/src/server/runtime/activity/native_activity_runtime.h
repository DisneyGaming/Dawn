#pragma once
#include "persistent_activity.h"
#include "lost_sector_runtime.h"
#include "moon_lost_sector_types.h"
namespace sunrise::server::runtime::activity::native_activity {
using Owner=population::Owner;

[[nodiscard]] lost_sector::RewardTicket lost_sector_reward(Owner,std::uint32_t,std::uint16_t,std::uint32_t) noexcept;
[[nodiscard]] lost_sector::RewardTicket lost_sector_reward_current(Owner,std::uint32_t,std::uint16_t,std::uint32_t) noexcept;
[[nodiscard]] moon_lost_sector::DestructibleRequest lost_sector_object_request(Owner,std::uint32_t) noexcept;
[[nodiscard]] bool lost_sector_object_observed(const moon_lost_sector::ObjectReceipt&,bool,bool replaced=false) noexcept;

namespace detail {
// Keep owner lookup independent from table position. Runtime entry tables are
// sparse and may retain unrelated activities ahead of the requested owner.
template<class Range,class OwnerOf>
[[nodiscard]] constexpr auto find_owned_entry(Range& entries,Owner owner,OwnerOf&& ownerOf) noexcept {
    using Pointer=decltype(&entries[0]);
    if(!owner)return Pointer{};
    for(auto& entry:entries)if(ownerOf(entry)==owner)return &entry;
    return Pointer{};
}
template<class Ledger>
[[nodiscard]] std::array<std::uint8_t,8> streamed_replacements(
    const population::Service& service,std::size_t source,const Ledger& ledger,
    const std::array<std::uint8_t,8>& streamedSurvivors,std::uint8_t categories,
    bool recurring=false) noexcept {
    std::array<std::uint8_t,8> result{};
    if(!categories || categories>result.size())return result;
    for(std::size_t category=0;category<categories;++category) {
        const auto desired=service.occupancy_target(source,category);
        const auto dead=ledger.counts(static_cast<std::uint8_t>(category)).dead;
        // One-shot cohorts never replace a confirmed death. Recurring patrol
        // sources may already contain casualty replacements, so their live
        // streamed actors are capped by intended occupancy without subtracting
        // generation-lifetime deaths a second time.
        const auto retiredCredit=recurring?desired:
            desired-(std::min)(desired,static_cast<std::uint32_t>(dead));
        result[category]=static_cast<std::uint8_t>((std::min)(
            static_cast<std::uint32_t>(streamedSurvivors[category]),retiredCredit));
    }
    return result;
}
}
// Uses the same retained document as update, before the roster is published.
[[nodiscard]] bool optional_registries(const NativeActivityDefinition& definition,
    ambient_population::RegistryBatch& output) noexcept;
// Caller has already validated creator-root ownership and admitted the profile's
// exact package registries. A sent frame is not a native readiness receipt.
[[nodiscard]] NativeActivityFrame update(Owner owner,std::uint32_t bubble,bool arrived,
    const NativeActivityDefinition& definition,const adventure_start::wire::Request& selected={},
    bool openingAdmissionReady=true,std::uint32_t populationPrefetchBubble=UINT32_MAX) noexcept;
void observe(Owner owner,std::uint32_t bubble,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept;
/** Read-only snapshot; never starts a script or executes an update tick. */
[[nodiscard]] bool snapshot_placements(Owner owner,const NativeActivityDefinition*& definition,
    placement::wire::Batch& output) noexcept;
// Project an already admitted creator clock for another qualified native
// activity recipient. Never initializes an Entry, script or clock domain.
[[nodiscard]] bool snapshot_clock(Owner owner,activity_clock::Publication& output) noexcept;
}
