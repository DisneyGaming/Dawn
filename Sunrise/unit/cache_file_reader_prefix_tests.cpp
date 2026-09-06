#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>

#include "../src/core/logging/log.h"
#include "../src/core/provenance/build_provenance.h"
#include "../src/state/build_data/cache/internal.h"
#include "../src/state/build_data/cache/read/cache_payload_reader.h"

namespace {

int g_failureCount = 0;
bool g_unexpectedPayloadHelperCall = false;
bool g_allowSyntheticPayload = false;
constexpr std::uint64_t kSyntheticChecksum = 0x123456789ABCDEF0ULL;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }

    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void unexpected_payload_helper(const char* helper) noexcept {
    std::cerr << "prefix test unexpectedly reached payload helper: " << helper << '\n';
    g_unexpectedPayloadHelperCall = true;
}

template <typename Value> void clear_span(std::span<Value> values) noexcept {
    std::fill(values.begin(), values.end(), Value{});
}

} // namespace

namespace sunrise::state::build_data::cache::read {

// `load` always clears its caller-owned transaction before opening the file. Keep that reachable
// dependency real in this standalone test; only the later payload-only dependencies are stubbed.
void clear(records::MutableDomains output) noexcept {
    if (output.constants != nullptr) {
        *output.constants = {};
    }
    clear_span(output.named);
    clear_span(output.items);
    clear_span(output.collectibles);
    clear_span(output.materialRequirementSets);
    clear_span(output.itemDetails);
    clear_span(output.socketPlugRules);
    clear_span(output.socketPlugPools);
    clear_span(output.socketPlugMembers);
    clear_span(output.inventoryBuckets);
    clear_span(output.socketEntryLists);
    clear_span(output.socketEntryTables);
    clear_span(output.abilityBuckets);
    clear_span(output.progressions);
    clear_span(output.scenarios);
    clear_span(output.rosterGroups);
    clear_span(output.spawnStems);
    clear_span(output.spawnNameHashes);
    clear_span(output.spawnPoints);
    clear_span(output.hashNames);
    clear_span(output.vendorIndex);
    clear_span(output.vendorDefinitions);
    clear_span(output.vendorSaleRows);
    clear_span(output.vendorInstalledRows);
}

bool expected_size(const records::DomainCounts&, std::uint64_t& size) noexcept {
    if (g_allowSyntheticPayload) {
        size = sizeof(records::Header);
        return true;
    }
    unexpected_payload_helper("expected_size");
    return false;
}

bool read_payload(HANDLE,
                  const records::InvestmentConstants&,
                  const records::DomainCounts&,
                  records::MutableDomains,
                   std::uint64_t& checksum) noexcept {
    if (g_allowSyntheticPayload) {
        checksum = kSyntheticChecksum;
        return true;
    }
    unexpected_payload_helper("read_payload");
    return false;
}

} // namespace sunrise::state::build_data::cache::read

namespace {

using sunrise::state::build_data::BuildIdentity;
using sunrise::state::build_data::cache::LoadStatus;
namespace cache = sunrise::state::build_data::cache;
namespace records = sunrise::state::build_data::cache::records;

class TemporaryCacheFile final {
  public:
    TemporaryCacheFile() {
        std::array<wchar_t, MAX_PATH + 1> temporaryRoot{};
        const DWORD rootLength = GetTempPathW(static_cast<DWORD>(temporaryRoot.size()),
                                              temporaryRoot.data());
        if (rootLength == 0 || rootLength >= temporaryRoot.size()) {
            CHECK(false && "GetTempPathW failed");
            return;
        }

        const std::wstring base = std::wstring(temporaryRoot.data(), rootLength)
                                  + L"sunrise-cache-prefix-"
                                  + std::to_wstring(GetCurrentProcessId()) + L'-'
                                  + std::to_wstring(GetTickCount64());
        for (unsigned attempt = 0; attempt != 128; ++attempt) {
            directory_ = base + L'-' + std::to_wstring(attempt);
            if (CreateDirectoryW(directory_.c_str(), nullptr) != FALSE) {
                path_ = directory_ + L"\\cache.bin";
                return;
            }
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                break;
            }
        }

        directory_.clear();
        CHECK(false && "could not create a unique temporary directory");
    }

    TemporaryCacheFile(const TemporaryCacheFile&) = delete;
    TemporaryCacheFile& operator=(const TemporaryCacheFile&) = delete;

    ~TemporaryCacheFile() {
        if (!path_.empty()) {
            (void)DeleteFileW(path_.c_str());
        }
        if (!directory_.empty()) {
            (void)RemoveDirectoryW(directory_.c_str());
        }
    }

    [[nodiscard]] bool ready() const noexcept { return !path_.empty(); }

    [[nodiscard]] bool remove() const noexcept {
        if (DeleteFileW(path_.c_str()) != FALSE) {
            return true;
        }
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    [[nodiscard]] bool write(std::span<const std::byte> bytes) const noexcept {
        const HANDLE file = CreateFileW(path_.c_str(),
                                        GENERIC_WRITE,
                                        0,
                                        nullptr,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD transferred = 0;
        const DWORD requested = static_cast<DWORD>(bytes.size());
        const bool written =
            (requested == 0
             || (WriteFile(file, bytes.data(), requested, &transferred, nullptr) != FALSE
                 && transferred == requested));
        return CloseHandle(file) != FALSE && written;
    }

    template <typename Value> [[nodiscard]] bool write_value(const Value& value) const noexcept {
        return write(std::as_bytes(std::span(&value, std::size_t{1})));
    }

    [[nodiscard]] const wchar_t* path() const noexcept { return path_.c_str(); }

  private:
    std::wstring directory_;
    std::wstring path_;
};

struct ReaderFixture final {
    records::InvestmentConstants constants{};
    records::MutableDomains output{};
    records::DomainCounts counts{};
    BuildIdentity expectedBuild{1, 1, 1};
    cache::LoadDiagnostic diagnostic{};
    std::array<sunrise::state::content::Definition, 1> named{};
    std::array<sunrise::state::build_data::items::Definition, 1> items{};
    std::array<sunrise::state::build_data::collectibles::Definition, 1> collectibles{};
    std::array<sunrise::state::build_data::material_requirements::Definition, 1>
        materialRequirements{};
    std::array<sunrise::state::build_data::items::socket_plugs::Rule, 1> socketPlugRules{};
    std::array<sunrise::state::build_data::items::socket_plugs::Pool, 1> socketPlugPools{};
    std::array<sunrise::state::build_data::inventory::buckets::Descriptor, 1> inventoryBuckets{};
    std::array<sunrise::state::build_data::socket_entry_lists::Definition, 1> socketEntryLists{};
    std::array<sunrise::state::build_data::progressions::Definition, 1> progressions{};
    std::array<sunrise::state::build_data::scenarios::Definition, 1> scenarios{};

    ReaderFixture() {
        output.constants = &constants;
        output.named = named;
        output.items = items;
        output.collectibles = collectibles;
        output.materialRequirementSets = materialRequirements;
        output.socketPlugRules = socketPlugRules;
        output.socketPlugPools = socketPlugPools;
        output.inventoryBuckets = inventoryBuckets;
        output.socketEntryLists = socketEntryLists;
        output.progressions = progressions;
        output.scenarios = scenarios;
    }

    [[nodiscard]] LoadStatus load(const TemporaryCacheFile& file) noexcept {
        constants.extracted = 1;
        counts.named = 1;
        return cache::load(file.path(), expectedBuild, output, counts, &diagnostic);
    }
};

void expect_status(const char* caseName, LoadStatus actual, LoadStatus expected) {
    if (actual == expected) {
        return;
    }
    std::cerr << caseName << ": expected status " << static_cast<int>(expected) << ", got "
              << static_cast<int>(actual) << '\n';
    ++g_failureCount;
}

[[nodiscard]] records::Prefix prefix_with_version(std::uint32_t version) noexcept {
    records::Prefix prefix{};
    prefix.magic = records::kCacheMagic;
    prefix.version = version;
    return prefix;
}

[[nodiscard]] records::Header header_for_build(const BuildIdentity& build) noexcept {
    records::Header header{};
    header.magic = records::kCacheMagic;
    header.version = records::kCacheFormatVersion;
    header.imageTimestamp = build.imageTimestamp;
    header.imageSize = build.imageSize;
    header.configuredEquipmentHash = build.configuredEquipmentHash;
    header.producerSourceSha256 = build.producerSourceSha256;
    header.producerImageSha256 = build.producerImageSha256;
    return header;
}

void cng_file_sha256_matches_known_vector(TemporaryCacheFile& file) {
    constexpr std::array<std::byte, 3> input{
        std::byte{'a'},
        std::byte{'b'},
        std::byte{'c'},
    };
    constexpr sunrise::core::provenance::Sha256Digest expected{
        std::byte{0xBA}, std::byte{0x78}, std::byte{0x16}, std::byte{0xBF},
        std::byte{0x8F}, std::byte{0x01}, std::byte{0xCF}, std::byte{0xEA},
        std::byte{0x41}, std::byte{0x41}, std::byte{0x40}, std::byte{0xDE},
        std::byte{0x5D}, std::byte{0xAE}, std::byte{0x22}, std::byte{0x23},
        std::byte{0xB0}, std::byte{0x03}, std::byte{0x61}, std::byte{0xA3},
        std::byte{0x96}, std::byte{0x17}, std::byte{0x7A}, std::byte{0x9C},
        std::byte{0xB4}, std::byte{0x10}, std::byte{0xFF}, std::byte{0x61},
        std::byte{0xF2}, std::byte{0x00}, std::byte{0x15}, std::byte{0xAD},
    };
    CHECK(file.write(input));
    sunrise::core::provenance::Sha256Digest actual{};
    CHECK(sunrise::core::provenance::file_sha256(file.path(), actual));
    CHECK(actual == expected);

    constexpr sunrise::core::provenance::Sha256Digest emptyExpected{
        std::byte{0xE3}, std::byte{0xB0}, std::byte{0xC4}, std::byte{0x42},
        std::byte{0x98}, std::byte{0xFC}, std::byte{0x1C}, std::byte{0x14},
        std::byte{0x9A}, std::byte{0xFB}, std::byte{0xF4}, std::byte{0xC8},
        std::byte{0x99}, std::byte{0x6F}, std::byte{0xB9}, std::byte{0x24},
        std::byte{0x27}, std::byte{0xAE}, std::byte{0x41}, std::byte{0xE4},
        std::byte{0x64}, std::byte{0x9B}, std::byte{0x93}, std::byte{0x4C},
        std::byte{0xA4}, std::byte{0x95}, std::byte{0x99}, std::byte{0x1B},
        std::byte{0x78}, std::byte{0x52}, std::byte{0xB8}, std::byte{0x55},
    };
    CHECK(file.write({}));
    CHECK(sunrise::core::provenance::file_sha256(file.path(), actual));
    CHECK(actual == emptyExpected);
}

void current_identity_preserves_destiny_fields_and_hashes_producer() {
    constexpr std::uint64_t kConfiguredEquipmentHash = 0xFEDCBA9876543210ULL;
    CHECK(!sunrise::core::provenance::capture(GetModuleHandleW(L"kernel32.dll")));
    CHECK(sunrise::core::provenance::capture(GetModuleHandleW(nullptr)));
    BuildIdentity identity{};
    CHECK(cache::current_build_identity(kConfiguredEquipmentHash, identity));
    CHECK(identity.configuredEquipmentHash == kConfiguredEquipmentHash);

    const HMODULE destiny = GetModuleHandleW(nullptr);
    CHECK(destiny != nullptr);
    if (destiny != nullptr) {
        const auto* image = reinterpret_cast<const std::byte*>(destiny);
        IMAGE_DOS_HEADER dos{};
        std::memcpy(&dos, image, sizeof dos);
        CHECK(dos.e_magic == IMAGE_DOS_SIGNATURE);
        if (dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0) {
            IMAGE_NT_HEADERS64 nt{};
            std::memcpy(&nt, image + dos.e_lfanew, sizeof nt);
            CHECK(identity.imageTimestamp == nt.FileHeader.TimeDateStamp);
            CHECK(identity.imageSize == nt.OptionalHeader.SizeOfImage);
        }
    }
    CHECK(std::any_of(identity.producerSourceSha256.begin(),
                      identity.producerSourceSha256.end(),
                      [](std::byte value) { return value != std::byte{}; }));
    CHECK(std::any_of(identity.producerImageSha256.begin(),
                      identity.producerImageSha256.end(),
                      [](std::byte value) { return value != std::byte{}; }));

    std::array<char, sunrise::core::log::kLineCapacity> line{};
    std::size_t length = 0;
    CHECK(sunrise::core::provenance::format_startup_record(line, length));
    CHECK(length < line.size());
    const std::string_view event(line.data(), length);
    CHECK(event.find("ev=build_identity schema=1") == 0);
    CHECK(event.find(" source_sha256=") != std::string_view::npos);
    CHECK(event.find(" module_sha256=") != std::string_view::npos);
    CHECK(event.find("source_path") == std::string_view::npos);
    CHECK(event.find("build_time") == std::string_view::npos);
}

void missing_file_is_missing(TemporaryCacheFile& file, ReaderFixture& fixture) {
    CHECK(file.remove());
    expect_status("missing file", fixture.load(file), LoadStatus::missing);
}

void bad_magic_is_invalid(TemporaryCacheFile& file, ReaderFixture& fixture) {
    records::Prefix prefix = prefix_with_version(records::kCacheFormatVersion);
    prefix.magic[0] = prefix.magic[0] == 'X' ? 'Y' : 'X';
    CHECK(file.write_value(prefix));
    expect_status("bad magic", fixture.load(file), LoadStatus::invalid);
}

void truncated_prefix_is_invalid(TemporaryCacheFile& file, ReaderFixture& fixture) {
    const records::Prefix prefix = prefix_with_version(records::kCacheFormatVersion);
    const auto bytes = std::as_bytes(std::span(&prefix, std::size_t{1}));
    CHECK(bytes.size() > 1);
    CHECK(file.write(bytes.first(bytes.size() - 1)));
    expect_status("truncated prefix", fixture.load(file), LoadStatus::invalid);
}

void noncurrent_valid_prefix_is_stale(TemporaryCacheFile& file,
                                      ReaderFixture& fixture,
                                      std::uint32_t version,
                                      const char* caseName) {
    CHECK(version != records::kCacheFormatVersion);
    const records::Prefix prefix = prefix_with_version(version);
    CHECK(file.write_value(prefix));
    expect_status(caseName, fixture.load(file), LoadStatus::stale);
}

void prior_version_is_stale(TemporaryCacheFile& file, ReaderFixture& fixture) {
    static_assert(records::kCacheFormatVersion > 0);
    noncurrent_valid_prefix_is_stale(file,
                                     fixture,
                                     records::kCacheFormatVersion - 1,
                                     "current version minus one");
}

void zero_format_is_present_and_stale(TemporaryCacheFile& file, ReaderFixture& fixture) {
    noncurrent_valid_prefix_is_stale(file, fixture, 0, "format version zero");
    CHECK(fixture.diagnostic.observedFormatAvailable);
    CHECK(fixture.diagnostic.observedFormat == 0);

    std::array<char, sunrise::core::log::kLineCapacity> line{};
    std::size_t length = 0;
    CHECK(cache::format_outcome_record(fixture.expectedBuild, fixture.diagnostic, line, length));
    const std::string_view event(line.data(), length);
    CHECK(event.find("format_cached=0") != std::string_view::npos);
    CHECK(event.find("format_cached=absent") == std::string_view::npos);
}

void transition_versions_are_stale_when_noncurrent(TemporaryCacheFile& file,
                                                    ReaderFixture& fixture) {
    constexpr std::array<std::uint32_t, 8> transitionVersions{39, 40, 44, 45, 46, 47, 49, 50};
    constexpr std::array<const char*, 8> caseNames{
        "format version 39",
        "format version 40",
        "format version 44",
        "format version 45",
        "format version 46",
        "format version 47",
        "format version 49",
        "format version 50"};

    for (std::size_t index = 0; index != transitionVersions.size(); ++index) {
        if (transitionVersions[index] != records::kCacheFormatVersion) {
            noncurrent_valid_prefix_is_stale(
                file, fixture, transitionVersions[index], caseNames[index]);
        }
    }
}

void current_prefix_with_truncated_header_is_invalid(TemporaryCacheFile& file,
                                                     ReaderFixture& fixture) {
    const records::Prefix prefix = prefix_with_version(records::kCacheFormatVersion);
    CHECK(file.write_value(prefix));
    expect_status("current prefix with truncated header",
                  fixture.load(file),
                  LoadStatus::invalid);
}

void exact_current_identity_loads(TemporaryCacheFile& file, ReaderFixture& fixture) {
    records::Header header = header_for_build(fixture.expectedBuild);
    header.namedCount = 1;
    header.itemCount = 1;
    header.collectibleCount = 1;
    header.materialRequirementSetCount = 1;
    header.socketPlugRuleCount = 1;
    header.socketPlugPoolCount = 1;
    header.inventoryBucketCount = 1;
    header.socketEntryListCount = 1;
    header.progressionCount = 1;
    header.scenarioCount = 1;
    header.payloadChecksum = kSyntheticChecksum;
    CHECK(file.write_value(header));
    g_allowSyntheticPayload = true;
    expect_status("exact current format identity", fixture.load(file), LoadStatus::loaded);
    g_allowSyntheticPayload = false;
    CHECK(fixture.diagnostic.reason == cache::LoadReason::match);
    CHECK(fixture.diagnostic.cachedIdentityAvailable);
    CHECK(fixture.diagnostic.cachedIdentity == fixture.expectedBuild);
    CHECK(fixture.counts.named == 1);
}

void producer_digest_mismatches_are_independent(TemporaryCacheFile& file,
                                                ReaderFixture& fixture) {
    records::Header header = header_for_build(fixture.expectedBuild);
    header.producerSourceSha256[0] = std::byte{0xA5};
    CHECK(file.write_value(header));
    expect_status("different Sunrise source digest", fixture.load(file), LoadStatus::stale);
    CHECK(fixture.diagnostic.mismatches == cache::Mismatch::producerSource);

    header = header_for_build(fixture.expectedBuild);
    header.producerImageSha256[0] = std::byte{0x5A};
    CHECK(file.write_value(header));
    expect_status("different Sunrise image digest", fixture.load(file), LoadStatus::stale);
    CHECK(fixture.diagnostic.mismatches == cache::Mismatch::producerImage);

    header.producerSourceSha256[0] = std::byte{0xA5};
    CHECK(file.write_value(header));
    expect_status("different Sunrise source and image digest", fixture.load(file), LoadStatus::stale);
    CHECK(fixture.diagnostic.mismatches
          == (cache::Mismatch::producerSource | cache::Mismatch::producerImage));
}

void cache_record_is_bounded_and_stable(ReaderFixture& fixture) {
    cache::LoadDiagnostic diagnostic{};
    diagnostic.status = LoadStatus::stale;
    diagnostic.reason = cache::LoadReason::identity;
    diagnostic.observedFormat = records::kCacheFormatVersion;
    diagnostic.observedFormatAvailable = true;
    diagnostic.cachedIdentityAvailable = true;
    diagnostic.cachedIdentity = fixture.expectedBuild;
    diagnostic.mismatches = cache::Mismatch::producerSource | cache::Mismatch::producerImage;
    std::array<char, sunrise::core::log::kLineCapacity> line{};
    std::size_t length = 0;
    CHECK(cache::format_outcome_record(fixture.expectedBuild, diagnostic, line, length));
    CHECK(length < line.size());
    const std::string_view event(line.data(), length);
    CHECK(event.find("reason=producer_source,producer_image") != std::string_view::npos);
    CHECK(event.find("source_cached=absent") == std::string_view::npos);

    diagnostic.status = LoadStatus::stale;
    diagnostic.reason = cache::LoadReason::format;
    diagnostic.observedFormat = 47;
    diagnostic.observedFormatAvailable = true;
    diagnostic.cachedIdentityAvailable = false;
    CHECK(cache::format_outcome_record(fixture.expectedBuild, diagnostic, line, length));
    const std::string_view oldFormat(line.data(), length);
    CHECK(oldFormat.find("reason=format") != std::string_view::npos);
    CHECK(oldFormat.find("source_cached=absent") != std::string_view::npos);
    CHECK(oldFormat.find("image_cached=absent") != std::string_view::npos);
}

void existing_identity_fields_still_control_staleness(TemporaryCacheFile& file,
                                                      ReaderFixture& fixture) {
    records::Header header = header_for_build(fixture.expectedBuild);
    ++header.imageTimestamp;
    CHECK(file.write_value(header));
    expect_status("different Destiny timestamp", fixture.load(file), LoadStatus::stale);

    header = header_for_build(fixture.expectedBuild);
    ++header.imageSize;
    CHECK(file.write_value(header));
    expect_status("different Destiny image size", fixture.load(file), LoadStatus::stale);

    header = header_for_build(fixture.expectedBuild);
    ++header.configuredEquipmentHash;
    CHECK(file.write_value(header));
    expect_status("different equipment hash", fixture.load(file), LoadStatus::stale);
}

} // namespace

int main() {
    TemporaryCacheFile file;
    if (!file.ready()) {
        return 1;
    }
    ReaderFixture fixture;

    cng_file_sha256_matches_known_vector(file);
    current_identity_preserves_destiny_fields_and_hashes_producer();
    missing_file_is_missing(file, fixture);
    bad_magic_is_invalid(file, fixture);
    truncated_prefix_is_invalid(file, fixture);
    prior_version_is_stale(file, fixture);
    zero_format_is_present_and_stale(file, fixture);
    transition_versions_are_stale_when_noncurrent(file, fixture);
    current_prefix_with_truncated_header_is_invalid(file, fixture);
    exact_current_identity_loads(file, fixture);
    producer_digest_mismatches_are_independent(file, fixture);
    cache_record_is_bounded_and_stable(fixture);
    existing_identity_fields_still_control_staleness(file, fixture);

    CHECK(!g_unexpectedPayloadHelperCall);
    sunrise::core::provenance::clear();
    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " cache file reader prefix check(s) failed\n";
        return 1;
    }

    std::cout << "all cache file reader prefix checks passed (format version "
              << records::kCacheFormatVersion << ")\n";
    return 0;
}
