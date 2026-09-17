#pragma once
#include "round_sequence_service.h"

namespace dawn::server::runtime::activity::haunted_forest::sequence_data {
namespace coo=state::activity::coo;
// Installed source components at +658: entity reference +58, zero parameter
// rows at +78. The native sequences retain their own authored effect graphs.
inline constexpr coo::CommandSpec kTeleportCommands[]{
    {coo::Operation::scene,{0x34D23982,0x81550021,5,89},1,coo::Wait::requested}};
inline constexpr coo::Step kTeleportSteps[]{{"seq_tele_boss",0,kTeleportCommands}};
inline constexpr coo::Definition kTeleportGraph{"haunted_teleport_presentation",coo::Schema::otherMissions,kTeleportSteps,{}};
inline constexpr coo::CommandSpec kDefeatedCommands[]{
    {coo::Operation::scene,{0x41D79D07,0x8155018E,5,129},1,coo::Wait::requested}};
inline constexpr coo::Step kDefeatedSteps[]{{"seq_boss_killed",0,kDefeatedCommands}};
inline constexpr coo::Definition kDefeatedGraph{"haunted_terror_defeated_presentation",coo::Schema::otherMissions,kDefeatedSteps,{}};
}
