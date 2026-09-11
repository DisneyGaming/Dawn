#include "ui_logo_component.h"
#include <imgui.h>
#include "../../textures/ui_texture_slots.h"

namespace sunrise::core::ui::components::logo {
bool draw(float extent) noexcept {
    const auto texture = textures::get(textures::Slot::logoSheet);
    if (texture == ImTextureID_Invalid) { return false; }
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddImage(texture, origin, {origin.x + extent, origin.y + extent},
        {0, 0}, {1, 1}, ImGui::GetColorU32(ImVec4{1, 1, 1, 1}));
    ImGui::Dummy({extent, extent});
    return true;
}
} // namespace sunrise::core::ui::components::logo
