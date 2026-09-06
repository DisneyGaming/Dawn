#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include "client/hooks/bootflow/omega_navigation.h"
#include "client/hooks/bootflow/omega_enemy_forest_receipts.h"
namespace nav=sunrise::client::hooks::bootflow::omega_navigation;
namespace enemy=sunrise::client::hooks::bootflow::forest_enemy;
unsigned failures{},checks{};
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; std::cerr<<__LINE__<<": "<<#e<<'\n'; }} while(false)
int main() {
    nav::Route route;
    CHECK(route.observe(1,true,nav::kIkora,15)==nav::Goal::portal);
    CHECK(route.observe(1,true,nav::kPortal,15)==nav::Goal::entrance);
    CHECK(route.observe(1,true,nav::kEntrance,11)==nav::Goal::gates);
    route.terminal_gate(); CHECK(route.current()==nav::Goal::lairApproach);
    CHECK(route.observe(1,true,nav::kEntrance,11)==nav::Goal::lairApproach);
    CHECK(route.observe(1,true,nav::kLairApproach,11)==nav::Goal::lairEntry);
    CHECK(route.observe(1,true,nav::kLairEntry,14)==nav::Goal::complete);
    CHECK(route.observe(1,true,nav::kIkora,15)==nav::Goal::complete);
    CHECK(route.observe(2,false,nav::kLairEntry,14)==nav::Goal::ikora);
    CHECK(route.observe(2,true,{NAN,0,0},14)==nav::Goal::ikora);
    CHECK(route.observe(2,true,{0,0,0},14)==nav::Goal::lairEntry);
    CHECK(nav::target(nav::Goal::entrance).bubble==11);
    CHECK(nav::target(nav::Goal::lairApproach).bubble==14);
    CHECK(!nav::target(nav::Goal::gates).present);
    std::array<std::byte,nav::kComponentBytes> bytes{};
    bytes.fill(std::byte{0xCD});
    nav::write<std::uint32_t>(bytes,0,0x80F47BD4);
    nav::write<std::uint32_t>(bytes,4,0x80804F54);
    nav::write<std::int64_t>(bytes,8,0xB88);
    const auto before=bytes;
    CHECK(nav::sync(bytes,nav::Goal::lairApproach));
    CHECK(nav::read<std::uint8_t>(bytes,0xA84)==3);
    CHECK(nav::read<std::uint8_t>(bytes,0xA8C)==2);
    CHECK(nav::read<std::uint32_t>(bytes,0xA98)==14);
    CHECK(nav::read<float>(bytes,0xAA0)==nav::kLairApproach.x);
    for (std::size_t i=0;i<bytes.size();++i) {
        const bool owned=i==0xA84 || i==0xA8C || (i>=0xA98 && i<0xA9C) || (i>=0xAA0 && i<0xAAC);
        if (!owned) CHECK(bytes[i]==before[i]);
    }
    CHECK(!nav::sync(bytes,nav::Goal::lairApproach));
    CHECK(nav::sync(bytes,nav::Goal::complete));
    CHECK(nav::read<std::uint8_t>(bytes,0xA84)==0);
    CHECK(!nav::sync(bytes,nav::Goal::complete));
    bytes[0]=std::byte{0}; const auto foreign=bytes;
    CHECK(!nav::sync(bytes,nav::Goal::portal)); CHECK(bytes==foreign);
    CHECK(nav::terminal(0,3,2,5,1,1,1));
    CHECK(!nav::terminal(1,3,2,5,1,1,1));
    CHECK(!nav::terminal(0,0,2,5,1,1,1));
    CHECK(!nav::terminal(0,3,-1,5,1,1,1));
    CHECK(!nav::terminal(0,3,5,5,1,1,1));
    CHECK(!nav::terminal(0,3,2,5,0,1,1));
    CHECK(!nav::terminal(0,3,2,5,1,1,0));
    CHECK(!nav::terminal(0,3,2,5,NAN,1,1));
    std::array<std::byte,0x150> header{};
    nav::write<std::uint32_t>(header,0x148,0x12345678);
    nav::write<std::uint32_t>(header,0x14C,4);
    nav::write<std::int64_t>(header,0xC0,2);
    nav::write<std::int32_t>(header,0x100,255);
    enemy::Population population{};
    CHECK(enemy::population_header(header,0x12345678,4,population));
    CHECK(population.tracked==255);
    std::array<std::byte,0x60> rows{};
    nav::write<std::int32_t>(rows,0x20,-1);
    nav::write<std::int32_t>(rows,0x24,7);
    // Native population completion treats every nonzero enable byte as active.
    rows[0x2C]=std::byte{2}; rows[0x5C]=std::byte{0xFF};
    nav::write<std::int32_t>(rows,0x50,2);
    nav::write<std::int32_t>(rows,0x54,4);
    CHECK(enemy::population_rows(0x1000,[&](std::uintptr_t address,void* out,std::size_t size) {
        if (address<0x1000 || address-0x1000+size>rows.size()) return false;
        std::memcpy(out,rows.data()+address-0x1000,size); return true;
    },population));
    CHECK(population.remaining==1 && population.queued==11);
    nav::write<std::int64_t>(header,0xC0,513);
    CHECK(!enemy::population_header(header,0x12345678,4,population));
    CHECK(population.status!=3);
    std::cout<<"Omega navigation/Forest diagnostics: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
