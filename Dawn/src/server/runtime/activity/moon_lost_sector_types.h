#pragma once
#include "population_service.h"
#include "../../../state/activity/coo/object_service.h"
namespace dawn::server::runtime::activity::moon_lost_sector {
struct ShieldRequest final {
 const registry::Definition* registry{};std::uint16_t effectSlot{},filterSlot{},protectedSource{};bool enabled{};
};
struct ShieldBatch final {std::array<ShieldRequest,8> entries{};std::uint8_t count{};};
struct ObjectReceipt final {
    population::Owner activity{};std::uint64_t boot{};
    state::activity::coo::Generation owner{};state::activity::coo::Asset source{};
    std::uintptr_t sourceAddress{};std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},health{UINT32_MAX};
    [[nodiscard]] bool valid() const noexcept {return activity && boot && owner.valid() && source.registry
        && source.type==4 && sourceAddress>=0x10000 && entity!=UINT32_MAX && serial!=UINT32_MAX && health!=UINT32_MAX;}
    friend bool operator==(const ObjectReceipt&,const ObjectReceipt&)=default;
};
struct DestructibleRequest final {
    population::Owner activity{};std::uint64_t boot{};state::activity::coo::Generation owner{};
    state::activity::coo::Asset source{};ObjectReceipt binding{};
    bool enabled{},vulnerable{},destroyed{};
    [[nodiscard]] bool valid() const noexcept {return activity && boot && owner.valid() && source.registry && source.type==4 && enabled;}
};
}
