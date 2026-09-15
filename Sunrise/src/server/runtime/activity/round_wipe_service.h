#pragma once
#include <cstdint>
namespace sunrise::server::runtime::activity::round_wipe {
// Same death-confirmed three-second recovery interval as 1AU-UnEx. Missing samples never
// mean death. A living receipt cancels the pending wipe; recycled actors cannot start it.
struct Service final {
    std::uint32_t living{UINT32_MAX};std::uint64_t deadline{};
    bool observe(std::uint32_t entity,bool alive,bool restricted,std::uint64_t now) noexcept {
        if(entity==UINT32_MAX)return false;
        if(alive) {living=entity;deadline=0;return false;}
        if(!restricted || entity!=living) {deadline=0;return false;}
        if(!deadline)deadline=now+3000;
        return now>=deadline;
    }
};
}
