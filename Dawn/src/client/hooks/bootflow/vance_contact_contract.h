#pragma once
#include "../../../state/activity/native_population_events.h"

namespace dawn::client::hooks::bootflow::vance_contact {
namespace events=state::activity::native_population;
struct Identity final {
    events::Event event{};
    std::uint32_t parent{UINT32_MAX};
    [[nodiscard]] bool valid() const noexcept {
        const auto& lease=event.lease;
        const auto& source=lease.source;
        return event.kind==events::Kind::admitted && lease.activity && source.valid()
            && event.actor.valid() && event.actor.owner==source
            && source.activity==lease.activity.sessionId
            && source.incarnation==lease.activity.incarnation.value && lease.bubble==15
            && source.source.registry==0x564C6ECE && source.source.definition==0x80F5B9CC
            && source.source.type==1 && source.source.slot==0
            && event.sourceHandle!=UINT32_MAX && parent!=UINT32_MAX;
    }
    friend bool operator==(const Identity& a,const Identity& b) noexcept {
        return a.event.lease==b.event.lease && a.event.actor==b.event.actor
            && a.event.sourceHandle==b.event.sourceHandle && a.event.kind==b.event.kind
            && a.parent==b.parent;
    }
};
// No pointer identity or continuously changing job address contributes to the
// log key. Identical frames emit once; each identity has a fixed total budget.
struct Signature final {
    std::int32_t rawHits{-1};
    std::uint32_t filter{},contact{},state{};
    std::uint8_t prepared{},request{},provider{},mode{};
    friend bool operator==(const Signature&,const Signature&)=default;
};
struct Budget final {
    std::array<Signature,2> last{};
    std::array<bool,2> seen{};
    unsigned lines{};
    std::array<unsigned,2> samples{};
    std::array<std::uint64_t,2> next{};
    [[nodiscard]] bool sample(unsigned boundary,std::uint64_t now) noexcept {
        if(boundary>=samples.size() || samples[boundary]>=40 || now<next[boundary]) return false;
        ++samples[boundary];next[boundary]=now>UINT64_MAX-250?UINT64_MAX:now+250;return true;
    }
    [[nodiscard]] bool admit(unsigned boundary,const Signature& value) noexcept {
        if(boundary>=last.size() || lines>=24 || (seen[boundary] && last[boundary]==value)) return false;
        last[boundary]=value;seen[boundary]=true;++lines;return true;
    }
};
[[nodiscard]] constexpr bool result_layout(std::uintptr_t row,std::uintptr_t data,
                                          std::int32_t capacity,std::int32_t count) noexcept {
    return row>=0x10000 && row<=UINTPTR_MAX-0x610 && data==row+0x1F0
        && capacity==16 && count>=0 && count<=16;
}
}
