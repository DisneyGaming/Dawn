#include "build_provenance.h"

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>

#if __has_include("sunrise_build_identity.generated.h")
#include "sunrise_build_identity.generated.h"
#endif
#include "../../state/build_data/cache/records/version.h"
#include "../settings/settings.h"

// Provenance identity is normally supplied by tools/build/build_release_candidate.ps1 through a
// generated header. Ordinary in-place builds (the plain MSBuild invocation) do not produce it, so
// fall back to local-dev sentinels and still build a working DLL. The cache-format/settings-version
// defaults reference the live constants so the static_asserts below can never go stale.
#ifndef SUNRISE_PROVENANCE_SCHEMA
#define SUNRISE_PROVENANCE_SCHEMA 1
#endif
#ifndef SUNRISE_BUILD_ID
#define SUNRISE_BUILD_ID "local-dev"
#endif
#ifndef SUNRISE_GIT_HEAD
#define SUNRISE_GIT_HEAD "0000000000000000000000000000000000000000"
#endif
#ifndef SUNRISE_GIT_BRANCH
#define SUNRISE_GIT_BRANCH "local"
#endif
#ifndef SUNRISE_GIT_DIRTY
#define SUNRISE_GIT_DIRTY 1
#endif
#ifndef SUNRISE_SOURCE_SHA256
#define SUNRISE_SOURCE_SHA256 "0000000000000000000000000000000000000000000000000000000000000001"
#endif
#ifndef SUNRISE_BUILD_CONFIGURATION
#define SUNRISE_BUILD_CONFIGURATION "local"
#endif
#ifndef SUNRISE_BUILD_PLATFORM
#define SUNRISE_BUILD_PLATFORM "x64"
#endif
#ifndef SUNRISE_COMPILER_ID
#define SUNRISE_COMPILER_ID "local"
#endif
#ifndef SUNRISE_TOOLSET_ID
#define SUNRISE_TOOLSET_ID "v145"
#endif
#ifndef SUNRISE_WINDOWS_SDK
#define SUNRISE_WINDOWS_SDK "local"
#endif
#ifndef SUNRISE_CACHE_FORMAT
#define SUNRISE_CACHE_FORMAT state::build_data::cache::records::kCacheFormatVersion
#endif
#ifndef SUNRISE_SETTINGS_VERSION
#define SUNRISE_SETTINGS_VERSION settings::kSettingsVersion
#endif

namespace sunrise::core::provenance {
namespace {

constexpr std::size_t kModulePathCapacity = 32768;
constexpr std::size_t kHashReadBufferSize = 64 * 1024;
constexpr std::string_view kHexDigits = "0123456789ABCDEF";

template <std::size_t Size>
[[nodiscard]] consteval std::byte decode_nibble(const char (&text)[Size], std::size_t index) {
    const char digit = text[index];
    if (digit >= '0' && digit <= '9') {
        return static_cast<std::byte>(digit - '0');
    }
    if (digit >= 'A' && digit <= 'F') {
        return static_cast<std::byte>(digit - 'A' + 10);
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<std::byte>(digit - 'a' + 10);
    }
    throw "generated SHA-256 contains a non-hexadecimal digit";
}

template <std::size_t Size>
[[nodiscard]] consteval Sha256Digest decode_sha256(const char (&text)[Size]) {
    static_assert(Size == 65, "generated SHA-256 must contain exactly 64 hexadecimal digits");
    Sha256Digest digest{};
    for (std::size_t index = 0; index != digest.size(); ++index) {
        const unsigned high = std::to_integer<unsigned>(decode_nibble(text, index * 2));
        const unsigned low = std::to_integer<unsigned>(decode_nibble(text, index * 2 + 1));
        digest[index] = static_cast<std::byte>((high << 4U) | low);
    }
    return digest;
}

constexpr Sha256Digest kSourceSha256 = decode_sha256(SUNRISE_SOURCE_SHA256);
static_assert(std::any_of(kSourceSha256.begin(), kSourceSha256.end(), [](std::byte value) {
    return value != std::byte{};
}), "generated source SHA-256 must not be all zeroes");
static_assert(SUNRISE_CACHE_FORMAT
              == state::build_data::cache::records::kCacheFormatVersion,
              "generated cache format is stale; regenerate the frozen candidate identity");
static_assert(SUNRISE_SETTINGS_VERSION == settings::kSettingsVersion,
              "generated settings version is stale; regenerate the frozen candidate identity");

constexpr BuildMetadata kMetadata{
    SUNRISE_PROVENANCE_SCHEMA,
    SUNRISE_BUILD_ID,
    SUNRISE_GIT_HEAD,
    SUNRISE_GIT_BRANCH,
    SUNRISE_GIT_DIRTY != 0,
    kSourceSha256,
    SUNRISE_SOURCE_SHA256,
    SUNRISE_BUILD_CONFIGURATION,
    SUNRISE_BUILD_PLATFORM,
    SUNRISE_COMPILER_ID,
    SUNRISE_TOOLSET_ID,
    SUNRISE_WINDOWS_SDK,
    SUNRISE_CACHE_FORMAT,
    SUNRISE_SETTINGS_VERSION,
};

SRWLOCK g_identityLock{SRWLOCK_INIT};
RuntimeIdentity g_identity{};
std::atomic_bool g_available{false};
HMODULE g_capturedModule{};
std::byte g_moduleAnchor{};

[[nodiscard]] bool cng_succeeded(NTSTATUS status) noexcept {
    return status >= 0;
}

[[nodiscard]] HMODULE implementation_module() noexcept {
    HMODULE owner{};
    constexpr DWORD kFlags =
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExW(kFlags, reinterpret_cast<LPCWSTR>(&g_moduleAnchor), &owner) == FALSE) {
        return nullptr;
    }
    return owner;
}

[[nodiscard]] bool module_path(HMODULE module,
                               std::array<wchar_t, kModulePathCapacity>& path) noexcept {
    const DWORD copied =
        GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    return copied != 0 && copied < path.size();
}

void module_basename(std::wstring_view path, RuntimeIdentity& output) noexcept {
    const std::size_t slash = path.find_last_of(L"\\/");
    const std::wstring_view name =
        slash == std::wstring_view::npos ? path : path.substr(slash + 1);
    const int copied = WideCharToMultiByte(CP_UTF8,
                                           WC_ERR_INVALID_CHARS,
                                           name.data(),
                                           static_cast<int>(name.size()),
                                           output.moduleName.data(),
                                           static_cast<int>(output.moduleName.size() - 1),
                                           nullptr,
                                           nullptr);
    if (copied > 0) {
        output.moduleNameLength = static_cast<std::size_t>(copied);
        output.moduleName[output.moduleNameLength] = '\0';
    }
}

[[nodiscard]] bool append_digest(std::span<char> output,
                                 std::size_t& offset,
                                 const Sha256Digest& digest) noexcept {
    if (offset + digest.size() * 2 >= output.size()) {
        return false;
    }
    for (const std::byte byte : digest) {
        const auto value = std::to_integer<unsigned>(byte);
        output[offset++] = kHexDigits[(value >> 4U) & 0xFU];
        output[offset++] = kHexDigits[value & 0xFU];
    }
    return true;
}

} // namespace

const BuildMetadata& metadata() noexcept {
    return kMetadata;
}

bool file_sha256(const wchar_t* path, Sha256Digest& output) noexcept {
    output = {};
    if (path == nullptr) {
        return false;
    }
    const HANDLE file = CreateFileW(path,
                                    GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    bool complete = cng_succeeded(
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
    if (complete) {
        complete = cng_succeeded(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
    }
    std::array<std::byte, kHashReadBufferSize> buffer{};
    while (complete) {
        DWORD transferred = 0;
        if (ReadFile(file,
                     buffer.data(),
                     static_cast<DWORD>(buffer.size()),
                     &transferred,
                     nullptr)
            == FALSE) {
            complete = false;
            break;
        }
        if (transferred == 0) {
            break;
        }
        complete = cng_succeeded(BCryptHashData(
            hash, reinterpret_cast<PUCHAR>(buffer.data()), transferred, 0));
    }
    if (complete) {
        complete = cng_succeeded(BCryptFinishHash(hash,
                                                  reinterpret_cast<PUCHAR>(output.data()),
                                                  static_cast<ULONG>(output.size()),
                                                  0));
    }
    if (hash != nullptr) {
        (void)BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    complete = CloseHandle(file) != FALSE && complete;
    if (!complete) {
        output = {};
    }
    return complete;
}

bool capture(HMODULE module) noexcept {
    if (module == nullptr || module != implementation_module()) {
        return false;
    }
    if (g_available.load(std::memory_order_acquire)) {
        return module == g_capturedModule;
    }
    std::array<wchar_t, kModulePathCapacity> path{};
    RuntimeIdentity pending{};
    pending.build = &kMetadata;
    if (!module_path(module, path) || !file_sha256(path.data(), pending.imageSha256)) {
        return false;
    }
    module_basename(path.data(), pending);
    if (pending.moduleNameLength == 0) {
        return false;
    }

    AcquireSRWLockExclusive(&g_identityLock);
    g_identity = pending;
    g_capturedModule = module;
    g_available.store(true, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_identityLock);
    return true;
}

const RuntimeIdentity* current() noexcept {
    return g_available.load(std::memory_order_acquire) ? &g_identity : nullptr;
}

void clear() noexcept {
    AcquireSRWLockExclusive(&g_identityLock);
    g_available.store(false, std::memory_order_release);
    g_identity = {};
    g_capturedModule = nullptr;
    ReleaseSRWLockExclusive(&g_identityLock);
}

bool format_startup_record(std::span<char> output, std::size_t& length) noexcept {
    length = 0;
    const RuntimeIdentity* identity = current();
    if (identity == nullptr || identity->build == nullptr || output.empty()) {
        return false;
    }
    const BuildMetadata& build = *identity->build;
    const int written = std::snprintf(
        output.data(),
        output.size(),
        "ev=build_identity schema=%u build_id=%.*s git_head=%.*s dirty=%u "
        "source_sha256=%.*s configuration=%.*s platform=%.*s compiler=%.*s toolset=%.*s "
        "windows_sdk=%.*s cache_format=%u settings_version=%u module=%.*s module_sha256=",
        build.schema,
        static_cast<int>(build.buildId.size()),
        build.buildId.data(),
        static_cast<int>(build.gitHead.size()),
        build.gitHead.data(),
        build.dirty ? 1U : 0U,
        static_cast<int>(build.sourceSha256Hex.size()),
        build.sourceSha256Hex.data(),
        static_cast<int>(build.configuration.size()),
        build.configuration.data(),
        static_cast<int>(build.platform.size()),
        build.platform.data(),
        static_cast<int>(build.compiler.size()),
        build.compiler.data(),
        static_cast<int>(build.toolset.size()),
        build.toolset.data(),
        static_cast<int>(build.windowsSdk.size()),
        build.windowsSdk.data(),
        build.cacheFormat,
        build.settingsVersion,
        static_cast<int>(identity->moduleNameLength),
        identity->moduleName.data());
    if (written <= 0 || static_cast<std::size_t>(written) >= output.size()) {
        return false;
    }
    length = static_cast<std::size_t>(written);
    if (!append_digest(output, length, identity->imageSha256) || length >= output.size()) {
        length = 0;
        return false;
    }
    output[length] = '\0';
    return true;
}

} // namespace sunrise::core::provenance
