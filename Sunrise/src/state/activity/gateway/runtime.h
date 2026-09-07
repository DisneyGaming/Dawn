#pragma once
#include "frame.h"
#include "ending_receipts.h"
#include "traversal_catalog.h"
namespace sunrise::state::activity::gateway {
[[nodiscard]] EndingRequest ending_request() noexcept;
void observe_module(const ModuleReceipt&,bool dead) noexcept;
void observe_scene(const SceneReceipt&,bool completed) noexcept;
void observe_vance(const SceneReceipt&,VanceMilestone) noexcept;
[[nodiscard]] bool prepare(std::uint64_t run,bool selected) noexcept;
[[nodiscard]] Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept;
[[nodiscard]] std::uint64_t native_run() noexcept;
[[nodiscard]] bool publication_due(std::uint64_t now) noexcept;
[[nodiscard]] bool observe_admission(const EnemyReceipt& receipt) noexcept;
[[nodiscard]] bool observe_death(const EnemyReceipt& receipt) noexcept;
void observe_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index) noexcept;
void observe_position(float x,float y,float z) noexcept;
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept;
}
