#pragma once

#include <cstddef>
#include <cstdint>

#include "../../../state/activity/omega_first_lair_encounter.h"
#include "../../hooking/call_gate.h"

namespace dawn::client::hooks::bootflow::omega_boss_health {

using Resolve = void*(__fastcall*)(std::uint32_t) noexcept;

/** Fractions returned by the original regional-health accessor. The body and
 * eye are different native regions; the body's raw +320 word is not health. */
struct Sample final {
    std::uint32_t handle{UINT32_MAX};
    float body{},eye{};
    bool dead{};
};

/** Why a native read was not accepted. Every silent return in this observer
 * names one of these so a missing receipt in the next live run is explained.
 * The values are stable log identifiers; append new ones at the end. */
enum class Reject : std::uint8_t {
    none,
    arguments,          // null image/resolve/character or invalid boss token
    getterPrefix,       // CD6C20 prologue bytes differ from the pinned image
    characterRead,      // character bytes unreadable
    characterIdentity,  // not {80F6690B,80806832,738} / self / entity / actor
    healthRead,         // +2E8 reference did not resolve or was unreadable
    healthIdentity,     // not {815B5A40,80804B8A,F98} / self / entity
    fractionInvalid,    // native getter returned NaN or out of [0,1]
    healthRecheck,      // health component changed under the native getters
    characterRecheck,   // character or its +2E8 reference changed under the getters
    stateNotFinalDeath, // encounter is not in the current cycle-3 endEyePhase
    actorRow,           // image+1F9D7F8 actor row self/entity mismatch
    memberReference,    // actor +60 member reference invalid
    memberIdentity,     // member prefix/self/generation/revision/disabled/actor mismatch
    leaseConflict,      // another live run already retains a death lease
    noLease,            // death observed with no armed lease (normal for adds)
    eventHeader,        // 9ECC70 header unreadable or class is not 80804C54
    deathBit,           // native health +338 bit0 not yet set by CDAC00
    gateClosed          // call gate no longer accepts side effects
};
[[nodiscard]] const char* reject_name(Reject reason) noexcept;

/** Runs synchronously inside the existing native boss-member callback. It
 * retains no character/health pointer, performs no writes, and does not advance
 * encounter state. The caller must qualify the sample against its current
 * run/cycle/action token before reporting a health milestone. `reason`, when
 * supplied, receives the first failing guard. */
[[nodiscard]] bool sample(std::uintptr_t image,Resolve resolve,
    const state::activity::omega_first_lair::Boss& boss,
    const void* character,Sample& result,Reject* reason=nullptr) noexcept;

/** Retain the exact final endEyePhase request before issuing its native graph
 * event. The existing C72390 detour delivers death before actor retirement. */
[[nodiscard]] bool arm_native_death(std::uintptr_t image,Resolve resolve,
    const state::activity::omega_first_lair::CrownToken& token,Reject* reason=nullptr) noexcept;
void observe_native_death(void* character,std::uint32_t event,
    const hooking::CallGate::Scope& scope) noexcept;
void reset() noexcept;

} // namespace dawn::client::hooks::bootflow::omega_boss_health
