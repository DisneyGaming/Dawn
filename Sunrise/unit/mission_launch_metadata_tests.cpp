#include <d3d11.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include "client/content/activity/activity_presentation_build.h"
#include "client/ui/mission_launch/mission_launch_art.h"
#include "client/ui/mission_launch/mission_launch_model.h"

namespace {
std::filesystem::path g_fixture;
std::map<std::uint32_t, std::uint32_t> g_classes;
std::uint32_t g_blockedTag{}, g_mutatedTag{};
std::size_t g_mutatedOffset{};
std::byte g_mutatedByte{};
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::vector<std::byte> load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(file), {}};
    std::vector<std::byte> bytes(chars.size());
    if (!chars.empty()) { std::memcpy(bytes.data(), chars.data(), chars.size()); }
    return bytes;
}
}
namespace sunrise::middleware::content::packages::reader {
bool read_tag(const Source&, Scratch&, std::uint32_t tag, std::vector<std::byte>& output,
    std::uint32_t& cls) noexcept {
    std::array<char, 32> name{};
    (void)std::snprintf(name.data(), name.size(), "%08X.bin", tag);
    output = load(g_fixture / name.data());
    if (tag == g_blockedTag) { output.clear(); return false; }
    if (tag == g_mutatedTag && g_mutatedOffset < output.size()) { output[g_mutatedOffset] = g_mutatedByte; }
    const auto it = g_classes.find(tag);
    cls = it == g_classes.end() ? 0 : it->second;
    return it != g_classes.end() && !output.empty();
}
bool read_tag(const Source& source, Scratch& scratch, std::uint32_t tag,
    std::vector<std::byte>& output) noexcept {
    std::uint32_t cls{};
    return read_tag(source, scratch, tag, output, cls);
}
}
int main(int argc, char** argv) {
    using namespace sunrise;
    namespace catalog = state::build_data::activities;
    namespace reader = middleware::content::packages::reader;
    namespace parser = middleware::content::packages::tables::activities;
    namespace art = client::ui::mission_launch::art;
    check(argc == 2, "provide extracted installed fixture directory");
    g_fixture = argv[1];
    std::ifstream classes(g_fixture / "classes.txt");
    std::uint32_t tag{}, cls{};
    while (classes >> std::hex >> tag >> cls) { g_classes.emplace(tag, cls); }
    static reader::Scratch scratch{};
    reader::Source source{};
    static std::array<catalog::Definition, catalog::kCapacity> rows{};
    const auto publicTable = load(g_fixture / "81327CF0.bin");
    std::size_t count{};
    check(parser::decode(publicTable, rows, count) && count == 1170, "installed public fixture parsed");
    client::content::activity::build_presentations(source, scratch, load(g_fixture / "globals.bin"), std::span(rows).first(count));
    check(std::string_view(catalog::presentation(266).title.data()) == "Homecoming", "native Homecoming title");
    check(std::string_view(catalog::presentation(299).title.data()) == "Omega", "native Omega title");
    check(std::string_view(catalog::presentation(282).title.data()) == "Chosen", "native Chosen title");
    check(std::string_view(catalog::presentation(70).title.data()) == "Daily Heroic Story Mission", "same-package variant retains its shipped title");
    check(std::string_view(catalog::presentation(299).description.data()).starts_with("It's now or never."), "bank-scoped native description");
    check(std::string_view(catalog::presentation(542).type.data()) == "Raid", "native activity-type text");
    std::size_t named{}, described{}, typed{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto& display = catalog::presentation(static_cast<std::uint16_t>(i));
        named += display.title[0] != 0; described += display.description[0] != 0; typed += display.type[0] != 0;
    }
    check(named == 1131, "all 1131 native named activities resolved");
    check(client::ui::mission_launch::matches(rows[299], 2, 1, "oMEGa"), "actual display-name search");
    namespace model = client::ui::mission_launch;
    constexpr std::array<const char*, 20> contentNames{"", "The Red War", "Curse of Osiris", "Warmind",
        "Forsaken", "Shadowkeep", "New Light", "Season of the Forge", "Season of the Drifter", "Season of Opulence",
        "Season of the Undying", "Season of Dawn", "Season of the Worthy", "Season of Arrivals", "Solstice of Heroes",
        "Festival of the Lost", "The Revelry", "Solstice of Heroes", "", "Crimson Days"};
    for (std::size_t i = 1; i < contentNames.size(); ++i) {
        if (i == 18) { continue; }
        check(std::string_view(model::library_name(i)) == contentNames[i], "category displays exact installed localization");
        check(catalog::menu_text().content[i].bank != 0xFFFF, "category retains native string provenance");
    }
    check(std::string_view(model::library_name(20)) == "The Dawning", "Dawning displays its installed event name");
    check(std::string_view(model::library_name(18)) == "Content name unavailable", "missing name has only an explicit UI placeholder");
    constexpr std::array<const char*, 7> kindNames{"", "Story", "Strike", "Adventure", "Patrol", "Dungeon", "Quest"};
    for (std::size_t i = 1; i < kindNames.size(); ++i) {
        check(std::string_view(catalog::menu_text().kinds[i].text.data()) == kindNames[i], "grouped kind displays exact native Director type text");
    }
    check(!catalog::publish_menu_text({}), "published runtime menu text is immutable");
    check(model::content_group(rows[266]) == 1 && model::content_group(rows[282]) == 1, "native Red War campaign category");
    check(model::content_group(rows[157]) == 5 && model::content_group(rows[1]) == 6, "Shadowkeep and New Light are distinct authored campaign categories");
    for (const auto i : {293, 295, 297}) {
        check(rows[i].destination != 16 && model::content_group(rows[i]) == 2, "Osiris campaign on an earlier destination stays in Osiris");
    }
    check(rows[304].destination == 9 && model::content_group(rows[304]) == 3, "Off-World Recovery on Earth stays in Warmind");
    check(model::content_group(rows[70]) == 2 && catalog::presentation(70).classificationSource == 4,
        "unlabeled daily heroic variant inherits unambiguous package campaign without losing its title or ID");
    check(model::content_group(rows[594]) == 2 && std::string_view(catalog::presentation(594).expansion.data()) == "Warmind",
        "A Garden World originated in Osiris despite later Warmind variant subtitle");
    check(model::content_group(rows[229]) == 2, "original strike variant retains its authored Osiris subtitle");
    check(model::content_group(rows[123]) == 9 && model::content_group(rows[222]) == 5,
        "Crown of Sorrow and Festering Core use original release evidence");
    check(model::content_group(rows[27]) == 1, "later Warmind Nightfall subtitle cannot reclassify original Pyramidion");
    check(model::experience_id(rows[229]) == model::experience_id(rows[624])
        && model::experience_id(rows[624]) == model::experience_id(rows[648]), "equivalent Garden World Strikes share a card");
    check(rows[296].name() == "mission_pact" && rows[298].name() == "mission_bond"
        && std::string_view(catalog::presentation(296).title.data()) == "Tree of Probabilities"
        && std::string_view(catalog::presentation(298).title.data()) == "A Garden World", "actual Story identities correct reported ID mismatch");
    check(model::experience_id(rows[298]) != model::experience_id(rows[229])
        && model::experience_id(rows[296]) != model::experience_id(rows[229]), "Story experiences remain separate from same-map Strike");
    check(model::experience_id(rows[604]) != model::experience_id(rows[306]), "Perfected Form Quest differs from Will of Thousands Story");
    check(model::experience_id(rows[342]) == model::experience_id(rows[1076])
        && model::activity_icon(rows[342]) == catalog::Icon::adventure
        && model::activity_icon(rows[1076]) == catalog::Icon::adventure, "Adventure menu/world-selector identities retain one presentation and actual sword");
    for (const auto i : {879, 880, 883, 885, 886}) {
        check(model::activity_icon(rows[i]) >= catalog::Icon::patrolKill
            && model::activity_icon(rows[i]) <= catalog::Icon::patrolAssassinate, "patrol mission gets its native display style icon");
    }
    std::array<bool, catalog::kCapacity> experiences{}; std::size_t experienceCount{}, resolved{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto* rule = catalog::releases::lookup(rows[i]);
        check(rule != nullptr && rule->experience < count, "every grouped member preserves a real native identity");
        check(catalog::releases::lookup_hash(rows[i].hash) == rule, "release lookup uses the exact native hash");
        experiences[rule->experience] = true; resolved += rule->release != model::kUnresolvedContent;
    }
    for (const bool exists : experiences) { experienceCount += exists; }
    check(experienceCount == 629 && resolved == 1141, "audited experiences and original-release coverage");
    check(model::experience_id(rows[110]) != model::experience_id(rows[111]), "blank Adventure names cannot merge unrelated native packages");
    auto incompatible = rows[229]; incompatible.hash ^= 1;
    check(catalog::releases::lookup(incompatible) == nullptr, "release index cannot apply to a different build identity");
    incompatible = rows[229]; incompatible.index = rows[298].index;
    check(catalog::releases::lookup(incompatible) == nullptr, "hash lookup does not reuse version-specific experience anchors at a different ordinal");
    check(catalog::releases::lookup_hash(0) == nullptr && catalog::releases::lookup_hash(0xFFFFFFFFU) == nullptr,
        "unknown activity hashes are not assigned a release");
    auto moved = rows[293]; moved.destination = 33;
    check(model::content_group(moved) == 2, "DLC classification cannot depend on destination family");
    std::array<std::size_t, model::kContent.size()> groups{};
    for (std::size_t i = 0; i < count; ++i) { ++groups[model::content_group(rows[i])]; }
    std::cout << "DLC groups:";
    for (std::size_t i = 1; i < groups.size(); ++i) { std::cout << ' ' << i << '=' << groups[i]; }
    std::cout << '\n';
    check(!catalog::publish_presentations(std::span<const catalog::Presentation>(&catalog::presentation(0), 1)), "metadata immutable");
    // Mutated pairs and spans must fail safely without returning a different bank's text.
    const auto display = load(g_fixture / "81327D35.bin"), types = load(g_fixture / "81327D64.bin");
    static std::array<parser::DisplayRefs, catalog::kCapacity> refs{};
    check(parser::display_refs(display, types, std::span(rows).first(count), refs), "matching public/client identity join");
    auto wrong = rows[299].hash;
    rows[299].hash ^= 1;
    check(!parser::display_refs(display, types, std::span(rows).first(count), refs), "mismatched client/public identity rejected");
    rows[299].hash = wrong;
    check(!parser::display_refs(std::span(display).first(200), types, std::span(rows).first(count), refs), "truncated client table rejected");
    check(parser::display_refs(display, types, std::span(rows).first(count), refs), "restore valid string references");
    catalog::MenuText isolated{};
    const auto globals = load(g_fixture / "globals.bin");
    g_blockedTag = 0x813364A7;
    client::content::activity::build_menu_text(source, scratch, globals, refs, isolated);
    for (std::size_t i = 1; i <= 5; ++i) { check(isolated.content[i].text[0] == 0, "missing campaign bank cannot use compiled title copies"); }
    check(std::string_view(isolated.content[20].text.data()) == "The Dawning", "missing campaign text does not discard independent event text");
    g_blockedTag = 0x81613D02;
    client::content::activity::build_menu_text(source, scratch, globals, refs, isolated);
    for (std::size_t i = 7; i <= 13; ++i) { check(isolated.content[i].text[0] == 0, "missing season definitions cannot use compiled aliases"); }
    g_blockedTag = 0;
    // Alter a copied localization byte only in the fixture reader: the menu must consume it verbatim.
    const auto campaign = load(g_fixture / "813364A7.bin");
    std::uint32_t campaignLanguage{};
    check(parser::read(campaign, 24, campaignLanguage), "native campaign language tag");
    std::array<char, 32> campaignFile{};
    (void)std::snprintf(campaignFile.data(), campaignFile.size(), "%08X.bin", campaignLanguage);
    const auto campaignBytes = load(g_fixture / campaignFile.data());
    middleware::content::packages::tables::Array campaignHashes{}, campaignCombinations{};
    check(middleware::content::packages::tables::find_array_at(campaign, 8, campaignHashes)
        && middleware::content::packages::tables::find_array_at(campaignBytes, 72, campaignCombinations), "native campaign arrays");
    bool mutated{};
    for (std::size_t i = 0; i < campaignHashes.count; ++i) {
        std::uint32_t hash{}; std::size_t first{}, payload{}; std::uint16_t shift{};
        check(parser::read(campaign, campaignHashes.dataOffset + i * 4, hash), "native campaign hash bounds");
        if (hash != 0x269A3283) { continue; }
        check(parser::relative(campaignBytes, campaignCombinations.dataOffset + i * 16, first)
            && parser::relative(campaignBytes, first + 8, payload)
            && parser::read(campaignBytes, first + 24, shift), "native campaign payload bounds");
        g_mutatedTag = campaignLanguage; g_mutatedOffset = payload;
        g_mutatedByte = static_cast<std::byte>(static_cast<unsigned char>('X' - shift)); mutated = true; break;
    }
    check(mutated, "native campaign name located for mutation regression");
    client::content::activity::build_menu_text(source, scratch, globals, refs, isolated);
    check(std::string_view(isolated.content[1].text.data()) == "Xhe Red War", "display source follows runtime bytes rather than English copies");
    g_mutatedTag = 0;
    check(std::string_view(model::library_name(1)) == "The Red War", "isolated failure tests cannot mutate published catalog");
    const auto banks = load(g_fixture / "81A27211.bin");
    middleware::content::packages::tables::Array bankRows{}, hashRows{}, combinations{};
    check(middleware::content::packages::tables::find_array_at(banks, 8, bankRows), "bank index array");
    check(parser::read(banks, bankRows.dataOffset + refs[299].title.bank * 8 + 4, tag), "exact Omega bank tag");
    std::vector<std::byte> container, language;
    check(reader::read_tag(source, scratch, tag, container), "exact Omega container");
    check(parser::read(container, 24, tag) && reader::read_tag(source, scratch, tag, language), "exact English bank");
    std::array<char, 32> text{};
    check(parser::localized_string(container, language, refs[299].title.hash, text)
        && std::string_view(text.data()) == "Omega", "bounded localized decoder");
    check(!parser::localized_string(container, language, 0xFFFFFFFFU, text) && text[0] == 0, "missing hash cannot alias another string");
    check(!parser::localized_string(container, language, refs[299].title.hash, std::span(text).first(4))
        && text[0] == 0, "small output fails without truncated name");
    check(!parser::localized_string(container, std::span(language).first(80), refs[299].title.hash, text), "truncated localized data rejected");
    check(middleware::content::packages::tables::find_array_at(container, 8, hashRows)
        && middleware::content::packages::tables::find_array_at(language, 72, combinations), "localized arrays");
    for (std::size_t i = 0; i < hashRows.count; ++i) {
        std::uint32_t hash{};
        check(parser::read(container, hashRows.dataOffset + i * 4, hash), "localized hash bounded");
        if (hash != refs[299].title.hash) { continue; }
        const auto badParts = (std::numeric_limits<std::int64_t>::max)();
        std::memcpy(language.data() + combinations.dataOffset + i * 16 + 8, &badParts, sizeof(badParts));
        check(!parser::localized_string(container, language, hash, text), "oversized localized part count rejected");
        break;
    }
    client::content::activity::build_artwork(source, scratch);
    const auto images = catalog::artwork();
    std::size_t total{};
    check(images.size() == catalog::kIconCount, "artwork published");
    for (std::size_t i = 1; i < images.size(); ++i) {
        check(!images[i].pixels.empty(), "every sampled native artwork extracted");
        std::vector<std::byte> header, nativePixels; std::uint32_t buffer{};
        check(reader::read_tag(source, scratch, images[i].tag, header, buffer)
            && reader::read_tag(source, scratch, buffer, nativePixels)
            && nativePixels.size() >= images[i].pixels.size()
            && std::equal(images[i].pixels.begin(), images[i].pixels.end(), nativePixels.begin()),
            "every displayed icon pixel is copied exactly from the installed texture payload");
        total += images[i].pixels.size();
    }
    check(images[1].tag == 0x8161309E && images[2].tag == 0x81613079, "colored Red War and Osiris selection shields");
    const auto eventSource = load(g_fixture / "81613CF8.bin");
    std::uint16_t eventIcon{}; std::uint32_t eventBank{}, eventName{};
    check(parser::read(eventSource, 0x9328, eventIcon) && eventIcon == 10586
        && parser::read(eventSource, 0x932C, eventBank) && eventBank == 1957
        && parser::read(eventSource, 0x9330, eventName) && eventName == 0x9554A910,
        "Dawning artwork is joined to the exact named native event source record");
    for (const auto& fixture : {std::pair{"8132CC78.bin", 10541}, std::pair{"8132D32F.bin", 11305},
                               std::pair{"8132D828.bin", 11578}}) {
        check(parser::read(load(g_fixture / fixture.first), 128, eventIcon) && eventIcon == fixture.second,
            "legacy season artwork follows actual installed Hammerhead/Spare Rations/Beloved display icon indices");
    }
    check(images[static_cast<std::size_t>(catalog::Icon::seasonForge)].tag == 0x813185CC
        && images[static_cast<std::size_t>(catalog::Icon::seasonDrifter)].tag == 0x813217A8
        && images[static_cast<std::size_t>(catalog::Icon::seasonOpulence)].tag == 0x813217CA,
        "legacy season logos use their native standalone inventory-background variants");
    for (const auto& expected : {std::pair{catalog::Icon::seasonUndying, 0x813217ECU},
         std::pair{catalog::Icon::seasonDawn, 0x81A2760CU}, std::pair{catalog::Icon::seasonWorthy, 0x81A27409U},
         std::pair{catalog::Icon::seasonArrivals, 0x81A27487U}}) {
        const auto& image = images[static_cast<std::size_t>(expected.first)];
        check(image.tag == expected.second && image.width == 45 && image.height == 45,
            "later seasons use full 45px inventory artwork instead of 32px season-table marks");
    }
    for (const auto icon : {catalog::Icon::seasonUndying, catalog::Icon::seasonDawn, catalog::Icon::seasonWorthy,
         catalog::Icon::seasonArrivals, catalog::Icon::seasonForge, catalog::Icon::seasonDrifter, catalog::Icon::seasonOpulence}) {
        for (const float density : {1.0F, 1.25F, 2.0F}) {
            const auto dimensions = client::ui::mission_launch::art::display_size(icon, 165.0F, density);
            const auto& image = images[static_cast<std::size_t>(icon)];
            check(dimensions.width * density <= image.width && dimensions.height * density <= image.height,
                "season mark never magnifies native pixels at high UI or framebuffer scale");
            check(std::abs(dimensions.width / dimensions.height - static_cast<float>(image.width) / image.height) < 0.0001F,
                "season mark preserves authored aspect ratio");
        }
    }
    for (std::size_t group = 7; group <= 21; ++group) {
        if (group == 18) { continue; }
        check(model::release_icon(group) != catalog::Icon::missing, "every installed season/event group has a real game emblem");
    }
    check(images[static_cast<std::size_t>(catalog::Icon::dawning)].tag == 0x8132D4B0
        && images[static_cast<std::size_t>(catalog::Icon::festival)].tag == 0x8132D6B0,
        "Dawning snowflake and Festival eye follow named native event/quest records");
    check(images[static_cast<std::size_t>(catalog::Icon::revelry)].tag == 0x8132D914,
        "original Revelry flower does not reuse the later Guardian Games activity watermark");
    for (const auto i : {2308, 2314, 2351, 2389}) {
        const auto seasons = load(g_fixture / "81613D02.bin"); bool found{};
        for (std::size_t season = 0; season < 12; ++season) {
            std::uint16_t icon{};
            check(parser::read(seasons, 48 + season * 72 + 32, icon), "bounded native season icon field");
            found |= icon == i;
        }
        check(found, "season logo index is authored in the installed season definition");
    }
    check(model::library_group(rows[145]) == 14 && model::content_group(rows[145]) == 17,
        "later Solstice keeps original release while sharing the recurring event card");
    check(model::content_group(rows[509]) == 21 && model::library_kind(21) == 3,
        "native Iron Banner playlist belongs to its recurring event");
    check(!model::library_card_visible(18) && !model::library_card_visible(21),
        "Beyond Light preview and Iron Banner cards are hidden by launcher preference");
    check(model::content_group(rows[264]) == 18 && !model::content_visible(model::content_group(rows[264])),
        "Beyond Light advertising identity remains in the catalog but is hidden in the launcher");
    for (const auto index : {509, 589, 590, 591}) {
        check(rows[index].nativeType == 21 && model::content_group(rows[index]) == 21
            && !model::content_visible(model::content_group(rows[index])),
            "all four Iron Banner native identities remain intact but hidden in launcher presentation");
    }
    for (std::size_t group = 1; group <= 6; ++group) {
        check(model::library_card_visible(group), "existing expansion and campaign cards remain visible");
    }
    for (const auto& row : std::span(rows).first(count)) {
        check(model::content_group(row) != 20, "Dawning card does not fabricate a standalone launch activity");
    }
    for (int generation = 0; generation < 3; ++generation) {
        ID3D11Device* device{};
        D3D_FEATURE_LEVEL feature{};
        check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &device, &feature, nullptr)), "isolated WARP D3D11 device");
        art::prepare(device);
        check(images[0].pixels.empty() && art::texture(catalog::Icon::missing) == 0, "missing native icon has no generated substitute");
        for (std::size_t i = 1; i < images.size(); ++i) {
            const auto handle = art::texture(static_cast<catalog::Icon>(i));
            check(handle != 0, "native texture uploaded on every device generation");
            art::prepare(device);
            check(art::texture(static_cast<catalog::Icon>(i)) == handle, "same device reuses texture");
        }
        art::release();
        check(art::texture(catalog::Icon::osiris) == 0, "release clears published UI handles");
        check(device->Release() == 0, "all device and texture COM references released");
    }
    std::cout << "PASS: " << named << " titles, " << described << " descriptions, " << typed
        << " type labels; 40 native icons (" << total << " bytes); malformed joins; 3 WARP device lifetimes\n";
    return 0;
}
