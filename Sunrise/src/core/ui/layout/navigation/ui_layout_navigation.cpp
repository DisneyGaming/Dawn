#include "ui_layout_navigation.h"
#include <algorithm>
#include <array>
#include <imgui.h>
#include "../../modules/registry/ui_module_registry.h"
#include "../../scaling/dpi/ui_dpi_scaling.h"
#include "../credits/sunrise_credits_badge.h"
#include "../ui_layout_lifecycle.h"

namespace sunrise::core::ui::layout::navigation {
Selection draw(const StateSnapshot& state) noexcept {
    const auto registry = modules::registry::snapshot();
    const auto entries = registry.entries();
    if (entries.empty()) { internal::select_module({}); return {}; }
    Selection selected{entries.front(), true};
    const std::string_view id(state.selectedStableId.data(), state.selectedStableIdLength);
    for (const auto& descriptor : entries) {
        if (descriptor.stable_id() == id) { selected.descriptor = descriptor; break; }
    }
    internal::select_module(selected.descriptor.stable_id());
    const float scale = scaling::dpi::current();
    const float end = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    bool first = true;
    for (const auto& descriptor : entries) {
        std::array<char, modules::kDisplayNameCapacity + 1> label{};
        const auto name = descriptor.display_name();
        std::copy(name.begin(), name.end(), label.begin());
        const float width = ImGui::CalcTextSize(label.data()).x + 28.0F * scale;
        if (!first && ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x
            + ImGui::GetStyle().ItemSpacing.x + width <= end) { ImGui::SameLine(); }
        first = false;
        const bool active = descriptor.stable_id() == selected.descriptor.stable_id();
        ImGui::PushID(descriptor.stable_id().data(), descriptor.stable_id().data() + descriptor.stable_id().size());
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4{});
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(active ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        if (ImGui::Button(label.data(), {width, 34.0F * scale})) {
            internal::select_module(descriptor.stable_id()); selected = {descriptor, true};
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
    return selected;
}
} // namespace sunrise::core::ui::layout::navigation
