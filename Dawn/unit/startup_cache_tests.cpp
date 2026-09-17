#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

// Exercise the exported production startup with a copied cache. All generated state remains
// in a unique test directory; the supplied DLL, settings, and cache are only read.
int wmain(int argc, wchar_t** argv) {
    if (argc != 4) {
        std::cerr << "Usage: startup_cache_tests <DLL> <settings.json> <obsolete build_data.bin>\n";
        return 2;
    }
    namespace fs = std::filesystem;
    const auto root = fs::absolute(L"build/unit/startup-cache")
        / (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    fs::create_directories(root / L"Dawn/cache");
    fs::copy_file(argv[1], root / L"steam_api64.dll");
    fs::copy_file(argv[2], root / L"Dawn/settings.json");
    fs::copy_file(argv[3], root / L"Dawn/cache/build_data.bin");
    const HMODULE module = LoadLibraryW((root / L"steam_api64.dll").c_str());
    if (module == nullptr) {
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
        return 1;
    }
    using Lifecycle = bool (*)() noexcept;
    const auto initialize = reinterpret_cast<Lifecycle>(GetProcAddress(module, "DawnInitialize"));
    const auto shutdown = reinterpret_cast<Lifecycle>(GetProcAddress(module, "DawnShutdown"));
    if (initialize == nullptr || shutdown == nullptr) return 1;
    const bool initialized = initialize();
    const bool stopped = shutdown();
    std::ifstream log(root / L"Dawn/logs/dawn.log");
    const std::string content{std::istreambuf_iterator<char>(log), {}};
    std::cout << content;
    const bool oldSignatureSeen = content.find("result=invalid reason=magic") != std::string::npos;
    const bool complete = content.find("ev=initialize result=ok") != std::string::npos;
    if (!initialized || !stopped || !oldSignatureSeen || !complete) {
        std::cerr << "FAIL: production startup did not recover the obsolete cache.\n";
        return 1;
    }
    std::cout << "PASS: production startup recovered the obsolete cache without publishing it.\n";
    // Process exit unloads the production module after all runtime threads have stopped.
    return 0;
}
