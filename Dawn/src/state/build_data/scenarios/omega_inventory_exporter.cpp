#include "omega_inventory_exporter.h"

#include <Windows.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/definition_index_table.h"
#include "omega_schema_catalog.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace dawn::state::build_data::scenarios {
namespace {

namespace path = core::path;
namespace tables = middleware::content::packages::tables;

constexpr std::string_view kOmegaName = "mission_scot";
constexpr std::array<std::uint32_t, 6> kOmegaKeys = {
    0x4786C0E0U, 0x29D7B029U, 0x82FB58B7U, 0xBA5F26EFU, 0xD00142CFU, 0xF7A6CE7FU};
constexpr std::size_t kExpectedSlotCount = 57;
constexpr std::wstring_view kExportDirectorySuffix = L"\\exports";
constexpr std::wstring_view kJsonFileSuffix = L"\\omega_inventory.json";
constexpr std::wstring_view kMarkdownFileSuffix = L"\\omega_inventory.md";

SRWLOCK g_exportLock = SRWLOCK_INIT;

struct Alias {
    const char* symbol{};
    const char* confidence{"generated"};
};

struct Scope {
    bool topLevel{};
    int topOrdinal{-1};
    std::array<std::uint64_t, kDestinationBubbleGroupCapacity> bubbleMasks{};
    std::size_t bubbleMaskCount{};
    std::array<std::uint8_t, kBubbleCapacity> authoredSlices{};
    std::size_t authoredSliceCount{};
};

void append_format(std::string& output, const char* format, ...) {
    std::array<char, 2048> buffer{};
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    va_end(arguments);
    if (written <= 0) {
        return;
    }
    const std::size_t count =
        static_cast<std::size_t>(written) < buffer.size() ? static_cast<std::size_t>(written)
                                                         : buffer.size() - 1;
    output.append(buffer.data(), count);
}

void append_schema_value(std::string& output, bool available, std::uint32_t value) {
    if (available) {
        append_format(output, "\"0x%08X\"", value);
    } else {
        output += "null";
    }
}

[[nodiscard]] std::string_view name_of(const Definition& definition) noexcept {
    return {definition.name.data(), definition.nameLength};
}

[[nodiscard]] const Definition*
find_omega(std::span<const Definition> definitions) noexcept {
    for (const Definition& definition : definitions) {
        if (name_of(definition) == kOmegaName) {
            return &definition;
        }
    }
    return nullptr;
}

[[nodiscard]] bool index_has_key(std::span<const RosterGroup> groups,
                                 std::uint16_t index,
                                 std::uint32_t key) noexcept {
    return index < groups.size() && groups[index].registryKey == key;
}

[[nodiscard]] std::size_t find_group_index(const Definition& omega,
                                           std::span<const RosterGroup> groups,
                                           std::uint32_t key) noexcept {
    for (std::size_t index = 0; index < omega.rosterGroupCount; ++index) {
        if (index_has_key(groups, omega.rosterGroups[index], key)) {
            return omega.rosterGroups[index];
        }
    }
    for (std::size_t index = 0; index < omega.bubbleGroupCount; ++index) {
        if (index_has_key(groups, omega.bubbleGroups[index], key)) {
            return omega.bubbleGroups[index];
        }
    }
    for (std::size_t slice = 0; slice < omega.authoredGroups.size(); ++slice) {
        for (std::size_t index = 0; index < omega.authoredGroupCounts[slice]; ++index) {
            if (index_has_key(groups, omega.authoredGroups[slice][index], key)) {
                return omega.authoredGroups[slice][index];
            }
        }
    }
    return groups.size();
}

[[nodiscard]] Scope scope_of(const Definition& omega, std::uint16_t groupIndex) noexcept {
    Scope scope{};
    for (std::size_t index = 0; index < omega.rosterGroupCount; ++index) {
        if (omega.rosterGroups[index] == groupIndex) {
            scope.topLevel = true;
            scope.topOrdinal = static_cast<int>(index);
        }
    }
    for (std::size_t index = 0; index < omega.bubbleGroupCount; ++index) {
        if (omega.bubbleGroups[index] == groupIndex
            && scope.bubbleMaskCount < scope.bubbleMasks.size()) {
            scope.bubbleMasks[scope.bubbleMaskCount++] = omega.bubbleGroupMasks[index];
        }
    }
    for (std::size_t slice = 0; slice < omega.authoredGroups.size(); ++slice) {
        for (std::size_t index = 0; index < omega.authoredGroupCounts[slice]; ++index) {
            if (omega.authoredGroups[slice][index] == groupIndex
                && scope.authoredSliceCount < scope.authoredSlices.size()) {
                scope.authoredSlices[scope.authoredSliceCount++] =
                    static_cast<std::uint8_t>(slice);
                break;
            }
        }
    }
    return scope;
}

[[nodiscard]] const char* type_label(std::uint8_t type) noexcept {
    switch (type) {
    case 1:
        return "squad_spawner";
    case 4:
        return "device";
    case 11:
        return "dialogue";
    case 18:
        return "activity_script";
    case 19:
        return "navigation";
    case 23:
        return "gate";
    case 30:
        return "trigger_volume";
    case 35:
        return "mission_director";
    case 43:
        return "scene";
    case 53:
        return "music";
    case 60:
        return "volume";
    case 68:
        return "directive_objective";
    case 70:
        return "engagement_sensor";
    default:
        return "unclassified";
    }
}

[[nodiscard]] Alias recovered_alias(std::uint32_t key, std::uint16_t index) noexcept {
    if (key == 0x4786C0E0U && index == 1) {
        return {"omega.mission_director", "recovered"};
    }
    if (key == 0x4786C0E0U && index == 2) {
        return {"omega.activity_script_manager", "recovered"};
    }
    if (key == 0x82FB58B7U && index == 0) {
        return {"omega.directive_objective_sensor", "recovered"};
    }
    if (key == 0x82FB58B7U && index == 1) {
        return {"omega.dialogue_sensor", "recovered"};
    }
    if (key == 0x82FB58B7U && index == 2) {
        return {"omega.music_sensor", "recovered"};
    }
    if (key == 0xBA5F26EFU && index == 0) {
        return {"omega.lighthouse_teleport", "recovered"};
    }
    if (key == 0xBA5F26EFU && index == 1) {
        return {"omega.infinite_forest_gate", "recovered"};
    }
    if (key == 0xBA5F26EFU && index == 2) {
        return {"omega.engagement_sensor", "recovered"};
    }
    if (key == 0xD00142CFU && index == 0) {
        return {"omega.ikora_spawner", "recovered"};
    }
    if (key == 0xD00142CFU && index == 1) {
        return {"omega.opening_scene", "recovered"};
    }
    if (key == 0xD00142CFU && index == 20) {
        return {"omega.opening_trigger_volume", "recovered"};
    }
    if (key == 0xD00142CFU && index == 24) {
        return {"omega.forest_entrance_monitor", "recovered"};
    }
    return {};
}

[[nodiscard]] std::string generated_symbol(std::uint32_t key,
                                           std::uint8_t type,
                                           std::uint16_t index) {
    std::array<char, 96> value{};
    const int written = std::snprintf(value.data(),
                                      value.size(),
                                      "omega.component.%08X.type_%u.slot_%u",
                                      key,
                                      static_cast<unsigned>(type),
                                      static_cast<unsigned>(index));
    return written > 0 ? std::string(value.data(), static_cast<std::size_t>(written))
                       : std::string("omega.component.unknown");
}

void append_bubble_ordinals(std::string& output, std::uint64_t mask) {
    bool first = true;
    for (std::size_t bit = 0; bit < kBubbleCapacity; ++bit) {
        if ((mask & (std::uint64_t{1} << bit)) == 0) {
            continue;
        }
        append_format(output, "%s%zu", first ? "" : ", ", bit);
        first = false;
    }
}

void append_json_scope(std::string& output, const Scope& scope) {
    append_format(output,
                  "        \"publication\": {\"top_level\": %s, \"top_ordinal\": ",
                  scope.topLevel ? "true" : "false");
    if (scope.topOrdinal >= 0) {
        append_format(output, "%d", scope.topOrdinal);
    } else {
        output += "null";
    }
    output += ", \"bubble_masks\": [";
    for (std::size_t index = 0; index < scope.bubbleMaskCount; ++index) {
        append_format(output,
                      "%s\"0x%016llX\"",
                      index == 0 ? "" : ", ",
                      static_cast<unsigned long long>(scope.bubbleMasks[index]));
    }
    output += "], \"bubble_ordinals\": [";
    bool firstBubble = true;
    for (std::size_t index = 0; index < scope.bubbleMaskCount; ++index) {
        const std::uint64_t mask = scope.bubbleMasks[index];
        for (std::size_t bit = 0; bit < kBubbleCapacity; ++bit) {
            if ((mask & (std::uint64_t{1} << bit)) == 0) {
                continue;
            }
            append_format(output, "%s%zu", firstBubble ? "" : ", ", bit);
            firstBubble = false;
        }
    }
    output += "], \"authored_slices\": [";
    for (std::size_t index = 0; index < scope.authoredSliceCount; ++index) {
        append_format(output,
                      "%s%u",
                      index == 0 ? "" : ", ",
                      static_cast<unsigned>(scope.authoredSlices[index]));
    }
    output += "]},\n";
}

[[nodiscard]] std::string scope_text(const Scope& scope) {
    std::string output;
    if (scope.topLevel) {
        append_format(output, "top:%d", scope.topOrdinal);
    }
    for (std::size_t index = 0; index < scope.bubbleMaskCount; ++index) {
        if (!output.empty()) {
            output += "; ";
        }
        output += "bubble:";
        append_bubble_ordinals(output, scope.bubbleMasks[index]);
    }
    if (scope.authoredSliceCount != 0) {
        if (!output.empty()) {
            output += "; ";
        }
        output += "authored:";
        for (std::size_t index = 0; index < scope.authoredSliceCount; ++index) {
            append_format(output,
                          "%s%u",
                          index == 0 ? "" : ",",
                          static_cast<unsigned>(scope.authoredSlices[index]));
        }
    }
    return output.empty() ? "unreferenced" : output;
}

[[nodiscard]] bool ensure_export_directory(path::Buffer& directory) noexcept {
    if (!path::artifact_directory(reinterpret_cast<void*>(&__ImageBase), directory)
        || !path::append(directory, kExportDirectorySuffix)) {
        return false;
    }
    if (CreateDirectoryW(directory.chars.data(), nullptr) != FALSE) {
        return true;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(directory.chars.data());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

[[nodiscard]] bool write_all(HANDLE file, std::string_view text) noexcept {
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t remaining = text.size() - offset;
        const DWORD requested =
            remaining > MAXDWORD ? MAXDWORD : static_cast<DWORD>(remaining);
        DWORD written = 0;
        if (WriteFile(file, text.data() + offset, requested, &written, nullptr) == FALSE
            || written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

[[nodiscard]] bool write_atomic(const path::Buffer& destination,
                                std::string_view text) noexcept {
    path::Buffer stage = destination;
    if (!path::append(stage, L".tmp")) {
        return false;
    }
    const HANDLE file = CreateFileW(stage.chars.data(),
                                    GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool complete = write_all(file, text) && FlushFileBuffers(file) != FALSE;
    complete = CloseHandle(file) != FALSE && complete;
    if (complete) {
        complete = MoveFileExW(stage.chars.data(),
                               destination.chars.data(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
                   != FALSE;
    }
    if (!complete) {
        DeleteFileW(stage.chars.data());
    }
    return complete;
}

void report(const char* result,
            std::size_t groups,
            std::size_t slots,
            bool complete) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=omega_inventory result=%s groups=%zu slots=%zu complete=%u "
                                      "authority=unchanged",
                                      result,
                                      groups,
                                      slots,
                                      complete ? 1U : 0U);
    if (written > 0) {
        core::log::write(core::log::Channel::state,
                         result == std::string_view{"ok"} ? core::log::Level::info
                                                          : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

} // namespace

void export_omega_inventory(std::span<const Definition> definitions,
                            std::span<const RosterGroup> groups) noexcept {
    const Definition* omega = find_omega(definitions);
    if (omega == nullptr) {
        return;
    }

    AcquireSRWLockExclusive(&g_exportLock);
    std::array<std::size_t, kOmegaKeys.size()> groupIndices{};
    std::size_t foundCount = 0;
    std::size_t slotCount = 0;
    for (std::size_t index = 0; index < kOmegaKeys.size(); ++index) {
        groupIndices[index] = find_group_index(*omega, groups, kOmegaKeys[index]);
        if (groupIndices[index] < groups.size()) {
            ++foundCount;
            slotCount += groups[groupIndices[index]].slotCount;
        }
    }
    const bool complete = foundCount == kOmegaKeys.size() && slotCount == kExpectedSlotCount;
    const bool schemasReady = prepare_omega_schemas(groups, groupIndices);
    const std::size_t schemaCount = omega_schema_count();

    std::string json;
    json.reserve(64 * 1024);
    append_format(json,
                  "{\n  \"format_version\": 3,\n  \"activity\": \"mission_scot\",\n  "
                  "\"authority_behavior\": \"read_only\",\n  \"complete\": %s,\n  "
                  "\"expected_group_count\": %zu,\n  \"found_group_count\": %zu,\n  "
                  "\"expected_slot_count\": %zu,\n  \"found_slot_count\": %zu,\n  "
                  "\"raw_schema_complete\": %s,\n  \"raw_schema_count\": %zu,\n  "
                  "\"schema_metadata\": \"raw descriptor hashes come from the validated Omega "
                  "schema sidecar; null means the sidecar was unavailable\",\n  "
                  "\"bubble_count\": %u,\n  \"bubbles\": [\n",
                  complete ? "true" : "false",
                  kOmegaKeys.size(),
                  foundCount,
                  kExpectedSlotCount,
                  slotCount,
                  schemasReady ? "true" : "false",
                  schemaCount,
                  static_cast<unsigned>(omega->bubbleCount));

    for (std::size_t bubble = 0; bubble < omega->bubbleCount; ++bubble) {
        const std::uint32_t region = static_cast<std::uint32_t>(bubble * 8U);
        const std::uint8_t stateCount = omega->bubbleStateCounts[bubble];
        append_format(json,
                      "%s    {\"ordinal\": %zu, \"region_index\": %u, "
                      "\"name_hash\": \"0x%08X\", \"map_index\": %u, \"enabled\": %s, "
                      "\"state_byte\": \"0x%02X\", \"slice_set_count\": %u, "
                      "\"slice_set_first\": %u, \"slice_set_last\": ",
                      bubble == 0 ? "" : ",\n",
                      bubble,
                      region,
                      omega->bubbleHashes[bubble],
                      static_cast<unsigned>(omega->bubbleMapIndices[bubble]),
                      omega->bubbleStates[bubble] == kBubbleEnabledByte ? "true" : "false",
                      static_cast<unsigned>(omega->bubbleStates[bubble]),
                      static_cast<unsigned>(stateCount),
                      region);
        if (stateCount == 0) {
            json += "null}";
        } else {
            append_format(json, "%u}", region + stateCount - 1U);
        }
    }
    json += "\n  ],\n  \"groups\": [\n";

    std::string markdown;
    markdown.reserve(24 * 1024);
    markdown += "# Omega authored-component inventory\n\n";
    markdown += "Generated from the published `mission_scot` scenario catalog. This exporter is "
                "read-only and does not send activity messages or change authority.\n\n";
    append_format(markdown,
                  "- Complete: **%s**\n- Groups: **%zu / %zu**\n- Slots: **%zu / %zu**\n\n",
                  complete ? "yes" : "no",
                  foundCount,
                  kOmegaKeys.size(),
                  slotCount,
                  kExpectedSlotCount);
    append_format(markdown,
                  "- Raw schemas: **%s (%zu / %zu)**\n\n",
                  schemasReady ? "complete" : "unavailable",
                  schemaCount,
                  kExpectedSlotCount);
    markdown += "Raw component/auth/sense hashes come from package descriptors and are accepted "
                "only when their object tags and all 57 real slots match the current roster.\n\n";
    markdown += "## Bubble topology\n\n";
    markdown += "A bubble's membership region is its ordinal multiplied by eight. Its authored "
                "slice sets begin at that region index.\n\n";
    markdown += "| Bubble | Region | Name hash | Map index | Enabled | State byte | Slice sets |\n";
    markdown += "|---:|---:|---:|---:|---|---:|---|\n";
    for (std::size_t bubble = 0; bubble < omega->bubbleCount; ++bubble) {
        const std::uint32_t region = static_cast<std::uint32_t>(bubble * 8U);
        const std::uint8_t stateCount = omega->bubbleStateCounts[bubble];
        std::array<char, 32> range{};
        if (stateCount == 0) {
            std::snprintf(range.data(), range.size(), "none");
        } else if (stateCount == 1) {
            std::snprintf(range.data(), range.size(), "%u", region);
        } else {
            std::snprintf(range.data(), range.size(), "%u-%u", region, region + stateCount - 1U);
        }
        append_format(markdown,
                      "| %zu | %u | `0x%08X` | %u | %s | `0x%02X` | %s |\n",
                      bubble,
                      region,
                      omega->bubbleHashes[bubble],
                      static_cast<unsigned>(omega->bubbleMapIndices[bubble]),
                      omega->bubbleStates[bubble] == kBubbleEnabledByte ? "yes" : "no",
                      static_cast<unsigned>(omega->bubbleStates[bubble]),
                      range.data());
    }
    markdown += "\n## Authored components\n\n";
    markdown += "| Symbol | Group | Type | Real index | Component | Auth schema | Sense schema | Publication |\n";
    markdown += "|---|---:|---:|---:|---:|---:|---:|---|\n";

    bool firstGroup = true;
    for (std::size_t expected = 0; expected < kOmegaKeys.size(); ++expected) {
        const std::size_t tableIndex = groupIndices[expected];
        if (tableIndex >= groups.size()) {
            continue;
        }
        const RosterGroup& group = groups[tableIndex];
        const Scope scope = scope_of(*omega, static_cast<std::uint16_t>(tableIndex));
        const std::uint16_t package = tables::package_of(group.objectTag);
        append_format(json,
                      "%s    {\n      \"registry_key\": \"0x%08X\",\n      "
                      "\"table_index\": %zu,\n      \"object_tag\": \"0x%08X\",\n      "
                      "\"package_id\": ",
                      firstGroup ? "" : ",\n",
                      group.registryKey,
                      tableIndex,
                      group.objectTag);
        if (package == tables::kAbsentPackageId) {
            json += "null,\n";
        } else {
            append_format(json, "\"0x%04X\",\n", package);
        }
        append_format(json, "      \"slot_count\": %u,\n", group.slotCount);
        append_json_scope(json, scope);
        json += "      \"slots\": [\n";
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            const std::uint8_t type = group.slotTypes[slot];
            const std::uint8_t flags = group.slotFlags[slot];
            const std::uint16_t realIndex = group.slotIndices[slot];
            OmegaSchema rawSchema{};
            const bool rawAvailable =
                find_omega_schema(group.registryKey, type, realIndex, rawSchema);
            const Alias alias = recovered_alias(group.registryKey, realIndex);
            const std::string generated =
                alias.symbol == nullptr ? generated_symbol(group.registryKey, type, realIndex)
                                        : std::string{};
            const char* symbol = alias.symbol == nullptr ? generated.c_str() : alias.symbol;
            append_format(json,
                          "%s        {\"symbol\": \"%s\", \"alias_confidence\": \"%s\", "
                          "\"type\": %u, \"type_label\": \"%s\", \"real_index\": %u, "
                          "\"flags\": \"0x%02X\", \"auth_schema_present\": %s, "
                          "\"sense_schema_present\": %s, \"schema_source\": \"%s\", "
                          "\"component_class\": ",
                          slot == 0 ? "" : ",\n",
                          symbol,
                          alias.confidence,
                          static_cast<unsigned>(type),
                          type_label(type),
                          static_cast<unsigned>(realIndex),
                          static_cast<unsigned>(flags),
                          (flags & kSlotAuthFlag) != 0 ? "true" : "false",
                          (flags & kSlotSenseFlag) != 0 ? "true" : "false",
                          rawAvailable ? "package_descriptor" : "unavailable");
            append_schema_value(json, rawAvailable, rawSchema.componentClass);
            json += ", \"auth_schema\": ";
            append_schema_value(json, rawAvailable, rawSchema.authSchema);
            json += ", \"sense_schema\": ";
            append_schema_value(json, rawAvailable, rawSchema.senseSchema);
            json += "}";
            const std::string publication = scope_text(scope);
            std::array<char, 16> componentText{};
            std::array<char, 16> authText{};
            std::array<char, 16> senseText{};
            if (rawAvailable) {
                std::snprintf(componentText.data(), componentText.size(), "0x%08X", rawSchema.componentClass);
                std::snprintf(authText.data(), authText.size(), "0x%08X", rawSchema.authSchema);
                std::snprintf(senseText.data(), senseText.size(), "0x%08X", rawSchema.senseSchema);
            } else {
                std::snprintf(componentText.data(), componentText.size(), "unavailable");
                std::snprintf(authText.data(), authText.size(), "unavailable");
                std::snprintf(senseText.data(), senseText.size(), "unavailable");
            }
            append_format(markdown,
                          "| `%s` | `0x%08X` | %u (`%s`) | %u | `%s` | `%s` | `%s` | %s |\n",
                          symbol,
                          group.registryKey,
                          static_cast<unsigned>(type),
                          type_label(type),
                          static_cast<unsigned>(realIndex),
                          componentText.data(),
                          authText.data(),
                          senseText.data(),
                          publication.c_str());
        }
        json += "\n      ]\n    }";
        firstGroup = false;
    }
    json += "\n  ],\n  \"missing_group_keys\": [";
    bool firstMissing = true;
    for (std::size_t index = 0; index < kOmegaKeys.size(); ++index) {
        if (groupIndices[index] < groups.size()) {
            continue;
        }
        append_format(json,
                      "%s\"0x%08X\"",
                      firstMissing ? "" : ", ",
                      kOmegaKeys[index]);
        firstMissing = false;
    }
    json += "]\n}\n";

    path::Buffer directory{};
    path::Buffer jsonPath{};
    path::Buffer markdownPath{};
    bool written = ensure_export_directory(directory);
    if (written) {
        jsonPath = directory;
        markdownPath = directory;
        written = path::append(jsonPath, kJsonFileSuffix)
                  && path::append(markdownPath, kMarkdownFileSuffix)
                  && write_atomic(jsonPath, json) && write_atomic(markdownPath, markdown);
    }
    report(written ? "ok" : "write_failed", foundCount, slotCount, complete);
    ReleaseSRWLockExclusive(&g_exportLock);
}

} // namespace dawn::state::build_data::scenarios
