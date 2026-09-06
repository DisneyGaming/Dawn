#include "cue_graph_manifest_exporter.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <string>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/slot_descriptor_reader.h"
#include "cue_graph_manifest.h"
#include "scenario_catalog.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace sunrise::state::build_data::scenarios {
namespace {

namespace path = core::path;
namespace tables = middleware::content::packages::tables;

constexpr std::string_view kTowerfallName = "mission_towerfall";
constexpr std::wstring_view kExportDirectorySuffix = L"\\exports";
constexpr std::wstring_view kManifestFileSuffix = L"\\towerfall_cue_manifest.json";
constexpr std::wstring_view kEdgeFileSuffix = L"\\towerfall_cue_edges.md";
constexpr std::wstring_view kMappingFileSuffix = L"\\towerfall_cue_mapping.json";
SRWLOCK g_exportLock = SRWLOCK_INIT;

void append_format(std::string& output, const char* format, ...) {
    std::array<char, 2048> buffer{};
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    va_end(arguments);
    if (written <= 0) {
        return;
    }
    output.append(buffer.data(),
                  (std::min)(static_cast<std::size_t>(written), buffer.size() - 1U));
}

[[nodiscard]] std::string_view name_of(const Definition& definition) noexcept {
    return {definition.name.data(), definition.nameLength};
}

[[nodiscard]] const Definition*
find_definition(std::span<const Definition> definitions, std::string_view name) noexcept {
    for (const Definition& definition : definitions) {
        if (name_of(definition) == name) {
            return &definition;
        }
    }
    return nullptr;
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
        const DWORD requested = static_cast<DWORD>(
            (std::min)(text.size() - offset, static_cast<std::size_t>(MAXDWORD)));
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

[[nodiscard]] bool output_path(std::wstring_view suffix, path::Buffer& output) noexcept {
    path::Buffer directory{};
    if (!ensure_export_directory(directory)) {
        return false;
    }
    output = directory;
    return path::append(output, suffix);
}

[[nodiscard]] const char* type_role(std::uint8_t type) noexcept {
    switch (type) {
    case 1: return "action_spawner";
    case 11: return "action_dialogue";
    case 18: return "action_script";
    case 23: return "trigger_gate";
    case 30: return "trigger_volume";
    case 35: return "action_director";
    case 43: return "action_scene";
    case 53: return "action_music";
    case 60: return "trigger_volume";
    case 68: return "action_directive";
    case 70: return "trigger_monitor";
    default: return "unclassified";
    }
}

void report_export(const char* stage,
                   bool ok,
                   std::size_t groups,
                   std::size_t nodes) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=cue_manifest stage=%s result=%s activity=mission_towerfall groups=%zu nodes=%zu authority=unchanged",
                                      stage,
                                      ok ? "ok" : "failed",
                                      groups,
                                      nodes);
    if (written > 0) {
        core::log::write(core::log::Channel::state,
                         ok ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

} // namespace

void export_towerfall_cue_manifest(std::span<const Definition> definitions,
                                  std::span<const RosterGroup> groups) noexcept {
    const Definition* definition = find_definition(definitions, kTowerfallName);
    if (definition == nullptr) {
        return;
    }
    AcquireSRWLockExclusive(&g_exportLock);
    const ManifestGroups selected = manifest_groups(*definition);
    const ManifestEdgeReport edges = inspect_manifest_edges(*definition, groups);
    std::string json;
    json.reserve(256 * 1024);
    append_format(json,
                  "{\n  \"format_version\": 1,\n  \"activity\": \"mission_towerfall\",\n  \"authority_behavior\": \"read_only\",\n  \"group_count\": %zu,\n  \"node_count\": %zu,\n  \"groups\": [\n",
                  selected.count,
                  edges.structuralContainmentEdges);
    bool firstGroup = true;
    for (std::size_t ordinal = 0; ordinal < selected.count; ++ordinal) {
        const std::size_t tableIndex = selected.indices[ordinal];
        if (tableIndex >= groups.size()) {
            continue;
        }
        const RosterGroup& group = groups[tableIndex];
        append_format(json,
                      "%s    {\"table_index\": %zu, \"registry_key\": \"0x%08X\", \"object_tag\": \"0x%08X\", \"slots\": [\n",
                      firstGroup ? "" : ",\n",
                      tableIndex,
                      group.registryKey,
                      group.objectTag);
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            append_format(json,
                          "%s      {\"type\": %u, \"index\": %u, \"role\": \"%s\", \"flags\": %u, \"descriptor_tag\": \"0x%08X\", \"descriptor_offset\": %u, \"component_class\": \"0x%08X\", \"sense_schema\": \"0x%08X\", \"auth_schema\": \"0x%08X\"}",
                          slot == 0 ? "" : ",\n",
                          static_cast<unsigned>(group.slotTypes[slot]),
                          static_cast<unsigned>(group.slotIndices[slot]),
                          type_role(group.slotTypes[slot]),
                          static_cast<unsigned>(group.slotFlags[slot]),
                          group.descriptorTags[slot],
                          group.descriptorOffsets[slot],
                          group.componentClasses[slot],
                          group.senseSchemas[slot],
                          group.authSchemas[slot]);
        }
        json += "\n    ]}";
        firstGroup = false;
    }
    append_format(json,
                  "\n  ],\n  \"edge_evidence\": {\"structural_containment\": %zu, \"trigger_nodes\": %zu, \"action_nodes\": %zu, \"co_resident_trigger_action_pairs\": %zu, \"explicit_successor_edges\": %zu, \"opening_successors_recoverable\": %s}\n}\n",
                  edges.structuralContainmentEdges,
                  edges.triggerNodes,
                  edges.actionNodes,
                  edges.coResidentTriggerActionPairs,
                  edges.explicitSuccessorEdges,
                  edges.openingSuccessorsRecoverable ? "true" : "false");

    std::string markdown;
    markdown.reserve(4096);
    markdown += "# Towerfall cue-edge evidence\n\n";
    append_format(markdown,
                  "- Manifest groups: **%zu**\n- Manifest nodes: **%zu**\n- Trigger-capable nodes: **%zu**\n- Action-capable nodes: **%zu**\n- Co-resident trigger/action pairs: **%zu**\n- Explicit successor edges: **%zu**\n\n",
                  selected.count,
                  edges.structuralContainmentEdges,
                  edges.triggerNodes,
                  edges.actionNodes,
                  edges.coResidentTriggerActionPairs,
                  edges.explicitSuccessorEdges);
    markdown += "## Verdict\n\n";
    markdown += "The decoded package records prove mission/group/slot containment and preserve each slot's component, sense, and authority schemas. They do not yet expose a typed successor-reference field. Trigger and action slots that share a group are candidates only; co-residence is not enough evidence to publish an activation edge. Towerfall's opening successors are therefore **not yet recoverable from the decoded package fields**. The next extraction target is the schema/default body behind its monitor and scene descriptors.\n";

    path::Buffer manifestPath{};
    path::Buffer edgePath{};
    const bool paths = output_path(kManifestFileSuffix, manifestPath)
                       && output_path(kEdgeFileSuffix, edgePath);
    const bool written = paths && write_atomic(manifestPath, json) && write_atomic(edgePath, markdown);
    report_export("export", written, selected.count, edges.structuralContainmentEdges);
    ReleaseSRWLockExclusive(&g_exportLock);
}

void export_cue_observation_mapping(
    std::string_view activity,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept {
    if (activity != kTowerfallName) {
        return;
    }
    ObservationMappingReport mapping{};
    if (!map_published_observations(activity, update, mapping)) {
        return;
    }
    std::string json;
    json.reserve(4096);
    append_format(json,
                  "{\n  \"format_version\": 1,\n  \"activity\": \"mission_towerfall\",\n  \"observed_objects\": %zu,\n  \"mapped_objects\": %zu,\n  \"missing_objects\": %zu,\n  \"ambiguous_objects\": %zu,\n  \"complete\": %s,\n  \"unresolved\": [",
                  mapping.observedObjects,
                  mapping.mappedObjects,
                  mapping.missingObjects,
                  mapping.ambiguousObjects,
                  mapping.mappedObjects == mapping.observedObjects ? "true" : "false");
    for (std::size_t index = 0; index < mapping.missingNodeCount; ++index) {
        const CueNodeId node = mapping.missingNodes[index];
        append_format(json,
                      "%s\n    {\"registry_key\": \"0x%08X\", \"type\": %u, \"index\": %u}",
                      index == 0 ? "" : ",",
                      node.registryKey,
                      static_cast<unsigned>(node.slotType),
                      static_cast<unsigned>(node.slotIndex));
    }
    json += mapping.missingNodeCount == 0 ? "],\n  \"objects\": [" : "\n  ],\n  \"objects\": [";
    for (std::size_t index = 0; index < update.objectCount; ++index) {
        const auto& object = update.objects[index];
        append_format(
            json,
            "%s\n    {\"registry_key\": \"0x%08X\", \"type\": %u, \"index\": %u, \"body_bits\": %u, \"set_bits\": %u, \"body_hash\": \"0x%016llX\", \"body_words\": [\"0x%016llX\", \"0x%016llX\", \"0x%016llX\", \"0x%016llX\"], \"inferred_width\": %s}",
            index == 0 ? "" : ",",
            object.registryKey,
            static_cast<unsigned>(object.slotType),
            static_cast<unsigned>(object.slotIndex),
            object.bodyBits,
            static_cast<unsigned>(object.bodySetBitCount),
            static_cast<unsigned long long>(object.bodyHash),
            static_cast<unsigned long long>(object.bodyFirst),
            static_cast<unsigned long long>(object.bodySecond),
            static_cast<unsigned long long>(object.bodyThird),
            static_cast<unsigned long long>(object.bodyFourth),
            object.inferredBodyWidth ? "true" : "false");
    }
    json += update.objectCount == 0 ? "]\n}\n" : "\n  ]\n}\n";
    AcquireSRWLockExclusive(&g_exportLock);
    path::Buffer mappingPath{};
    const bool written = output_path(kMappingFileSuffix, mappingPath)
                         && write_atomic(mappingPath, json);
    std::array<char, core::log::kLineCapacity> line{};
    const int lineLength = std::snprintf(
        line.data(),
        line.size(),
        "ev=cue_manifest stage=observation_map result=%s activity=mission_towerfall observed=%zu mapped=%zu missing=%zu ambiguous=%zu authority=unchanged",
        written ? "ok" : "failed",
        mapping.observedObjects,
        mapping.mappedObjects,
        mapping.missingObjects,
        mapping.ambiguousObjects);
    if (lineLength > 0) {
        core::log::write(core::log::Channel::state,
                         written ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(lineLength)});
    }
    ReleaseSRWLockExclusive(&g_exportLock);
}

} // namespace sunrise::state::build_data::scenarios
