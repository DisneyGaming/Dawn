#pragma once
#include "../../../server/runtime/activity/adventure_native_bridge.h"
namespace sunrise::client::hooks::bootflow::adventure_cue_observer {
namespace bridge=server::runtime::activity::adventure::native_bridge;
namespace feedback=bridge::feedback;
struct Context final {
    bridge::Binding binding{};
    feedback::Source source{};
    std::uintptr_t component{},packet{},packetBody{};
    std::uint32_t authority{};
    std::uint32_t componentLink{UINT32_MAX};
    std::array<std::byte,16> header{};
    std::array<std::byte,16> packetHeader{};
    std::array<std::byte,0x60> definition{};
    std::array<std::byte,0x70> authorityObject{};
    feedback::wire::Decoded incoming{},prior{};
    std::uint64_t managerCount{};bool managerReady{};unsigned matchingEntries{};
    std::uint64_t entryIndex{UINT64_MAX};std::array<std::byte,0x148> priorEntry{};
};
// Additive original-forwarding observer. Reads native data only; no creation,
// native state repair, eligibility changes or invocation of a launch routine.
[[nodiscard]] Context begin(void* component,const void* packet) noexcept;
[[nodiscard]] bool finish(void* component,const Context& before) noexcept;
}
