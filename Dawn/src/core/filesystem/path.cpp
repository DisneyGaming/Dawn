#include "path.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <filesystem>

namespace dawn::core::path {
namespace {

/** One owned folder prevents generated files from accumulating beside game binaries. */
constexpr std::wstring_view kArtifactDirectorySuffix = L"Dawn";

// Preserve the previous runtime without embedding an obsolete project name.
// Only a unique sibling with a settings file is eligible (older runtimes can predate SQLite).
// The settings file is copied last so an interrupted copy is retried at next boot.
[[nodiscard]] bool migrate_runtime(const Buffer& output) noexcept {
    namespace fs = std::filesystem;
    const fs::path destination(output.chars.data());
    std::error_code error;
    // An existing database belongs to Dawn even if settings were deliberately reset.
    if (fs::exists(destination / L"settings.json", error)
        || fs::exists(destination / L"player-state.db", error)) return !error;
    if (error) return false;
    fs::path source;
    for (fs::directory_iterator it(destination.parent_path(), error), end; !error && it != end; it.increment(error)) {
        const auto& entry = *it;
        if (entry.path() == destination || !entry.is_directory(error) || entry.is_symlink(error)) continue;
        if (fs::is_regular_file(entry.path() / L"settings.json", error)) {
            if (!source.empty()) return false;
            source = entry.path();
        }
        if (error == std::errc::no_such_file_or_directory) error.clear();
        if (error) return false;
    }
    if (error) return false;
    if (source.empty()) return true;
    for (const auto* leaf : {L"player-state.db", L"player-state.db-wal", L"player-state.db-shm",
            L"hud.json", L"movement.json", L"player.json", L"scripts", L"cache", L"event_presets"}) {
        const auto from = source / leaf;
        if (!fs::exists(from, error)) { if (error) return false; continue; }
        fs::copy(from, destination / leaf, fs::copy_options::recursive | fs::copy_options::overwrite_existing
            | fs::copy_options::skip_symlinks, error);
        if (error) return false;
    }
    fs::copy_file(source / L"settings.json", destination / L"settings.json", fs::copy_options::none, error);
    return !error;
}

} // namespace

/** Replaces one fixed path without truncation. */
bool assign(Buffer& path, std::wstring_view value) noexcept {
    if (value.size() >= path.chars.size()) {
        return false;
    }
    path = {};
    std::memcpy(path.chars.data(), value.data(), value.size() * sizeof(wchar_t));
    path.length = value.size();
    path.chars[path.length] = L'\0';
    return true;
}

/** Resolves a loaded module path and trims it to the containing directory. */
bool module_directory(void* module, Buffer& output) noexcept {
    if (module == nullptr) {
        return false;
    }

    const DWORD copied = GetModuleFileNameW(
        static_cast<HMODULE>(module), output.chars.data(), static_cast<DWORD>(output.chars.size()));
    if (copied == 0 || copied == output.chars.size()) {
        return false;
    }

    output.length = copied;
    while (output.length != 0 && output.chars[output.length - 1] != L'\\') {
        --output.length;
    }
    if (output.length == 0) {
        return false;
    }
    output.chars[output.length] = L'\0';
    return true;
}

/** Resolves and creates the shared module-relative root for every generated artifact. */
bool artifact_directory(void* module, Buffer& output) noexcept {
    if (!module_directory(module, output) || !append(output, kArtifactDirectorySuffix)) {
        return false;
    }
    if (CreateDirectoryW(output.chars.data(), nullptr) != FALSE) {
        return migrate_runtime(output);
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    // ERROR_ALREADY_EXISTS also covers files, so verify the existing object is a directory.
    const DWORD attributes = GetFileAttributesW(output.chars.data());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
        && migrate_runtime(output);
}

bool artifact_file(std::wstring_view relative, Buffer& output) noexcept {
    HMODULE self{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&artifact_file),
                           &self)
            == FALSE
        || self == nullptr) {
        return false;
    }
    if (!artifact_directory(self, output)) {
        return false;
    }
    std::size_t start = 0;
    for (std::size_t index = 0; index < relative.size(); ++index) {
        if (relative[index] != L'\\') {
            continue;
        }
        if (!append(output, L"\\") || !append(output, relative.substr(start, index - start))) {
            return false;
        }
        if (CreateDirectoryW(output.chars.data(), nullptr) == FALSE
            && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
        start = index + 1;
    }
    return append(output, L"\\") && append(output, relative.substr(start));
}

/** Appends a path suffix without exceeding fixed storage. */
bool append(Buffer& path, std::wstring_view suffix) noexcept {
    if (path.length + suffix.size() >= path.chars.size()) {
        return false;
    }
    std::memcpy(path.chars.data() + path.length, suffix.data(), suffix.size() * sizeof(wchar_t));
    path.length += suffix.size();
    path.chars[path.length] = L'\0';
    return true;
}

bool read_artifact_text(std::wstring_view relative, std::span<char> text) noexcept {
    if (text.empty()) {
        return false;
    }
    text[0] = '\0';
    Buffer file{};
    if (!artifact_file(relative, file)) {
        return false;
    }
    const HANDLE handle = CreateFileW(file.chars.data(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    DWORD read = 0;
    const bool measured = GetFileSizeEx(handle, &size) != FALSE;
    const bool fits = measured && size.QuadPart >= 0
                      && static_cast<std::uint64_t>(size.QuadPart) < text.size();
    const bool ok = fits
                    && ReadFile(handle, text.data(), static_cast<DWORD>(text.size() - 1), &read,
                                nullptr) != FALSE;
    (void)CloseHandle(handle);
    if (!ok || read == 0) {
        return false;
    }
    text[read] = '\0';
    return true;
}

/** Writes one Dawn-owned text file whole, replacing what was there. */
bool write_artifact_text(std::wstring_view relative, std::string_view text) noexcept {
    if (text.size() > (std::numeric_limits<DWORD>::max)()) {
        return false;
    }
    Buffer file{};
    if (!artifact_file(relative, file)) {
        return false;
    }
    const HANDLE handle = CreateFileW(file.chars.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const auto size = static_cast<DWORD>(text.size());
    const bool complete = WriteFile(handle, text.data(), size, &written, nullptr) != FALSE
                          && written == size;
    return CloseHandle(handle) != FALSE && complete;
}

} // namespace dawn::core::path
