#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace dawn::state::gameplay::replication {
using Address=std::array<unsigned char,86>;
// Only the embedded server may publish this role. It implements activity/group
// control, not a remote world's physics replica. Ordinary/external peers are unknown.
class Roles final {
public:
    void begin(std::uint64_t epoch) noexcept {epoch_=epoch;rows_={};used_=0;}
    [[nodiscard]] bool publish(std::uint64_t machine,const Address& address) noexcept {
        if(!epoch_ || !machine)return false;
        for(std::size_t i=0;i<used_;++i)if(rows_[i].machine==machine)return rows_[i].address==address;
        if(used_==rows_.size())return false;
        rows_[used_++]={machine,address};return true;
    }
    [[nodiscard]] std::uint64_t lookup(std::uint64_t machine,const Address& address) const noexcept {
        for(std::size_t i=0;i<used_;++i)if(rows_[i].machine==machine && rows_[i].address==address)return epoch_;
        return 0;
    }
private:
    struct Row {std::uint64_t machine{};Address address{};};
    std::uint64_t epoch_{};std::array<Row,64> rows_{};std::size_t used_{};
};
void begin_control_host_epoch(std::uint64_t epoch) noexcept;
[[nodiscard]] bool publish_control_host(std::uint64_t machine,const Address& address) noexcept;
[[nodiscard]] std::uint64_t control_host_epoch(std::uint64_t machine,const Address& address) noexcept;
}
