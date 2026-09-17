#pragma once
#include <cstdint>
namespace dawn::state::activity::omega::mission {
// Per activity link: failed staging never acknowledges a revision. Coalesce
// native receipts without tying Scene and dialogue delivery to the keepalive.
struct Publication {
    std::uint64_t run{}, next{};
    std::uint32_t generation{}, revision{};
    std::uint8_t cue{255};
    bool due(std::uint64_t r,std::uint32_t g,std::uint32_t v,std::uint8_t c,std::uint64_t now) const noexcept {
        return r && g && (run!=r || (now>=next && (generation!=g || revision!=v || cue!=c)));
    }
    void delivered(std::uint64_t r,std::uint32_t g,std::uint32_t v,std::uint8_t c,std::uint64_t now) noexcept {
        run=r;generation=g;revision=v;cue=c;next=now+100;
    }
};
}
