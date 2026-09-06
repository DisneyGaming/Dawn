#include "state/activity/coo/omega_script.h"
#include <cstdio>
#include <cstdlib>
namespace {
// Load the shipped document before the existing end-to-end parity tests run.
// This storage outlives every run constructed by main(). No test reload API.
const auto document=[] {
    namespace script=sunrise::state::activity::coo::script;
    std::string error;auto result=script::Document::read("Sunrise/scripts/omega.json",error);
    if(!result || !result->activate()) {
        std::fprintf(stderr,"Omega JSON bootstrap failed: %s\n",error.c_str());std::abort();
    }
    std::printf("Omega JSON loaded: %016llX\n",static_cast<unsigned long long>(result->fingerprint()));
    return result;
}();
}
