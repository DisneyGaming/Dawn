#pragma once

#include <cstdint>

namespace dawn::client::hooks::bootflow::type31_capture {

/** Lifecycle state of the sole +B20640 observation owner. */
enum class OwnerPhase : std::uint8_t {
    detached,
    active,
    quiescing,
    removed_pending_reset,
};

/** Result returned by the protected native-detour removal operation. */
enum class OwnerRemovalResult : std::uint8_t {
    removed,
    protected_code_active,
    failed,
};

/**
 * Production lifecycle policy for the Type-31 owner.
 *
 * Operations is intentionally duck-typed so the production owner and focused tests execute this
 * exact state machine without importing Detours into the pure capture test. Operations provides:
 * enabled(), admit_image(), allocate_epoch(), reset_for_new_epoch(epoch), attach(), publish(epoch),
 * close_admission(), remove(), and finalize_removed().
 */
class OwnerLifecycle final {
public:
    template <class Operations>
    [[nodiscard]] bool install(Operations& operations) noexcept {
        if (phase_ == OwnerPhase::active) {
            return true;
        }
        if (phase_ == OwnerPhase::quiescing) {
            return false;
        }
        if (phase_ == OwnerPhase::removed_pending_reset) {
            if (!operations.finalize_removed()) {
                return false;
            }
            phase_ = OwnerPhase::detached;
            epoch_ = 0U;
        }
        if (!operations.enabled() || !operations.admit_image()) {
            return false;
        }

        const std::uint64_t nextEpoch = operations.allocate_epoch();
        if (nextEpoch == 0U || !operations.reset_for_new_epoch(nextEpoch)
            || !operations.attach()) {
            return false;
        }

        operations.publish(nextEpoch);
        epoch_ = nextEpoch;
        phase_ = OwnerPhase::active;
        return true;
    }

    template <class Operations>
    void quiesce(Operations& operations) noexcept {
        if (phase_ == OwnerPhase::active) {
            operations.close_admission();
            phase_ = OwnerPhase::quiescing;
        }
    }

    template <class Operations>
    [[nodiscard]] bool uninstall(Operations& operations) noexcept {
        quiesce(operations);
        if (phase_ == OwnerPhase::detached) {
            return true;
        }
        if (phase_ == OwnerPhase::quiescing) {
            const OwnerRemovalResult result = operations.remove();
            if (result != OwnerRemovalResult::removed) {
                return false;
            }
            phase_ = OwnerPhase::removed_pending_reset;
        }
        if (!operations.finalize_removed()) {
            return false;
        }
        phase_ = OwnerPhase::detached;
        epoch_ = 0U;
        return true;
    }

    [[nodiscard]] OwnerPhase phase() const noexcept { return phase_; }
    [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }

private:
    OwnerPhase phase_{OwnerPhase::detached};
    std::uint64_t epoch_{};
};

} // namespace dawn::client::hooks::bootflow::type31_capture
