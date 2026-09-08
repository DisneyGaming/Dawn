#pragma once
#include "catalog.h"
#include "../coo/objective_service.h"
#include "../coo/lifecycle_service.h"
namespace sunrise::state::activity::deadly_trial {
struct PikeMount {
    coo::Generation owner{};
    std::uint32_t player{UINT32_MAX},vehicle{UINT32_MAX},seat{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && player!=UINT32_MAX && vehicle!=UINT32_MAX && player!=vehicle && seat!=UINT32_MAX; }
};
struct SceneReceipt {
    std::uint64_t run{};std::uint32_t generation{},group{},sensor{},selector{};
    bool valid() const noexcept { return run && generation && group!=UINT32_MAX && sensor!=UINT32_MAX && selector!=UINT32_MAX; }
    bool operator==(const SceneReceipt&) const noexcept=default;
};
struct InteractionBinding {
    coo::Generation owner{};std::uint64_t source{};
    std::uint32_t entity{UINT32_MAX},serial{},controller{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source && entity!=UINT32_MAX && controller!=UINT32_MAX; }
    bool operator==(const InteractionBinding&) const noexcept=default;
};
struct Frame {
    bool enabled{},checked{},finished{},populationFault{},barrierOpen{},reviveEnabled{},interacted{},sceneBound{},sceneStarted{},sceneComplete{};
    std::uint8_t section{},pikes{};std::uint32_t spawnGeneration{},sceneGeneration{},cohorts{},objective{},revision{};
    std::array<std::uint32_t,11> generations{};std::uint8_t activeRow{coo::kNoDialogue};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
};
struct Request { coo::Generation owner{};std::uint32_t sceneGeneration{};bool enabled{};InteractionBinding interaction{};bool preparing{}; };
}
