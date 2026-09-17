#pragma once
#include <cstdint>
namespace dawn::client::hooks::bootflow {
// Called only on the admitted game-thread observer, never a server snapshot.
void poll_mission_population_readiness(std::uintptr_t image,std::uint64_t now) noexcept;
}
