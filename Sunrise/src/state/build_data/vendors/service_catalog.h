#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sunrise::state::build_data::vendors::services {
// Keep the authored blobs immutable. Nested price/condition arrays contain relative
// pointers, so copying isolated sale records would lose their referents.
struct Blob {std::uint16_t index{};std::uint32_t hash{};std::vector<std::byte> bytes;};
struct Cost {std::uint16_t item{};std::int32_t quantity{};};
struct Instruction {std::uint32_t opcode{},operand{};};
using Expression=std::vector<Instruction>;
struct Offer {
    std::uint16_t vendor{},sale{},item{};
    std::int32_t category{},quantity{};
    std::vector<Cost> costs;
    std::vector<Expression> conditions;
};
struct Reply {std::uint16_t selection{},site{0xffff};Expression condition;};
struct Interaction {std::uint16_t item{0xffff},category{0xffff};Expression condition;std::vector<Reply> replies;std::uint32_t presentation{};};
struct Loot {std::uint16_t item{},vendor{};};
struct QuestStep {std::uint16_t item{},slot{};std::int32_t value{};std::uint16_t ordinal{};};
struct ItemFlag {std::uint16_t item{},slot{};};
struct Objective {std::int32_t threshold{};Expression expression;std::vector<std::uint16_t> flags;};
struct Pursuit {std::uint16_t item{},site{0xffff};bool requireAll{},globalQuest{};std::vector<std::uint16_t> objectives;};
bool ready() noexcept;
bool publish(std::vector<Blob>&&,std::vector<Loot> loot={},std::vector<QuestStep> quests={},std::vector<ItemFlag> flags={},
    std::vector<Pursuit> pursuits={},std::vector<std::byte> objectives={}) noexcept;
bool quest_step(std::uint16_t item,QuestStep&) noexcept;
bool next_step(const QuestStep&,QuestStep&) noexcept;
bool pursuit(std::uint16_t item,Pursuit&) noexcept;
bool objective(std::uint16_t index,Objective&) noexcept;
bool read_pursuit(std::span<const std::byte> item,std::uint16_t index,Pursuit&) noexcept;
bool hand_in(const Pursuit&,const Interaction&) noexcept;
bool manual(const Pursuit&) noexcept;
bool item_flag(std::uint16_t item,std::uint16_t slot) noexcept;
bool offer(std::uint16_t vendor,std::uint16_t sale,Offer&) noexcept;
bool interaction(std::uint16_t vendor,std::uint16_t index,Interaction&) noexcept;
bool reward_items(std::uint16_t vendor,std::vector<std::uint16_t>&) noexcept;
bool loot_vendor(std::uint16_t item,std::uint16_t& vendor) noexcept;
struct Binding {std::uint16_t row{};std::uint8_t bank{};bool present{};};
bool publish_conditions(std::vector<std::byte> flags,std::vector<std::byte> values,std::vector<std::byte> pool) noexcept;
Binding binding(bool value,std::uint16_t slot) noexcept;
bool pooled(std::uint16_t index,Expression&) noexcept;
}
