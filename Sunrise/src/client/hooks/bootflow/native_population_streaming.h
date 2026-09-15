#pragma once
#include <cstdint>

namespace sunrise::client::hooks::bootflow::native_population_streaming {
// Only server-qualified single-category free-roam sources opt in. The requested
// population and native AI mode are never changed. Preserve the consumption
// observed before native deactivation fills the remaining budget.
struct Counters final {
    std::int32_t categories{},requested{},consumed{},pending{};
};
constexpr bool checkpoint(Counters value) noexcept {
    return value.categories==1 && value.requested>=0 && value.consumed>=0
        && value.consumed<=value.requested && value.pending==0;
}
constexpr bool restore(Counters saved,Counters current,bool destroyed,bool allocationReleased) noexcept {
    return destroyed && allocationReleased && checkpoint(saved) && checkpoint(current)
        && saved.requested==current.requested
        && (current.consumed==0 || current.consumed==saved.consumed);
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
