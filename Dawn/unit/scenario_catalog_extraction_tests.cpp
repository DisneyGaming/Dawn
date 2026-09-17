#include "client/content/scenarios/internal.h"
#include "core/settings/settings.h"
#include "state/build_data/cache/records/codec.h"
#include "state/build_data/scenarios/omega_schema_catalog.h"
#include "state/activity/coo/open_world_catalog.h"
#include "state/activity/coo/lost_sector_group_catalog.h"
#include "server/runtime/activity/registry_admission.h"
#include "server/runtime/activity/lost_sector_catalog.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <memory>
#include <vector>

namespace extract = dawn::client::content::scenarios;
namespace layouts = dawn::state::build_data::scenarios;
namespace rec = dawn::state::build_data::cache::records;
namespace packages = dawn::middleware::content::packages::reader;
namespace authored = dawn::state::activity::coo::open_world;
namespace sectors = dawn::state::activity::coo::lost_sector;
namespace sectorProfiles = dawn::server::runtime::activity::lost_sector::catalog;
namespace admission = dawn::server::runtime::activity::registry;

// Only the byte transport is replaced. The package walker, slot decoder,
// admission comparison and publication code below are production code.
namespace dawn::middleware::content::packages::reader {
bool read_tag(const Source&, Scratch&, std::uint32_t tag,
              std::vector<std::byte>& output, std::uint32_t& classId) noexcept {
    std::printf("READ %08X\n", tag);std::fflush(stdout);
    std::uint32_t header[2]{};
    if(std::fread(header,sizeof(header),1,stdin)!=1)std::exit(3);
    classId=header[0];output.clear();
    if(header[1]==UINT32_MAX)return false;
    if(header[1]>64U*1024U*1024U)std::exit(4);
    output.resize(header[1]);
    if(!output.empty() && std::fread(output.data(),output.size(),1,stdin)!=1)std::exit(5);
    return true;
}
bool read_tag(const Source& source,Scratch& scratch,std::uint32_t tag,
              std::vector<std::byte>& output) noexcept {
    std::uint32_t ignored{};return read_tag(source,scratch,tag,output,ignored);
}
}
namespace dawn::core::settings {
const Settings& get() noexcept {
    static const Settings settings=[] {Settings value{};value.client.rosterForceAuthored=true;return value;}();
    return settings;
}
}
namespace dawn::core::log {
void write(Channel,Level,std::string_view) noexcept {}
}
namespace dawn::state::build_data::scenarios {
void record_omega_schema(const OmegaSchema&) noexcept {}
}
namespace dawn::client::hooks::retail_log {
void register_schema_marker(std::uint32_t) noexcept {}
}

int main(int argc,char** argv) {
    if(argc!=3)return 2;
    (void)_setmode(_fileno(stdin),_O_BINARY);
    std::ifstream input(argv[1],std::ios::binary);
    std::uint32_t count{};input.read(reinterpret_cast<char*>(&count),sizeof(count));
    if(!input || !count || count>layouts::kDefinitionCapacity)return 6;
    std::vector<layouts::Definition> rows(count);
    for(auto& row:rows) {
        rec::ScenarioRecord record{};input.read(reinterpret_cast<char*>(&record),sizeof(record));
        if(!input || !rec::decode(record,row))return 7;
    }
    auto storage=std::make_unique<extract::RosterStorage>();
    auto scratch=std::make_unique<packages::Scratch>();
    for(std::size_t calls=0;!extract::build_rosters({},*scratch,*storage,rows);++calls)
        if(calls>10000)return 8;
    std::ofstream extracted(argv[2],std::ios::binary);
    const auto groupCount=static_cast<std::uint32_t>(storage->groupCount);
    extracted.write(reinterpret_cast<const char*>(&count),sizeof(count));
    extracted.write(reinterpret_cast<const char*>(&groupCount),sizeof(groupCount));
    for(const auto& row:rows) {
        rec::ScenarioRecord record{};if(!rec::encode(row,record))return 9;
        extracted.write(reinterpret_cast<const char*>(&record),sizeof(record));
    }
    for(std::size_t i=0;i<storage->groupCount;++i) {
        rec::RosterGroupRecord record{};if(!rec::encode(storage->groups[i],record))return 10;
        extracted.write(reinterpret_cast<const char*>(&record),sizeof(record));
    }
    if(!extracted)return 11;
    std::size_t missing{},mismatch{},required{};
    for(const auto* destination:authored::kDestinations) {
        for(const auto& expected:destination->registries) {
            ++required;const layouts::RosterGroup* found{};
            for(std::size_t i=0;i<storage->groupCount;++i)
                if(storage->groups[i].registryKey==expected.key){found=&storage->groups[i];break;}
            if(!found){++missing;std::printf("MISSING %.*s %08X\n",static_cast<int>(expected.activity.size()),expected.activity.data(),expected.key);}
            else if(!admission::matches(*found,expected)){++mismatch;std::printf("MISMATCH %08X\n",expected.key);}
        }
    }
    // Inventory admission is not encounter activation. Assert that all reviewed
    // sector owners survive the real package traversal with their source slots.
    for(const auto& expected:sectors::kRegistryGroups) {
        ++required;const layouts::RosterGroup* found{};
        for(std::size_t i=0;i<storage->groupCount;++i)
            if(storage->groups[i].registryKey==expected.key){found=&storage->groups[i];break;}
        if(!found){++missing;std::printf("MISSING lost_sector %08X\n",expected.key);continue;}
        std::size_t sources{};
        for(std::size_t i=0;i<found->slotCount;++i)if(found->slotTypes[i]==1)++sources;
        if(found->objectTag!=expected.object || sources!=expected.sourceCount) {
            ++mismatch;std::printf("MISMATCH lost_sector %08X object=%08X sources=%zu expected=%u\n",
                expected.key,found->objectTag,sources,static_cast<unsigned>(expected.sourceCount));
        }
    }
    const std::array<std::span<const admission::Definition>,6> sectorRegistries{{
        sectorProfiles::mercury::kRegistries,sectorProfiles::mars::kRegistries,
        sectorProfiles::io::kRegistries,sectorProfiles::titan::kRegistries,
        sectorProfiles::nessus::kRegistries,sectorProfiles::tangled_shore::kRegistries}};
    std::size_t sectorSchemas{},expectedSectorSchemas{};
    for(const auto registries:sectorRegistries)for(const auto& expected:registries) {
        ++expectedSectorSchemas;
        const layouts::RosterGroup* found{};
        for(std::size_t i=0;i<storage->groupCount;++i)
            if(storage->groups[i].registryKey==expected.key){found=&storage->groups[i];break;}
        if(!found || !sectors::required(expected.scenario,expected.objectTag,expected.key,
                std::uint64_t{1}<<expected.bubble)
            || !admission::valid(expected) || !admission::matches(*found,expected)) {
            ++mismatch;std::printf("MISMATCH lost_sector_full_schema %08X\n",expected.key);
        } else ++sectorSchemas;
    }
    std::printf("SECTOR_SCHEMAS matched=%zu expected=%zu\n",sectorSchemas,expectedSectorSchemas);
    if(!expectedSectorSchemas || sectorSchemas!=expectedSectorSchemas)++mismatch;
    std::printf("RESULT groups=%zu capacity=%zu required=%zu missing=%zu mismatch=%zu catalog_overflow=%zu authored_overflow=%zu unresolved=%zu\n",
        storage->groupCount,layouts::kRosterGroupCapacity,required,missing,mismatch,
        storage->catalogOverflows,storage->authoredOverflows,storage->unresolvedGroups);
    return missing || mismatch || storage->catalogOverflows ? 1 : 0;
}
