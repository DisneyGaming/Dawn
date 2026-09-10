// Integration fixture: production parser, catalog, panel, card renderer, font allocator and DX11 backend.
// The launch and scenario services are isolated stubs; this executable never calls the game.
#define main metadata_fixture_main
#include "mission_launch_metadata_tests.cpp"
#undef main
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx11.h>
#include "core/ui/memory/allocator.h"
#include "core/ui/theme/sunrise_ui_theme.h"
#include "core/ui/animation/transition/ui_transition_animation.h"
#include "../src/client/ui/mission_launch/mission_launch_panel.cpp"

namespace {
float g_scale = 1.0F;
bool g_override{};
unsigned g_launches{}, g_errors{};
ImGuiID g_activate{};
std::uint16_t g_launchIndex{};
sunrise::state::build_data::scenarios::Definition g_manualLayout{};
sunrise::state::activity::forced::ForcedDestination g_manualRequest{}, g_storedOverride{};
unsigned g_manualLaunches{}, g_overrideClears{};
bool g_captureText{};
std::string g_frameText{};
void error_callback(ImGuiContext*, void*, const char* message) {
    ++g_errors; std::cerr << "ImGui error: " << message << '\n';
}
ID3D11Device* g_gpu{};
ID3D11DeviceContext* g_context{};
ID3D11Texture2D* g_target{};
ID3D11RenderTargetView* g_rtv{};
constexpr UINT kWidth = 1500, kHeight = 1000;
void screenshot(const std::filesystem::path& path) {
    D3D11_TEXTURE2D_DESC desc{};
    g_target->GetDesc(&desc); desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging{};
    check(SUCCEEDED(g_gpu->CreateTexture2D(&desc, nullptr, &staging)), "screenshot staging");
    g_context->CopyResource(staging, g_target);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(SUCCEEDED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)), "screenshot readback");
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (UINT y = 0; y < kHeight; ++y) {
        const auto* row = static_cast<const char*>(mapped.pData) + y * mapped.RowPitch;
        for (UINT x = 0; x < kWidth; ++x) { out.write(row + x * 4, 3); }
    }
    g_context->Unmap(staging, 0); staging->Release();
}
void frame(float width, float height, const char* activate = nullptr, float scroll = -1.0F) {
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
    ImGui::SetNextWindowPos({20, 20}); ImGui::SetNextWindowSize({width, height});
    ImGui::Begin("Mission Launch fixture", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
    if (g_activate != 0) { ImGui::ActivateItemByID(g_activate); g_activate = 0; }
    if (activate != nullptr) { ImGui::ActivateItemByID(ImGui::GetID(activate)); }
    if (scroll >= 0) {
        for (auto* window : ImGui::GetCurrentContext()->Windows) {
            if (std::strstr(window->Name, "mission_activity_grid") != nullptr) { ImGui::SetScrollY(window, scroll); }
        }
    }
    static bool checkTitle = true;
    if (checkTitle || g_captureText) { ImGui::LogToBuffer(0); }
    sunrise::core::ui::components::section::header("Activity Launcher");
    sunrise::client::ui::mission_launch::draw();
    if (checkTitle) {
        const std::string_view logged(ImGui::GetCurrentContext()->LogBuffer.c_str());
        const auto first = logged.find("Activity Launcher");
        check(first != std::string_view::npos && logged.find("Activity Launcher", first + 1) == std::string_view::npos,
            "outer module and production panel render exactly one Activity Launcher title");
    }
    if (g_captureText) { g_frameText = ImGui::GetCurrentContext()->LogBuffer.c_str(); }
    if (checkTitle || g_captureText) { ImGui::LogFinish(); checkTitle = false; }
    ImGui::End(); ImGui::Render();
    const float clear[]{0.025F, 0.032F, 0.045F, 1};
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr); g_context->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    check(g_errors == 0, "full panel produced no cursor/table/clipper/font assertions");
}
unsigned focused_close(const std::filesystem::path& screens) {
    namespace panel = sunrise::client::ui::mission_launch;
    namespace transition = sunrise::core::ui::animation::transition;
    std::array<ImTextureID, sunrise::state::build_data::activities::kIconCount> originalTextures{};
    for (std::size_t i = 1; i < originalTextures.size(); ++i) {
        originalTextures[i] = panel::art::texture(static_cast<sunrise::state::build_data::activities::Icon>(i));
    }
    struct FadeFrame { float progress{}; unsigned imageElements{}; };
    unsigned frames{}, imageElements{};
    const auto fadeFrame = [&](bool visible) {
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        // Same production transition and rates as ui_layout_render; no texture lifetime changes on close.
        const float progress = transition::update(1, transition::Lane::visibility, visible, {16.0F, 14.0F}, 1.0F);
        if (progress > 0.0F) {
            ImGui::SetNextWindowPos({20, 20}); ImGui::SetNextWindowSize({680, 720});
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, progress);
            if (ImGui::Begin("Mission Launch close fixture", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar)) {
                sunrise::core::ui::components::section::header("Activity Launcher");
                panel::draw();
            }
            ImGui::End(); ImGui::PopStyleVar();
        }
        ImGui::Render();
        unsigned elements{};
        auto* data = ImGui::GetDrawData();
        const auto expected = static_cast<unsigned>(std::lround(progress * 255.0F));
        for (const auto* list : data->CmdLists) {
            for (const auto& cmd : list->CmdBuffer) {
                if (cmd.UserCallback != nullptr || cmd.ElemCount == 0
                    || std::find(originalTextures.begin() + 1, originalTextures.end(), cmd.GetTexID()) == originalTextures.end()) { continue; }
                for (unsigned i = 0; i < cmd.ElemCount; ++i) {
                    const auto vertex = cmd.VtxOffset + list->IdxBuffer[static_cast<int>(cmd.IdxOffset + i)];
                    const unsigned alpha = (list->VtxBuffer[static_cast<int>(vertex)].col >> IM_COL32_A_SHIFT) & 255U;
                    check(alpha == expected, "every native launcher icon inherits the current surface alpha");
                }
                elements += cmd.ElemCount;
            }
        }
        if (progress == 0.0F) { check(elements == 0 && data->TotalVtxCount == 0, "closed surface emits no icon or window geometry"); }
        const float clear[]{0.025F, 0.032F, 0.045F, 1};
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr); g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(data);
        check(g_errors == 0, "close transition emits no ImGui errors");
        ++frames; imageElements += elements;
        return FadeFrame{progress, elements};
    };
    for (const float fps : {30.0F, 60.0F, 144.0F}) {
        ImGui::GetIO().DeltaTime = 1.0F / fps;
        for (unsigned page = 0; page < 4; ++page) {
            panel::reset_library(); panel::g_libraryKind = page == 1 ? 2 : 0;
            if (page == 2) { panel::navigate(2); }
            if (page == 3) { panel::navigate(2, 299); }
            transition::reset(); (void)fadeFrame(true);
            const auto open = fadeFrame(true);
            check(open.progress == 1.0F && open.imageElements > 0, "open library, season, activity and detail pages draw native artwork");
            if (fps == 60.0F && page == 1) { screenshot(screens / "close-open.ppm"); }
            FadeFrame closing{}; unsigned elapsed{};
            do {
                closing = fadeFrame(false); ++elapsed;
                if (fps == 60.0F && page == 1 && elapsed == 10) { screenshot(screens / "close-fading.ppm"); }
                check(elapsed < 120, "closing settles within the bounded native transition");
            } while (closing.progress > 0.0F);
            check(elapsed > 1, "test observes intermediate closing frames");
            for (unsigned i = 0; i < 3; ++i) { check(fadeFrame(false).imageElements == 0, "icons remain absent after surface closes"); }
            if (fps == 60.0F && page == 1) { screenshot(screens / "close-closed.ppm"); }
            elapsed = 0;
            do { closing = fadeFrame(true); check(++elapsed < 120, "reopening settles"); } while (closing.progress < 1.0F);
            check(closing.imageElements > 0, "reopening restores the selected page's artwork");
            for (std::size_t i = 1; i < originalTextures.size(); ++i) {
                check(originalTextures[i] == panel::art::texture(static_cast<sunrise::state::build_data::activities::Icon>(i)),
                    "closing/reopening does not release, recreate or replace native textures");
            }
        }
    }
    transition::reset();
    std::cout << "Close regression: " << imageElements << " native image elements checked at production fade alpha; 12 close/reopen cases\n";
    return frames;
}
unsigned focused_details(const std::filesystem::path& screens) {
    namespace panel = sunrise::client::ui::mission_launch;
    namespace forced = sunrise::state::activity::forced;
    panel::manual::g_lists.spawnCount = 1;
    panel::manual::g_lists.spawnHashes[0] = 0x2EA8FB98;
    (void)std::snprintf(panel::manual::g_lists.spawns[0].data(), panel::manual::g_lists.spawns[0].size(),
        "0x2EA8FB98  default  (candidate)  (not loaded)");
    check(std::string_view(panel::manual::spawn_label(0).data()) == "0x2EA8FB98  (candidate)  (not loaded)",
        "manual spawn names cannot use compiled aliases when installed hash-name lookup is absent");
    const auto colors = ImGui::GetStyle();
    const auto luminance = [](const ImVec4& color) {
        const auto linear = [](float v) { return v <= 0.04045F ? v / 12.92F : std::pow((v + 0.055F) / 1.055F, 2.4F); };
        return 0.2126F * linear(color.x) + 0.7152F * linear(color.y) + 0.0722F * linear(color.z);
    };
    const auto contrast = [&](const ImVec4& a, const ImVec4& b) {
        const float x = luminance(a), y = luminance(b);
        return ((std::max)(x, y) + 0.05F) / ((std::min)(x, y) + 0.05F);
    };
    {
        const panel::detail::Style style{};
        const auto& c = ImGui::GetStyle().Colors;
        check(contrast(c[ImGuiCol_Text], c[ImGuiCol_FrameBg]) >= 4.5F
            && contrast(c[ImGuiCol_Text], c[ImGuiCol_Header]) >= 4.5F,
            "dropdown text and selected rows retain readable contrast");
        check(contrast(c[ImGuiCol_Border], c[ImGuiCol_FrameBg]) >= 3.0F, "control edges contrast with dropdown surfaces");
        check(contrast({0.035F, 0.045F, 0.065F, 1}, {0.97F, 0.51F, 0.25F, 1}) >= 4.5F,
            "primary launch label has readable contrast on its accent surface");
        check(std::memcmp(&c[ImGuiCol_FrameBg], &c[ImGuiCol_FrameBgHovered], sizeof(ImVec4)) != 0
            && std::memcmp(&c[ImGuiCol_FrameBgHovered], &c[ImGuiCol_FrameBgActive], sizeof(ImVec4)) != 0,
            "dropdown default, hover and active colors are distinct");
    }
    unsigned frames{};
    g_captureText = true;
    panel::navigate(2, 299); frame(680, 720); frame(680, 720);
    check(g_frameText.find("Omega") != std::string::npos && g_frameText.find("Native arrival") != std::string::npos,
        "clean default detail retains activity heading and native arrival limitation");
    check(g_frameText.find("Launch details") == std::string::npos && g_frameText.find("native launch variants") == std::string::npos
        && g_frameText.find("Bubble") == std::string::npos, "default details omit redundant navigation, count and manual rows");
    screenshot(screens / "detail-normal.ppm");
    ImGui::GetIO().AddMousePosEvent(320, 260); frame(680, 720); frame(680, 720);
    screenshot(screens / "detail-hover.ppm");
    ImGui::GetIO().AddMousePosEvent(-1, -1);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab, true); frame(680, 720);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab, false); frame(680, 720);
    screenshot(screens / "detail-focus.ppm");
    const auto before = g_launches;
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_launches == before + 1 && g_launchIndex == 299, "clean primary button retains exact ordinary launch");
    panel::navigate(2, 648); frame(680, 720); frame(680, 720);
    frame(680, 720, "##launch_variant"); frame(680, 720); frame(680, 720);
    screenshot(screens / "detail-variants.ppm");
    auto* popup = ImGui::FindWindowByName("##Combo_00");
    check(popup != nullptr && popup->Active, "compact variant dropdown opens");
    std::array<char, 260> variant{};
    const auto rows = sunrise::state::build_data::activities::entries();
    (void)std::snprintf(variant.data(), variant.size(), "#229 | %s | %s",
        sunrise::state::build_data::activities::presentation(229).type.data(), panel::title(rows[229]).data());
    g_activate = popup->GetID(variant.data()); frame(680, 720); frame(680, 720); frame(680, 720);
    check(panel::g_selected == 229, "variant selection preserves exact native ID after layout change");
    panel::navigate(15, 78); frame(680, 720); frame(680, 720);
    frame(680, 720, "Manual Mode"); frame(680, 720); frame(680, 720);
    check(panel::manual::g_enabled && g_frameText.find("Bubble") != std::string::npos && g_frameText.find("Slice") != std::string::npos
        && g_frameText.find("Spawn") != std::string::npos, "manual checkbox reveals dependent controls only when requested");
    const auto beforeManual = g_manualLaunches;
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_manualLaunches == beforeManual, "incomplete manual detail remains disabled");
    screenshot(screens / "detail-disabled.ppm");
    panel::manual::select_bubble(13); frame(680, 720); frame(680, 720);
    screenshot(screens / "detail-manual.ppm");
    frame(360, 720); frame(360, 720); screenshot(screens / "detail-narrow.ppm");
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_manualLaunches == beforeManual + 1 && g_manualRequest.bubble == 13 && g_manualRequest.sliceSet == 104,
        "paired manual controls preserve exact launch arguments");
    panel::navigate(1, 282); frame(680, 720); frame(680, 720);
    g_storedOverride = forced::profiles::kTowerfallOpening;
    const auto beforeBlocked = g_launches;
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_launches == beforeBlocked, "staged override guard remains enforced");
    screenshot(screens / "detail-override.ppm");
    frame(680, 720, "Clear active override"); frame(680, 720); frame(680, 720);
    check(!g_storedOverride.enabled, "explicit clear remains visible and delegates to override owner");
    panel::reset_library(); panel::manual::start_custom(); panel::g_custom = true;
    panel::manual::select_destination("infinite_abyss"); panel::manual::g_transport = 78; panel::manual::select_bubble(13);
    frame(680, 720); frame(680, 720); screenshot(screens / "detail-custom.ppm");
    frame(680, 720, "##manual_spawn"); frame(680, 720); frame(680, 720);
    screenshot(screens / "detail-spawns.ppm");
    popup = ImGui::FindWindowByName("##Combo_00");
    check(popup != nullptr && popup->Active, "high-contrast spawn popup opens");
    g_activate = popup->GetID(panel::manual::spawn_label(0).data()); frame(680, 720); frame(680, 720); frame(680, 720);
    check(panel::manual::g_value.spawnSetHash == 0x79E3AB1F && panel::manual::g_value.hasSpawnSetHash,
        "restyled spawn dropdown retains actual native hash");
    frame(360, 720); frame(360, 720); screenshot(screens / "detail-custom-narrow.ppm");
    g_captureText = false;
    for (float scale : {0.9F, 1.0F, 1.125F, 2.5F}) {
        g_scale = scale; sunrise::core::ui::theme::apply();
        const auto expected = ImGui::GetStyle();
        for (float width : {280.0F, 360.0F, 680.0F, 1200.0F}) {
            for (const int index : {78, 229, 299, 282}) {
                panel::navigate(index == 78 ? 15 : 2, index);
                for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 720); ++frames; }
                panel::manual::g_enabled = true;
                if (index == 78) { panel::manual::select_bubble(13); }
                for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 720); ++frames; }
            }
            panel::reset_library(); panel::manual::start_custom(); panel::g_custom = true;
            panel::manual::select_destination("infinite_abyss"); panel::manual::g_transport = 78; panel::manual::select_bubble(13);
            for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 720); ++frames; }
            check(std::memcmp(expected.Colors, ImGui::GetStyle().Colors, sizeof(expected.Colors)) == 0,
                "detail palette is fully restored and cannot change library cards");
            check(expected.FontSizeBase == ImGui::GetStyle().FontSizeBase, "heading font restores base size without compounded DPI");
        }
    }
    g_scale = 1; sunrise::core::ui::theme::apply();
    check(std::memcmp(colors.Colors, ImGui::GetStyle().Colors, sizeof(colors.Colors)) == 0, "original palette retained after focused details");
    panel::reset_library(); frame(680, 720); frame(680, 720); screenshot(screens / "library-unchanged.ppm");
    frame(680, 720, "Seasons"); frame(680, 720); frame(680, 720);
    check(panel::g_libraryKind == 2, "runtime named-season library remains navigable");
    screenshot(screens / "seasons-runtime.ppm");
    frame(680, 720, "Events"); frame(680, 720); frame(680, 720);
    check(panel::g_libraryKind == 3, "runtime named-event library remains navigable");
    screenshot(screens / "events-runtime.ppm");
    panel::navigate(20); frame(680, 720); frame(680, 720);
    screenshot(screens / "dawning-runtime.ppm");
    return frames;
}
}
namespace sunrise::core::ui::scaling::dpi {
float current() noexcept { return g_scale; }
float pixels(float value) noexcept { return value * g_scale; }
}
namespace sunrise::state::build_data {
std::size_t scenario_layout_count() noexcept { return 1038; }
bool find_scenario_layout(std::string_view name, scenarios::Definition& output) noexcept {
    output = name == "infinite_abyss" ? g_manualLayout : scenarios::Definition{};
    return !name.empty();
}
bool snapshot_scenario_layouts(std::span<scenarios::Definition> output, std::size_t& count) noexcept {
    count = 0; if (output.empty()) { return false; } output[0] = g_manualLayout; count = 1; return true;
}
bool find_hash_name(std::uint32_t, hash_names::Name&) noexcept { return false; }
bool find_spawn_sets(std::string_view stem, std::span<spawn_sets::NameHash> output, std::size_t& count) noexcept {
    count = 0;
    if (stem != "infinite_forest_live" || output.empty()) { return false; }
    // Reduced installed membership fixture: spawn 79E3AB1F has three points in map package 0685.
    output[0] = {}; output[0].value = 0x79E3AB1F; output[0].pointCount = 3;
    output[0].inMapPackage = 1; output[0].bubbleMask[30 / 8] = 1U << (30 % 8);
    count = 1; return true;
}
}
namespace sunrise::state::activity::forced {
bool override_active() noexcept { return g_override; }
void stored(ForcedDestination& output) noexcept { output = g_storedOverride; }
void snapshot(ForcedDestination& output) noexcept { output = g_storedOverride; }
bool omega_completion_suspended() noexcept { return false; }
void clear() noexcept { g_storedOverride = {}; g_override = false; ++g_overrideClears; }
}
namespace sunrise::client::activity::mission_launch {
bool request(std::uint16_t index) noexcept { ++g_launches; g_launchIndex = index; return true; }
bool request_manual(std::uint16_t index, const state::activity::forced::ForcedDestination& destination) noexcept {
    ++g_manualLaunches; g_launchIndex = index; g_manualRequest = destination; return true;
}
Snapshot snapshot() noexcept { return {}; }
const char* description(Status status) noexcept {
    return status == Status::overrideActive ? "An activity override is active. Clear it in Activity Override before launching."
        : "Return to orbit before launching an activity. Your native fireteam selection is preserved.";
}
}
int main(int argc, char** argv) {
    using namespace sunrise;
    namespace panel = client::ui::mission_launch;
    namespace memory = core::ui::memory;
    check(argc == 5 || (argc == 6 && (std::string_view(argv[5]) == "--details-only" || std::string_view(argv[5]) == "--close-only")),
        "provide metadata, OTF, screenshots, scenario, optional --details-only or --close-only");
    check(metadata_fixture_main(2, argv) == 0, "metadata/art fixture checks");
    static std::array<state::build_data::activities::Definition, state::build_data::activities::kCapacity> rows{};
    std::size_t count{};
    check(middleware::content::packages::tables::activities::decode(load(g_fixture / "81327CF0.bin"), rows, count), "activity fixture");
    check(state::build_data::activities::publish(std::span(rows).first(count)), "publish installed rows");
    const auto scenario = load(argv[4]);
    namespace tables = middleware::content::packages::tables;
    tables::Array bubbles{};
    check(tables::scenario_bubbles(scenario, bubbles) && bubbles.count == 20, "installed Haunted scenario bubbles");
    g_manualLayout.tag = 0x81550015; g_manualLayout.bubbleCount = 20;
    (void)std::snprintf(g_manualLayout.name.data(), g_manualLayout.name.size(), "infinite_abyss");
    g_manualLayout.nameLength = 14;
    (void)std::snprintf(g_manualLayout.spawnStem.data(), g_manualLayout.spawnStem.size(), "infinite_forest_live");
    g_manualLayout.spawnStemLength = 20;
    for (std::uint8_t i = 0; i < 20; ++i) {
        tables::Bubble bubble{}; tables::SliceState slice{};
        check(tables::bubble_at(scenario, bubbles, i, bubble) && tables::slice_state_at(scenario, bubble, 0, slice), "installed bubble and slice");
        g_manualLayout.bubbleHashes[i] = bubble.nameHash;
        g_manualLayout.bubbleStateCounts[i] = static_cast<std::uint8_t>(bubble.stateCount);
        g_manualLayout.bubbleMapIndices[i] = static_cast<std::uint16_t>(slice.mapBubbleIndex);
        g_manualLayout.bubbleStates[i] = slice.enabled ? state::build_data::scenarios::kBubbleEnabledByte : state::build_data::scenarios::kBubbleDisabledByte;
    }
    check(memory::initialize(), "production fixed arena");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {static_cast<float>(kWidth), static_cast<float>(kHeight)};
    io.DeltaTime = 1.0F / 60; io.IniFilename = nullptr;
    if (argc == 6) { io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; }
    io.ConfigErrorRecoveryEnableAssert = false; io.ConfigErrorRecoveryEnableTooltip = false;
    ImGui::GetCurrentContext()->ErrorCallback = error_callback;
    const auto font = load(argv[2]);
    check(!font.empty() && font.size() < 160 * 1024, "actual installed font fixture");
    ImFontConfig config{}; config.FontDataOwnedByAtlas = false; config.RasterizerDensity = 2.0F;
    check(io.Fonts->AddFontFromMemoryTTF(const_cast<std::byte*>(font.data()), static_cast<int>(font.size()), 16, &config) != nullptr,
        "actual installed OTF loaded at production rasterizer density");
    ImGui::GetStyle().FontSizeBase = 16;
    D3D_FEATURE_LEVEL feature{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &g_gpu, &feature, &g_context)), "offscreen WARP device");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = kWidth; desc.Height = kHeight; desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    check(SUCCEEDED(g_gpu->CreateTexture2D(&desc, nullptr, &g_target)), "offscreen render texture");
    check(SUCCEEDED(g_gpu->CreateRenderTargetView(g_target, nullptr, &g_rtv)), "offscreen render view");
    check(ImGui_ImplDX11_Init(g_gpu, g_context), "production DX11 backend");
    panel::art::prepare(g_gpu);
    core::ui::theme::apply();
    std::filesystem::create_directories(argv[3]);
    if (argc == 6) {
        const bool closeOnly = std::string_view(argv[5]) == "--close-only";
        const auto frames = closeOnly ? focused_close(argv[3]) : focused_details(argv[3]);
        panel::art::release(); ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
        g_context->ClearState(); g_context->Flush();
        g_rtv->Release(); g_target->Release(); g_context->Release();
        check(g_gpu->Release() == 0, "focused detail GPU resources released");
        const auto stats = memory::snapshot();
        check(stats.outstandingAllocations == 0 && memory::shutdown(), "focused detail fixed arena released");
        std::cout << "PASS: " << frames << (closeOnly ? " focused close/reopen frames; " : " focused detail resize/manual/custom frames plus interaction checks; ") << "zero ImGui errors; "
            << stats.highWaterBytes << "/" << stats.capacityBytes << " arena high water\n";
        return 0;
    }
    frame(680, 720); frame(680, 720); screenshot(std::filesystem::path(argv[3]) / "dlc-library.ppm");
    frame(680, 720, "Seasons"); frame(680, 720); frame(680, 720);
    check(panel::g_libraryKind == 2, "named Seasons library filter activates");
    screenshot(std::filesystem::path(argv[3]) / "seasons-library.ppm");
    frame(680, 720, "Events"); frame(680, 720); frame(680, 720);
    check(panel::g_libraryKind == 3, "recurring Events library filter activates");
    screenshot(std::filesystem::path(argv[3]) / "events-library.ppm");
    panel::navigate(20); frame(680, 720); frame(680, 720);
    screenshot(std::filesystem::path(argv[3]) / "dawning-catalog.ppm");
    panel::navigate(15); frame(680, 720); frame(680, 720);
    screenshot(std::filesystem::path(argv[3]) / "festival-activities.ppm");
    panel::g_libraryKind = 0;
    panel::navigate(2);
    frame(680, 720); frame(680, 720); screenshot(std::filesystem::path(argv[3]) / "osiris-activities.ppm");
    panel::navigate(2, 299);
    frame(680, 720); frame(680, 720); screenshot(std::filesystem::path(argv[3]) / "omega-detail.ppm");
    panel::navigate(2); (void)std::snprintf(panel::g_type.data(), panel::g_type.size(), "Adventure");
    frame(680, 720); frame(680, 720); screenshot(std::filesystem::path(argv[3]) / "adventures.ppm");
    panel::g_type.fill(0); panel::navigate(1, 879);
    frame(680, 720); frame(680, 720); screenshot(std::filesystem::path(argv[3]) / "patrol-detail.ppm");
    panel::navigate(2); (void)std::snprintf(panel::g_search.data(), panel::g_search.size(), "A Garden World");
    panel::g_showUnavailable = false; frame(680, 720); frame(680, 720);
    screenshot(std::filesystem::path(argv[3]) / "garden-world.ppm");
    std::array<std::uint16_t, state::build_data::activities::kCapacity> visible{};
    check(panel::visible_experiences(std::span(rows).first(count), visible) == 2, "Garden World shows one Story and one Strike card");
    (void)std::snprintf(panel::g_search.data(), panel::g_search.size(), "648");
    check(panel::visible_experiences(std::span(rows).first(count), visible) == 1
        && panel::experience_id(rows[visible[0]]) == panel::experience_id(rows[229]), "hidden variant ID search finds its experience card");
    panel::g_search.fill(0);
    // All Content searches native activity names/identities across releases, preserving experience grouping.
    panel::reset_library(); panel::g_showUnavailable = true;
    (void)std::snprintf(panel::g_globalSearch.data(), panel::g_globalSearch.size(), "a GaRdEn WoRlD");
    check(panel::visible_experiences(std::span(rows).first(count), visible) == 2,
        "case-insensitive All Content search retains distinct Garden World Story and Strike experiences");
    frame(680, 720); frame(680, 720);
    screenshot(std::filesystem::path(argv[3]) / "all-content-search.ppm");
    io.AddMousePosEvent(120, 350); frame(680, 720);
    io.AddMouseButtonEvent(0, true); frame(680, 720);
    io.AddMouseButtonEvent(0, false); frame(680, 720);
    check(panel::g_content == 0 && panel::g_selected >= 0,
        "global activity card click opens detail while preserving search navigation");
    panel::navigate(0); panel::g_showUnavailable = false;
    (void)std::snprintf(panel::g_globalSearch.data(), panel::g_globalSearch.size(), "648");
    const auto identityMatches = panel::visible_experiences(std::span(rows).first(count), visible);
    unsigned strikeMatches{};
    for (std::size_t i = 0; i < identityMatches; ++i) {
        strikeMatches += panel::experience_id(rows[visible[i]]) == panel::experience_id(rows[229]) ? 1U : 0U;
    }
    check(strikeMatches == 1, "global hidden variant identity search resolves to one grouped Strike card");
    panel::navigate(0, 648); frame(680, 720); frame(680, 720);
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_launches == 1 && g_launchIndex == 648, "global detail launches the exact retained variant identity");
    g_launches = 0;
    frame(680, 720, "< Back to activities"); frame(680, 720); frame(680, 720);
    check(panel::g_content == 0 && panel::g_selected == -1 && std::string_view(panel::g_globalSearch.data()) == "648",
        "detail Back preserves global search and exact variant context");
    frame(680, 720, "Clear search"); frame(680, 720); frame(680, 720);
    check(!panel::g_globalSearch[0] && panel::g_content == 0, "Clear search restores content library");
    panel::g_showUnavailable = true;
    const auto allVisible = panel::visible_experiences(std::span(rows).first(count), visible);
    std::array<bool, panel::kContent.size()> searchedReleases{};
    for (std::size_t i = 0; i < allVisible; ++i) {
        const auto group = panel::content_group(rows[visible[i]]);
        check(panel::content_visible(group), "hidden releases never leak into global results even with unavailable scenarios enabled");
        searchedReleases[group] = true;
    }
    check(searchedReleases[1] && searchedReleases[2] && searchedReleases[7] && searchedReleases[15],
        "global results include base campaign, DLC, season and recurring event activities");
    for (const char* hidden : {"Iron Banner", "Beyond Light"}) {
        (void)std::snprintf(panel::g_globalSearch.data(), panel::g_globalSearch.size(), "%s", hidden);
        check(panel::visible_experiences(std::span(rows).first(count), visible) == 0,
            "removed preview/event entries are absent from global name search");
    }
    panel::reset_library();
    // All screens resize at fractional and high scales, then filters and child scroll change.
    unsigned frames{};
    for (float scale : {0.9F, 1.0F, 1.125F, 2.0F, 2.5F}) {
        g_scale = scale; core::ui::theme::apply();
        ImGui::GetStyle().ItemSpacing.y = 7.0F * scale;
        for (float width : {280.0F, 480.0F, 720.0F, 1200.0F}) {
            panel::g_libraryKind = 0; panel::navigate(0);
            for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 700); ++frames; }
            for (const char* query : {"a GaRdEn WoRlD", "mission", "no_matches_xyz"}) {
                (void)std::snprintf(panel::g_globalSearch.data(), panel::g_globalSearch.size(), "%s", query);
                panel::g_resetScroll = true;
                for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 700); ++frames; }
                frame(width, 700, nullptr, 1000000.0F); ++frames;
            }
            panel::reset_library();
            for (std::size_t dlc = 1; dlc < panel::kContent.size(); ++dlc) {
                panel::navigate(dlc); panel::g_showUnavailable = true;
                for (int filter = 0; filter < 3; ++filter) {
                    panel::g_search.fill(0);
                    if (filter == 1) { (void)std::snprintf(panel::g_search.data(), panel::g_search.size(), "mission"); }
                    if (filter == 2) { (void)std::snprintf(panel::g_search.data(), panel::g_search.size(), "no_matches_xyz"); }
                    panel::g_resetScroll = true;
                    for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 700); ++frames; }
                }
                panel::g_search.fill(0);
                panel::g_resetScroll = false;
                for (float scroll : {0.0F, 7500.0F, 1000000.0F}) {
                    frame(width, 700, nullptr, scroll); frame(width, 700); frames += 2;
                }
                panel::navigate(dlc, 299);
                for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 700); ++frames; }
            }
        }
    }
    g_scale = 1.0F; core::ui::theme::apply();
    panel::g_libraryKind = 0; panel::navigate(0); frame(680, 720); frame(680, 720);
    io.AddMousePosEvent(560, 250); frame(680, 720);
    io.AddMouseButtonEvent(0, true); frame(680, 720);
    io.AddMouseButtonEvent(0, false); frame(680, 720);
    check(panel::g_content == 2 && panel::g_selected == -1, "native DLC card click opens activity grid");
    (void)std::snprintf(panel::g_search.data(), panel::g_search.size(), "Omega");
    panel::g_resetScroll = true; frame(680, 720); frame(680, 720);
    io.AddMousePosEvent(120, 350); frame(680, 720);
    io.AddMouseButtonEvent(0, true); frame(680, 720);
    io.AddMouseButtonEvent(0, false); frame(680, 720);
    check(panel::g_selected == 299, "activity card click opens exact Omega detail");
    panel::g_search.fill(0);
    panel::navigate(2, 299); g_override = true;
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_launches == 0, "override-disabled launch cannot queue a native request");
    g_override = false;
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_launches == 1 && g_launchIndex == 299, "launch button queues exactly the selected public ID");
    panel::navigate(2, 648); frame(680, 720);
    frame(680, 720, "Launch activity"); frame(680, 720); frame(680, 720);
    check(g_launches == 2 && g_launchIndex == 648, "alternate variant launches its exact retained identity");
    frame(680, 720, "##launch_variant"); frame(680, 720); frame(680, 720);
    screenshot(std::filesystem::path(argv[3]) / "strike-variants.ppm");
    auto* popup = ImGui::FindWindowByName("##Combo_00");
    check(popup != nullptr && popup->Active, "compact native variant chooser opens");
    std::array<char, 260> variantLabel{};
    (void)std::snprintf(variantLabel.data(), variantLabel.size(), "#229 | %s | %s",
        state::build_data::activities::presentation(229).type.data(), panel::title(rows[229]).data());
    g_activate = popup->GetID(variantLabel.data());
    frame(680, 720); frame(680, 720); frame(680, 720);
    check(panel::g_selected == 229 && g_launches == 2, "variant chooser changes identity without initiating a launch");
    frame(680, 720, "< Back to activities"); frame(680, 720); frame(680, 720);
    check(panel::g_content == 2 && panel::g_selected == -1, "back preserves DLC context");
    frame(680, 720, "Content library"); frame(680, 720); frame(680, 720);
    check(panel::g_content == 0, "breadcrumb returns to library");
    io.AddMousePosEvent(120, 250); frame(680, 720);
    io.AddMouseButtonEvent(0, true); frame(680, 720);
    io.AddMouseButtonEvent(0, false); frame(680, 720);
    check(panel::g_custom && panel::g_selected == -1, "first Custom Launch card opens directly to manual detail");
    check(g_manualLaunches == 0 && !g_storedOverride.enabled, "opening custom detail does not publish or launch an override");
    panel::manual::select_destination("infinite_abyss");
    panel::manual::g_transport = panel::manual::default_transport("infinite_abyss");
    frame(680, 950); frame(680, 950);
    frame(680, 950, "Launch activity"); frame(680, 950); frame(680, 950);
    check(g_manualLaunches == 0, "incomplete manual bubble/slice cannot queue a launch");
    panel::manual::select_bubble(13);
    check(panel::manual::g_value.sliceSet == 104 && !panel::manual::g_value.hasSpawnSetHash,
        "actual selected Haunted bubble seeds authored slice and clears spawn");
    frame(680, 950); frame(680, 950); screenshot(std::filesystem::path(argv[3]) / "custom-launch.ppm");
    frame(680, 950, "##manual_spawn"); frame(680, 950); frame(680, 950);
    auto* spawnPopup = ImGui::FindWindowByName("##Combo_00");
    check(spawnPopup != nullptr && spawnPopup->Active, "manual spawn dropdown opens with actual available membership");
    check(panel::manual::g_lists.spawnCount == 1 && panel::manual::g_lists.spawnHashes[0] == 0x79E3AB1F,
        "manual dropdown retains the installed Forest Gate spawn hash");
    g_activate = spawnPopup->GetID(panel::manual::spawn_label(0).data());
    frame(680, 950); frame(680, 950); frame(680, 950);
    check(panel::manual::g_value.hasSpawnSetHash && panel::manual::g_value.spawnSetHash == 0x79E3AB1F,
        "spawn dropdown selects exact native hash");
    panel::manual::g_value.hasSpawnSetHash = false; panel::manual::g_value.spawnSetHash = 0;
    frame(680, 950, "Launch activity"); frame(680, 950); frame(680, 950);
    check(g_manualLaunches == 1 && g_manualRequest.bubble == 13 && g_manualRequest.sliceSet == 104
        && !g_manualRequest.hasSpawnSetHash && panel::manual::g_transport == g_launchIndex,
        "manual launch queues copied exact bubble/slice/client-spawn and native transport");
    panel::manual::g_value.hasSpawnSetHash = true; panel::manual::g_value.spawnSetHash = 0x2EA8FB98;
    panel::manual::select_bubble(0);
    check(panel::manual::g_value.sliceSet == 0 && !panel::manual::g_value.hasSpawnSetHash,
        "changing bubble clears prior spawn and derives its own slice");
    panel::manual::select_destination("mission_scot");
    check(!panel::manual::g_value.hasBubble && !panel::manual::g_value.hasSliceSet && !panel::manual::g_value.hasSpawnSetHash,
        "changing custom activity invalidates every dependent arrival field");
    panel::navigate(15, 78); frame(680, 950); frame(680, 950);
    check(!panel::manual::g_enabled, "ordinary activity detail starts with Manual Mode unchecked");
    frame(680, 950, "Manual Mode"); frame(680, 950); frame(680, 950);
    check(panel::manual::g_enabled, "small Manual Mode checkbox enables the draft dropdowns");
    panel::manual::select_bubble(13); frame(680, 950); frame(680, 950);
    screenshot(std::filesystem::path(argv[3]) / "manual-activity-detail.ppm");
    panel::navigate(15, 79); frame(680, 950); frame(680, 950);
    check(!panel::manual::g_enabled && !panel::manual::g_value.hasBubble,
        "changing exact native activity variant cannot retain stale manual arrival");
    g_storedOverride = g_manualRequest; g_override = true;
    frame(680, 950, "Clear active override"); frame(680, 950); frame(680, 950);
    check(g_overrideClears == 1 && !g_override && !g_storedOverride.enabled,
        "explicit clear delegates to existing override service");
    g_storedOverride = state::activity::forced::profiles::kTowerfallOpening;
    panel::navigate(1, 282); frame(680, 950); frame(680, 950);
    const auto launchesBeforeStaged = g_launches;
    frame(680, 950, "Launch activity"); frame(680, 950); frame(680, 950);
    check(g_launches == launchesBeforeStaged, "unchecked native launch cannot accidentally activate staged Homecoming override");
    g_storedOverride = {};
    for (float scale : {0.9F, 1.125F, 2.5F}) {
        g_scale = scale; core::ui::theme::apply();
        for (float width : {280.0F, 680.0F, 1200.0F}) {
            panel::reset_library(); panel::manual::start_custom(); panel::g_custom = true;
            panel::manual::select_destination("infinite_abyss"); panel::manual::g_transport = 78;
            panel::manual::select_bubble(13);
            for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 950); ++frames; }
            frame(width, 950, "##custom_transport"); frame(width, 950); frames += 2;
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true); frame(width, 950);
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false); frame(width, 950); frames += 2;
            panel::navigate(15, 78); frame(width, 950); ++frames;
            panel::manual::g_enabled = true; panel::manual::select_bubble(13);
            for (int repeat = 0; repeat < 3; ++repeat) { frame(width, 950); ++frames; }
        }
    }
    panel::art::release();
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    g_context->ClearState(); g_context->Flush();
    g_rtv->Release(); g_target->Release(); g_context->Release();
    check(g_gpu->Release() == 0, "offscreen UI GPU resources released");
    const auto stats = memory::snapshot();
    check(stats.outstandingAllocations == 0, "all ImGui allocations released");
    check(memory::shutdown(), "fixed allocator shutdown");
    std::cout << "PASS: " << frames << " full-panel resize/filter/navigation frames, actual font+40 installed icons, zero ImGui errors, "
        << stats.highWaterBytes << "/" << stats.capacityBytes << " arena high water; isolated launch and Back/breadcrumb checks\n";
}
