#pragma once
#include <cstdint>
namespace sunrise::client::hooks::bootflow::native_population_retirement {
// Native actor pool descriptor and per-slot generation, copied before/after
// A85540 -> A7E400 -> 34F790. This does not prove network-facet retirement.
struct Slot final {
    std::uintptr_t base{};
    std::uint32_t stride{},generationOffset{},mask{},generation{};
};
constexpr bool valid(const Slot& slot) noexcept {
    return slot.base>=0x10000 && slot.stride>=0x74 && slot.stride<=0x100000
        && slot.generationOffset<=slot.stride-4 && slot.generationOffset%4==0 && slot.mask!=0;
}
constexpr bool released(const Slot& before,const Slot& after) noexcept {
    return valid(before) && valid(after) && before.base==after.base && before.stride==after.stride
        && before.generationOffset==after.generationOffset && before.mask==after.mask
        && after.generation!=before.generation && after.generation==((before.generation+1U)&before.mask);
}
// Source backlinks may already have been cleared by the time native teardown
// reaches A85540. The complete birth identity was retained while they existed.
constexpr bool identity(std::uint32_t expectedActor,std::uint32_t expectedEntity,std::uint32_t expectedParent,
    std::uint32_t expectedSource,std::uint32_t actor,std::uint32_t entity,std::uint32_t parent,std::uint32_t source) noexcept {
    return expectedActor!=UINT32_MAX && expectedEntity!=UINT32_MAX && expectedParent!=UINT32_MAX
        && expectedSource!=UINT32_MAX && actor==expectedActor && entity==expectedEntity && parent==expectedParent
        && (source==expectedSource || source==UINT32_MAX);
}
template<class Original,class Complete>
void forward(Original original,Complete complete,std::uint32_t actor,std::uint8_t mode) noexcept {
    original(actor,mode);
    complete();
}
}
