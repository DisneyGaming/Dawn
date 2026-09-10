#pragma once
#include "../../../server/runtime/activity/adventure_dialogue_bridge.h"
namespace sunrise::client::hooks::bootflow::adventure_dialogue_observer {
namespace bridge=server::runtime::activity::adventure::dialogue_bridge;
namespace feedback=bridge::feedback;
struct Context final {
    bridge::Binding binding{};std::uintptr_t component{};feedback::Source source{};
    std::uint32_t componentLink{},authority{},processed{};std::uint64_t sequence{};
    std::array<std::byte,16> header{};std::array<std::byte,0x60> definition{};
    std::array<std::byte,0x70> authorityObject{};feedback::wire::Decoded applied{};
};
[[nodiscard]] Context begin(void* component) noexcept;
[[nodiscard]] bool finish(void* component,const Context&,bool originalForwarded) noexcept;
}
