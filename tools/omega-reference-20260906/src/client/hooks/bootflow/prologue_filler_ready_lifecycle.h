#pragma once

#include <cstdint>

namespace sunrise::client::hooks::bootflow {

namespace prologue_filler_lifecycle {

/** Serialized lifecycle state of the three-detour prologue-filler owner. */
enum class OwnerPhase : std::uint8_t {
    detached,
    active,
    quiescing,
};

/** Result of attempting to remove the complete protected detour batch. */
enum class RemovalResult : std::uint8_t {
    removed,
    protected_code_active,
    failed,
};

/**
 * Pure policy shared by production and the focused lifecycle tests.
 *
 * The caller serializes every method. Operations supplies close_side_effects(),
 * has_ownership(), attach_all_or_none(), publish_originals(), open_side_effects(), remove(), and
 * clear_removed(). A failed attach may report retained ownership; that state closes immediately
 * and can only proceed through the checked removal path.
 */
class OwnerLifecycle final {
public:
    template <class Operations> [[nodiscard]] bool install(Operations& operations) noexcept {
        if (phase_ == OwnerPhase::active) {
            return true;
        }
        if (phase_ == OwnerPhase::quiescing || operations.has_ownership()) {
            phase_ = OwnerPhase::quiescing;
            operations.close_side_effects();
            return false;
        }

        operations.close_side_effects();
        if (!operations.attach_all_or_none()) {
            if (operations.has_ownership()) {
                phase_ = OwnerPhase::quiescing;
            }
            return false;
        }

        operations.publish_originals();
        operations.open_side_effects();
        phase_ = OwnerPhase::active;
        return true;
    }

    template <class Operations> void quiesce(Operations& operations) noexcept {
        operations.close_side_effects();
        phase_ = OwnerPhase::quiescing;
    }

    template <class Operations> [[nodiscard]] bool uninstall(Operations& operations) noexcept {
        quiesce(operations);
        if (!operations.has_ownership()) {
            phase_ = OwnerPhase::detached;
            return true;
        }

        if (operations.remove() != RemovalResult::removed) {
            return false;
        }

        operations.clear_removed();
        phase_ = OwnerPhase::detached;
        return true;
    }

    /** @return Current state; the caller provides synchronization. */
    [[nodiscard]] OwnerPhase phase() const noexcept {
        return phase_;
    }

private:
    OwnerPhase phase_{OwnerPhase::detached};
};

} // namespace prologue_filler_lifecycle

/** Stops new Sunrise-owned prologue work while preserving native forwarding. */
void quiesce_prologue_filler_ready() noexcept;

/**
 * Detaches the complete protected prologue batch.
 * @return True only when no prologue detour remains owned.
 */
[[nodiscard]] bool uninstall_prologue_filler_ready_checked() noexcept;

} // namespace sunrise::client::hooks::bootflow
