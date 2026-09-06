#include "omega_schema_catalog.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/slot_descriptor_reader.h"
#include "../table.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace sunrise::state::build_data::scenarios {
namespace {

namespace path = core::path;
namespace tables = middleware::content::packages::tables;

constexpr std::array<std::uint32_t, 6> kOmegaKeys = {
    0x4786C0E0U, 0x29D7B029U, 0x82FB58B7U, 0xBA5F26EFU, 0xD00142CFU, 0xF7A6CE7FU};
constexpr std::size_t kOmegaSlotCount = 57;
/** Retains alternate layouts until the published Omega row identifies the exact six object tags. */
constexpr std::size_t kSchemaCapacity = 256;
constexpr std::uint32_t kSidecarMagic = 0x53474D4FU; // OMGS
constexpr std::uint16_t kSidecarVersion = 1;
constexpr std::wstring_view kCacheDirectorySuffix = L"\\cache";
constexpr std::wstring_view kSidecarFileSuffix = L"\\omega_schema.bin";

struct SidecarHeader final {
    std::uint32_t magic{};
    std::uint16_t version{};
    std::uint16_t entrySize{};
    std::uint32_t count{};
    std::uint32_t reserved{};
};

static_assert(std::is_trivially_copyable_v<OmegaSchema>);
static_assert(std::is_trivially_copyable_v<SidecarHeader>);

Lock g_lock;
std::array<OmegaSchema, kSchemaCapacity> g_schemas{};
std::size_t g_count{};
bool g_conflict{};

[[nodiscard]] bool omega_key(std::uint32_t key) noexcept {
    for (const std::uint32_t expected : kOmegaKeys) {
        if (expected == key) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool schema_id(std::uint32_t value) noexcept {
    return value == tables::kAbsentSchema || tables::is_class_id(value);
}

[[nodiscard]] bool canonical(const OmegaSchema& schema) noexcept {
    return omega_key(schema.registryKey) && schema.objectTag != 0
           && tables::is_class_id(schema.componentClass) && schema_id(schema.senseSchema)
           && schema_id(schema.authSchema) && schema.slotType != 0
           && schema.slotType <= kMaximumSlotType && schema.slotIndex < kRosterSlotCapacity;
}

[[nodiscard]] bool same_address(const OmegaSchema& left,
                                const OmegaSchema& right) noexcept {
    return left.registryKey == right.registryKey && left.slotType == right.slotType
           && left.slotIndex == right.slotIndex;
}

[[nodiscard]] bool same_descriptor_identity(const OmegaSchema& left,
                                            const OmegaSchema& right) noexcept {
    return same_address(left, right) && left.objectTag == right.objectTag;
}

[[nodiscard]] bool same_schema(const OmegaSchema& left,
                               const OmegaSchema& right) noexcept {
    return same_address(left, right) && left.objectTag == right.objectTag
           && left.componentClass == right.componentClass
           && left.senseSchema == right.senseSchema && left.authSchema == right.authSchema;
}

[[nodiscard]] const OmegaSchema*
find_entry(std::span<const OmegaSchema> schemas,
           std::uint32_t key,
           std::uint8_t type,
           std::uint16_t index) noexcept {
    for (const OmegaSchema& schema : schemas) {
        if (schema.registryKey == key && schema.slotType == type
            && schema.slotIndex == index) {
            return &schema;
        }
    }
    return nullptr;
}

[[nodiscard]] bool matches_roster(std::span<const OmegaSchema> schemas,
                                  std::span<const RosterGroup> groups,
                                  std::span<const std::size_t> omegaGroupIndices) noexcept {
    if (schemas.size() != kOmegaSlotCount || omegaGroupIndices.size() != kOmegaKeys.size()) {
        return false;
    }
    std::size_t rosterSlots = 0;
    for (std::size_t ordinal = 0; ordinal < kOmegaKeys.size(); ++ordinal) {
        if (omegaGroupIndices[ordinal] >= groups.size()) {
            return false;
        }
        const RosterGroup& group = groups[omegaGroupIndices[ordinal]];
        if (group.registryKey != kOmegaKeys[ordinal]) {
            return false;
        }
        rosterSlots += group.slotCount;
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            const OmegaSchema* schema = find_entry(
                schemas, group.registryKey, group.slotTypes[slot], group.slotIndices[slot]);
            if (schema == nullptr || !canonical(*schema) || schema->objectTag != group.objectTag) {
                return false;
            }
            const bool authPresent = schema->authSchema != tables::kAbsentSchema;
            const bool sensePresent = schema->senseSchema != tables::kAbsentSchema;
            const std::uint8_t expectedFlags =
                (authPresent ? kSlotAuthFlag : 0U) | (sensePresent ? kSlotSenseFlag : 0U);
            if (group.slotFlags[slot] != expectedFlags) {
                return false;
            }
        }
    }
    if (rosterSlots != kOmegaSlotCount) {
        return false;
    }
    for (std::size_t left = 0; left < schemas.size(); ++left) {
        if (!canonical(schemas[left])) {
            return false;
        }
        for (std::size_t right = left + 1; right < schemas.size(); ++right) {
            if (same_address(schemas[left], schemas[right])) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool select_roster_schemas(
    std::span<const OmegaSchema> candidates,
    std::span<const RosterGroup> groups,
    std::span<const std::size_t> omegaGroupIndices,
    std::array<OmegaSchema, kSchemaCapacity>& selected,
    std::size_t& selectedCount) noexcept {
    selected = {};
    selectedCount = 0;
    if (omegaGroupIndices.size() != kOmegaKeys.size()) {
        return false;
    }
    for (std::size_t ordinal = 0; ordinal < kOmegaKeys.size(); ++ordinal) {
        if (omegaGroupIndices[ordinal] >= groups.size()) {
            return false;
        }
        const RosterGroup& group = groups[omegaGroupIndices[ordinal]];
        if (group.registryKey != kOmegaKeys[ordinal]) {
            return false;
        }
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            const OmegaSchema* matched = nullptr;
            for (const OmegaSchema& candidate : candidates) {
                if (candidate.registryKey == group.registryKey
                    && candidate.objectTag == group.objectTag
                    && candidate.slotType == group.slotTypes[slot]
                    && candidate.slotIndex == group.slotIndices[slot]) {
                    if (matched != nullptr && !same_schema(*matched, candidate)) {
                        return false;
                    }
                    matched = &candidate;
                }
            }
            if (matched == nullptr || selectedCount == selected.size()) {
                return false;
            }
            selected[selectedCount++] = *matched;
        }
    }
    return matches_roster(
        std::span(selected).first(selectedCount), groups, omegaGroupIndices);
}

[[nodiscard]] bool sidecar_path(path::Buffer& directory, path::Buffer& file) noexcept {
    if (!path::artifact_directory(reinterpret_cast<void*>(&__ImageBase), directory)
        || !path::append(directory, kCacheDirectorySuffix)) {
        return false;
    }
    if (CreateDirectoryW(directory.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(directory.chars.data());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return false;
    }
    file = directory;
    return path::append(file, kSidecarFileSuffix);
}

[[nodiscard]] bool read_exact(HANDLE file, void* output, DWORD size) noexcept {
    DWORD transferred = 0;
    return ReadFile(file, output, size, &transferred, nullptr) != FALSE && transferred == size;
}

[[nodiscard]] bool write_exact(HANDLE file, const void* input, DWORD size) noexcept {
    DWORD transferred = 0;
    return WriteFile(file, input, size, &transferred, nullptr) != FALSE && transferred == size;
}

[[nodiscard]] bool load_sidecar(std::array<OmegaSchema, kSchemaCapacity>& schemas,
                                std::size_t& count) noexcept {
    count = 0;
    path::Buffer directory{};
    path::Buffer filePath{};
    if (!sidecar_path(directory, filePath)) {
        return false;
    }
    const HANDLE file = CreateFileW(filePath.chars.data(),
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    SidecarHeader header{};
    bool loaded = read_exact(file, &header, sizeof header)
                  && header.magic == kSidecarMagic && header.version == kSidecarVersion
                  && header.entrySize == sizeof(OmegaSchema) && header.count <= schemas.size();
    if (loaded && header.count != 0) {
        loaded = read_exact(file,
                            schemas.data(),
                            static_cast<DWORD>(header.count * sizeof(OmegaSchema)));
    }
    LARGE_INTEGER position{};
    LARGE_INTEGER size{};
    loaded = loaded && SetFilePointerEx(file, {}, &position, FILE_CURRENT) != FALSE
             && GetFileSizeEx(file, &size) != FALSE && position.QuadPart == size.QuadPart;
    CloseHandle(file);
    if (loaded) {
        count = header.count;
    }
    return loaded;
}

[[nodiscard]] bool save_sidecar(std::span<const OmegaSchema> schemas) noexcept {
    path::Buffer directory{};
    path::Buffer filePath{};
    if (!sidecar_path(directory, filePath)) {
        return false;
    }
    path::Buffer stage = filePath;
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
    const SidecarHeader header{kSidecarMagic,
                               kSidecarVersion,
                               static_cast<std::uint16_t>(sizeof(OmegaSchema)),
                               static_cast<std::uint32_t>(schemas.size()),
                               0};
    bool saved = write_exact(file, &header, sizeof header)
                 && write_exact(file,
                                schemas.data(),
                                static_cast<DWORD>(schemas.size_bytes()))
                 && FlushFileBuffers(file) != FALSE;
    saved = CloseHandle(file) != FALSE && saved;
    if (saved) {
        saved = MoveFileExW(stage.chars.data(),
                            filePath.chars.data(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
                != FALSE;
    }
    if (!saved) {
        DeleteFileW(stage.chars.data());
    }
    return saved;
}

void report(const char* source, bool ready, std::size_t count) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=omega_schema result=%s source=%s entries=%zu expected=%zu",
                                      ready ? "ok" : "unavailable",
                                      source,
                                      count,
                                      kOmegaSlotCount);
    if (written > 0) {
        core::log::write(core::log::Channel::state,
                         ready ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

} // namespace

void clear_omega_schemas() noexcept {
    const Lock::Exclusive guard(g_lock);
    g_schemas = {};
    g_count = 0;
    g_conflict = false;
}

void record_omega_schema(const OmegaSchema& schema) noexcept {
    if (!canonical(schema)) {
        return;
    }
    const Lock::Exclusive guard(g_lock);
    for (std::size_t index = 0; index < g_count; ++index) {
        if (!same_descriptor_identity(g_schemas[index], schema)) {
            continue;
        }
        g_conflict = g_conflict || !same_schema(g_schemas[index], schema);
        return;
    }
    if (g_count == g_schemas.size()) {
        g_conflict = true;
        return;
    }
    g_schemas[g_count++] = schema;
}

bool prepare_omega_schemas(std::span<const RosterGroup> groups,
                           std::span<const std::size_t> omegaGroupIndices) noexcept {
    std::array<OmegaSchema, kSchemaCapacity> candidate{};
    std::size_t candidateCount = 0;
    bool scanConflict = false;
    {
        const Lock::Shared guard(g_lock);
        candidate = g_schemas;
        candidateCount = g_count;
        scanConflict = g_conflict;
    }
    std::array<OmegaSchema, kSchemaCapacity> selected{};
    std::size_t selectedCount = 0;
    const bool scanCandidate =
        !scanConflict
        && select_roster_schemas(std::span(candidate).first(candidateCount),
                                 groups,
                                 omegaGroupIndices,
                                 selected,
                                 selectedCount);
    if (scanCandidate) {
        {
            const Lock::Exclusive guard(g_lock);
            g_schemas = selected;
            g_count = selectedCount;
            g_conflict = false;
        }
        const bool saved = save_sidecar(std::span(selected).first(selectedCount));
        report(saved ? "package_scan" : "package_scan_sidecar_write_failed",
               true,
               selectedCount);
        return true;
    }

    candidate = {};
    candidateCount = 0;
    const bool loaded = load_sidecar(candidate, candidateCount)
                        && matches_roster(std::span(candidate).first(candidateCount),
                                          groups,
                                          omegaGroupIndices);
    if (!loaded) {
        report("none", false, candidateCount);
        return false;
    }
    {
        const Lock::Exclusive guard(g_lock);
        g_schemas = candidate;
        g_count = candidateCount;
        g_conflict = false;
    }
    report("sidecar", true, candidateCount);
    return true;
}

bool find_omega_schema(std::uint32_t registryKey,
                       std::uint8_t slotType,
                       std::uint16_t slotIndex,
                       OmegaSchema& schema) noexcept {
    schema = {};
    const Lock::Shared guard(g_lock);
    const OmegaSchema* found = find_entry(
        std::span(g_schemas).first(g_count), registryKey, slotType, slotIndex);
    if (found != nullptr) {
        schema = *found;
        return true;
    }
    return false;
}

std::size_t omega_schema_count() noexcept {
    const Lock::Shared guard(g_lock);
    return g_count;
}

} // namespace sunrise::state::build_data::scenarios
