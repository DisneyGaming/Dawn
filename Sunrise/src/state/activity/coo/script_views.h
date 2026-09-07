#pragma once
#include "mission_runtime.h"
#include "dialogue_service.h"
#include "presentation_cues.h"
#include "presentation_services.h"
#include "objective_service.h"

namespace sunrise::state::activity::coo::script {
struct CommandBinding final { std::string_view id, capability; std::uint8_t step{}, command{}; };
struct GraphView final {
    std::string_view id, domain;
    Definition definition{};
    std::span<const CommandBinding> commands;
    [[nodiscard]] const CommandBinding* command(std::string_view name) const noexcept {
        for(const auto& item:commands) { if(item.id==name) { return &item; } }return nullptr;
    }
};
struct RoleView final { std::string_view id; const GraphView* graph{}; };
struct CueSet final { std::string_view id; std::span<const PresentationCue> cues; };
struct ActionSet final { std::string_view id; std::span<const PresentationAction> actions; };
struct PresentationTable final { std::string_view id; PresentationBindings bindings{}; };
struct Views final {
    bool valid{};
    std::string_view missionId, profileId;
    std::span<const GraphView> graphs;
    std::span<const RoleView> roles;
    MissionDefinition mission{};
    DialogueDefinition dialogue{};
    std::span<const CueSet> cueSets;
    std::span<const ActionSet> actionSets;
    std::span<const PresentationTable> tables;
    std::span<const ObjectiveMarker> markers{};
    [[nodiscard]] const GraphView* graph(std::string_view id) const noexcept {
        for(const auto& item:graphs) { if(item.id==id) { return &item; } }return nullptr;
    }
    [[nodiscard]] const GraphView* role(std::string_view id) const noexcept {
        for(const auto& item:roles) { if(item.id==id) { return item.graph; } }return nullptr;
    }
    [[nodiscard]] std::span<const PresentationCue> cues(std::string_view id) const noexcept {
        for(const auto& item:cueSets) { if(item.id==id) { return item.cues; } }return {};
    }
    [[nodiscard]] std::span<const PresentationAction> actions(std::string_view id) const noexcept {
        for(const auto& item:actionSets) { if(item.id==id) { return item.actions; } }return {};
    }
    [[nodiscard]] const PresentationBindings* table(std::string_view id) const noexcept {
        for(const auto& item:tables) { if(item.id==id) { return &item.bindings; } }return nullptr;
    }
};
} // namespace sunrise::state::activity::coo::script
