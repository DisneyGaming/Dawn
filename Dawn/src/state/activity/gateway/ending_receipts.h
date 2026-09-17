#pragma once
#include <cstdint>
namespace dawn::state::activity::gateway {
struct ModuleReceipt final {
    std::uint64_t run{};
    std::uint32_t generation{};
    std::uintptr_t source{}; // Exact observed component address; +0x24 is not a source handle.
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},health{UINT32_MAX};
    bool valid() const noexcept { return run && generation && source>=0x10000 && source!=UINTPTR_MAX && entity!=UINT32_MAX && health!=UINT32_MAX && serial!=UINT32_MAX; }
    friend bool operator==(const ModuleReceipt&,const ModuleReceipt&)=default;
};
struct SceneReceipt final {
    std::uint64_t run{};
    std::uint32_t generation{},group{UINT32_MAX},sensor{UINT32_MAX},selector{UINT32_MAX};
    bool valid() const noexcept { return run && generation && group!=UINT32_MAX && sensor!=UINT32_MAX && selector!=UINT32_MAX; }
    friend bool operator==(const SceneReceipt&,const SceneReceipt&)=default;
};
enum class VanceMilestone : std::uint8_t { turned, conversationStarted };
struct EndingRequest final { bool enabled{},moduleVulnerable{}; std::uint64_t run{}; std::uint32_t generation{},sceneGeneration{}; ModuleReceipt owner{}; bool moduleDestroyed{}; };
}
