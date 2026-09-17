#pragma once

namespace dawn::core::settings::experiments {

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
    /** Enables Dawn's provisional Omega stage policy; its numbers are not retail states. */
    bool syntheticStageMachine{};
    /** Enables invasive/high-volume RE hooks which are unsuitable for a trusted baseline run. */
    bool unsafeDiagnostics{};
    /** Writes the privacy-allowlisted open-world population census JSONL artifact. */
    bool openWorldCensus{};
    /** Select the CoO adapter at the next Omega run; retain legacy by default. */
    bool cooExecutor{};
    /**
     * Haunted Forest candy drops: the client-side hook that writes the one pending-drop record the
     * stripped reward sheets would have produced, so the native bauble spawns and reports 601.
     */
    bool forestCandyDrops{};
    /**
     * Haunted Forest reward coffers: publishes the five o_coffer placements (registry slots 63-67)
     * alongside the end-of-run chest. Off until a live roster proves those slots carry the
     * placement auth flag, because the rewards-phase placement frame is all-or-nothing.
     */
    bool forestRewardCoffers{};
};

} // namespace dawn::core::settings::experiments
