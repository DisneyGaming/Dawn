#include "../src/state/activity/beyond_infinity/catalog.h"
#include <cmath>
#include <cstdio>

namespace native = dawn::state::activity::beyond_infinity;
static_assert(native::kScenario == 0x80F46015U);
static_assert(std::size(native::kAssets) == 365);
static_assert(std::size(native::kVolumes) == 87);
static_assert(native::find(0x03632571U,68,0)->asset.definition == 0x80F46225U);
static_assert(native::find(0x8E70632BU,37,3)->name == "map_generator_sensor");
static_assert(native::find(0x233E7149U,4,36)->name == "laser_lense_object");
static_assert(native::find(0x15FFBE16U,1,6)->name == "squad_boss_bad_guy");
static_assert(native::find(0x1194F70FU,43,14)->asset.definition == 0x80F46367U);
static_assert(native::find(0x03632571U,43,14) == nullptr);
static_assert(native::objective(0xD60FA7DFU)->description == "Study Mercury's past.");
// Equal displayed text must not collapse distinct authored events.
static_assert(native::objective(0x5569523AU) != native::objective(0x5AD156F5U));
static_assert(native::objective(0x5569523AU)->title == native::objective(0x5AD156F5U)->title);
static_assert(native::objective(0xFFFFFFFFU) == nullptr);

int main() {
    for(std::size_t i=0;i<std::size(native::kAssets);++i) {
        const auto& a=native::kAssets[i].asset;
        for(std::size_t j=i+1;j<std::size(native::kAssets);++j) {
            const auto& b=native::kAssets[j].asset;
            if(a.registry==b.registry && a.type==b.type && a.slot==b.slot) { return 1; }
        }
    }
    for(const auto& volume:native::kVolumes) {
        if(volume.vertices.size()<3 || volume.min.x>volume.max.x || volume.min.y>volume.max.y
            || volume.min.z>volume.max.z) { return 2; }
        for(const auto& point:volume.vertices) {
            if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) { return 3; }
        }
    }
    std::puts("PASS: Beyond Infinity package catalog; gameplay and native receipts are not tested");
}
