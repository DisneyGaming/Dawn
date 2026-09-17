#include "towerfall_cue_source_probe.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/definition_index_table.h"
#include "../../../middleware/content/packages/tables/scenario_reader.h"
#include "../../../middleware/content/packages/tables/slot_descriptor_reader.h"
#include "../../../state/build_data/scenarios/cue_graph_manifest.h"
#include "../../../state/build_data/scenarios/cue_table_decoder.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace dawn::client::content::scenarios {
namespace {

namespace layouts = state::build_data::scenarios;
namespace path = core::path;
namespace reader = middleware::content::packages::reader;
namespace tables = middleware::content::packages::tables;

constexpr std::string_view kTowerfallName = "mission_towerfall";
constexpr std::wstring_view kAnalysisSuffix = L"\\analysis";
constexpr std::wstring_view kExportsSuffix = L"\\exports";
constexpr std::wstring_view kReportSuffix = L"\\towerfall_cue_sources.json";
// Type-43 descriptors name their scene definition directly at this measured offset. The
// definition can live in a package that is not otherwise named by the activity destination.
constexpr std::size_t kSceneDefinitionTagOffset = 0x60;
constexpr std::size_t kSchemaCapacity = 32;
constexpr std::size_t kSchemaTagCapacity = 256;

struct SchemaTarget final {
    std::uint32_t id{};
    std::uint32_t typeMask{};
    bool sense{};
    bool auth{};
};

struct SchemaTags final {
    std::array<std::uint32_t, kSchemaTagCapacity> tags{};
    std::size_t count{};
    bool overflow{};
};

struct ReferenceCandidate final {
    layouts::CueNodeId source{};
    std::uint32_t tag{};
    std::uint32_t descriptorWordOffset{};
    std::size_t occurrences{1};
};

struct NamedNode final {
    layouts::CueNodeId node{};
    std::uint32_t nameHash{};
};

struct CueCorrelation final {
    std::size_t tables{};
    std::size_t records{};
    std::size_t values{};
    std::size_t keyMatches{};
    std::size_t hashMatches{};
    std::size_t transitionMatches{};
    std::size_t candidateEdges{};
};

struct ProbeState final {
    std::array<std::uint16_t, layouts::kDestinationPackageCapacity
                                  + layouts::kManifestGroupCapacity>
        packages{};
    std::size_t packageCount{};
    std::array<SchemaTarget, kSchemaCapacity> schemas{};
    std::size_t schemaCount{};
    std::vector<ReferenceCandidate> references{};
};

[[nodiscard]] bool relevant_type(std::uint8_t type) noexcept {
    return type == 1U || type == 3U || type == 11U || type == 23U || type == 30U
           || type == 43U || type == 53U || type == 68U || type == 70U;
}

[[nodiscard]] std::string_view name_of(const layouts::Definition& definition) noexcept {
    return {definition.name.data(), definition.nameLength};
}

[[nodiscard]] const layouts::Definition*
find_towerfall(std::span<const layouts::Definition> definitions) noexcept {
    for (const layouts::Definition& definition : definitions) {
        if (name_of(definition) == kTowerfallName) {
            return &definition;
        }
    }
    return nullptr;
}

void add_package(ProbeState& state, std::uint16_t package) noexcept {
    if (package == tables::kAbsentPackageId) {
        return;
    }
    for (std::size_t index = 0; index < state.packageCount; ++index) {
        if (state.packages[index] == package) {
            return;
        }
    }
    if (state.packageCount < state.packages.size()) {
        state.packages[state.packageCount++] = package;
    }
}

[[nodiscard]] bool allowed_package(const ProbeState& state, std::uint32_t tag) noexcept {
    const std::uint16_t package = tables::package_of(tag);
    for (std::size_t index = 0; index < state.packageCount; ++index) {
        if (state.packages[index] == package) {
            return true;
        }
    }
    return false;
}

void add_schema(ProbeState& state,
                std::uint32_t schema,
                std::uint8_t type,
                bool sense) noexcept {
    if (schema == tables::kAbsentSchema || !tables::is_class_id(schema)) {
        return;
    }
    for (std::size_t index = 0; index < state.schemaCount; ++index) {
        if (state.schemas[index].id == schema) {
            state.schemas[index].sense = state.schemas[index].sense || sense;
            state.schemas[index].auth = state.schemas[index].auth || !sense;
            if (type < 32U) {
                state.schemas[index].typeMask |= std::uint32_t{1} << type;
            }
            return;
        }
    }
    if (state.schemaCount == state.schemas.size()) {
        return;
    }
    SchemaTarget& target = state.schemas[state.schemaCount++];
    target.id = schema;
    target.sense = sense;
    target.auth = !sense;
    target.typeMask = type < 32U ? std::uint32_t{1} << type : 0U;
}

void add_reference(ProbeState& state,
                   layouts::CueNodeId source,
                   std::uint32_t tag,
                   std::uint32_t offset) noexcept {
    for (ReferenceCandidate& existing : state.references) {
        if (existing.tag == tag) {
            ++existing.occurrences;
            return;
        }
    }
    state.references.push_back({source, tag, offset, 1});
}

[[nodiscard]] bool ensure_directory(std::wstring_view suffix, path::Buffer& output) noexcept {
    if (!path::artifact_directory(reinterpret_cast<void*>(&__ImageBase), output)
        || !path::append(output, suffix)) {
        return false;
    }
    if (CreateDirectoryW(output.chars.data(), nullptr) != FALSE) {
        return true;
    }
    const DWORD error = GetLastError();
    const DWORD attributes = GetFileAttributesW(output.chars.data());
    return error == ERROR_ALREADY_EXISTS && attributes != INVALID_FILE_ATTRIBUTES
           && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

[[nodiscard]] bool write_bytes(const wchar_t* filename,
                               std::span<const std::byte> bytes) noexcept {
    if (filename == nullptr || bytes.empty() || bytes.size() > MAXDWORD) {
        return false;
    }
    const HANDLE file = CreateFileW(filename,
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    bool complete = WriteFile(file,
                              bytes.data(),
                              static_cast<DWORD>(bytes.size()),
                              &written,
                              nullptr)
                        != FALSE
                    && written == bytes.size() && FlushFileBuffers(file) != FALSE;
    complete = CloseHandle(file) != FALSE && complete;
    return complete;
}

[[nodiscard]] bool write_text(const wchar_t* filename, std::string_view text) noexcept {
    return write_bytes(filename,
                       {reinterpret_cast<const std::byte*>(text.data()), text.size()});
}

void append_hex(std::string& output, std::span<const std::byte> bytes) {
    constexpr char kHex[] = "0123456789ABCDEF";
    for (std::byte value : bytes) {
        const std::uint8_t byte = std::to_integer<std::uint8_t>(value);
        output.push_back(kHex[byte >> 4U]);
        output.push_back(kHex[byte & 0x0FU]);
    }
}

[[nodiscard]] bool target_registry(std::span<const layouts::RosterGroup> groups,
                                   const layouts::ManifestGroups& manifest,
                                   std::uint32_t value,
                                   std::uint32_t& target) noexcept {
    target = 0;
    for (std::size_t index = 0; index < manifest.count; ++index) {
        const std::size_t groupIndex = manifest.indices[index];
        if (groupIndex < groups.size() && groups[groupIndex].registryKey == value) {
            target = value;
            return true;
        }
    }
    return false;
}

bool collect_schema_tag(void* context, const reader::ClassEntry& entry) noexcept {
    auto& pair = *static_cast<std::pair<const ProbeState*, SchemaTags*>*>(context);
    if (!allowed_package(*pair.first, entry.tag)) {
        return true;
    }
    SchemaTags& tags = *pair.second;
    if (tags.count == tags.tags.size()) {
        tags.overflow = true;
        return false;
    }
    tags.tags[tags.count++] = entry.tag;
    return true;
}

void append_value_matches(std::string& json,
                          std::span<const std::byte> blob,
                          std::span<const layouts::RosterGroup> groups,
                          const layouts::ManifestGroups& manifest,
                          std::size_t& matchCount) {
    json += "[";
    bool first = true;
    for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= blob.size();
         offset += sizeof(std::uint32_t)) {
        std::uint32_t value = 0;
        std::memcpy(&value, blob.data() + offset, sizeof value);
        std::uint32_t target = 0;
        if (!target_registry(groups, manifest, value, target)) {
            continue;
        }
        char item[128]{};
        const int written = std::snprintf(item,
                                          sizeof item,
                                          "%s{\"offset\":%zu,\"registry_key\":\"0x%08X\"}",
                                          first ? "" : ",",
                                          offset,
                                          target);
        if (written > 0) {
            json.append(item, static_cast<std::size_t>(written));
            first = false;
            ++matchCount;
        }
    }
    json += "]";
}

[[nodiscard]] bool same_node(layouts::CueNodeId left, layouts::CueNodeId right) noexcept {
    return left.registryKey == right.registryKey && left.slotType == right.slotType
           && left.slotIndex == right.slotIndex;
}

[[nodiscard]] bool transition_type(std::uint8_t type) noexcept {
    return type == 3U || type == 11U || type == 53U;
}

void collect_named_nodes(const reader::Source& source,
                         reader::Scratch& scratch,
                         std::span<const layouts::RosterGroup> groups,
                         const layouts::ManifestGroups& manifest,
                         std::vector<NamedNode>& output,
                         std::size_t& unreadGroups,
                         std::size_t& unreadSlots) {
    std::vector<std::byte> object;
    for (std::size_t ordinal = 0; ordinal < manifest.count; ++ordinal) {
        const std::size_t groupIndex = manifest.indices[ordinal];
        if (groupIndex >= groups.size()) {
            ++unreadGroups;
            continue;
        }
        const layouts::RosterGroup& group = groups[groupIndex];
        tables::Array slots{};
        if (!reader::read_tag(source, scratch, group.objectTag, object)
            || !tables::object_slots(object, slots)) {
            ++unreadGroups;
            continue;
        }
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            tables::Slot declared{};
            if (!tables::object_slot_at(object, slots, group.slotIndices[slot], declared)
                || declared.type != group.slotTypes[slot]) {
                ++unreadSlots;
                continue;
            }
            const NamedNode candidate{{group.registryKey,
                                       group.slotIndices[slot],
                                       group.slotTypes[slot]},
                                      declared.nameHash};
            bool duplicate = false;
            for (const NamedNode& existing : output) {
                if (same_node(existing.node, candidate.node)
                    && existing.nameHash == candidate.nameHash) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                output.push_back(candidate);
            }
        }
    }
}

std::size_t append_name_matches(std::string& json,
                                std::uint32_t hash,
                                std::span<const NamedNode> nodes,
                                CueCorrelation& correlation) {
    json += "[";
    bool first = true;
    std::size_t count = 0;
    for (const NamedNode& named : nodes) {
        if (named.nameHash != hash) {
            continue;
        }
        char item[192]{};
        const int written = std::snprintf(
            item,
            sizeof item,
            "%s{\"registry_key\":\"0x%08X\",\"type\":%u,\"index\":%u}",
            first ? "" : ",",
            named.node.registryKey,
            static_cast<unsigned>(named.node.slotType),
            static_cast<unsigned>(named.node.slotIndex));
        if (written > 0) {
            json.append(item, static_cast<std::size_t>(written));
            first = false;
            ++count;
            if (transition_type(named.node.slotType)) {
                ++correlation.transitionMatches;
            }
        }
    }
    json += "]";
    return count;
}

void append_cue_table(std::string& json,
                      std::uint32_t tag,
                      const layouts::CueTable& table,
                      std::span<const NamedNode> nodes,
                      CueCorrelation& correlation,
                      bool firstTable) {
    char prefix[192]{};
    const int prefixLength = std::snprintf(
        prefix,
        sizeof prefix,
        "%s    {\"tag\":\"0x%08X\",\"class\":\"0x%08X\",\"records\":[\n",
        firstTable ? "" : ",\n",
        tag,
        layouts::kCueTableClass);
    if (prefixLength > 0) {
        json.append(prefix, static_cast<std::size_t>(prefixLength));
    }
    for (std::size_t recordOrdinal = 0; recordOrdinal < table.recordCount; ++recordOrdinal) {
        const layouts::CueTableRecord& record = table.records[recordOrdinal];
        char recordPrefix[256]{};
        const int recordPrefixLength = std::snprintf(
            recordPrefix,
            sizeof recordPrefix,
            "%s      {\"ordinal\":%zu,\"key\":\"0x%08X\",\"domain\":\"0x%08X\",\"enabled\":%llu,\"flags\":\"0x%016llX\",\"key_matches\":",
            recordOrdinal == 0 ? "" : ",\n",
            recordOrdinal,
            record.key,
            record.domain,
            static_cast<unsigned long long>(record.enabled),
            static_cast<unsigned long long>(record.flags));
        if (recordPrefixLength > 0) {
            json.append(recordPrefix, static_cast<std::size_t>(recordPrefixLength));
        }
        const std::size_t sourceMatches =
            append_name_matches(json, record.key, nodes, correlation);
        correlation.keyMatches += sourceMatches;
        json += ",\"values\":[";
        std::size_t recordTargets = 0;
        for (std::size_t valueOrdinal = 0; valueOrdinal < record.valueCount; ++valueOrdinal) {
            const layouts::CueTableValue& value =
                table.values[record.firstValue + valueOrdinal];
            char valuePrefix[96]{};
            const int valuePrefixLength = std::snprintf(
                valuePrefix,
                sizeof valuePrefix,
                "%s{\"ordinal\":%zu,\"flags\":\"0x%08X\",\"hashes\":[",
                valueOrdinal == 0 ? "" : ",",
                valueOrdinal,
                value.flags);
            if (valuePrefixLength > 0) {
                json.append(valuePrefix, static_cast<std::size_t>(valuePrefixLength));
            }
            for (std::size_t position = 0; position < value.hashes.size(); ++position) {
                const layouts::CueTableHash hash = value.hashes[position];
                char hashPrefix[192]{};
                const int hashPrefixLength = std::snprintf(
                    hashPrefix,
                    sizeof hashPrefix,
                    "%s{\"position\":%zu,\"class\":\"0x%08X\",\"value\":\"0x%08X\",\"absent\":%s,\"matches\":",
                    position == 0 ? "" : ",",
                    position,
                    hash.classId,
                    hash.value,
                    hash.value == layouts::kCueTableAbsentHash ? "true" : "false");
                if (hashPrefixLength > 0) {
                    json.append(hashPrefix, static_cast<std::size_t>(hashPrefixLength));
                }
                const std::size_t matches = hash.value == layouts::kCueTableAbsentHash
                                                ? (json += "[]", 0U)
                                                : append_name_matches(
                                                      json, hash.value, nodes, correlation);
                correlation.hashMatches += matches;
                recordTargets += matches;
                json += "}";
            }
            json += "]}";
        }
        correlation.candidateEdges += sourceMatches * recordTargets;
        json += "]}";
    }
    json += "\n    ]}";
    ++correlation.tables;
    correlation.records += table.recordCount;
    correlation.values += table.valueCount;
}

} // namespace

bool probe_towerfall_cue_sources(
    const reader::Source& source,
    reader::Scratch& scratch,
    std::span<const layouts::Definition> definitions,
    std::span<const layouts::RosterGroup> groups) noexcept {
    const layouts::Definition* definition = find_towerfall(definitions);
    if (definition == nullptr) {
        return true;
    }
    const layouts::ManifestGroups manifest = layouts::manifest_groups(*definition);
    ProbeState state{};
    for (std::size_t index = 0; index < definition->packageCount; ++index) {
        add_package(state, definition->packages[index]);
    }
    for (std::size_t index = 0; index < manifest.count; ++index) {
        const std::size_t groupIndex = manifest.indices[index];
        if (groupIndex >= groups.size()) {
            continue;
        }
        const layouts::RosterGroup& group = groups[groupIndex];
        add_package(state, tables::package_of(group.objectTag));
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            add_package(state, tables::package_of(group.descriptorTags[slot]));
            if (relevant_type(group.slotTypes[slot])) {
                add_schema(state, group.senseSchemas[slot], group.slotTypes[slot], true);
                add_schema(state, group.authSchemas[slot], group.slotTypes[slot], false);
            }
        }
    }

    path::Buffer analysis{};
    path::Buffer exports{};
    if (!ensure_directory(kAnalysisSuffix, analysis)
        || !ensure_directory(kExportsSuffix, exports)) {
        return false;
    }

    std::vector<NamedNode> namedNodes;
    std::size_t unreadNameGroups = 0;
    std::size_t unreadNameSlots = 0;
    collect_named_nodes(source,
                        scratch,
                        groups,
                        manifest,
                        namedNodes,
                        unreadNameGroups,
                        unreadNameSlots);

    std::string json;
    json.reserve(512 * 1024);
    char header[256]{};
    const int headerLength = std::snprintf(
        header,
        sizeof header,
        "{\n  \"format_version\":1,\n  \"activity\":\"mission_towerfall\",\n  \"authority_behavior\":\"read_only\",\n  \"descriptor_nodes\":[\n");
    if (headerLength > 0) {
        json.append(header, static_cast<std::size_t>(headerLength));
    }

    std::vector<std::byte> blob;
    bool firstNode = true;
    std::size_t descriptorNodes = 0;
    std::size_t directRegistryMatches = 0;
    for (std::size_t manifestOrdinal = 0; manifestOrdinal < manifest.count; ++manifestOrdinal) {
        const std::size_t groupIndex = manifest.indices[manifestOrdinal];
        if (groupIndex >= groups.size()) {
            continue;
        }
        const layouts::RosterGroup& group = groups[groupIndex];
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            const std::uint8_t type = group.slotTypes[slot];
            if (!relevant_type(type)
                || !reader::read_tag(source, scratch, group.descriptorTags[slot], blob)) {
                continue;
            }
            const std::size_t base = group.descriptorOffsets[slot];
            if (base > blob.size()
                || tables::kDescriptorSize > blob.size() - base) {
                continue;
            }
            const auto descriptor = std::span(blob).subspan(base, tables::kDescriptorSize);
            char prefix[512]{};
            const int prefixLength = std::snprintf(
                prefix,
                sizeof prefix,
                "%s    {\"registry_key\":\"0x%08X\",\"type\":%u,\"index\":%u,\"object_tag\":\"0x%08X\",\"descriptor_tag\":\"0x%08X\",\"descriptor_offset\":%u,\"component_class\":\"0x%08X\",\"sense_schema\":\"0x%08X\",\"auth_schema\":\"0x%08X\",\"raw\":\"",
                firstNode ? "" : ",\n",
                group.registryKey,
                static_cast<unsigned>(type),
                static_cast<unsigned>(group.slotIndices[slot]),
                group.objectTag,
                group.descriptorTags[slot],
                group.descriptorOffsets[slot],
                group.componentClasses[slot],
                group.senseSchemas[slot],
                group.authSchemas[slot]);
            if (prefixLength > 0) {
                json.append(prefix, static_cast<std::size_t>(prefixLength));
            }
            append_hex(json, descriptor);
            json += "\",\"raw_registry_matches\":[";
            bool firstMatch = true;
            for (std::size_t wordOffset = 0;
                 wordOffset + sizeof(std::uint32_t) <= descriptor.size();
                 wordOffset += sizeof(std::uint32_t)) {
                std::uint32_t value = 0;
                std::memcpy(&value, descriptor.data() + wordOffset, sizeof value);
                const bool directSceneDefinition =
                    type == 43U && wordOffset == kSceneDefinitionTagOffset
                    && tables::package_of(value) != tables::kAbsentPackageId;
                if (directSceneDefinition) {
                    // Admit this one explicit descriptor edge before filtering references. This
                    // lets the package reader recover the scene's 55-bit active-entry records
                    // without broadening the probe to arbitrary tag-shaped descriptor words.
                    add_package(state, tables::package_of(value));
                }
                std::uint32_t target = 0;
                if (target_registry(groups, manifest, value, target)
                    && target != group.registryKey) {
                    char match[96]{};
                    const int matchLength = std::snprintf(
                        match,
                        sizeof match,
                        "%s{\"offset\":%zu,\"target\":\"0x%08X\"}",
                        firstMatch ? "" : ",",
                        wordOffset,
                        target);
                    if (matchLength > 0) {
                        json.append(match, static_cast<std::size_t>(matchLength));
                        firstMatch = false;
                        ++directRegistryMatches;
                    }
                }
                if (wordOffset != tables::kDescriptorOwnTagOffset
                    && wordOffset != tables::kDescriptorComponentClassOffset
                    && wordOffset != tables::kDescriptorRegistryKeyOffset
                    && wordOffset != tables::kDescriptorSenseSchemaOffset
                    && wordOffset != tables::kDescriptorAuthSchemaOffset
                    && (directSceneDefinition || allowed_package(state, value))
                    && value != group.objectTag && value != group.descriptorTags[slot]) {
                    add_reference(state,
                                  {group.registryKey, group.slotIndices[slot], type},
                                  value,
                                  static_cast<std::uint32_t>(wordOffset));
                }
            }
            json += "]}";
            firstNode = false;
            ++descriptorNodes;
        }
    }
    json += "\n  ],\n  \"schema_classes\":[\n";

    bool firstSchema = true;
    std::size_t schemaEntries = 0;
    std::size_t schemaRegistryMatches = 0;
    for (std::size_t schemaOrdinal = 0; schemaOrdinal < state.schemaCount; ++schemaOrdinal) {
        const SchemaTarget& schema = state.schemas[schemaOrdinal];
        SchemaTags tags{};
        std::pair<const ProbeState*, SchemaTags*> context{&state, &tags};
        reader::ScanResult scan{};
        const bool scanned = reader::scan_class_entries(
            source.directory, schema.id, &collect_schema_tag, &context, scan);
        char schemaPrefix[320]{};
        const int schemaPrefixLength = std::snprintf(
            schemaPrefix,
            sizeof schemaPrefix,
            "%s    {\"schema\":\"0x%08X\",\"sense\":%s,\"auth\":%s,\"scan_ok\":%s,\"matches\":%zu,\"entries\":[",
            firstSchema ? "" : ",\n",
            schema.id,
            schema.sense ? "true" : "false",
            schema.auth ? "true" : "false",
            scanned ? "true" : "false",
            tags.count);
        if (schemaPrefixLength > 0) {
            json.append(schemaPrefix, static_cast<std::size_t>(schemaPrefixLength));
        }
        bool firstEntry = true;
        for (std::size_t tagOrdinal = 0; tagOrdinal < tags.count; ++tagOrdinal) {
            std::uint32_t classId = 0;
            if (!reader::read_tag(source, scratch, tags.tags[tagOrdinal], blob, classId)) {
                continue;
            }
            path::Buffer dump = analysis;
            std::array<wchar_t, 128> filename{};
            const int filenameLength = std::swprintf(filename.data(),
                                                     filename.size(),
                                                     L"\\towerfall_cue_schema_%08X_tag_%08X.bin",
                                                     schema.id,
                                                     tags.tags[tagOrdinal]);
            const bool dumped = filenameLength > 0 && path::append(dump, filename.data())
                                && write_bytes(dump.chars.data(), blob);
            char entryPrefix[256]{};
            const int entryPrefixLength = std::snprintf(
                entryPrefix,
                sizeof entryPrefix,
                "%s{\"tag\":\"0x%08X\",\"class\":\"0x%08X\",\"bytes\":%zu,\"dumped\":%s,\"registry_matches\":",
                firstEntry ? "" : ",",
                tags.tags[tagOrdinal],
                classId,
                blob.size(),
                dumped ? "true" : "false");
            if (entryPrefixLength > 0) {
                json.append(entryPrefix, static_cast<std::size_t>(entryPrefixLength));
            }
            append_value_matches(json, blob, groups, manifest, schemaRegistryMatches);
            json += "}";
            firstEntry = false;
            ++schemaEntries;
        }
        json += "]}";
        firstSchema = false;
    }

    json += "\n  ],\n  \"descriptor_tag_references\":[\n";
    bool firstReference = true;
    std::size_t readableReferences = 0;
    std::size_t referenceRegistryMatches = 0;
    std::string decodedTables;
    decodedTables.reserve(64 * 1024);
    CueCorrelation correlation{};
    std::vector<std::uint32_t> decodedCueTags;
    for (const ReferenceCandidate& reference : state.references) {
        std::uint32_t classId = 0;
        if (!allowed_package(state, reference.tag)
            || !reader::read_tag(source, scratch, reference.tag, blob, classId)) {
            continue;
        }
        path::Buffer dump = analysis;
        std::array<wchar_t, 128> filename{};
        const int filenameLength = std::swprintf(filename.data(),
                                                 filename.size(),
                                                 L"\\towerfall_cue_ref_%08X_class_%08X.bin",
                                                 reference.tag,
                                                 classId);
        const bool dumped = filenameLength > 0 && path::append(dump, filename.data())
                            && write_bytes(dump.chars.data(), blob);
        char referencePrefix[448]{};
        const int referencePrefixLength = std::snprintf(
            referencePrefix,
            sizeof referencePrefix,
            "%s    {\"source\":{\"registry_key\":\"0x%08X\",\"type\":%u,\"index\":%u},\"descriptor_word_offset\":%u,\"occurrences\":%zu,\"tag\":\"0x%08X\",\"class\":\"0x%08X\",\"bytes\":%zu,\"dumped\":%s,\"registry_matches\":",
            firstReference ? "" : ",\n",
            reference.source.registryKey,
            static_cast<unsigned>(reference.source.slotType),
            static_cast<unsigned>(reference.source.slotIndex),
            reference.descriptorWordOffset,
            reference.occurrences,
            reference.tag,
            classId,
            blob.size(),
            dumped ? "true" : "false");
        if (referencePrefixLength > 0) {
            json.append(referencePrefix, static_cast<std::size_t>(referencePrefixLength));
        }
        append_value_matches(json, blob, groups, manifest, referenceRegistryMatches);
        json += "}";
        firstReference = false;
        ++readableReferences;

        bool alreadyDecoded = false;
        for (std::uint32_t decodedTag : decodedCueTags) {
            if (decodedTag == reference.tag) {
                alreadyDecoded = true;
                break;
            }
        }
        if (!alreadyDecoded && classId == layouts::kCueTableClass) {
            layouts::CueTable table{};
            if (layouts::decode_cue_table(blob, table)) {
                append_cue_table(decodedTables,
                                 reference.tag,
                                 table,
                                 namedNodes,
                                 correlation,
                                 correlation.tables == 0);
                decodedCueTags.push_back(reference.tag);
            }
        }
    }
    json += "\n  ],\n  \"decoded_cue_tables\":[\n";
    json += decodedTables;
    char summary[768]{};
    const int summaryLength = std::snprintf(
        summary,
        sizeof summary,
        "\n  ],\n  \"summary\":{\"manifest_groups\":%zu,\"descriptor_nodes\":%zu,\"named_nodes\":%zu,\"unread_name_groups\":%zu,\"unread_name_slots\":%zu,\"schema_classes\":%zu,\"schema_entries\":%zu,\"readable_descriptor_tag_references\":%zu,\"direct_registry_matches\":%zu,\"schema_registry_matches\":%zu,\"reference_registry_matches\":%zu,\"decoded_cue_tables\":%zu,\"decoded_cue_records\":%zu,\"decoded_cue_values\":%zu,\"cue_key_name_matches\":%zu,\"cue_hash_name_matches\":%zu,\"cue_type_3_11_53_matches\":%zu,\"candidate_name_edges\":%zu,\"reference_overflow\":false}\n}\n",
        manifest.count,
        descriptorNodes,
        namedNodes.size(),
        unreadNameGroups,
        unreadNameSlots,
        state.schemaCount,
        schemaEntries,
        readableReferences,
        directRegistryMatches,
        schemaRegistryMatches,
        referenceRegistryMatches,
        correlation.tables,
        correlation.records,
        correlation.values,
        correlation.keyMatches,
        correlation.hashMatches,
        correlation.transitionMatches,
        correlation.candidateEdges);
    if (summaryLength > 0) {
        json.append(summary, static_cast<std::size_t>(summaryLength));
    }

    path::Buffer reportPath = exports;
    const bool written = path::append(reportPath, kReportSuffix)
                         && write_text(reportPath.chars.data(), json);
    std::array<char, core::log::kLineCapacity> line{};
    const int lineLength = std::snprintf(
        line.data(),
        line.size(),
        "ev=cue_source_probe result=%s activity=mission_towerfall groups=%zu descriptor_nodes=%zu named_nodes=%zu unread_name_groups=%zu unread_name_slots=%zu schemas=%zu schema_entries=%zu readable_refs=%zu direct_edges=%zu schema_edges=%zu reference_edges=%zu cue_tables=%zu cue_records=%zu cue_values=%zu cue_key_matches=%zu cue_hash_matches=%zu cue_transition_matches=%zu candidate_name_edges=%zu overflow=0 authority=unchanged",
        written ? "ok" : "failed",
        manifest.count,
        descriptorNodes,
        namedNodes.size(),
        unreadNameGroups,
        unreadNameSlots,
        state.schemaCount,
        schemaEntries,
        readableReferences,
        directRegistryMatches,
        schemaRegistryMatches,
        referenceRegistryMatches,
        correlation.tables,
        correlation.records,
        correlation.values,
        correlation.keyMatches,
        correlation.hashMatches,
        correlation.transitionMatches,
        correlation.candidateEdges);
    if (lineLength > 0) {
        core::log::write(core::log::Channel::state,
                         written ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(lineLength)});
    }
    return written;
}

} // namespace dawn::client::content::scenarios
