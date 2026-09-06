#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include "core/settings/parser.h"

// This target deliberately links only the root/JSON/Omega parser translation units. These
// fail-visible seams satisfy settings_parser.cpp's unrelated references without pulling the
// Client, Server, Steam, or State settings parsers into this narrow contract test.
namespace sunrise::core::settings::parser {

std::size_t g_unexpectedParserBranchCalls = 0U;

bool Parser::client_settings(client::Settings&) noexcept {
    ++g_unexpectedParserBranchCalls;
    return false;
}

bool Parser::server_settings(server::Settings&) noexcept {
    ++g_unexpectedParserBranchCalls;
    return false;
}

bool Parser::steam_settings(steam::Settings&) noexcept {
    ++g_unexpectedParserBranchCalls;
    return false;
}

bool Parser::state_settings(Settings&) noexcept {
    ++g_unexpectedParserBranchCalls;
    return false;
}

} // namespace sunrise::core::settings::parser

// settings::defaults() composes these three bundled defaults. Their contents are outside this
// test's scope; empty valid-shaped values keep the test linked to the real defaults()/parse().
namespace sunrise::core::log {

Settings defaults() noexcept {
    return {};
}

} // namespace sunrise::core::log

namespace sunrise::state::entitlements {

Table authored() noexcept {
    return {};
}

} // namespace sunrise::state::entitlements

namespace sunrise::state::activity::defaults {

ActivityDefaults authored() noexcept {
    return {};
}

} // namespace sunrise::state::activity::defaults

namespace {

using sunrise::core::settings::Settings;
using sunrise::core::settings::experiments::Omega;

enum class Leaf : std::size_t {
    directiveUi,
    ikoraCarrierModelSuppression,
    ikoraVfxRebind,
    sceneAuthority,
    gateAuthority,
    portalMutation,
    syntheticStageMachine,
    unsafeDiagnostics,
    count,
};

constexpr std::size_t kLeafCount = static_cast<std::size_t>(Leaf::count);
constexpr std::array<std::string_view, kLeafCount> kLeafKeys{
    "directive_ui",
    "ikora_carrier_model_suppression",
    "ikora_vfx_rebind",
    "scene_authority",
    "gate_authority",
    "portal_mutation",
    "synthetic_stage_machine",
    "unsafe_diagnostics",
};

using FlagSet = std::array<bool, kLeafCount>;

struct EffectivePolicy final {
    FlagSet enabled{};
    bool dependencyMissing{};

    friend bool operator==(const EffectivePolicy&, const EffectivePolicy&) = default;
};

int g_failureCount = 0;
std::string_view g_context = "startup";

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }

    std::cerr << __FILE__ << ':' << line << ": check failed in " << g_context << ": " << expression
              << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

constexpr std::size_t index(Leaf leaf) noexcept {
    return static_cast<std::size_t>(leaf);
}

constexpr FlagSet requested_flags(const Omega& omega) noexcept {
    return {
        omega.directiveUi,
        omega.ikoraCarrierModelSuppression,
        omega.ikoraVfxRebind,
        omega.sceneAuthority,
        omega.gateAuthority,
        omega.portalMutation,
        omega.syntheticStageMachine,
        omega.unsafeDiagnostics,
    };
}

void set_all(Omega& omega, bool value) noexcept {
    omega.directiveUi = value;
    omega.ikoraCarrierModelSuppression = value;
    omega.ikoraVfxRebind = value;
    omega.sceneAuthority = value;
    omega.gateAuthority = value;
    omega.portalMutation = value;
    omega.syntheticStageMachine = value;
    omega.unsafeDiagnostics = value;
}

// Consumers preserve every requested bit, but the bootflow manifest and activity publishers make
// Scene, Gate, and Portal effective only under the provisional stage-machine master.
constexpr EffectivePolicy effective_policy(const Omega& requested) noexcept {
    EffectivePolicy effective{};
    const FlagSet flags = requested_flags(requested);
    effective.enabled[index(Leaf::directiveUi)] = flags[index(Leaf::directiveUi)];
    effective.enabled[index(Leaf::ikoraCarrierModelSuppression)] =
        flags[index(Leaf::ikoraCarrierModelSuppression)];
    effective.enabled[index(Leaf::ikoraVfxRebind)] = flags[index(Leaf::ikoraVfxRebind)];
    effective.enabled[index(Leaf::syntheticStageMachine)] =
        flags[index(Leaf::syntheticStageMachine)];
    effective.enabled[index(Leaf::unsafeDiagnostics)] = flags[index(Leaf::unsafeDiagnostics)];

    const bool master = flags[index(Leaf::syntheticStageMachine)];
    effective.enabled[index(Leaf::sceneAuthority)] = master && flags[index(Leaf::sceneAuthority)];
    effective.enabled[index(Leaf::gateAuthority)] = master && flags[index(Leaf::gateAuthority)];
    effective.enabled[index(Leaf::portalMutation)] = master && flags[index(Leaf::portalMutation)];
    effective.dependencyMissing =
        !master
        && (flags[index(Leaf::sceneAuthority)] || flags[index(Leaf::gateAuthority)]
            || flags[index(Leaf::portalMutation)]);
    return effective;
}

std::string document_with_omega_member(std::string_view member) {
    std::string document = R"({"version":6,"experiments":{"omega":{)";
    document.append(member);
    document.append("}}}");
    return document;
}

bool parse_document(std::string_view document, Settings& output) noexcept {
    return sunrise::core::settings::parse(document, output);
}

void all_off_defaults_are_stable() {
    g_context = "all-off defaults";
    CHECK(sunrise::core::settings::kSettingsVersion == 6U);

    const Settings fixedDefaults = sunrise::core::settings::defaults();
    CHECK(fixedDefaults.version == 6U);
    CHECK(requested_flags(fixedDefaults.omegaExperiments) == FlagSet{});
    CHECK(effective_policy(fixedDefaults.omegaExperiments) == EffectivePolicy{});

    for (const std::string_view document : {
             std::string_view{R"({})"},
             std::string_view{R"({"version":6})"},
             std::string_view{R"({"version":6,"experiments":{}})"},
             std::string_view{R"({"version":6,"experiments":{"omega":{}}})"},
         }) {
        Settings parsed{};
        CHECK(parse_document(document, parsed));
        CHECK(parsed.version == 6U);
        CHECK(requested_flags(parsed.omegaExperiments) == FlagSet{});
        CHECK(effective_policy(parsed.omegaExperiments) == EffectivePolicy{});
    }

    Settings explicitOff{};
    CHECK(parse_document(
        R"({"version":6,"experiments":{"omega":{"directive_ui":false,"ikora_carrier_model_suppression":false,"ikora_vfx_rebind":false,"scene_authority":false,"gate_authority":false,"portal_mutation":false,"synthetic_stage_machine":false,"unsafe_diagnostics":false}}})",
        explicitOff));
    CHECK(requested_flags(explicitOff.omegaExperiments) == FlagSet{});
}

void every_requested_leaf_is_parsed_independently() {
    for (std::size_t leaf = 0U; leaf < kLeafCount; ++leaf) {
        g_context = kLeafKeys[leaf];
        const std::string member = std::string{"\""} + std::string{kLeafKeys[leaf]} + "\":true";
        Settings parsed{};
        CHECK(parse_document(document_with_omega_member(member), parsed));

        FlagSet expected{};
        expected[leaf] = true;
        CHECK(requested_flags(parsed.omegaExperiments) == expected);

        EffectivePolicy expectedEffective{};
        if (leaf != index(Leaf::sceneAuthority) && leaf != index(Leaf::gateAuthority)
            && leaf != index(Leaf::portalMutation)) {
            expectedEffective.enabled[leaf] = true;
        } else {
            expectedEffective.dependencyMissing = true;
        }
        CHECK(effective_policy(parsed.omegaExperiments) == expectedEffective);
    }
}

void stage_master_gates_only_its_three_dependants() {
    constexpr std::array<Leaf, 3> kDependants{
        Leaf::sceneAuthority,
        Leaf::gateAuthority,
        Leaf::portalMutation,
    };

    for (const Leaf dependant : kDependants) {
        g_context = kLeafKeys[index(dependant)];
        const std::string member = std::string{"\"synthetic_stage_machine\":true,\""}
                                   + std::string{kLeafKeys[index(dependant)]} + "\":true";
        Settings parsed{};
        CHECK(parse_document(document_with_omega_member(member), parsed));

        FlagSet expectedRequested{};
        expectedRequested[index(Leaf::syntheticStageMachine)] = true;
        expectedRequested[index(dependant)] = true;
        CHECK(requested_flags(parsed.omegaExperiments) == expectedRequested);

        EffectivePolicy expectedEffective{};
        expectedEffective.enabled = expectedRequested;
        CHECK(effective_policy(parsed.omegaExperiments) == expectedEffective);
    }

    g_context = "master with all dependants";
    Settings parsed{};
    CHECK(parse_document(
        document_with_omega_member(
            R"("scene_authority":true,"gate_authority":true,"portal_mutation":true,"synthetic_stage_machine":true)"),
        parsed));
    CHECK(requested_flags(parsed.omegaExperiments)
          == (FlagSet{false, false, false, true, true, true, true, false}));
    CHECK(effective_policy(parsed.omegaExperiments).enabled
          == (FlagSet{false, false, false, true, true, true, true, false}));
    CHECK(!effective_policy(parsed.omegaExperiments).dependencyMissing);
}

void unknown_values_are_ignored_without_enabling_flags() {
    g_context = "unknown values";
    Settings parsed{};
    CHECK(parse_document(
        R"({"version":6,"future_root":{"experiments":true},"experiments":{"future_group":[null,false,{"nested":7}],"omega":{"future_leaf":{"array":[1,2,3]},"directive_ui":true}}})",
        parsed));

    FlagSet expected{};
    expected[index(Leaf::directiveUi)] = true;
    CHECK(requested_flags(parsed.omegaExperiments) == expected);

    Settings unknownOnly{};
    CHECK(parse_document(
        R"({"version":6,"experiments":{"omega":{"future_bool":true,"future_null":null,"future_number":-1.5e2,"future_string":"scene_authority","future_array":[true,false],"future_object":{"x":1}}}})",
        unknownOnly));
    CHECK(requested_flags(unknownOnly.omegaExperiments) == FlagSet{});
}

void malformed_and_duplicate_values_fail_transactionally() {
    constexpr std::array<std::string_view, 5> kMalformedKnownValues{
        "null",
        "0",
        R"("true")",
        "{}",
        "[]",
    };

    for (std::size_t leaf = 0U; leaf < kLeafCount; ++leaf) {
        g_context = kLeafKeys[leaf];
        const std::string member = std::string{"\""} + std::string{kLeafKeys[leaf]} + "\":null";
        Settings output{};
        output.version = 123U;
        set_all(output.omegaExperiments, true);
        CHECK(!parse_document(document_with_omega_member(member), output));
        CHECK(output.version == 123U);
        CHECK(requested_flags(output.omegaExperiments)
              == (FlagSet{true, true, true, true, true, true, true, true}));

        const std::string duplicate = std::string{"\""} + std::string{kLeafKeys[leaf]}
                                      + "\":true,\"" + std::string{kLeafKeys[leaf]} + "\":false";
        CHECK(!parse_document(document_with_omega_member(duplicate), output));
        CHECK(output.version == 123U);
        CHECK(requested_flags(output.omegaExperiments)
              == (FlagSet{true, true, true, true, true, true, true, true}));
    }

    for (const std::string_view value : kMalformedKnownValues) {
        g_context = value;
        Settings output{};
        const std::string member = std::string{"\"directive_ui\":"} + std::string{value};
        CHECK(!parse_document(document_with_omega_member(member), output));
    }

    for (
        const std::string_view malformed : {
            std::string_view{R"({"version":6,"experiments":[]})"},
            std::string_view{R"({"version":6,"experiments":{"omega":[]}})"},
            std::string_view{
                R"({"version":6,"experiments":{"omega":{"directive_ui":true,"directive_ui":false}}})"},
            std::string_view{R"({"version":6,"experiments":{"omega":{},"omega":{}}})"},
            std::string_view{R"({"version":6,"experiments":{},"experiments":{}})"},
            std::string_view{R"({"version":6,"experiments":{"omega":{"future":[1,]}}})"},
            std::string_view{R"({"version":"6","experiments":{"omega":{}}})"},
            std::string_view{R"({"version":6,"experiments":{"omega":{"directive_ui":true,}}})"},
        }) {
        g_context = malformed;
        Settings output{};
        output.version = 123U;
        set_all(output.omegaExperiments, true);
        CHECK(!parse_document(malformed, output));
        CHECK(output.version == 123U);
        CHECK(requested_flags(output.omegaExperiments)
              == (FlagSet{true, true, true, true, true, true, true, true}));
    }
}

void version_six_documents_keep_optional_experiments_compatible() {
    g_context = "version 6 compatibility";
    Settings legacy{};
    CHECK(parse_document(R"({"version":6,"unchanged_v6_member":{"value":17}})", legacy));
    CHECK(legacy.version == 6U);
    CHECK(requested_flags(legacy.omegaExperiments) == FlagSet{});

    Settings extended{};
    CHECK(parse_document(R"({"experiments":{"omega":{"unsafe_diagnostics":true}},"version":6})",
                         extended));
    CHECK(extended.version == 6U);
    FlagSet expected{};
    expected[index(Leaf::unsafeDiagnostics)] = true;
    CHECK(requested_flags(extended.omegaExperiments) == expected);
}

} // namespace

void coo_selection_is_optional_and_strict() {
    Settings output{};
    CHECK(parse_document(R"({"version":6})", output));
    CHECK(!output.omegaExperiments.cooExecutor);
    CHECK(parse_document(R"({"version":6,"experiments":{"omega":{"coo_executor":true}}})", output));
    CHECK(output.omegaExperiments.cooExecutor);
    CHECK(!parse_document(R"({"version":6,"experiments":{"omega":{"coo_executor":1}}})", output));
    CHECK(!parse_document(R"({"version":6,"experiments":{"omega":{"coo_executor":true,"coo_executor":false}}})", output));
}

int main() {
    coo_selection_is_optional_and_strict();
    all_off_defaults_are_stable();
    every_requested_leaf_is_parsed_independently();
    stage_master_gates_only_its_three_dependants();
    unknown_values_are_ignored_without_enabling_flags();
    malformed_and_duplicate_values_fail_transactionally();
    version_six_documents_keep_optional_experiments_compatible();

    g_context = "unrelated parser seam";
    CHECK(sunrise::core::settings::parser::g_unexpectedParserBranchCalls == 0U);

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " Omega experiment setting check(s) failed\n";
        return 1;
    }

    std::cout << "all Omega experiment setting checks passed\n";
    return 0;
}
