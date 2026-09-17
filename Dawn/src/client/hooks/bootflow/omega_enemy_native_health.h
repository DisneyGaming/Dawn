#pragma once

#include <cstdint>

namespace dawn::client::hooks::bootflow::omega_enemy_native_health {

[[nodiscard]] constexpr bool owner(std::uint32_t actorEntity,
    std::uint32_t characterEntity) noexcept {
    return actorEntity!=UINT32_MAX && characterEntity==actorEntity;
}

/** Character+2E8 is an authored health interface reference. Validate its exact
 * typed runtime and complete identities before interpreting copied raw fields. */
[[nodiscard]] constexpr bool identity(std::uint32_t referenceHandle,
    std::uint32_t referenceKind,std::int64_t referenceOffset,
    std::uint32_t runtimeKind,std::uint32_t runtimeSelf,
    std::uint32_t expectedEntity,std::uint32_t runtimeEntity) noexcept {
    return referenceHandle!=UINT32_MAX && referenceKind==0x80804BEEU
        && referenceOffset==0 && runtimeKind==0x80804B8AU
        && runtimeSelf==referenceHandle && expectedEntity!=UINT32_MAX
        && runtimeEntity==expectedEntity;
}

/** First failing death qualification, in the exact order death() tests them.
 * Diagnostic only: the encounter never consumes this value. */
enum class DeathRejection : std::uint8_t {
    none,eventInvalid,eventClass,healthInvalid,deathBitUnset,generation
};
[[nodiscard]] constexpr DeathRejection death_rejection(bool eventValid,
    std::uint32_t eventClass,bool healthValid,std::uint8_t healthFlags,
    std::uint32_t authorityGeneration,std::uint32_t senseGeneration) noexcept {
    if(!eventValid) { return DeathRejection::eventInvalid; }
    if(eventClass!=0x80804C54U) { return DeathRejection::eventClass; }
    if(!healthValid) { return DeathRejection::healthInvalid; }
    if((healthFlags&1U)==0) { return DeathRejection::deathBitUnset; }
    if(authorityGeneration==0 || authorityGeneration!=senseGeneration) {
        return DeathRejection::generation;
    }
    return DeathRejection::none;
}

/** CDAC00 sets the native health death bit before publishing this exact event.
 * Source reset changes authority generation before retiring old actors and only
 * updates sense generation afterward. Original admission identity is checked by
 * the encounter state; enum and instigator intentionally do not restrict deaths. */
[[nodiscard]] constexpr bool death(bool eventValid,std::uint32_t eventClass,
    bool healthValid,std::uint8_t healthFlags,std::uint32_t authorityGeneration,
    std::uint32_t senseGeneration) noexcept {
    return death_rejection(eventValid,eventClass,healthValid,healthFlags,
        authorityGeneration,senseGeneration)==DeathRejection::none;
}

} // namespace dawn::client::hooks::bootflow::omega_enemy_native_health
