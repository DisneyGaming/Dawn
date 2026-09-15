#pragma once
#include "../account/account_state.h"
namespace sunrise::state::vendors::persistence {
// Caller holds the root State lock. Before the first vendor purchase the existing
// New Light journal remains authoritative; afterwards this inventory journal also
// owns Pursuits, so rewards and their costs survive the same atomic replacement.
bool restore(void* module,AccountState&) noexcept;
bool active() noexcept;
bool save(const AccountState&,bool activate=false) noexcept;
}
