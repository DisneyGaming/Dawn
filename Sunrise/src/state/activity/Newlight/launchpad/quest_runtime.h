#pragma once
#include "quest.h"
#include <atomic>

namespace sunrise::state::activity::newlight::launchpad::quest {
inline std::atomic_uint64_t towerRun{};
inline bool selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,bool /*unfinished*/=false) noexcept {
    const bool active=run && scenario==0x80B4A0F4U
        && index==20;
    towerRun.store(active?run:0);return active;
}
}

namespace sunrise::state {
// The Pursuit, item rewards and currencies advance as one optimistic transaction.
struct PendingNewlightQuest {
    CharacterState beforeCharacter{},afterCharacter{};
    std::array<account::inventory::ProfileItem,account::inventory::kProfileItemCapacity> beforeProfile{},afterProfile{};
    std::uint64_t accountSoid{},characterSoid{},questInstanceSoid{};
    std::array<std::uint64_t,2> rewardInstances{};
    std::array<std::uint64_t,2> removedInstances{};
    std::size_t characterIndex{},rewardCount{},beforeProfileCount{},afterProfileCount{};
    std::size_t removedCount{};
    std::uint8_t expectedStep{};
    bool prepared{},profileChanged{},equipmentChanged{};
};
bool prepare_newlight_quest(std::uint8_t expectedStep,PendingNewlightQuest&) noexcept;
bool preview_newlight_quest(const PendingNewlightQuest&,AccountState&) noexcept;
bool commit_newlight_quest(PendingNewlightQuest&) noexcept;
bool restore_newlight_quests(void* module,AccountState&) noexcept;
bool prepare_newlight_start(AccountState&) noexcept;
}
