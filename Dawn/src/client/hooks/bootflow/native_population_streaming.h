#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::client::hooks::bootflow::native_population_streaming {
inline constexpr std::size_t kSourceCapacity=512;
inline constexpr std::size_t kMaximumCategories=8;
// Only server-qualified free-roam sources opt in. The
// requested population and native AI mode are never changed. Preserve each
// category's consumption observed before native deactivation fills its budget.
struct Counters final {
    std::int32_t categories{},requested{},consumed{},pending{};
    std::int32_t secondRequested{},secondConsumed{},secondPending{};
    std::array<std::int32_t,kMaximumCategories-2> additionalRequested{};
    std::array<std::int32_t,kMaximumCategories-2> additionalConsumed{};
    std::array<std::int32_t,kMaximumCategories-2> additionalPending{};
};
constexpr std::int32_t requested(const Counters& value,std::size_t category) noexcept {
    return category==0?value.requested:category==1?value.secondRequested:
        category<kMaximumCategories?value.additionalRequested[category-2]:-1;
}
constexpr std::int32_t consumed(const Counters& value,std::size_t category) noexcept {
    return category==0?value.consumed:category==1?value.secondConsumed:
        category<kMaximumCategories?value.additionalConsumed[category-2]:-1;
}
constexpr std::int32_t pending(const Counters& value,std::size_t category) noexcept {
    return category==0?value.pending:category==1?value.secondPending:
        category<kMaximumCategories?value.additionalPending[category-2]:-1;
}
constexpr void set_consumed(Counters& value,std::size_t category,std::int32_t count) noexcept {
    if(category==0)value.consumed=count;
    else if(category==1)value.secondConsumed=count;
    else if(category<kMaximumCategories)value.additionalConsumed[category-2]=count;
}
constexpr bool checkpoint(Counters value) noexcept {
    if(value.categories<1 || value.categories>static_cast<std::int32_t>(kMaximumCategories))return false;
    for(std::size_t category=0;category<kMaximumCategories;++category) {
        const auto target=requested(value,category),used=consumed(value,category),queued=pending(value,category);
        if(category<static_cast<std::size_t>(value.categories)) {
            if(target<0 || used<0 || used>target || queued!=0)return false;
        } else if(target || used || queued)return false;
    }
    return true;
}
constexpr bool restore(Counters saved,Counters current,bool destroyed,bool allocationReleased) noexcept {
    if(!destroyed || !allocationReleased || !checkpoint(saved) || !checkpoint(current)
        || saved.categories!=current.categories)return false;
    for(std::size_t category=0;category<static_cast<std::size_t>(saved.categories);++category)
        if(requested(saved,category)!=requested(current,category)
            || (consumed(current,category)!=0 && consumed(current,category)!=consumed(saved,category)))return false;
    return true;
}
constexpr bool local_facet(std::int8_t kind,std::int8_t owner,std::uint16_t flags,std::uint32_t peers) noexcept {
    return kind==0 && owner==-1 && (flags&4U)==0 && (peers&0x7FFFFFFFU)==0;
}
// Teardown relinquishes local ownership before detaching the object. Only a
// previously captured local registration may use this broader cleanup check.
constexpr bool retained_facet(std::int8_t kind,std::int8_t owner,std::uint16_t flags,std::uint32_t peers) noexcept {
    return kind==0 && (owner==-1 || owner==-2) && (flags&4U)==0 && (peers&0x7FFFFFFFU)==0;
}
}
