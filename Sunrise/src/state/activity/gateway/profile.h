#pragma once
#include "bindings.h"
namespace sunrise::state::activity::gateway {
inline constexpr auto& kProfile=kNativeBindings;
[[nodiscard]] bool valid_document(const coo::script::Views&) noexcept;
}
