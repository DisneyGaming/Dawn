#pragma once
#include "adventure_native_bridge.h"

namespace sunrise::server::runtime::activity::public_event_cue_pending {
namespace bridge=adventure::native_bridge;
namespace feedback=bridge::feedback;
// Exact native lease retained across original1009C00 calls. Packet/stack
// addresses are deliberately excluded: a later publication owns a new packet.
struct Identity final {
    bridge::Binding binding{};
    feedback::Source source{};
    std::uintptr_t component{};
    std::uint32_t authority{},componentLink{UINT32_MAX};
    std::array<std::byte,16> header{};
    std::array<std::byte,0x60> definition{};
    std::array<std::byte,0x70> authorityObject{};
    friend bool operator==(const Identity& a,const Identity& b) noexcept {
        return a.binding.epoch==b.binding.epoch && a.binding.ticket==b.binding.ticket
            && a.source==b.source && a.component==b.component && a.authority==b.authority
            && a.componentLink==b.componentLink && a.header==b.header
            && a.definition==b.definition && a.authorityObject==b.authorityObject;
    }
};
class Pending final {
public:
    [[nodiscard]] std::uint64_t index(const Identity& identity) const noexcept {
        return active_ && identity_==identity ? insertion_.managerCountBefore : UINT64_MAX;
    }
    [[nodiscard]] bool active() const noexcept {return active_;}
    void reset() noexcept {*this={};}
    // Only a witnessed NEW insertion, otherwise fully qualifying except native
    // mode0, may wait. Original1008B40 sets component+B05=1 in exactly this case.
    [[nodiscard]] bool remember(const Identity& identity,const feedback::Capture& c,
        std::uint8_t nativePending,unsigned matchesBefore,unsigned matchesAfter) noexcept {
        reset();
        const auto& ticket=identity.binding.ticket;
        if(!identity.binding.epoch || identity.component<0x10000 || identity.authority==UINT32_MAX
            || identity.componentLink==UINT32_MAX || c.ticket!=ticket || c.source!=identity.source
            || ticket.presentation!=feedback::Presentation::authoredProgress || ticket.request.clear
            || ticket.request.readiness.type!=70 || nativePending!=1 || matchesBefore || matchesAfter!=1
            || c.entry.size()!=entry_.size() || feedback::field<std::int32_t>(c.entry,0x70)!=0)return false;
        feedback::Observation ignored{};
        if(feedback::qualify(ticket,c,ignored)!=feedback::Result::managerEntry)return false;
        std::array<std::byte,0x148> ready{};std::copy(c.entry.begin(),c.entry.end(),ready.begin());
        const std::int32_t mode=1;std::memcpy(ready.data()+0x70,&mode,sizeof(mode));
        auto normalized=c;normalized.entry=ready;
        if(feedback::qualify(ticket,normalized,ignored)!=feedback::Result::accepted)return false;
        identity_=identity;insertion_=c;
        std::copy(c.prior.begin(),c.prior.end(),prior_.begin());
        std::copy(c.incoming.begin(),c.incoming.end(),body_.begin());
        std::copy(c.entry.begin(),c.entry.end(),entry_.begin());
        // No stored spans may outlive their originating native-call capture.
        insertion_.incoming={};insertion_.prior={};insertion_.applied={};insertion_.entry={};insertion_.priorEntry={};
        active_=true;return true;
    }
    [[nodiscard]] bool observe(const Identity& identity,const feedback::Capture& c,
        std::uint8_t nativePending,unsigned matches,feedback::Observation& out) noexcept {
        out={};
        if(!active_)return false;
        if(identity_!=identity || c.ticket!=identity_.binding.ticket || c.source!=identity_.source) {
            reset();return false;
        }
        if(!c.originalForwarded || c.producerRva!=feedback::kProducerRva || c.sequence<=insertion_.sequence
            || !c.managerReadyBefore || !c.managerReadyAfter || c.managerCountBefore!=c.managerCountAfter
            || c.managerCountAfter<insertion_.managerCountAfter || c.managerCountAfter>16
            || c.entryIndex!=insertion_.managerCountBefore || matches!=1 || c.entry.size()!=entry_.size()
            || c.incoming.size()!=body_.size() || c.prior.size()!=body_.size() || c.applied.size()!=body_.size()
            || !std::equal(c.incoming.begin(),c.incoming.end(),body_.begin())
            || !std::equal(c.prior.begin(),c.prior.end(),body_.begin())
            || !std::equal(c.applied.begin(),c.applied.end(),body_.begin())) {reset();return false;}
        // Original137BD50 assigns the native insertion serial at entry+2.
        // Keep that serial, world/registry identity, authored strings, lifecycle
        // and transported counts. UI-owned dirty flags need not stay constant.
        for(std::size_t i=0;i<entry_.size();++i) {
            const bool immutable=(i>=2 && i<0x29) || i==0x2B || (i>=0x58 && i<0x70)
                || (i>=0x74 && i<0x90) || (i>=0x110 && i<0x118) || (i>=0x124 && i<0x126);
            if(immutable && c.entry[i]!=entry_[i]) {reset();return false;}
        }
        // Native100A3A0 re-evaluates readiness, clears B05 and invokes
        // 1009E20/137E2C0/137CC60 to change this same entry's mode0 to mode1.
        if(nativePending!=0)return false;
        auto completed=insertion_;completed.sequence=c.sequence;completed.entry=c.entry;
        completed.incoming=body_;completed.prior=prior_;completed.applied=c.applied;
        feedback::Observation qualified{};
        if(feedback::qualify(identity_.binding.ticket,completed,qualified)!=feedback::Result::accepted)return false;
        out=qualified;reset();return true;
    }
private:
    Identity identity_{};feedback::Capture insertion_{};
    feedback::wire::Decoded prior_{},body_{};std::array<std::byte,0x148> entry_{};bool active_{};
};
}
