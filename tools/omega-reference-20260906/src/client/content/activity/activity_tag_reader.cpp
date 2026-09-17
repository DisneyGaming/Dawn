#include "source.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/definition_index_table.h"

namespace dawn::client::content::activity {
namespace {

struct Target {
    std::uint32_t tag{};
    std::wstring_view filename;
};

/** Property and component classes observed behind native behavior condition 0x75. */
constexpr std::uint32_t kOmegaPropertyClass = 0x80809C36U;
constexpr std::uint32_t kOmegaComponentClass = 0x80809C0FU;
constexpr std::uint32_t kOmegaRequiredHash = 0x2B5D6A48U;

/** Package ids carried by both live condition-75 candidates in the Omega run. */
constexpr std::array<std::uint16_t, 2> kOmegaPackageIds{{0x037BU, 0x03F7U}};

struct OmegaNeedle {
    std::uint32_t hash{};
    std::uint32_t expectedTag{};
    std::string_view name;
};

/** The observed needles make the offline scan self-validating before its required hit is used. */
constexpr std::array<OmegaNeedle, 3> kOmegaNeedles{{
    {kOmegaRequiredHash, 0U, "required"},
    {0x9CBEE071U, 0x80FEF337U, "observed_9CBEE071"},
    {0x394E1E5DU, 0x80EF637AU, "observed_394E1E5D"},
}};

inline constexpr std::size_t kOmegaTagCapacity = 2048;
inline constexpr std::size_t kOmegaRequiredTagCapacity = 16;
inline constexpr std::size_t kOmegaHitLogCapacity = 64;
inline constexpr std::size_t kOmegaParentLogCapacity = 64;

struct OmegaTagCollection {
    std::array<std::uint32_t, kOmegaTagCapacity> tags{};
    std::size_t count{};
    bool overflow{};
};

/** @return True when a tag belongs to one of the two live Omega package families. */
[[nodiscard]] bool is_omega_package(std::uint32_t tag) noexcept {
    const std::uint16_t packageId = packages::tables::package_of(tag);
    for (const std::uint16_t expected : kOmegaPackageIds) {
        if (packageId == expected) {
            return true;
        }
    }
    return false;
}

/** Collects only matching-class tags from the two live package families. */
bool collect_omega_tag(void* context, const packages::reader::ClassEntry& entry) noexcept {
    auto& collection = *static_cast<OmegaTagCollection*>(context);
    if (!is_omega_package(entry.tag)) {
        return true;
    }
    if (collection.count >= collection.tags.size()) {
        collection.overflow = true;
        return false;
    }
    collection.tags[collection.count++] = entry.tag;
    return true;
}

/** Adds a tag once to fixed diagnostic storage. */
void add_unique_tag(std::array<std::uint32_t, kOmegaRequiredTagCapacity>& tags,
                    std::size_t& count,
                    std::uint32_t tag) noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (tags[index] == tag) {
            return;
        }
    }
    if (count < tags.size()) {
        tags[count++] = tag;
    }
}

/** Emits one exact property-hash hit without dumping package key material or unrelated content. */
void report_omega_property_hit(const OmegaNeedle& needle,
                               std::uint32_t tag,
                               std::uint32_t classId,
                               std::size_t bytes,
                               std::size_t offset,
                               std::size_t ordinal) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_property_scan stage=property_hit kind=%.*s ordinal=%zu hash=0x%08X "
        "tag=0x%08X expected_tag=0x%08X expected_match=%u class=0x%08X "
        "package=0x%04X entry=0x%04X bytes=%zu offset=0x%zX",
        static_cast<int>(needle.name.size()),
        needle.name.data(),
        ordinal,
        needle.hash,
        tag,
        needle.expectedTag,
        needle.expectedTag == 0U || needle.expectedTag == tag ? 1U : 0U,
        classId,
        packages::tables::package_of(tag),
        tag & packages::reader::layout::kTagEntryMask,
        bytes,
        offset);
    if (count > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** Emits the component blob that directly references a watched property definition. */
void report_omega_parent(std::uint32_t componentTag,
                         std::uint32_t propertyTag,
                         const char* kind,
                         std::size_t offset,
                         std::size_t ordinal) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_property_scan stage=parent_ref kind=%s ordinal=%zu "
        "component_tag=0x%08X property_tag=0x%08X package=0x%04X "
        "component_entry=0x%04X property_entry=0x%04X offset=0x%zX",
        kind,
        ordinal,
        componentTag,
        propertyTag,
        packages::tables::package_of(componentTag),
        componentTag & packages::reader::layout::kTagEntryMask,
        propertyTag & packages::reader::layout::kTagEntryMask,
        offset);
    if (count > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** Emits one bounded summary even when no required property exists in the candidate families. */
void report_omega_scan_summary(const char* result,
                               const packages::reader::ScanResult& propertyScan,
                               const packages::reader::ScanResult& componentScan,
                               const OmegaTagCollection& properties,
                               const OmegaTagCollection& components,
                               std::size_t propertyReads,
                               std::size_t propertyReadFailures,
                               std::uint64_t propertyBytes,
                               const std::array<std::size_t, kOmegaNeedles.size()>& hits,
                               std::size_t requiredTags,
                               std::size_t componentReads,
                               std::size_t componentReadFailures,
                               std::size_t parentRefs) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_property_scan stage=complete result=%s property_class=0x%08X "
        "component_class=0x%08X required_hash=0x%08X packages=%04X,%04X "
        "property_sweep_packages=%zu property_sweep_matches=%zu property_tags=%zu "
        "property_reads=%zu property_read_failures=%zu property_bytes=%llu "
        "required_hits=%zu required_tags=%zu observed_a_hits=%zu observed_b_hits=%zu "
        "component_sweep_packages=%zu component_sweep_matches=%zu component_tags=%zu "
        "component_reads=%zu component_read_failures=%zu parent_refs=%zu "
        "property_overflow=%u component_overflow=%u mutation=observe_only",
        result,
        kOmegaPropertyClass,
        kOmegaComponentClass,
        kOmegaRequiredHash,
        kOmegaPackageIds[0],
        kOmegaPackageIds[1],
        propertyScan.packages,
        propertyScan.matches,
        properties.count,
        propertyReads,
        propertyReadFailures,
        static_cast<unsigned long long>(propertyBytes),
        hits[0],
        requiredTags,
        hits[1],
        hits[2],
        componentScan.packages,
        componentScan.matches,
        components.count,
        componentReads,
        componentReadFailures,
        parentRefs,
        properties.overflow ? 1U : 0U,
        components.overflow ? 1U : 0U);
    if (count > 0) {
        core::log::write(core::log::Channel::client,
                         std::string_view(result) == "ok" ? core::log::Level::info
                                                          : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** Known named roots of the Red War opening activity. */
constexpr std::array<Target, 17> kHomecomingTargets{{
    {0x80B500ACU, L"mission_towerfall.activity.bin"},
    {0x80B500BCU, L"mission_towerfall.scenario.bin"},
    {0x80B508A9U, L"mission_towerfall.scenario_patch.bin"},
    {0x80FDB97FU, L"mission_towerfall.launch_descriptor.bin"},
    {0x80FEB810U, L"mission_towerfall.launch_child_80feb810.bin"},
    {0x80FDB980U, L"mission_towerfall.launch_child_80fdb980.bin"},
    {0x80FDB981U, L"mission_towerfall.launch_child_80fdb981.bin"},
    {0x80FC16CCU, L"mission_towerfall.launch_leaf_80fc16cc.bin"},
    {0x80FC1354U, L"mission_towerfall.launch_leaf_80fc1354.bin"},
    {0x80FC1398U, L"mission_towerfall.launch_leaf_80fc1398.bin"},
    {0x80B754B4U, L"investment_globals.01bb.bin"},
    {0x8133719CU, L"investment_globals.059b.bin"},
    {0x81A2926EU, L"investment_globals.0914.bin"},
    {0x81327D22U, L"investment_root.bin"},
    {0x80B4A0EFU, L"city_tower_social_d2.activity.bin"},
    {0x80B4A0E1U, L"cine_110_twr.activity.bin"},
    {0x80B4A0EAU, L"cine_110_twr.scenario.bin"},
}};

constexpr std::uint32_t kHomecomingDefinitionHash = 0x9ACCB518U;
constexpr std::uint32_t kHomecomingActivityTag = 0x80B500ACU;
constexpr std::uint32_t kTowerCinematicActivityTag = 0x80B4A0E1U;
/** Investment activity hash printed by the client when it launches the bundled Tower index 20. */
constexpr std::uint32_t kTowerInvestmentActivityHash = 0xE8ABA41BU;
constexpr std::uint32_t kInvestmentRootTag = 0x81327D22U;

struct TableNeedle {
    std::uint32_t value{};
    std::string_view name;
};

/** Values that distinguish the public activity table from package-definition tables. */
constexpr std::array<TableNeedle, 4> kTableNeedles{{
    {kTowerInvestmentActivityHash, "tower_investment"},
    {kHomecomingDefinitionHash, "homecoming_package"},
    {kHomecomingActivityTag, "homecoming_activity_tag"},
    {kTowerCinematicActivityTag, "cine_activity_tag"},
}};

/** Writes one complete decoded tag blob without truncating it to a Windows DWORD. */
[[nodiscard]] bool write_blob(const wchar_t* path, std::span<const std::byte> bytes) noexcept {
    if (path == nullptr || bytes.empty()
        || bytes.size() > (std::numeric_limits<DWORD>::max)()) {
        return false;
    }
    const HANDLE file = CreateFileW(path,
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
    const bool complete =
        WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE
        && written == bytes.size() && FlushFileBuffers(file) != FALSE;
    return CloseHandle(file) != FALSE && complete;
}

/** Reports one decoded root so its class and byte size survive outside the binary dump. */
void report(std::uint32_t tag,
            std::uint32_t classId,
            std::size_t size,
            const char* result) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(line.data(),
                                    line.size(),
                                    "ev=homecoming stage=dump tag=0x%08X class=0x%08X "
                                    "bytes=%zu result=%s",
                                    tag,
                                    classId,
                                    size,
                                    result);
    if (count > 0) {
        core::log::write(core::log::Channel::client,
                         result == std::string_view("ok") ? core::log::Level::info
                                                          : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** Finds one little-endian scalar anywhere in a decoded definition blob. */
[[nodiscard]] bool find_value(std::span<const std::byte> bytes,
                              std::uint32_t value,
                              std::size_t& offset) noexcept {
    offset = 0;
    if (bytes.size() < sizeof value) {
        return false;
    }
    for (std::size_t candidate = 0; candidate <= bytes.size() - sizeof value; ++candidate) {
        std::uint32_t found = 0;
        std::memcpy(&found, bytes.data() + candidate, sizeof found);
        if (found == value) {
            offset = candidate;
            return true;
        }
    }
    return false;
}

/**
 * Locates the public activity table and the package-definition table independently.
 * The package activity hash is not the investment activity hash the descriptor indexes, so an
 * index-row parser cannot be used to equate them. Raw matches preserve the table evidence needed
 * to recover each layout without making that assumption again.
 */
[[nodiscard]] bool dump_homecoming_table(const packages::reader::Source& source,
                                         packages::reader::Scratch& scratch,
                                         const wchar_t* directory) noexcept {
    namespace tables = middleware::content::packages::tables;
    std::vector<std::byte> root;
    std::uint32_t rootClass = 0;
    if (!packages::reader::read_tag(source, scratch, kInvestmentRootTag, root, rootClass)) {
        return false;
    }

    bool foundHomecomingPackage = false;
    bool foundTowerInvestment = false;
    std::vector<std::byte> table;
    for (std::size_t slot = 0; slot < tables::child_count(root); ++slot) {
        std::uint32_t tableTag = 0;
        if (!tables::slot_tag(root, slot, tableTag) || tableTag == 0 || tableTag == 0xFFFFFFFFU) {
            continue;
        }
        std::uint32_t tableClass = 0;
        if (!packages::reader::read_tag(source, scratch, tableTag, table, tableClass)) {
            continue;
        }
        tables::Array array{};
        const bool hasArray =
            tables::find_array_at(table, tables::kTableArrayDescriptor, array);
        bool wroteTable = false;
        for (const TableNeedle& needle : kTableNeedles) {
            std::size_t offset = 0;
            if (!find_value(table, needle.value, offset)) {
                continue;
            }
            foundHomecomingPackage =
                foundHomecomingPackage || needle.value == kHomecomingDefinitionHash;
            foundTowerInvestment =
                foundTowerInvestment || needle.value == kTowerInvestmentActivityHash;
            if (!wroteTable) {
                core::path::Buffer path{};
                if (!core::path::assign(path, directory)) {
                    return false;
                }
                std::array<wchar_t, 96> filename{};
                const int filenameLength = std::swprintf(filename.data(),
                                                         filename.size(),
                                                         L"\\red_war.table.slot_%zu.tag_%08X.bin",
                                                         slot,
                                                         tableTag);
                if (filenameLength <= 0 || !core::path::append(path, filename.data())
                    || !write_blob(path.chars.data(), table)) {
                    return false;
                }
                wroteTable = true;
            }
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(line.data(),
                                             line.size(),
                                             "ev=homecoming stage=table needle=%.*s value=0x%08X "
                                             "slot=%zu offset=%zu table=0x%08X class=0x%08X "
                                             "array=%llu data=%zu element=0x%08X result=ok",
                                             static_cast<int>(needle.name.size()),
                                             needle.name.data(),
                                             needle.value,
                                             slot,
                                             offset,
                                             tableTag,
                                             tableClass,
                                             static_cast<unsigned long long>(hasArray ? array.count
                                                                                      : 0),
                                             hasArray ? array.dataOffset : 0,
                                             hasArray ? array.elementClass : 0U);
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return foundHomecomingPackage && foundTowerInvestment;
}

} // namespace

/** Dumps the decoded Homecoming roots once package keys are available. */
bool dump_homecoming(const packages::reader::Source& source,
                     packages::reader::Scratch& scratch) noexcept {
    const HMODULE module = GetModuleHandleW(L"steam_api64.dll");
    core::path::Buffer directory{};
    if (module == nullptr || !core::path::artifact_directory(module, directory)
        || !core::path::append(directory, L"\\analysis")) {
        return false;
    }
    if (CreateDirectoryW(directory.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }

    bool complete = true;
    std::vector<std::byte> blob;
    for (const Target& target : kHomecomingTargets) {
        std::uint32_t classId = 0;
        if (!packages::reader::read_tag(source, scratch, target.tag, blob, classId)) {
            report(target.tag, classId, 0, "read");
            complete = false;
            continue;
        }
        core::path::Buffer path = directory;
        if (!core::path::append(path, L"\\") || !core::path::append(path, target.filename)
            || !write_blob(path.chars.data(), blob)) {
            report(target.tag, classId, blob.size(), "write");
            complete = false;
            continue;
        }
        report(target.tag, classId, blob.size(), "ok");
    }
    return dump_homecoming_table(source, scratch, directory.chars.data()) && complete;
}

/**
 * Resolves the exact property definition behind condition 0x75 and, when possible, the component
 * definition that references it. The observed hashes and tags are scanned alongside the required
 * hash so a malformed package walk cannot masquerade as a useful negative result.
 */
bool scan_omega_behavior_properties(const packages::reader::Source& source,
                                    packages::reader::Scratch& scratch) noexcept {
    OmegaTagCollection properties{};
    packages::reader::ScanResult propertyScan{};
    if (!packages::reader::scan_class_entries(source.directory,
                                              kOmegaPropertyClass,
                                              &collect_omega_tag,
                                              &properties,
                                              propertyScan)) {
        report_omega_scan_summary("property_sweep",
                                  propertyScan,
                                  {},
                                  properties,
                                  {},
                                  0,
                                  0,
                                  0,
                                  {},
                                  0,
                                  0,
                                  0,
                                  0);
        return false;
    }

    std::vector<std::byte> blob;
    std::array<std::size_t, kOmegaNeedles.size()> hits{};
    std::array<std::uint32_t, kOmegaRequiredTagCapacity> requiredTags{};
    std::size_t requiredTagCount = 0;
    std::size_t propertyReads = 0;
    std::size_t propertyReadFailures = 0;
    std::size_t propertyHitLogs = 0;
    std::uint64_t propertyBytes = 0;
    for (std::size_t index = 0; index < properties.count; ++index) {
        const std::uint32_t tag = properties.tags[index];
        std::uint32_t classId = 0;
        if (!packages::reader::read_tag(source, scratch, tag, blob, classId)) {
            ++propertyReadFailures;
            continue;
        }
        ++propertyReads;
        propertyBytes += blob.size();
        if (blob.size() < sizeof(std::uint32_t)) {
            continue;
        }
        for (std::size_t offset = 0; offset <= blob.size() - sizeof(std::uint32_t); ++offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, blob.data() + offset, sizeof value);
            for (std::size_t needleIndex = 0; needleIndex < kOmegaNeedles.size(); ++needleIndex) {
                const OmegaNeedle& needle = kOmegaNeedles[needleIndex];
                if (value != needle.hash) {
                    continue;
                }
                ++hits[needleIndex];
                if (needle.hash == kOmegaRequiredHash) {
                    add_unique_tag(requiredTags, requiredTagCount, tag);
                }
                if (propertyHitLogs < kOmegaHitLogCapacity) {
                    report_omega_property_hit(
                        needle, tag, classId, blob.size(), offset, hits[needleIndex]);
                    ++propertyHitLogs;
                }
            }
        }
    }

    OmegaTagCollection components{};
    packages::reader::ScanResult componentScan{};
    if (!packages::reader::scan_class_entries(source.directory,
                                              kOmegaComponentClass,
                                              &collect_omega_tag,
                                              &components,
                                              componentScan)) {
        report_omega_scan_summary("component_sweep",
                                  propertyScan,
                                  componentScan,
                                  properties,
                                  components,
                                  propertyReads,
                                  propertyReadFailures,
                                  propertyBytes,
                                  hits,
                                  requiredTagCount,
                                  0,
                                  0,
                                  0);
        return false;
    }

    std::array<std::uint32_t, kOmegaRequiredTagCapacity + 2> watchedTags{};
    watchedTags[0] = kOmegaNeedles[1].expectedTag;
    watchedTags[1] = kOmegaNeedles[2].expectedTag;
    std::size_t watchedTagCount = 2;
    for (std::size_t index = 0;
         index < requiredTagCount && watchedTagCount < watchedTags.size();
         ++index) {
        bool duplicate = false;
        for (std::size_t existing = 0; existing < watchedTagCount; ++existing) {
            duplicate = duplicate || watchedTags[existing] == requiredTags[index];
        }
        if (!duplicate) {
            watchedTags[watchedTagCount++] = requiredTags[index];
        }
    }

    std::size_t componentReads = 0;
    std::size_t componentReadFailures = 0;
    std::size_t parentRefs = 0;
    std::size_t parentLogs = 0;
    for (std::size_t index = 0; index < components.count; ++index) {
        const std::uint32_t componentTag = components.tags[index];
        std::uint32_t classId = 0;
        if (!packages::reader::read_tag(source, scratch, componentTag, blob, classId)) {
            ++componentReadFailures;
            continue;
        }
        ++componentReads;
        if (blob.size() < sizeof(std::uint32_t)) {
            continue;
        }
        for (std::size_t offset = 0; offset <= blob.size() - sizeof(std::uint32_t); ++offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, blob.data() + offset, sizeof value);
            for (std::size_t watched = 0; watched < watchedTagCount; ++watched) {
                if (value != watchedTags[watched]) {
                    continue;
                }
                ++parentRefs;
                if (parentLogs < kOmegaParentLogCapacity) {
                    const char* const kind = watched == 0 ? "observed_9CBEE071"
                                              : watched == 1 ? "observed_394E1E5D"
                                                             : "required";
                    report_omega_parent(componentTag, value, kind, offset, parentRefs);
                    ++parentLogs;
                }
            }
        }
    }

    const bool observedValidated = hits[1] != 0 && hits[2] != 0;
    report_omega_scan_summary(observedValidated ? "ok" : "validation",
                              propertyScan,
                              componentScan,
                              properties,
                              components,
                              propertyReads,
                              propertyReadFailures,
                              propertyBytes,
                              hits,
                              requiredTagCount,
                              componentReads,
                              componentReadFailures,
                              parentRefs);
    return observedValidated && !properties.overflow && !components.overflow;
}

} // namespace dawn::client::content::activity
