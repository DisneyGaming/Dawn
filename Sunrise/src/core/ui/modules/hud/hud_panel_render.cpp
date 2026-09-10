/** The HUD page. One switch per overlay, in the order the overlays stack on screen. */

#include <cstddef>
#include <imgui.h>

#include "../../../../client/diagnostics/native_overlays.h"
#include "../../components/section/ui_section_component.h"
#include "../../components/toggle/ui_toggle_component.h"
#include "../../hud/overlay.h"
#include "internal.h"

namespace sunrise::core::ui::modules::hud::internal {

/** Draws the HUD page inside the active Core UI frame. */
void draw() noexcept {
    ImGui::TextWrapped("Overlays draw in the top-left corner while the game runs, with or "
                       "without this menu open.");
    ImGui::Spacing();

    for (std::size_t index = 0; index < static_cast<std::size_t>(ui::hud::Overlay::count);
         ++index) {
        const auto overlay = static_cast<ui::hud::Overlay>(index);
        bool on = ui::hud::enabled(overlay);
        // The label is unique per overlay, so it carries the control's identity on its own.
        if (components::toggle::control(ui::hud::display_name(overlay), on)) {
            ui::hud::set_enabled(overlay, on);
        }
    }

    ImGui::Spacing();
    components::section::header("Current status lines",
                                "Each line of the current status overlay, on its own.");
    ImGui::Spacing();
    for (std::size_t index = 0; index < static_cast<std::size_t>(ui::hud::StatusLine::count);
         ++index) {
        const auto line = static_cast<ui::hud::StatusLine>(index);
        bool on = ui::hud::enabled(line);
        if (components::toggle::control(ui::hud::display_name(line), on)) {
            ui::hud::set_enabled(line, on);
        }
    }

    ImGui::Spacing();
    components::section::header("Native overlays",
                                "Session-only switches for the game's own diagnostic displays. "
                                "Each switch shows its current native state.");
    ImGui::Spacing();
    namespace native = client::diagnostics::native_overlays;
    for (std::size_t index = 0; index < static_cast<std::size_t>(native::Overlay::count); ++index) {
        const auto overlay = static_cast<native::Overlay>(index);
        const auto state = native::snapshot(overlay);
        bool on = state.value != 0;
        ImGui::PushID(static_cast<int>(index));
        ImGui::BeginDisabled(!state.available);
        const bool changed = components::toggle::control(native::display_name(overlay), on);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", state.available ? native::description(overlay) : state.reason);
        }
        if (!state.available) {
            ImGui::TextDisabled("%s", state.reason);
        } else if (changed) {
            (void)native::set_enabled(overlay, state.value, on);
        } else if (state.lastError != nullptr) {
            ImGui::TextDisabled("%s", state.lastError);
        }
        ImGui::PopID();
    }
}

} // namespace sunrise::core::ui::modules::hud::internal
