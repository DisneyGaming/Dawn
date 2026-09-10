#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace sunrise::client::hooks::bootflow::replication_bindings {
struct Binding {std::uintptr_t manager{},peer{};std::uint32_t index{};};
class Excluded final {
public:
    [[nodiscard]] bool remember(Binding binding,std::uintptr_t nativeSlot,std::uint32_t nativeMask) noexcept {
        if(!binding.manager || !binding.peer || binding.index>=31 || nativeSlot || (nativeMask&(1U<<binding.index)))return false;
        for(std::size_t i=0;i<used_;++i)
            if(rows_[i].manager==binding.manager && rows_[i].index==binding.index)return rows_[i].peer==binding.peer;
        if(used_==rows_.size())return false;
        rows_[used_++]=binding;return true;
    }
    // Only an earlier exclusion can suppress removal. An occupied/rebound native
    // slot is forwarded normally, even if stale local bookkeeping exists.
    [[nodiscard]] bool remove(std::uintptr_t manager,std::uint32_t index,std::uintptr_t nativeSlot,std::uint32_t nativeMask) noexcept {
        for(std::size_t i=0;i<used_;++i)if(rows_[i].manager==manager && rows_[i].index==index) {
            rows_[i]=rows_[--used_];return !nativeSlot && index<31 && !(nativeMask&(1U<<index));
        }
        return false;
    }
    [[nodiscard]] std::size_t size() const noexcept {return used_;}
private:std::array<Binding,128> rows_{};std::size_t used_{};
};
}
