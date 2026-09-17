#include "native_activation_registry.h"

#include <limits>

namespace dawn::client::hooks::activity_lifecycle {

NativeActivationRegistry::NativeActivationRegistry(NativeActivationClock clock) noexcept
    : clock_(clock) {
}

BeginModuleResult NativeActivationRegistry::begin_module() noexcept {
    const std::lock_guard lock(mutex_);
    if (moduleActive_) {
        return BeginModuleResult{BeginModuleStatus::alreadyActive, currentModule_};
    }
    if (!state::activity::advance(clock_.module, clock_.moduleExhausted)) {
        mutationEnabled_ = false;
        return BeginModuleResult{BeginModuleStatus::generationExhausted, {}};
    }

    slots_ = {};
    globalDropMarks_ = {};
    currentModule_ = clock_.module;
    moduleActive_ = true;
    mutationEnabled_ = true;
    return BeginModuleResult{BeginModuleStatus::started, currentModule_};
}

bool NativeActivationRegistry::quiesce_module(
    state::activity::ModuleGeneration expectedModule) noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_ || currentModule_ != expectedModule) {
        return false;
    }
    disable_mutation_locked();
    return true;
}

bool NativeActivationRegistry::finish_module(
    state::activity::ModuleGeneration expectedModule) noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_ || currentModule_ != expectedModule) {
        return false;
    }

    disable_mutation_locked();
    slots_ = {};
    globalDropMarks_ = {};
    currentModule_ = {};
    moduleActive_ = false;
    return true;
}

PublishResult NativeActivationRegistry::publish(const NativeActivationAttempt& attempt) noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_) {
        return PublishResult{PublishStatus::moduleInactive, {}};
    }
    if (!mutationEnabled_) {
        return PublishResult{PublishStatus::mutationDisabled, {}};
    }
    if (attempt.wrapper == 0U || attempt.fullHandle == kInvalidNativeActivityHandle) {
        disable_mutation_locked();
        return PublishResult{PublishStatus::invalidAttempt, {}};
    }

    NativeActivationSnapshot& slot = slots_[slot_index(attempt.fullHandle)];
    if (slot.state == NativeActivationState::active) {
        const PublishStatus status =
            slot.fullHandle == attempt.fullHandle && slot.wrapper == attempt.wrapper
                ? PublishStatus::currentConflict
                : PublishStatus::slotCollision;
        disable_mutation_locked();
        return PublishResult{status, {}};
    }
    if (slot.state == NativeActivationState::quiescing
        && (slot.fullHandle != attempt.fullHandle || slot.wrapper != attempt.wrapper
            || slot.key.module != currentModule_)) {
        disable_mutation_locked();
        return PublishResult{PublishStatus::slotCollision, {}};
    }
    if (!state::activity::advance(clock_.activation, clock_.activationExhausted)) {
        disable_mutation_locked();
        return PublishResult{PublishStatus::generationExhausted, {}};
    }

    slot = NativeActivationSnapshot{
        state::activity::NativeActivationKey{currentModule_, clock_.activation},
        attempt.wrapper,
        attempt.identity,
        attempt.fullHandle,
        attempt.mode,
        NativeActivationState::active,
        attempt.identityValid,
    };
    globalDropMarks_[slot_index(attempt.fullHandle)] = 0U;
    return PublishResult{PublishStatus::published, slot};
}

CaptureResult NativeActivationRegistry::capture_predecessor(std::uintptr_t wrapper,
                                                             std::uint32_t fullHandle,
                                                             bool nativeActive) noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_) {
        return CaptureResult{CaptureStatus::moduleInactive, {}, {}};
    }
    if (!mutationEnabled_) {
        return CaptureResult{CaptureStatus::mutationDisabled, {}, {}};
    }
    if (!nativeActive) {
        return CaptureResult{CaptureStatus::invalidNativeState, {}, {}};
    }
    if (wrapper == 0U || fullHandle == kInvalidNativeActivityHandle) {
        disable_mutation_locked();
        return CaptureResult{CaptureStatus::ownershipConflict, {}, {}};
    }

    NativeActivationSnapshot& slot = slots_[slot_index(fullHandle)];
    if (slot.state != NativeActivationState::active || slot.wrapper != wrapper
        || slot.fullHandle != fullHandle || slot.key.module != currentModule_) {
        // A readable native-active close must name the exact current publication. Any mismatch
        // proves that this mirror has lost pool ownership. Fail closed, but return no token: a
        // stale/mismatched close must never acquire authority to retire a legitimate successor.
        disable_mutation_locked();
        return CaptureResult{CaptureStatus::ownershipConflict, slot, {}};
    }

    slot.state = NativeActivationState::quiescing;
    globalDropMarks_[slot_index(fullHandle)] = 0U;
    const NativeActivationToken token{slot.key, slot.wrapper, slot.fullHandle};
    return CaptureResult{CaptureStatus::captured, slot, token};
}

RetireStatus NativeActivationRegistry::retire(NativeActivationToken token) noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_) {
        return RetireStatus::moduleInactive;
    }
    if (!token) {
        return RetireStatus::invalidToken;
    }

    NativeActivationSnapshot& slot = slots_[slot_index(token.fullHandle)];
    if (!exact(slot, token)) {
        return RetireStatus::staleToken;
    }
    if (slot.state != NativeActivationState::quiescing) {
        return RetireStatus::notQuiescing;
    }

    slot.state = NativeActivationState::retired;
    globalDropMarks_[slot_index(token.fullHandle)] = 0U;
    return RetireStatus::retired;
}

GlobalDropBeginResult NativeActivationRegistry::begin_global_drop() noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_) {
        return GlobalDropBeginResult{GlobalDropBeginStatus::moduleInactive, {}};
    }
    if (!mutationEnabled_) {
        return GlobalDropBeginResult{GlobalDropBeginStatus::mutationDisabled, {}};
    }
    if (globalDropEpochExhausted_
        || globalDropEpoch_ == (std::numeric_limits<std::uint64_t>::max)()) {
        globalDropEpochExhausted_ = true;
        disable_mutation_locked();
        return GlobalDropBeginResult{GlobalDropBeginStatus::generationExhausted, {}};
    }

    ++globalDropEpoch_;
    std::size_t captured = 0U;
    for (std::size_t slotIndex = 0U; slotIndex < slots_.size(); ++slotIndex) {
        NativeActivationSnapshot& slot = slots_[slotIndex];
        if (slot.state != NativeActivationState::active || slot.key.module != currentModule_) {
            continue;
        }
        slot.state = NativeActivationState::quiescing;
        globalDropMarks_[slotIndex] = globalDropEpoch_;
        ++captured;
    }
    return GlobalDropBeginResult{
        GlobalDropBeginStatus::captured,
        NativeActivationGlobalDropToken{currentModule_, globalDropEpoch_, captured},
    };
}

GlobalDropFinishResult NativeActivationRegistry::finish_global_drop(
    NativeActivationGlobalDropToken token) noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_ || !token || token.module != currentModule_) {
        return {};
    }

    std::size_t retired = 0U;
    for (std::size_t slotIndex = 0U; slotIndex < slots_.size(); ++slotIndex) {
        if (globalDropMarks_[slotIndex] != token.epoch) {
            continue;
        }
        NativeActivationSnapshot& slot = slots_[slotIndex];
        if (slot.state == NativeActivationState::quiescing
            && slot.key.module == token.module) {
            slot.state = NativeActivationState::retired;
            ++retired;
        }
        globalDropMarks_[slotIndex] = 0U;
    }
    return GlobalDropFinishResult{retired, token.captured - retired, true};
}

NativeActivePoolJoinResult NativeActivationRegistry::join_native_active_pool(
    const NativeActivePoolJoinInput* inputs,
    std::size_t inputCount,
    NativeActivePoolJoinOutput* outputs,
    std::size_t outputCapacity) noexcept {
    const std::lock_guard lock(mutex_);
    if (outputs != nullptr && outputCapacity != 0U) {
        for (std::size_t index = 0U; index < outputCapacity; ++index) {
            outputs[index] = {};
        }
    }
    if (inputCount > kNativeActivePoolPinnedCapacity
        || (inputCount != 0U && (inputs == nullptr || outputs == nullptr))
        || outputCapacity < inputCount) {
        if (moduleActive_) {
            disable_mutation_locked();
        }
        return NativeActivePoolJoinResult{NativeActivePoolJoinStatus::invalidArguments};
    }
    if (!moduleActive_) {
        return NativeActivePoolJoinResult{NativeActivePoolJoinStatus::moduleInactive};
    }
    if (!mutationEnabled_) {
        return NativeActivePoolJoinResult{
            NativeActivePoolJoinStatus::mutationDisabled, currentModule_};
    }

    NativeActivePoolJoinResult result{
        NativeActivePoolJoinStatus::joined, currentModule_};
    std::array<bool, kNativeActivePoolPinnedCapacity> structurallyValid{};
    for (std::size_t index = 0U; index < inputCount; ++index) {
        const NativeActivePoolJoinInput& input = inputs[index];
        bool uniqueSlot = true;
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if ((inputs[prior].fullHandle & kNativeActivationSlotMask)
                == (input.fullHandle & kNativeActivationSlotMask)) {
                uniqueSlot = false;
                break;
            }
        }
        if (input.wrapper == 0U || input.fullHandle == kInvalidNativeActivityHandle
            || !input.identityValid || !uniqueSlot) {
            outputs[index].status = NativeActivePoolJoinMemberStatus::conflict;
            ++result.conflicts;
            continue;
        }
        structurallyValid[index] = true;

        NativeActivationSnapshot& slot = slots_[slot_index(input.fullHandle)];
        if (slot.state == NativeActivationState::empty
            || slot.state == NativeActivationState::retired) {
            outputs[index].status = NativeActivePoolJoinMemberStatus::poolOnly;
            ++result.poolOnly;
            continue;
        }
        if (slot.state != NativeActivationState::active || slot.key.module != currentModule_
            || slot.wrapper != input.wrapper || slot.fullHandle != input.fullHandle) {
            outputs[index].status = NativeActivePoolJoinMemberStatus::conflict;
            outputs[index].snapshot = slot;
            ++result.conflicts;
            continue;
        }

        outputs[index].identityMatch = slot.identityValid && slot.identity == input.identity;
        outputs[index].modeMatch = slot.mode == input.mode;
        outputs[index].snapshot = slot;
        if (!outputs[index].identityMatch || !outputs[index].modeMatch) {
            outputs[index].status = NativeActivePoolJoinMemberStatus::conflict;
            ++result.conflicts;
            continue;
        }

        outputs[index].status = NativeActivePoolJoinMemberStatus::exact;
        outputs[index].token =
            NativeActivationToken{slot.key, slot.wrapper, slot.fullHandle};
        ++result.exact;
    }

    for (const NativeActivationSnapshot& slot : slots_) {
        if (slot.state != NativeActivationState::active || slot.key.module != currentModule_) {
            continue;
        }
        bool represented = false;
        for (std::size_t index = 0U; index < inputCount; ++index) {
            if (structurallyValid[index] && inputs[index].wrapper == slot.wrapper
                && inputs[index].fullHandle == slot.fullHandle) {
                represented = true;
                break;
            }
        }
        if (!represented) {
            ++result.registryOnly;
        }
    }

    for (std::size_t index = 0U; index < inputCount; ++index) {
        NativeActivePoolJoinOutput& output = outputs[index];
        if (output.status != NativeActivePoolJoinMemberStatus::exact || !output.token) {
            continue;
        }
        NativeActivationSnapshot& slot = slots_[slot_index(output.token.fullHandle)];
        slot.state = NativeActivationState::quiescing;
        globalDropMarks_[slot_index(output.token.fullHandle)] = 0U;
    }

    if (result.poolOnly != 0U || result.registryOnly != 0U || result.conflicts != 0U) {
        result.status = NativeActivePoolJoinStatus::diverged;
        disable_mutation_locked();
    }
    return result;
}

NativeActivationSnapshot NativeActivationRegistry::snapshot(std::uintptr_t wrapper,
                                                             std::uint32_t fullHandle) const noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_ || wrapper == 0U || fullHandle == kInvalidNativeActivityHandle) {
        return {};
    }

    const NativeActivationSnapshot& slot = slots_[slot_index(fullHandle)];
    if ((slot.state != NativeActivationState::active
         && slot.state != NativeActivationState::quiescing)
        || slot.wrapper != wrapper || slot.fullHandle != fullHandle
        || slot.key.module != currentModule_) {
        return {};
    }
    return slot;
}

bool NativeActivationRegistry::is_current(NativeActivationToken token) const noexcept {
    const std::lock_guard lock(mutex_);
    if (!moduleActive_ || !token) {
        return false;
    }
    const NativeActivationSnapshot& slot = slots_[slot_index(token.fullHandle)];
    return slot.state == NativeActivationState::active && exact(slot, token)
           && slot.key.module == currentModule_;
}

state::activity::ModuleGeneration NativeActivationRegistry::current_module() const noexcept {
    const std::lock_guard lock(mutex_);
    return currentModule_;
}

NativeActivationClock NativeActivationRegistry::clock() const noexcept {
    const std::lock_guard lock(mutex_);
    return clock_;
}

bool NativeActivationRegistry::mutation_enabled() const noexcept {
    const std::lock_guard lock(mutex_);
    return moduleActive_ && mutationEnabled_;
}

bool NativeActivationRegistry::exact(const NativeActivationSnapshot& slot,
                                     NativeActivationToken token) noexcept {
    return slot.key == token.key && slot.wrapper == token.wrapper
           && slot.fullHandle == token.fullHandle;
}

void NativeActivationRegistry::disable_mutation_locked() noexcept {
    mutationEnabled_ = false;
    for (NativeActivationSnapshot& slot : slots_) {
        if (slot.state == NativeActivationState::active) {
            slot.state = NativeActivationState::quiescing;
        }
    }
}

namespace detail {

NativeActivationRegistry& native_activation_registry() noexcept {
    static NativeActivationRegistry registry{};
    return registry;
}

} // namespace detail

} // namespace dawn::client::hooks::activity_lifecycle
