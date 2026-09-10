#pragma once
#include <cmath>
#include <imgui.h>

namespace sunrise::client::ui::mission_launch {
/** The Selectable owns the complete item; the clipper includes the style's trailing spacing. */
[[nodiscard]] inline float row_stride() noexcept {
    // ItemSize truncates the next cursor position to a pixel. Make the complete advance an
    // integer on both sides of the viewport origin, including fractional font/style scales.
    return std::ceil(ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeight()
        + ImGui::GetStyle().FramePadding.y * 2.0F + ImGui::GetStyle().ItemSpacing.y);
}
[[nodiscard]] inline float row_height() noexcept {
    return row_stride() - ImGui::GetStyle().ItemSpacing.y;
}
[[nodiscard]] inline bool result_row(const char* heading, const char* detail, bool selected) noexcept {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::Selectable("##activity", selected, 0, ImVec2(0, row_height()));
    if (ImGui::IsItemVisible()) {
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const ImVec2 position{origin.x + padding.x, origin.y + padding.y};
        ImDrawList* const draw = ImGui::GetWindowDrawList();
        // Clip long names to the row rather than growing the child or adding horizontal scroll.
        draw->PushClipRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), true);
        draw->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), heading);
        draw->AddText({position.x, position.y + ImGui::GetTextLineHeightWithSpacing()},
            ImGui::GetColorU32(ImGuiCol_TextDisabled), detail);
        draw->PopClipRect();
    }
    return pressed;
}
} // namespace sunrise::client::ui::mission_launch
