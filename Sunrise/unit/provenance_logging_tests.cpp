#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <iostream>
#include <string_view>

#include "../src/core/logging/log.h"
#include "../src/core/logging/snapshot/snapshot.h"

int main() {
    namespace log = sunrise::core::log;
    log::Settings settings = log::defaults();
    settings.debuggerSink = false;
    settings.fileSink = false;
    if (!log::initialize(GetModuleHandleW(nullptr), settings)) {
        std::cerr << "logging initialization failed\n";
        return 1;
    }
    log::write(log::Channel::core, log::Level::info, "ordinary_info_must_be_filtered");
    log::write_startup_record(log::Channel::core, "ev=build_identity required=1");
    const log::snapshot::Snapshot snapshot = log::snapshot::take();
    log::shutdown();
    const auto entries = snapshot.entries();
    if (entries.size() != 1 || entries[0].level() != log::Level::info
        || entries[0].text().find("ev=build_identity required=1") == std::string_view::npos
        || entries[0].text().find("ordinary_info_must_be_filtered") != std::string_view::npos) {
        std::cerr << "required startup record did not bypass only the severity threshold\n";
        return 1;
    }
    std::cout << "provenance logging threshold check passed\n";
    return 0;
}
