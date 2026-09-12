#include "mission_launch_panel.h"
#include "mission_launch_cards.h"
#include "../../activity/mission_launch.h"
#include "../../activity/mission_launch_options.h"
#include "../../activity/campaign_openings.h"
#include <array>
#include <cstdio>
#include <limits>

namespace sunrise::client::ui::mission_launch {
namespace {
namespace launch = client::activity::mission_launch;
namespace openings = launch::openings;
unsigned g_campaign{1};
bool g_resetScroll{}, g_scenariosReady{}, g_spawnsReady{};
std::array<bool, openings::kMissions.size()> g_ready{};
std::size_t g_layoutRevision{(std::numeric_limits<std::size_t>::max)()}, g_catalogRevision{};
launch::ManualScratch g_validation{};

void refresh() noexcept {
    const auto rows = state::build_data::activities::entries();
    const auto revision = state::build_data::scenario_layout_count();
    const bool scenariosReady = state::build_data::scenario_layouts_ready();
    const bool spawnsReady = state::build_data::spawn_sets_ready();
    if (revision == g_layoutRevision && rows.size() == g_catalogRevision
        && scenariosReady == g_scenariosReady && spawnsReady == g_spawnsReady) { return; }
    g_layoutRevision = revision; g_catalogRevision = rows.size();
    g_scenariosReady = scenariosReady; g_spawnsReady = spawnsReady;
    for (std::size_t i = 0; i < openings::kMissions.size(); ++i) {
        const auto route = openings::resolve(i, rows);
        state::build_data::scenarios::Definition donor{};
        g_ready[i] = route.valid() && scenariosReady && (!route.destination.hasSpawnSetHash || spawnsReady)
            && state::build_data::find_scenario_layout(rows[route.transport].name(), donor)
            && launch::validate_manual(route.destination, g_validation) == launch::ManualError::none;
    }
}

void campaign_tabs() noexcept {
    constexpr std::array<const char*, 2> names{"Red War", "Curse of Osiris"};
    const float scale = card_scale();
    const float width = (std::max)(1.0F, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5F);
    for (unsigned i = 0; i < names.size(); ++i) {
        if (i != 0) { ImGui::SameLine(); }
        const bool selected = g_campaign == i;
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(selected ? ImGuiCol_Header : ImGuiCol_FrameBg));
        if (ImGui::Button(names[i], {width, 38.0F * scale})) {
            g_campaign = i; g_resetScroll = true;
        }
        ImGui::PopStyleColor();
        if (selected) {
            const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled({a.x, b.y - 3.0F * scale}, b,
                ImGui::GetColorU32(ImGuiCol_CheckMark));
        }
    }
}

bool is_current_mission(std::size_t index, const launch::Snapshot& status) noexcept {
    return status.inMission && status.current_name() == launch::destination_name(openings::kMissions[index].destination);
}
const char* mission_action(std::size_t index, const launch::Snapshot& status) noexcept {
    if (is_current_mission(index, status)) { return "IN MISSION"; }
    if (status.busy && status.opening
        && launch::destination_name(status.destination) == launch::destination_name(openings::kMissions[index].destination)) {
        return status.status == launch::Status::preparing ? "PREPARING" : "LAUNCHING";
    }
    return g_ready[index] ? "LAUNCH >" : "UNAVAILABLE";
}
const char* current_title(const launch::Snapshot& status) noexcept {
    for (const auto& mission : openings::kMissions) {
        if (status.current_name() == launch::destination_name(mission.destination)) { return mission.title; }
    }
    const auto rows = state::build_data::activities::entries();
    if (status.currentIndex >= 0 && static_cast<std::size_t>(status.currentIndex) < rows.size()
        && rows[status.currentIndex].name() == status.current_name()) {
        const auto& text = state::build_data::activities::presentation(static_cast<std::uint16_t>(status.currentIndex));
        if (text.title[0]) { return text.title.data(); }
    }
    return status.current_name() == "mission_reunion" ? "Chosen" : "Current activity";
}

bool mission_row(std::size_t index, unsigned ordinal, const launch::Snapshot& status) noexcept {
    const auto& mission = openings::kMissions[index];
    const float scale = card_scale();
    const float width = (std::max)(1.0F, ImGui::GetContentRegionAvail().x);
    const float height = 54.0F * scale;
    const bool ready = g_ready[index];
    ImGui::PushID(static_cast<int>(index));
    const bool active = is_current_mission(index, status);
    ImGui::BeginDisabled(!ready || status.busy || status.inMission);
    const bool clicked = ImGui::InvisibleButton("##launch_opening", {width, height}, ImGuiButtonFlags_EnableNav);
    ImGui::EndDisabled();
    const bool hovered = ready && !status.busy && !status.inMission
        && (ImGui::IsItemHovered() || ImGui::IsItemFocused());
    const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(a, b, ImGui::GetColorU32(active ? ImGuiCol_Header
        : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 3.0F * scale);
    if (hovered || active) { draw->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_CheckMark), 3.0F * scale); }
    std::array<char, 8> number{};
    (void)std::snprintf(number.data(), number.size(), "%02u", ordinal);
    const float inset = width >= 300.0F * scale ? 56.0F * scale : 14.0F * scale;
    if (inset > 20.0F * scale) {
        draw->AddText({a.x + 16.0F * scale, a.y + 19.0F * scale}, ImGui::GetColorU32(ImGuiCol_TextDisabled), number.data());
    }
    const char* action = mission_action(index, status);
    const float actionWidth = ImGui::CalcTextSize(action).x;
    const float textRight = (std::max)(a.x + inset + 1.0F, b.x - actionWidth - 32.0F * scale);
    draw->PushClipRect({a.x + inset, a.y}, {textRight, b.y}, true);
    draw->AddText({a.x + inset, a.y + 9.0F * scale}, ImGui::GetColorU32(ImGuiCol_Text), mission.title);
    draw->AddText({a.x + inset, a.y + 30.0F * scale}, ImGui::GetColorU32(ImGuiCol_TextDisabled), mission.location);
    draw->PopClipRect();
    draw->AddText({b.x - actionWidth - 16.0F * scale, a.y + 19.0F * scale},
        ImGui::GetColorU32(active || (ready && !status.inMission) ? ImGuiCol_Text : ImGuiCol_TextDisabled), action);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s%s", mission.description, status.inMission ? "\nReturn to orbit to launch a mission."
            : ready ? "" : "\nThis opening is not available in the installed content.");
    }
    ImGui::PopID();
    return clicked;
}
} // namespace

void draw() noexcept {
    refresh();
    const float scale = card_scale();
    campaign_tabs();
    ImGui::Dummy({0, 10.0F * scale});
    const bool wide = ImGui::GetContentRegionAvail().x >= 740.0F * scale;
    const float remaining = ImGui::GetContentRegionAvail().y;
    const float footer = ImGui::GetTextLineHeightWithSpacing() * 2.0F + 14.0F * scale;
    const bool table = wide && ImGui::BeginTable("##campaign_body", 2, ImGuiTableFlags_SizingStretchProp);
    if (table) {
        ImGui::TableSetupColumn("##campaign_identity", ImGuiTableColumnFlags_WidthFixed, 245.0F * scale);
        ImGui::TableSetupColumn("##mission_list", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
    }
    const auto icon = g_campaign == 0 ? state::build_data::activities::Icon::redWar
                                     : state::build_data::activities::Icon::osiris;
    const float extent = (table ? 84.0F : 42.0F) * scale;
    (void)draw_icon(icon, ImGui::GetCursorScreenPos(), extent);
    ImGui::Dummy({extent, extent});
    if (!table) { ImGui::SameLine(); }
    ImGui::BeginGroup();
    ImGui::TextDisabled(g_campaign == 0 ? "CAMPAIGN 01" : "CAMPAIGN 02");
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * (table ? 1.9F : 1.5F));
    ImGui::TextWrapped("%s", g_campaign == 0 ? "The Red War" : "Curse of Osiris");
    ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", g_campaign == 0 ? "Take back the Light. Revisit the fall of the Last City."
                                            : "Find Osiris. Step into the Infinite Forest.");
    ImGui::Spacing();
    if (table) {
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextWrapped("Select a mission to begin at its opening.");
        ImGui::TextDisabled("Launch from orbit.");
        ImGui::TableNextColumn();
    }
    const auto count = static_cast<unsigned>(std::count_if(openings::kMissions.begin(), openings::kMissions.end(),
        [](const auto& mission) { return mission.campaign == g_campaign; }));
    ImGui::TextDisabled("%u MISSION OPENING%s", count, count == 1 ? "" : "S");
    ImGui::Spacing();
    const float height = (std::max)(60.0F * scale,
        (table ? remaining - ImGui::GetTextLineHeightWithSpacing() : ImGui::GetContentRegionAvail().y) - footer);
    if (ImGui::BeginChild("##campaign_missions", {0, height}, ImGuiChildFlags_None)) {
        if (g_resetScroll) { ImGui::SetScrollY(0); g_resetScroll = false; }
        const auto status = launch::snapshot();
        unsigned ordinal{};
        for (std::size_t i = 0; i < openings::kMissions.size(); ++i) {
            if (openings::kMissions[i].campaign != g_campaign) { continue; }
            if (mission_row(i, ++ordinal, status)) { (void)launch::request_opening(i); }
        }
    }
    ImGui::EndChild();
    if (table) { ImGui::EndTable(); }
    ImGui::Spacing(); ImGui::Separator();
    const auto latest = launch::snapshot();
    if (latest.inMission) {
        ImGui::TextWrapped("In mission: %s", current_title(latest));
        if (latest.status == launch::Status::unexpectedDestination) {
            ImGui::TextWrapped("%s", launch::description(latest.status));
        } else {
            ImGui::TextDisabled("Return to orbit to launch another mission.");
        }
    } else if (latest.opening && latest.status != launch::Status::idle && latest.status != launch::Status::arrived) {
        ImGui::TextWrapped("%s", launch::description(latest.status));
    } else if (state::build_data::activities::entries().empty() || !g_scenariosReady || !g_spawnsReady) {
        ImGui::TextWrapped("%s", state::build_data::activities::extraction_failed()
            ? "Campaign content could not be read." : "Reading campaign content...");
    } else {
        ImGui::TextDisabled("Select a mission to launch its opening. Start from orbit.");
    }
}
} // namespace sunrise::client::ui::mission_launch
