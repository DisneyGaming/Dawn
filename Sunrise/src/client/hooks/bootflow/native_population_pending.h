#pragma once
#include "../../../state/activity/native_population_events.h"

namespace sunrise::client::hooks::bootflow::native_population_pending {
namespace events=state::activity::native_population;
// A0D510 creates the salted actor and source backlink before an entity is
// attached. Only that successful native call may enqueue a birth here.
struct Birth final {
    events::Event event{};
    std::uint32_t parent{UINT32_MAX};
};
enum class Intake { accepted, duplicate, invalid, conflict, overflow };
template<std::size_t Capacity> class Queue final {
public:
    Intake add(const Birth& birth) noexcept {
        if(birth.event.kind!=events::Kind::admitted || !birth.event.lease.activity
            || !birth.event.lease.source.valid() || birth.event.actor.owner!=birth.event.lease.source
            || birth.event.actor.actor==UINT32_MAX || birth.event.sourceHandle==UINT32_MAX
            || birth.parent==UINT32_MAX) return Intake::invalid;
        for(std::size_t i=0;i<used_;++i) {
            const auto& previous=items_[i];
            if(previous.event.lease==birth.event.lease && previous.event.actor.actor==birth.event.actor.actor) {
                return previous.parent==birth.parent && previous.event.sourceHandle==birth.event.sourceHandle
                    ?Intake::duplicate:Intake::conflict;
            }
        }
        if(used_==Capacity) return Intake::overflow;
        items_[used_++]=birth;return Intake::accepted;
    }
    std::size_t size() const noexcept {return used_;}
    const Birth& operator[](std::size_t index) const noexcept {return items_[index];}
    void erase(std::size_t index) noexcept {
        for(std::size_t i=index+1;i<used_;++i) items_[i-1]=items_[i];
        --used_;
    }
private:
    std::array<Birth,Capacity> items_{};std::size_t used_{};
};
// Typed source bodies have package-specific offsets (pond 728, Vance 878).
// The class marker, exact registered resource and scoped identity still qualify
// the reference; no offset from an activity JSON is accepted here.
constexpr bool definition(std::uint32_t kind,std::int64_t offset,std::uint32_t marker) noexcept {
    return kind==0x8080948FU && marker==kind && offset>=4 && offset<=0x100000 && offset%4==0;
}
}
