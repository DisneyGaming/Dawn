#include "../parser.h"

namespace dawn::core::settings::parser {

/** Parses the top-level experimental feature group. */
bool Parser::experiment_settings(experiments::Omega& output) noexcept {
    if (!consume('{')) {
        return false;
    }
    experiments::Omega candidate = output;
    bool hasOmega = false;
    if (consume('}')) {
        return true;
    }
    for (;;) {
        std::string_view key;
        if (!string(key) || !consume(':')) {
            return false;
        }
        if (key == "omega") {
            if (hasOmega || !omega_experiment_settings(candidate)) {
                return false;
            }
            hasOmega = true;
        } else if (!skip_value(0)) {
            return false;
        }
        if (consume('}')) {
            output = candidate;
            return true;
        }
        if (!consume(',')) {
            return false;
        }
    }
}

/** Parses independently controlled Omega experiments over their default-off state. */
bool Parser::omega_experiment_settings(experiments::Omega& output) noexcept {
    if (!consume('{')) {
        return false;
    }
    experiments::Omega candidate = output;
    bool hasDirectiveUi = false;
    bool hasCarrierSuppression = false;
    bool hasVfxRebind = false;
    bool hasSceneAuthority = false;
    bool hasGateAuthority = false;
    bool hasPortalMutation = false;
    bool hasSyntheticStageMachine = false;
    bool hasUnsafeDiagnostics = false;
    if (consume('}')) {
        return true;
    }
    for (;;) {
        std::string_view key;
        if (!string(key) || !consume(':')) {
            return false;
        }
        if (key == "directive_ui") {
            if (hasDirectiveUi || !boolean(candidate.directiveUi)) {
                return false;
            }
            hasDirectiveUi = true;
        } else if (key == "ikora_carrier_model_suppression") {
            if (hasCarrierSuppression || !boolean(candidate.ikoraCarrierModelSuppression)) {
                return false;
            }
            hasCarrierSuppression = true;
        } else if (key == "ikora_vfx_rebind") {
            if (hasVfxRebind || !boolean(candidate.ikoraVfxRebind)) {
                return false;
            }
            hasVfxRebind = true;
        } else if (key == "scene_authority") {
            if (hasSceneAuthority || !boolean(candidate.sceneAuthority)) {
                return false;
            }
            hasSceneAuthority = true;
        } else if (key == "gate_authority") {
            if (hasGateAuthority || !boolean(candidate.gateAuthority)) {
                return false;
            }
            hasGateAuthority = true;
        } else if (key == "portal_mutation") {
            if (hasPortalMutation || !boolean(candidate.portalMutation)) {
                return false;
            }
            hasPortalMutation = true;
        } else if (key == "synthetic_stage_machine") {
            if (hasSyntheticStageMachine || !boolean(candidate.syntheticStageMachine)) {
                return false;
            }
            hasSyntheticStageMachine = true;
        } else if (key == "unsafe_diagnostics") {
            if (hasUnsafeDiagnostics || !boolean(candidate.unsafeDiagnostics)) {
                return false;
            }
            hasUnsafeDiagnostics = true;
        } else if (!skip_value(0)) {
            return false;
        }
        if (consume('}')) {
            output = candidate;
            return true;
        }
        if (!consume(',')) {
            return false;
        }
    }
}

} // namespace dawn::core::settings::parser
