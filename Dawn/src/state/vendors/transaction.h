#pragma once
#include "../account/account_state.h"
#include <memory>
#include <vector>

namespace dawn::state::vendors {
struct Request {
    std::uint16_t vendor{};
    std::int32_t sale{-1},interaction{-1};
    std::int16_t reply{};
};
struct TransactionData {
    AccountState before{},after{};
    std::size_t character{};
    std::vector<std::uint64_t> changed,removed;
};
// Only preparation creates this immutable plan. No inventory is changed until
// all native instance, character and account publications have been encoded.
struct Pending {std::shared_ptr<const TransactionData> data;};
bool prepare(const Request&,Pending&) noexcept;
bool prepare_progress(Pending&) noexcept;
bool prepare_decryption(std::uint64_t instance,std::uint16_t item,Pending&,bool postmaster=false) noexcept;
bool prepare_recovery(std::uint64_t instance,std::uint16_t item,std::int32_t quantity,Pending&) noexcept;
bool prepare_postmaster_discard(std::uint64_t instance,std::uint16_t item,Pending&) noexcept;
bool preview(const Pending&,AccountState&) noexcept;
bool commit(Pending&) noexcept;
}
