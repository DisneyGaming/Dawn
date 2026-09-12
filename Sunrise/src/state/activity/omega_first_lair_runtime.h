#pragma once

#include "omega_first_lair_encounter.h"
#include "omega_crown_respawn_authority.h"
#include "omega_archive_arm_control.h"
#include "omega_archive_intro_control.h"

namespace sunrise::state::activity::omega_first_lair {
struct Authority final {
    omega_archive_arm::Control arm{};
    omega_archive_intro::Program intro{};
    std::uint32_t generation{};
    std::array<std::uint8_t,21> loose{};
    bool anchor{},cannon{};
    bool finalCannon{};
    bool crownRestricted{};
    std::uint32_t crownGeneration{};
    std::array<std::array<std::uint8_t,2>,16> crownLoose{};
    bool crownAnchor{};
    std::array<std::array<std::uint8_t,2>,21> hiveLoose{};
    std::array<std::array<std::uint8_t,2>,19> vexLoose{};
    std::array<std::array<std::uint8_t,2>,12> cabalLoose{};
    bool hiveAnchor{},vexAnchor{},cabalAnchor{};
    omega_rescue_npc::Commands rescueScenes{};
    std::uint8_t cycle{},wave{};
    bool routeEnabled{},chargeEnabled{},exitCannon{},endingRequested{},chargeDunked{};
    bool eyeStatusActive{};
    bool transitLaunches{},transitBridge{},transitTarget{},finalTraversal{};
    bool returnLaunch{};
    std::uint64_t transitCreated{}; // native creation receipts per omega_crown_transit::kSources index
    omega_crown_respawn::Restriction restriction{};
    std::uint16_t rescueMarkerReadyMask{};

};
struct Status final {
    Boss boss{};
    Action action{};
    Phase phase{};
    bool enabled{},failed{};
    std::uint8_t island{};
    std::uint8_t cycle{},wave{};
    CrownStage crownStage{};
    CrownToken token{};
    omega_archive_arm::Status arm{};
    omega_archive_intro::Status intro{};
};
/** Called for the admitted local Omega roster. Preparation preserves all
 * authored source placements and uses zero loose requests until a lift starts. */
[[nodiscard]] Authority authority(std::uint64_t run,std::uint32_t generation,bool executorOwned=false) noexcept;
[[nodiscard]] Status status(std::uint64_t run) noexcept;
/** Native intro graph node 4 (two-arm summon clip) loaded for the bound boss. */
[[nodiscard]] bool request_intro(const omega_archive_intro::Owner& owner) noexcept;
[[nodiscard]] bool request_boss_program(const omega_archive_intro::Owner& owner,std::uint32_t sequence) noexcept;
[[nodiscard]] bool request_boss_movement(const omega_archive_intro::Owner& owner) noexcept;
[[nodiscard]] bool observe_intro_control(const omega_archive_intro::Owner& owner) noexcept;
void observe_initial_summon(const Boss& boss) noexcept;
void observe_initial_idle(const Boss& boss) noexcept;
[[nodiscard]] bool claim_action(const Boss& boss,Action action) noexcept;
void observe_summon(const Boss& boss,Action action,bool finished) noexcept;
[[nodiscard]] bool prepare_arm(const omega_archive_arm::Owner& owner) noexcept;
[[nodiscard]] bool observe_arm_control(const omega_archive_arm::Owner& owner,
    const omega_archive_arm::NativeControl& receipt,const std::array<float,4>& left,
    const std::array<float,4>& right) noexcept;
[[nodiscard]] bool release_arm(const omega_archive_arm::Owner& owner,bool completed) noexcept;
[[nodiscard]] bool observe_admission(const ActorReceipt& receipt) noexcept;
[[nodiscard]] bool observe_death(const ActorReceipt& receipt) noexcept;
void observe_departure(const Boss& boss,bool folded,bool atMilestone) noexcept;
[[nodiscard]] bool observe_arrival(std::uint64_t run,std::uint8_t islandIndex) noexcept;
[[nodiscard]] bool observe_crown_arrival(std::uint64_t run) noexcept;
void observe_cannon_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index=0) noexcept;
[[nodiscard]] bool observe_animation(const CrownToken& token,AnimationMilestone event) noexcept;
[[nodiscard]] bool observe_scene(const CrownToken& token,std::uint16_t sceneSlot,SceneMilestone event) noexcept;
[[nodiscard]] SceneRequest scene_request(std::uint64_t run,std::uint16_t sceneSlot) noexcept;
[[nodiscard]] bool observe_charge(const ChargeReceipt& receipt,ChargeMilestone event) noexcept;
[[nodiscard]] bool observe_gate_arrival(const CrownToken& token,GateMilestone gate,std::uint32_t player) noexcept;
[[nodiscard]] bool observe_health(const CrownToken& token,HealthMilestone event) noexcept;
[[nodiscard]] bool observe_ending(const CrownToken& token,bool finished) noexcept;
void observe_transit_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t coreIndex) noexcept;
/** Native entity creation at the exact active/committed revision. Return
 * launcher receipts additionally require its full new entity each cycle. */
bool observe_transit_created(std::uint64_t run,std::uint8_t index,std::uint32_t entity=UINT32_MAX) noexcept;
/** Exact native creation of an authored transform provider, before Scene cast. */
[[nodiscard]] bool observe_rescue_marker_created(std::uint64_t run,std::uint32_t generation,
                                                 std::uint16_t slot) noexcept;
void invalidate(std::uint64_t run) noexcept;
[[nodiscard]] bool publication_due(std::uint64_t now) noexcept;
void reset() noexcept;
} // namespace sunrise::state::activity::omega_first_lair
