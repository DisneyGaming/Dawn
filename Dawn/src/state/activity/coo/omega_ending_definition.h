#pragma once
#include "executor.h"
#include "../omega_ending_rules.h"

namespace dawn::state::activity::coo::ending {
namespace native=omega_ending;
// Mechanic bindings identify adapters; they do not invent native roster slots.
inline constexpr Asset kRetirement{0x95FB2E01U,0x80F45253U,0,0};
inline constexpr Asset kBookend{native::kRegistry,native::kDefinition,6,native::kSlot};
inline constexpr Asset kArrival{0x80F50039U,native::kArrivalSpawnSet,0,native::kSlice};
inline constexpr Asset kHandoff{0x76D13282U,0x3FF918C6U,0,29};
enum class Request : std::uint32_t { retire=1,arrive,play,active,finish,handoff };
inline constexpr CommandSpec kDialogue[]{{Operation::observation,kRetirement,0,Wait::observed}};
inline constexpr CommandSpec kRetire[]{
    {Operation::mechanic,kRetirement,static_cast<unsigned>(Request::retire),Wait::requested},
    {Operation::observation,kRetirement,1,Wait::observed}};
inline constexpr CommandSpec kPrepare[]{
    {Operation::traversal,kArrival,static_cast<unsigned>(Request::arrive),Wait::requested},
    {Operation::observation,kBookend,2,Wait::observed}};
inline constexpr CommandSpec kEligibility[]{{Operation::observation,kBookend,2,Wait::observed}};
inline constexpr CommandSpec kPlay[]{
    {Operation::cinematic,kBookend,static_cast<unsigned>(Request::play),Wait::requested},
    {Operation::observation,kBookend,3,Wait::observed}};
inline constexpr CommandSpec kActive[]{
    {Operation::cinematic,kBookend,static_cast<unsigned>(Request::active),Wait::requested},
    {Operation::observation,kBookend,4,Wait::observed}};
inline constexpr CommandSpec kFinish[]{{Operation::cinematic,kBookend,static_cast<unsigned>(Request::finish),Wait::requested}};
inline constexpr CommandSpec kLaunch[]{
    {Operation::traversal,kHandoff,static_cast<unsigned>(Request::handoff),Wait::requested},
    {Operation::observation,kHandoff,6,Wait::observed}};
inline constexpr Step kSteps[]{
    {"final dialogue",0,kDialogue},
    {"native Lair retirement",1,kRetire},
    {"bookend arrival and native camera eligibility",2,kPrepare},
    {"native cinematic activation",4,kPlay},
    {"native cinematic completion",8,kActive},
    {"retire cinematic command",16,kFinish},
    {"native Mercury launch queued",32,kLaunch}};
// Retry only camera acquisition. Retirement and teleport are not replayed.
inline constexpr Step kRetrySteps[]{
    {"native camera eligibility",0,kEligibility},
    {"native cinematic activation",1,kPlay},
    {"native cinematic completion",2,kActive},
    {"retire cinematic command",4,kFinish},
    {"native Mercury launch queued",8,kLaunch}};
inline constexpr ReceiptBinding kReceipts[]{{"dialogue.complete",0,0},{"lair.retired",1,1},{"camera.eligible",2,1},{"camera.active",3,1},{"camera.complete",4,1},{"handoff.queued",6,1}};
inline constexpr Definition kDefinition{"Omega ending and handoff",Schema::omegaArchive,kSteps, kReceipts};
inline constexpr ReceiptBinding kRetryReceipts[]{{"camera.eligible",0,0},{"camera.active",1,1},{"camera.complete",2,1},{"handoff.queued",4,1}};
inline constexpr Definition kRetry{"Omega ending camera retry",Schema::omegaArchive,kRetrySteps, kRetryReceipts};
} // namespace dawn::state::activity::coo::ending
