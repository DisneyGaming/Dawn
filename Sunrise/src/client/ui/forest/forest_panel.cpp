/**
 * The Infinite Forest generator dial. Every control writes the shared tuner state; the
 * generator tick hook copies it into the sensor's authority record each cycle, and the
 * worker's change-detect rebuilds the layout in-place (~1 s). Nothing here persists —
 * the dial resets to defaults each launch while the selection semantics are being mapped.
 */

#include "forest_panel.h"

#include <array>
#include <cstdio>
#include <imgui.h>

#include "../../hooks/bootflow/forest_tuner_state.h"

namespace sunrise::client::ui::forest {
namespace {

namespace tuner = client::hooks::bootflow::forest_tuner;

/** Draws one selection group row: write toggle, two bytes, weight, active. */
void group_row(int index, tuner::Group& group) noexcept {
    ImGui::PushID(index);
    bool write = group.write.load(std::memory_order_relaxed);
    std::array<char, 16> label{};
    (void)std::snprintf(label.data(), label.size(), "g%d", index);
    if (ImGui::Checkbox(label.data(), &write)) {
        group.write.store(write, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!write);
    int a = group.a.load(std::memory_order_relaxed);
    int b = group.b.load(std::memory_order_relaxed);
    float weight = group.weight.load(std::memory_order_relaxed);
    bool active = group.active.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(90.0F);
    if (ImGui::InputInt("a", &a)) {
        group.a.store(a & 0xFF, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0F);
    if (ImGui::InputInt("b", &b)) {
        group.b.store(b & 0xFF, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0F);
    if (ImGui::InputFloat("w", &weight, 0.0F, 0.0F, "%.2f")) {
        group.weight.store(weight, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("on", &active)) {
        group.active.store(active, std::memory_order_relaxed);
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}

} // namespace

void draw() noexcept {
    tuner::State& dial = tuner::state();

    ImGui::TextUnformatted("Infinite Forest generator");
    ImGui::TextWrapped("Omega uses a random seed per mission run and one route between the fixed stairs. "
                       "The controls below are temporary diagnostic overrides.");
    ImGui::Separator();
    if (tuner::sensor_present()) {
        ImGui::Text("Sensor: LIVE   applies: %u",
                    dial.applies.load(std::memory_order_relaxed));
    } else {
        ImGui::TextUnformatted("Sensor: not constructed (enter the forest)");
    }
    ImGui::Spacing();

    bool enable = dial.enable.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Generator enabled (ignition)", &enable)) {
        dial.enable.store(enable, std::memory_order_relaxed);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Force entry (toggle by index, one row per ticking worker)");
    ImGui::Separator();
    static std::array<int, tuner::State::kWorkerSlots> s_forceEntries{};
    for (int slot = 0; slot < static_cast<int>(tuner::State::kWorkerSlots); ++slot) {
        const int entries =
            dial.entryCountPer[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed);
        if (entries <= 0) {
            continue;
        }
        ImGui::PushID(slot + 100);
        int& index = s_forceEntries[static_cast<std::size_t>(slot)];
        ImGui::Text("Worker %d (%d entries)", slot, entries);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0F);
        ImGui::InputInt("##force_entry", &index);
        ImGui::SameLine();
        if (ImGui::Button("Activate") && index >= 0) {
            dial.forceEntryPer[static_cast<std::size_t>(slot)].store(
                index, std::memory_order_relaxed);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next")) {
            dial.forceEntryPer[static_cast<std::size_t>(slot)].store(
                index, std::memory_order_relaxed);
            ++index;
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Seed");
    ImGui::Separator();
    bool writeSeed = dial.writeSeed.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Write seed", &writeSeed)) {
        dial.writeSeed.store(writeSeed, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!writeSeed);
    int seed = dial.seed.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(140.0F);
    if (ImGui::InputInt("##seed", &seed)) {
        dial.seed.store(seed, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    if (ImGui::Button("+1")) {
        dial.seed.store(seed + 1, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    if (ImGui::Button("+100")) {
        dial.seed.store(seed + 100, std::memory_order_relaxed);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextUnformatted("Mode");
    ImGui::Separator();
    bool writeMode = dial.writeMode.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Write mode", &writeMode)) {
        dial.writeMode.store(writeMode, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!writeMode);
    int mode = dial.mode.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(140.0F);
    if (ImGui::SliderInt("##mode", &mode, 0, 7)) {
        dial.mode.store(mode, std::memory_order_relaxed);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextUnformatted("Selection groups (a, b, weight, on)");
    ImGui::Separator();
    for (int index = 0; index < static_cast<int>(dial.groups.size()); ++index) {
        group_row(index, dial.groups[static_cast<std::size_t>(index)]);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Density floats");
    ImGui::Separator();
    bool writeFloats = dial.writeFloats.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Write f0/f1", &writeFloats)) {
        dial.writeFloats.store(writeFloats, std::memory_order_relaxed);
    }
    ImGui::BeginDisabled(!writeFloats);
    float f0 = dial.f0.load(std::memory_order_relaxed);
    float f1 = dial.f1.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(140.0F);
    if (ImGui::InputFloat("f0", &f0, 0.0F, 0.0F, "%.2f")) {
        dial.f0.store(f0, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0F);
    if (ImGui::InputFloat("f1", &f1, 0.0F, 0.0F, "%.2f")) {
        dial.f1.store(f1, std::memory_order_relaxed);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextUnformatted("Ints");
    ImGui::Separator();
    bool writeInts = dial.writeInts.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Write i0/i1", &writeInts)) {
        dial.writeInts.store(writeInts, std::memory_order_relaxed);
    }
    ImGui::BeginDisabled(!writeInts);
    int i0 = dial.i0.load(std::memory_order_relaxed);
    int i1 = dial.i1.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(140.0F);
    if (ImGui::InputInt("i0", &i0)) {
        dial.i0.store(i0, std::memory_order_relaxed);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0F);
    if (ImGui::InputInt("i1", &i1)) {
        dial.i1.store(i1, std::memory_order_relaxed);
    }
    ImGui::EndDisabled();
}

} // namespace sunrise::client::ui::forest
