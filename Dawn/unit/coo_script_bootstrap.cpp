#include "state/activity/coo/omega_script.h"
#include <cstdio>
#include <cstdlib>
namespace {
// Load the shipped document before the existing end-to-end parity tests run.
// This storage outlives every run constructed by main(). No test reload API.
const auto document=[] {
    namespace script=dawn::state::activity::coo::script;
    std::string error;auto result=script::Document::read("Dawn/scripts/omega.lua",error);
    if(!result || !result->activate()) {
        std::fprintf(stderr,"Omega Lua bootstrap failed: %s\n",error.c_str());std::abort();
    }
    std::printf("Omega Lua loaded: %016llX\n",static_cast<unsigned long long>(result->fingerprint()));
    return result;
}();
}
