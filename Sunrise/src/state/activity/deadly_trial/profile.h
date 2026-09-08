#pragma once
#include "bindings.h"
namespace sunrise::state::activity::deadly_trial {
inline constexpr coo::script::Profile kProfile{"deadly_trial.native.v1","otherMissions",coo::Schema::otherMissions,kCapabilities,kModules,kFacts,kDialogue,kObjectives,{},{},{}};
bool valid_document(const coo::script::Views&) noexcept;
}
