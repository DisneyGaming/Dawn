#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "../../../state/activity/omega_arc_charge_authority.h"
#include "../../../state/activity/omega_arc_charge_catalog.h"

namespace dawn::client::hooks::bootflow::omega_arc_charge_native {

namespace catalog = state::activity::omega_arc_charge;
inline constexpr std::uint32_t kInvalid = UINT32_MAX;

/** Registry row of a typed native handle: bits 13..22, sign-extended the way
 * original 351C90/352310 index the 0x2439C70 registry. Different object kinds
 * (world entity, player, actor) live in different rows. */
[[nodiscard]] constexpr std::uint32_t registry_row(std::uint32_t handle) noexcept {
    const std::uint32_t sign = handle & 0x80000000U ? 0x3C00U : 0U;
    return (sign | 0x3FFU) & (handle >> 13) & 0xFFFFU;
}

struct Binding final {
    std::uint64_t run{};
    /** `generation` is the encounter's base generation, i.e. the applied
     * +180 revision minus the deferred activation offset. */
    std::uint32_t source{kInvalid}, generation{}, entity{kInvalid};
    std::uint8_t cycle{};
    catalog::Object object{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return run != 0 && source != kInvalid && generation != 0
            && generation < catalog::kMaximumGeneration && entity != kInvalid
            && cycle < catalog::kCycles.size()
            && (object == catalog::Object::carry || object == catalog::Object::sink);
    }
    friend constexpr bool operator==(const Binding&, const Binding&) noexcept = default;
};

/** `player` is the world entity the carried item is parented to (original
 * 597B10: item entity record +3C), not a player-table or actor handle. */
struct Held final {
    Binding binding{};
    std::uint32_t controller{kInvalid}, player{kInvalid};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return binding.valid() && binding.object == catalog::Object::carry
            && controller != kInvalid && player != kInvalid;
    }
    friend constexpr bool operator==(const Held&, const Held&) noexcept = default;
};

/** Native 80804FB8 authority command consumed by original F33930. It enables
 * the interaction while retaining its existing scoped predicate. The native
 * consumer alone owns the blocked/dirty bytes; this never requests a dunk. */
struct alignas(16) SinkEnableCommand final {
    std::array<std::byte, 0x70> bytes{};
    explicit SinkEnableCommand(std::span<const std::byte, 8> predicate) noexcept {
        const std::uint32_t count = 1, schema = 0x80804FB8U;
        std::memcpy(bytes.data(), &count, 4);
        std::memcpy(bytes.data() + 0x10, &schema, 4);
        bytes[0x20] = std::byte{2};
        std::memcpy(bytes.data() + 0x24, predicate.data(), predicate.size());
    }
};

/** Only a current host-owned, created and still unused sink may be enabled.
 * A source's active deferred revision is G+1, never the dormant G or retired
 * G+2. Full entity/controller identities are verified around the native call. */
[[nodiscard]] constexpr bool sink_enable_allowed(const Binding& binding,
    std::uint64_t run, std::uint32_t generation, std::uint8_t cycle,
    bool hostEnabled, bool sourceCurrent, std::uint32_t controller,
    std::uint32_t entity, bool blocked, bool used, std::int32_t requested,
    std::int32_t consumed) noexcept {
    return binding.valid() && binding.object == catalog::Object::sink
        && binding.run == run && binding.generation == generation
        && binding.cycle + 1U == cycle && hostEnabled && sourceCurrent
        && controller != kInvalid && entity == binding.entity && blocked
        && !used && requested == 0 && consumed == 0;
}

[[nodiscard]] constexpr bool created_revision(std::uint32_t generation,
    bool deferred, std::uint32_t applied, std::uint32_t committed,
    bool active, std::uint32_t entity) noexcept {
    return generation != 0 && generation < catalog::kMaximumGeneration
        && active && entity != kInvalid && committed == applied
        && applied == generation + (deferred ? 1U : 0U);
}

/** Qualification is deliberately independent of proximity and presentation.
 * The caller supplies bounded copies taken around an original native use.
 *
 * `requester` is the raw typed handle F33A90 stored at sink +2E0 (resolved by
 * original 352310). Its kind is assigned by the reflective method dispatcher
 * and is not statically known, so the caller resolves it to a world entity
 * when it can (`requesterEntity`): directly when it already is one, or through
 * the player table (+54 controlled entity, the arithmetic original 4B2260 and
 * F2F9F0 use) when its registry row is the local player's row. A resolved
 * entity must be the holder. An unresolvable kind is still accepted: the sink's
 * authored predicate 80804D75 already requires the requester to hold the
 * charge property 9C99BE55, so the native use itself proves requester==holder. */
struct Dunk final {
    Held held{};
    Binding sink{};
    std::uint32_t controller{kInvalid}, requester{kInvalid}, requesterEntity{kInvalid};
    std::int32_t requested{}, consumedBefore{}, consumedAfter{};
    bool activeAfter{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return held.valid() && sink.valid() && sink.object == catalog::Object::sink
            && sink.run == held.binding.run && sink.cycle == held.binding.cycle
            && sink.generation == held.binding.generation
            && requester != kInvalid && controller != kInvalid
            && (requesterEntity == kInvalid || requesterEntity == held.player)
            && requested > consumedBefore && consumedBefore >= 0
            && consumedAfter == requested && activeAfter;
    }
};

/** At most one pickup and sink per authored cycle. Full source generations and
 * entity handles prevent recycled slots or callbacks from an older run joining. */
class Bindings final {
public:
    constexpr void reset(std::uint64_t run) noexcept { run_ = run; slots_ = {}; held_ = {}; }
    [[nodiscard]] constexpr bool observe(const Binding& binding) noexcept {
        if (!binding.valid() || binding.run != run_) { return false; }
        const auto index = static_cast<std::size_t>(binding.cycle) * 2U
            + (binding.object == catalog::Object::sink ? 1U : 0U);
        const bool changed = slots_[index] != binding;
        if (held_.valid() && held_.binding == slots_[index] && changed) { held_ = {}; }
        slots_[index] = binding;
        return changed;
    }
    [[nodiscard]] constexpr Binding find(std::uint32_t entity, catalog::Object object) const noexcept {
        Binding result{};
        for (const auto& slot : slots_) {
            if (!slot.valid() || slot.entity != entity || slot.object != object) { continue; }
            if (result.valid()) { return {}; } // Ambiguous publication fails closed.
            result = slot;
        }
        return result;
    }
    /** @return True when the entity is bound to more than one source (fails closed). */
    [[nodiscard]] constexpr bool ambiguous(std::uint32_t entity, catalog::Object object) const noexcept {
        unsigned matches{};
        for (const auto& slot : slots_) {
            if (slot.valid() && slot.entity == entity && slot.object == object) { ++matches; }
        }
        return matches > 1;
    }
    [[nodiscard]] constexpr bool carry(const Held& receipt) noexcept {
        if (!receipt.valid() || receipt.binding != find(receipt.binding.entity, catalog::Object::carry)) {
            return false;
        }
        if (held_ == receipt) { return false; }
        held_ = receipt;
        return true;
    }
    [[nodiscard]] constexpr Held held() const noexcept { return held_; }
    [[nodiscard]] constexpr Held release(std::uint32_t item, std::uint32_t controller) noexcept {
        if (!held_.valid() || held_.binding.entity != item || held_.controller != controller) { return {}; }
        const auto old = held_; held_ = {}; return old;
    }
    [[nodiscard]] constexpr bool current(const Dunk& receipt) const noexcept {
        // The native dunk may consume/drop the item inside its original call.
        // Validate the captured pre-call held identity against persistent source
        // bindings, not against the intentionally cleared held slot.
        return receipt.valid() && receipt.held.binding.run == run_
            && receipt.held.binding == find(receipt.held.binding.entity, catalog::Object::carry)
            && receipt.sink == find(receipt.sink.entity, catalog::Object::sink);
    }
private:
    std::uint64_t run_{};
    std::array<Binding, 6> slots_{};
    Held held_{};
};

} // namespace dawn::client::hooks::bootflow::omega_arc_charge_native
