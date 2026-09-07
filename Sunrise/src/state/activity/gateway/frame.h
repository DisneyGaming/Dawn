#pragma once
#include "../coo/dialogue_service.h"
#include "../coo/object_service.h"
#include "../coo/objective_service.h"
namespace sunrise::state::activity::gateway {
struct Frame final {
    bool enabled{}, checked{},openingChecked{};
    bool moduleVulnerable{},moduleDestroyed{},lighthouseOpen{},sceneStarted{},sceneComplete{},finished{};
    std::uint8_t section{},preparedMask{};
    std::uint32_t sceneGeneration{};
    bool vanceEntered{},vanceConversation{},vanceTurned{},conversationStarted{};
    std::uint8_t lighthouseChannels{};
    std::uint32_t objective{},revision{};
    std::uint32_t spawnGeneration{},cohorts{};
    bool marchers{},cannons{},finalCannon{},populationFault{};
    std::array<std::uint32_t,16> generations{};
    std::uint8_t activeRow{coo::kNoDialogue};
    std::array<coo::ObjectState,3> objects{};
    coo::ObjectiveState presentation{};
    bool services{},pendingServices{},returnCuePending{};
    coo::CompletionPublication completion{};
};
}
