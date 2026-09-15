#pragma once
#include <cstddef>
#include <cstdint>

namespace sunrise::client::hooks::bootflow::native_population_streaming {
inline constexpr std::size_t kSourceCapacity=384;
// Only server-qualified one- or two-category free-roam sources opt in. The
// requested population and native AI mode are never changed. Preserve each
// category's consumption observed before native deactivation fills its budget.
struct Counters final {
    std::int32_t categories{},requested{},consumed{},pending{};
    std::int32_t secondRequested{},secondConsumed{},secondPending{};
};
constexpr bool checkpoint(Counters value) noexcept {
    if((value.categories!=1 && value.categories!=2) || value.requested<0 || value.consumed<0
        || value.consumed>value.requested || value.pending!=0)return false;
    if(value.categories==1)return value.secondRequested==0 && value.secondConsumed==0 && value.secondPending==0;
    return value.secondRequested>=0 && value.secondConsumed>=0
        && value.secondConsumed<=value.secondRequested && value.secondPending==0;
}
constexpr bool restore(Counters saved,Counters current,bool destroyed,bool allocationReleased) noexcept {
    return destroyed && allocationReleased && checkpoint(saved) && checkpoint(current)
        && saved.categories==current.categories && saved.requested==current.requested
        && saved.secondRequested==current.secondRequested
        && (current.consumed==0 || current.consumed==saved.consumed)
        && (current.secondConsumed==0 || current.secondConsumed==saved.secondConsumed);
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
