#pragma once

#include "omega_arc_charge_catalog.h"
#include "omega_first_mancannon_authority.h"

namespace dawn::state::activity::omega_arc_charge {

struct Authority final {
    std::uint32_t generation{};
    std::uint8_t cycle{}; // Encounter cycle 1..3; zero prepares every carrier dormant.
    bool enabled{}, dunked{};
    // Transit bridge latch (route enabled and the charge picked up at least once).
    // Retail spawns the dunk pedestal together with the extending bridge after the
    // pickup, not with the charge; the carry object itself only needs `enabled`.
    bool carried{};
};

[[nodiscard]] constexpr bool active(const Source& source, const Authority& authority) noexcept {
    if (source.cycle == nullptr || authority.cycle != source.cycle->index + 1U) { return false; }
    // The authored o_dunk_end_fx is the target at the end of the walkway.
    // Create it with the pickup/bridge latch; its device fades it on dunk.
    // Keep the source generation active so dunk cannot recreate the effect.
    if (source.object == Object::effect) { return authority.carried || authority.dunked; }
    if (source.object == Object::sink) { return authority.enabled && authority.carried && !authority.dunked; }
    return source.object == Object::carry && authority.enabled && !authority.dunked;
}

/** Every Arc-charge source definition (80F47672/78/75 and the BF05/BF03 rows)
 * carries definition byte +4C8+94 == 1: native deferred creation. Original
 * 9F19F0 never creates a deferred object; tick 9F2F30 creates only while
 * `+2F0 (last committed generation) < +180 (applied generation)` and +188 is
 * active, and the dormant prepare itself commits +2F0 = generation. Activating
 * at the unchanged generation therefore never creates the item, sink or end FX.
 * Retiring needs another newer generation so 9EFAE0 releases the entity.
 * This is the same rule transit::write_source and the first-cannon FX use. */
enum class Lifecycle : std::uint8_t { dormant, active, held, retired };
inline constexpr std::uint32_t kActiveRevisionOffset = 1;
inline constexpr std::uint32_t kRetiredRevisionOffset = 2;
inline constexpr std::uint32_t kMaximumGeneration = 0x7FFFFFFDU; // headroom for +2

[[nodiscard]] constexpr Lifecycle lifecycle(const Source& source, const Authority& authority) noexcept {
    if (source.cycle == nullptr || authority.cycle > kCycles.size()) { return Lifecycle::dormant; }
    const auto current = static_cast<std::uint8_t>(source.cycle->index + 1U);
    if (authority.cycle > current) { return Lifecycle::retired; }
    if (authority.cycle < current) { return Lifecycle::dormant; }
    if (active(source, authority)) { return Lifecycle::active; }
    // A dunked carry/sink keeps its created generation: the native sink graph
    // and consumed item finish on their own. Nothing respawns because +2F0
    // already equals the applied generation; the next cycle retires them.
    if (authority.dunked && source.object != Object::effect) { return Lifecycle::held; }
    return Lifecycle::dormant;
}
[[nodiscard]] constexpr std::uint32_t revision(std::uint32_t generation, Lifecycle state) noexcept {
    return generation + (state == Lifecycle::retired ? kRetiredRevisionOffset
        : state == Lifecycle::dormant ? 0U : kActiveRevisionOffset);
}
/** Observers read the applied revision at instance +180 of an active carrier
 * and report the encounter's base generation. */
[[nodiscard]] constexpr std::uint32_t base_generation(std::uint32_t appliedRevision) noexcept {
    return appliedRevision > kActiveRevisionOffset ? appliedRevision - kActiveRevisionOffset : 0U;
}

/** Standard native activity-object authority. No transform or component-state
 * override is needed: the Arc item and interaction retain their authored
 * predicates, hold-to-use behavior, carry mechanics and native graph events. */
template<class Writer>
[[nodiscard]] bool write_authority(Writer& writer, const Source& source,
                                   const Authority& authority) noexcept {
    if (source.cycle == nullptr || authority.generation == 0
        || authority.generation >= kMaximumGeneration) { return false; }
    const auto state = lifecycle(source, authority); // cycle > 3 stays dormant, as before
    return omega_first_mancannon::write_authority(writer, revision(authority.generation, state),
                                                  state == Lifecycle::active);
}

} // namespace dawn::state::activity::omega_arc_charge
