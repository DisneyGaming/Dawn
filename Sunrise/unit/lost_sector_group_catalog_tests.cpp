#include "../src/state/activity/coo/lost_sector_group_catalog.h"
#include <cstdio>

namespace catalog=sunrise::state::activity::coo::lost_sector;

int main() {
    unsigned checks{};
    for(const auto& group:catalog::kRegistryGroups) {
        ++checks;
        if(!group.scenario || !group.object || !group.key || !group.sliceMask || !group.sourceCount
            || (group.sliceMask&(group.sliceMask-1))
            || !catalog::required(group.scenario,group.object,group.key,group.sliceMask))return 1;
    }
    // Known public-event and campaign owners must not be admitted by this catalog.
    if(catalog::required(0x80B43A1CU,0x80B43A1CU,0x5C01717FU,UINT64_C(8)))return 1;
    if(catalog::required(0x80FC9645U,0x80FC9645U,0x112E6282U,UINT64_C(0x80)))return 1;
    if(catalog::required(0x80FC9645U,0x80FD44D6U,0x11EDE3EDU,UINT64_C(8)))return 1;
    std::printf("PASS %u lost-sector registry catalog checks, sources=%zu\n",checks,
        catalog::source_count(0x80F4696AU)+catalog::source_count(0x80F6AB20U)
        +catalog::source_count(0x80B3E142U)+catalog::source_count(0x80B56B1BU)
        +catalog::source_count(0x80B43A1CU)+catalog::source_count(0x80FC9645U));
}
