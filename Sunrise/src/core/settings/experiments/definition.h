#pragma once

namespace sunrise::core::settings::experiments {

/**
 * Experimental Omega paths. Every switch is deliberately off by default: a missing setting must
 * preserve the retail/native path until the corresponding authority contract is proven.
 */
struct Omega final {
    /** Installs C252E306 through the local HUD manager instead of replicated directive authority. */
    bool directiveUi{};
    /** Suppresses the three render-model constructors owned by Scene carrier 80EC0FA8. */
    bool ikoraCarrierModelSuppression{};
    /** Reserved for a proven provider/socket VFX rebind; the retired actor-root delta is invalid. */
    bool ikoraVfxRebind{};
    /** Enables experimental Scene authority and its temporary local retirement bridge. */
    bool sceneAuthority{};
    /** Enables experimental 80804F45 barrier authority. */
    bool gateAuthority{};
    /** Enables experimental portal visuals and region-transition mutation. */
    bool portalMutation{};
    /** Enables Sunrise's provisional Omega stage policy; its numbers are not retail states. */
    bool syntheticStageMachine{};
    /** Enables invasive/high-volume RE hooks which are unsuitable for a trusted baseline run. */
    bool unsafeDiagnostics{};
    /** Select the CoO adapter at the next Omega run; retain legacy by default. */
    bool cooExecutor{};
};

} // namespace sunrise::core::settings::experiments
