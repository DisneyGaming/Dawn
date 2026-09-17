#pragma once
#include <cstdint>
namespace dawn::client::hooks::bootflow::hijacked_placements {
bool install() noexcept;
void quiesce() noexcept;
bool uninstall() noexcept;
void poll() noexcept;
void retain_enemy(std::uint64_t run,std::uint16_t source,std::uint32_t owner,std::uint32_t actor) noexcept;
}
