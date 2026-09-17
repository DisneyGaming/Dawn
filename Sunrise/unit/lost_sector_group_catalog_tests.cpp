#include "../src/state/activity/coo/lost_sector_group_catalog.h"
#include "../src/state/activity/coo/edz_moon_lost_sector_group_catalog.h"
#include <cstdio>

namespace catalog=sunrise::state::activity::coo::lost_sector;

int main() {
    unsigned checks{},completionGroups{};
    for(const auto& group:catalog::kRegistryGroups) {
        ++checks;
        if(!group.scenario || !group.object || !group.key || !group.sliceMask
            || (group.sliceMask&(group.sliceMask-1))
            || !catalog::required(group.scenario,group.object,group.key,group.sliceMask))return 1;
        completionGroups+=group.sourceCount?0U:1U;
    }
    // Known public-event and campaign owners must not be admitted by this catalog.
    if(catalog::required(0x80B43A1CU,0x80B43A1CU,0x5C01717FU,UINT64_C(8)))return 1;
    if(catalog::required(0x80FC9645U,0x80FC9645U,0x112E6282U,UINT64_C(0x80)))return 1;
    if(catalog::required(0x80FC9645U,0x80FD44D6U,0x11EDE3EDU,UINT64_C(8)))return 1;
    if(completionGroups!=22)return 1;
    unsigned edzMoonCompletionGroups{};
    for(const auto& group:sunrise::state::activity::coo::edz_moon_lost_sector_groups::kRegistryGroups) {
        ++checks;
        if(!group.scenario || !group.object || !group.key || !group.sliceMask
            || (group.sliceMask&(group.sliceMask-1))
            || !sunrise::state::activity::coo::edz_moon_lost_sector_groups::required(
                group.scenario,group.object,group.key,group.sliceMask))return 1;
        edzMoonCompletionGroups+=group.sourceCount?0U:1U;
    }
    if(edzMoonCompletionGroups!=20)return 1;
    if(sunrise::state::activity::coo::edz_moon_lost_sector_groups::required(
        0x80B2F00AU,0x80BE3307U,0x483ECB05U,UINT64_C(0x20)))return 1;
    std::printf("PASS %u lost-sector registry catalog checks, sources=%zu\n",checks,
        catalog::source_count(0x80F4696AU)+catalog::source_count(0x80F6AB20U)
        +catalog::source_count(0x80B3E142U)+catalog::source_count(0x80B56B1BU)
        +catalog::source_count(0x80B43A1CU)+catalog::source_count(0x80FC9645U)
        +catalog::source_count(0x80F1404DU));
}
