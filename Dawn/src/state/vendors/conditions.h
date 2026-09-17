#pragma once
#include "../account/account_state.h"
#include "../investment/investment.h"
#include "../build_data/vendors/service_catalog.h"

namespace dawn::state::vendors {
bool condition(std::span<const build_data::vendors::services::Instruction>,
    const AccountState&,std::size_t character,const Family5State&,std::int32_t&) noexcept;
}
