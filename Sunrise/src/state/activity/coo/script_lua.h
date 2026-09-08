#pragma once
#include "mission_script.h"
#include "script_value.h"

namespace sunrise::state::activity::coo::script::lua {
// Evaluate a definition once. No Lua state or executable callback survives this
// function; the returned tree still requires MissionDocument validation.
[[nodiscard]] value::Value evaluate(std::string_view source,const Profile& profile,std::string_view sourceName);
}
