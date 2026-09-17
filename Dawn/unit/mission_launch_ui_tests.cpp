#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>
#include "client/ui/mission_launch/mission_launch_row.h"
#include "core/ui/memory/allocator.h"

namespace {
unsigned g_errors{};
bool g_dynamicFonts{};
unsigned g_textureUpdates{};
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void error(ImGuiContext*, void*, const char* message) {
    ++g_errors;
    std::cerr << "ImGui: " << message << '\n';
}
void scenario(int count, float scroll, float scale, int frames = 3) {
    namespace rows = dawn::client::ui::mission_launch;
    ImGui::GetStyle() = ImGuiStyle{};
    ImGui::GetStyle().ScaleAllSizes(scale);
    // Deliberately differ from the old hardcoded 4px spacing at both DPI scales.
    ImGui::GetStyle().ItemSpacing.y = 7.0F * scale;
    if (g_dynamicFonts) {
        ImGui::GetStyle().FontSizeBase = 16.0F;
        ImGui::GetStyle().FontScaleMain = scale;
    }
    for (int frame = 0; frame < frames; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({480, 300});
        ImGui::Begin("Dawn", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginChild("mission_results", {340, 160}, ImGuiChildFlags_Borders);
        if (frame == 0) { ImGui::SetScrollY(scroll); }
        ImGuiListClipper clipper;
        clipper.Begin(count, rows::row_stride());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::PushID(i);
                const float before = ImGui::GetCursorPosY();
                (void)rows::result_row("A long installed activity title that must remain inside the row boundaries",
                    "Missions  |  #299  |  unavailable", i == 0);
                if (std::abs(ImGui::GetCursorPosY() - before - rows::row_stride()) >= 0.01F) {
                    std::cerr << "row=" << i << " frame=" << frame << " scale=" << scale
                        << " before=" << before << " after=" << ImGui::GetCursorPosY()
                        << " stride=" << rows::row_stride() << " height=" << rows::row_height() << '\n';
                }
                check(std::abs(ImGui::GetCursorPosY() - before - rows::row_stride()) < 0.01F,
                    "Selectable advances exactly the clipper stride");
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::End();
        ImGui::Render();
        // CPU-only backend: acknowledge demand-rasterized atlas updates after validating pixels.
        // This exercises ImGui's production font path without creating a game/GPU window.
        if (g_dynamicFonts) {
            for (ImTextureData* texture : ImGui::GetPlatformIO().Textures) {
                if (texture->Status == ImTextureStatus_WantCreate
                    || texture->Status == ImTextureStatus_WantUpdates) {
                    check(texture->Pixels != nullptr && texture->Width > 0 && texture->Height > 0,
                        "dynamic font atlas update has pixels");
                    texture->SetTexID(static_cast<ImTextureID>(1));
                    texture->SetStatus(ImTextureStatus_OK);
                    ++g_textureUpdates;
                } else if (texture->Status == ImTextureStatus_WantDestroy) {
                    texture->SetTexID(ImTextureID_Invalid);
                    texture->SetStatus(ImTextureStatus_Destroyed);
                }
            }
        }
        check(g_errors == 0, "row/clipper render has no ImGui recovery errors");
    }
}
}
int main(int argc, char** argv) {
    namespace memory = dawn::core::ui::memory;
    check(memory::initialize(), "production fixed allocator initialized");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {800, 600};
    io.DeltaTime = 1.0F / 60.0F;
    io.IniFilename = nullptr;
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    ImGui::GetCurrentContext()->ErrorCallback = error;
    std::vector<char> fontBytes;
    g_dynamicFonts = argc == 2;
    if (g_dynamicFonts) {
        std::ifstream font(argv[1], std::ios::binary);
        fontBytes.assign(std::istreambuf_iterator<char>(font), {});
        check(!fontBytes.empty() && fontBytes.size() < 160 * 1024, "installed font fixture");
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImFontConfig config{};
        config.FontDataOwnedByAtlas = false;
        config.RasterizerDensity = 2.0F;
        io.FontDefault = io.Fonts->AddFontFromMemoryTTF(fontBytes.data(),
            static_cast<int>(fontBytes.size()), 16.0F, &config);
        check(io.FontDefault != nullptr, "installed font admitted");
    } else {
        unsigned char* pixels{};
        int width{}, height{};
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        check(pixels != nullptr && width > 0 && height > 0, "headless font atlas");
    }
    for (const float scale : {1.0F, 2.0F}) {
        scenario(0, 0, scale);
        scenario(1, 0, scale);
        scenario(1170, 0, scale);
        scenario(1170, 18000, scale);
        scenario(1170, 100000, scale);
    }
    if (g_dynamicFonts) {
        for (int pass = 0; pass < 100; ++pass) {
            const float scales[]{0.9F, 1.125F, 2.0F, 2.5F};
            scenario(1170, static_cast<float>((pass * 997) % 100000), scales[pass % 4], 30);
        }
        check(g_textureUpdates > 0, "on-demand rasterization was exercised");
    }
    const auto stats = memory::snapshot();
    ImGui::DestroyContext();
    check(memory::snapshot().outstandingAllocations == 0, "all ImGui arena allocations released");
    check(memory::shutdown(), "production allocator shutdown");
    std::cout << "PASS: " << (g_dynamicFonts ? 3030 : 30)
        << " production-row ImGui frames; zero recovery errors; atlas updates=" << g_textureUpdates
        << "; fixed-arena high-water=" << stats.highWaterBytes << '/' << stats.capacityBytes << '\n';
}
