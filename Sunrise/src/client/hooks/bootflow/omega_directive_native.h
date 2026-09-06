#pragma once
#include <cstddef>

namespace sunrise::client::hooks::bootflow::omega_directive_native {
// The existing presentation observer verifies and owns the +1009ED0 detour.
// Other Omega controllers must use its trampoline through the same lifetime gate.
[[nodiscard]] bool available() noexcept;
[[nodiscard]] bool install_checkpoint(std::byte* component, std::byte* record, void* content) noexcept;
}
