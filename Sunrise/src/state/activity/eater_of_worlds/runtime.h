#pragma once

#include "controller.h"
#include "../lifecycle_generation.h"

namespace sunrise::state::activity::eater_of_worlds {

using ResetReceipts = void(*)() noexcept;
void set_reset_receipts(ResetReceipts callback) noexcept;

[[nodiscard]] coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept;
bool prepare(std::uint64_t run, bool selected) noexcept;
Frame snapshot(std::uint64_t run, std::uint64_t now, bool ready, int region) noexcept;
Request request() noexcept;
ObjectRequest object_request(std::size_t assetIndex) noexcept;
GateRequest gate_request(coo::Asset gate) noexcept;
GrateRequest grate_request() noexcept;
SourceRequest source_request(std::uint32_t registry,std::uint16_t slot) noexcept;
ContactRequest contact_request() noexcept;
ArrivalRequest arrival_request() noexcept;
bool observe_arrival_motion(const ArrivalMotion&) noexcept;
bool owner_current(coo::Generation owner) noexcept;
std::uint64_t native_run() noexcept;
// Native selection runs inside source publication. Do not acquire the controller
// mutex there; this publication authenticates the selected Eater run only.
std::uint64_t native_roster_run() noexcept;
bool publication_due(std::uint64_t now) noexcept;
bool due(std::uint64_t now) noexcept;
void observe_position(float x, float y, float z) noexcept;
void observe_prepared(coo::Generation owner, coo::Asset asset) noexcept;
void observe_object(const coo::ObjectReceipt& receipt) noexcept;
bool observe_grate_pose(const GratePoseReceipt& receipt) noexcept;
bool observe_platform_pose(const PlatformPoseReceipt& receipt) noexcept;
bool observe_platform_contact(const PlatformContact& receipt) noexcept;
void observe_player(std::uint32_t player) noexcept;
void observe_player_health(std::uint64_t run,std::uint32_t player,bool dead) noexcept;
bool observe_health(const HealthReceipt& receipt, bool dead) noexcept;
bool observe_cranium(const CraniumReceipt& receipt, bool held, std::uint32_t player) noexcept;
bool observe_station(const StationReceipt& receipt, std::uint32_t player) noexcept;
void observe_device(coo::Generation owner, coo::Asset asset, std::int16_t revision,
                    float position) noexcept;
bool observe_admission(const EnemyReceipt& receipt) noexcept;
bool observe_death(const EnemyReceipt& receipt) noexcept;
bool observe_source_retired(std::uint64_t run,std::uint32_t key,std::uint16_t slot,std::uint32_t generation) noexcept;
void observe_readiness(const EnemyReceipt& receipt, coo::EnemyReadiness value) noexcept;
void observe_costs(std::uint32_t key, std::uint16_t slot, const coo::TaskCosts& costs) noexcept;
void observe_player_trigger(std::uint64_t run, std::uint32_t key, std::uint16_t slot) noexcept;
void observe_monitor(std::uint32_t key, std::uint16_t slot, bool any,
                     std::int32_t count, std::int32_t value) noexcept;
void observe_combatant(std::uint64_t run, std::uint32_t key, std::uint16_t slot,
                       const middleware::bap::activity_message::combatant_sense::Output&) noexcept;
void observe_scene(std::uint64_t run, std::uint32_t key, std::uint16_t slot,
                   const middleware::bap::activity_message::scene_sense::Output&) noexcept;
void observe_squad(std::uint64_t run, std::uint32_t key, std::uint16_t slot,
                   const middleware::bap::activity_message::squad_sense::Output&) noexcept;
void observe_submission(std::uint64_t run, std::uint32_t tag, std::int64_t offset,
                        std::uint32_t bank, std::uint8_t row, std::uint32_t generation) noexcept;
void observe_capacity(std::uint64_t run, coo::PopulationCapacity capacity) noexcept;

} // namespace sunrise::state::activity::eater_of_worlds
