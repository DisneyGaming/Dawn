#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "middleware/content/packages/tables/activity_table.h"
#include "client/ui/mission_launch/mission_launch_model.h"

namespace {
void check(bool value, const char* what) {
    if (!value) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
template<class T> void put(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
}
int main(int argc, char** argv) {
    using namespace sunrise;
    namespace parser = middleware::content::packages::tables::activities;
    namespace model = client::ui::mission_launch;
    check(argc == 2, "provide the installed public activity table fixture");
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<char> chars{std::istreambuf_iterator<char>(file), {}};
    std::vector<std::byte> bytes(chars.size());
    if (!chars.empty()) { std::memcpy(bytes.data(), chars.data(), chars.size()); }
    static std::array<state::build_data::activities::Definition,
        state::build_data::activities::kCapacity> rows{};
    std::size_t count{};
    check(parser::decode(bytes, rows, count), "decode public table");
    check(count == 1170, "complete installed table (including unsupported variants)");
    check(rows[20].hash == 0xE8ABA41BU && rows[20].name() == "city_tower_social_d2", "Tower identity");
    check(rows[266].hash == 0x62D85FB3U && rows[266].name() == "mission_towerfall", "Homecoming identity");
    check(rows[299].hash == 0x87AC2003U && rows[299].name() == "mission_scot", "Omega identity");
    check(rows[29].name() == "mercury_freeroam", "Mercury identity");
    check(model::matches(rows[299], model::kUnresolvedContent, 1, "MISSION_SCOT"), "unresolved title uses exact package, case-insensitive DLC and native type");
    check(model::matches(rows[299], 0, 0, "87ac2003"), "hash search");
    check(model::matches(rows[299], 0, 0, "299"), "ordinal search");
    check(model::matches(rows[299], 0, 0, "mission_scot"), "package search");
    check(!model::matches(rows[299], 3, 0, ""), "unrelated DLC excluded");
    check(!model::matches(rows[299], model::kUnresolvedContent, 2, ""), "mission excluded from strike filter");
    check(rows[0].name().empty(), "missing direct destination retained honestly");
    check(model::activity_type(rows[229]) == 2, "strike classification");
    check(model::content_group(rows[30]) == model::kUnresolvedContent, "Mars destination does not invent DLC without metadata");
    check(model::content_group(rows[40]) == model::kUnresolvedContent, "Reef destination does not invent DLC without metadata");
    check(model::content_group(rows[157]) == model::kUnresolvedContent, "Moon destination does not invent DLC without metadata");
    check(state::build_data::activities::publish(std::span(rows).first(count)), "immutable publication");
    check(!state::build_data::activities::publish(std::span(rows).first(count)), "published table cannot be replaced");
    check(state::build_data::activities::entries()[299].hash == 0x87AC2003U, "published identity");
    std::size_t decoded{};
    for (const std::size_t length : {std::size_t{0}, std::size_t{16}, std::size_t{160}, bytes.size() - 1}) {
        check(!parser::decode(std::span(bytes).first(length), rows, decoded) && decoded == 0,
            "truncated table fails without partial publication");
    }
    auto corrupt = bytes;
    put(corrupt, 0x98, std::uint32_t{0x80807A35});
    check(!parser::decode(corrupt, rows, decoded), "package table class cannot masquerade as public table");
    corrupt = bytes;
    put(corrupt, 0xA8, (std::numeric_limits<std::int64_t>::max)());
    check(!parser::decode(corrupt, rows, decoded), "overflowing self-relative pointer rejected");
    corrupt = bytes;
    put(corrupt, 0xA0, std::uint32_t{1});
    check(!parser::decode(corrupt, rows, decoded), "row/record identity mismatch rejected");
    check(!parser::decode(bytes, std::span(rows).first(10), decoded), "capacity rejected");
    std::cout << "PASS: 1170 real activity definitions, identities, filters, immutable publication and malformed-table guards\n";
}
