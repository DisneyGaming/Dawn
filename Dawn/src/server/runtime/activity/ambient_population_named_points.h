#pragma once
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <array>

namespace dawn::server::runtime::activity::ambient_population::named_points {
using Owner=state::activity::ActivityInstanceKey;
struct Point final {
    std::uint32_t index{},entityDefinition{};
    std::uint64_t guid{};
    friend constexpr bool operator==(const Point&,const Point&)=default;
};
struct Dependency final {
    const registry::Definition* registry{};
    std::uint32_t list{},placementCount{};
    std::span<const Point> points;
};
[[nodiscard]] inline bool valid(const Dependency& value) noexcept {
    if(!value.registry || !registry::valid(*value.registry) || !value.list
        || !value.placementCount || value.placementCount>256 || value.points.empty() || value.points.size()>8)return false;
    for(std::size_t i=0;i<value.points.size();++i) {
        const auto& p=value.points[i];
        if(p.index>=value.placementCount || !p.guid || !p.entityDefinition || p.entityDefinition==UINT32_MAX)return false;
        for(std::size_t j=0;j<i;++j)if(value.points[j].index==p.index || value.points[j].guid==p.guid)return false;
    }
    return true;
}
struct Ticket final {
    Owner owner{};
    std::uint64_t boot{};
    std::uint32_t registry{},list{},placementCount{};
    std::array<Point,8> points{};
    std::uint8_t count{};
    friend constexpr bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] inline Ticket ticket(Owner owner,std::uint64_t boot,const Dependency& dependency) noexcept {
    if(!owner || !boot || !valid(dependency))return {};
    Ticket out{owner,boot,dependency.registry->key,dependency.list,dependency.placementCount};
    out.count=static_cast<std::uint8_t>(dependency.points.size());
    std::copy(dependency.points.begin(),dependency.points.end(),out.points.begin());return out;
}
struct Binding final {
    Ticket ticket{};
    std::uint64_t epoch{};
    friend constexpr bool operator==(const Binding&,const Binding&)=default;
};
struct Snapshot final {
    Binding binding{};
    std::array<std::uint32_t,8> handles{};
    std::uint8_t constructed{},interfaces{};
    bool active{};
    std::uint8_t observed{},retired{};
    bool incomplete{};
    [[nodiscard]] bool ready() const noexcept {
        return active && !incomplete && binding.epoch && binding.ticket.count && constructed==((1U<<binding.ticket.count)-1U);
    }
};
// Value-only callback mailbox. Capture epochs prevent an old constructor return
// from reauthorizing a point after removal or activity release.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const Ticket& value) noexcept {
        if(!value.owner || !value.boot || !value.registry || !value.list || !value.count || value.count>8
            || !value.placementCount || value.placementCount>256)return false;
        for(std::size_t i=0;i<value.count;++i) {
            const auto& p=value.points[i];if(!p.guid || !p.entityDefinition || p.entityDefinition==UINT32_MAX
                || p.index>=value.placementCount)return false;
            for(std::size_t j=0;j<i;++j)if(value.points[j].guid==p.guid || value.points[j].index==p.index)return false;
        }
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket.list==value.list) {
            if(rows_[i].active)return rows_[i].binding.ticket==value;
            if(epoch_==UINT64_MAX)return false;
            rows_[i]={{value,++epoch_},{},0,0,true};return true;
        }
        if(count_==rows_.size() || epoch_==UINT64_MAX)return false;
        rows_[count_++]={{value,++epoch_},{},0,0,true};return true;
    }
    [[nodiscard]] Snapshot lookup(std::uint32_t list) const noexcept {
        const auto state=retirement(list);return state.active?state:Snapshot{};
    }
    [[nodiscard]] Snapshot retirement(std::uint32_t list) const noexcept {
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket.list==list)return rows_[i];return {};
    }
    // Cleanup identity survives list rebuilds and owner changes. This view can
    // never authorize construction or population activation.
    [[nodiscard]] Snapshot retirement(std::uint32_t list,std::uint32_t handle) const noexcept {
        for(const auto& retained:retained_)if(retained.used && retained.binding.ticket.list==list && retained.handle==handle) {
            Snapshot out{};out.binding=retained.binding;
            for(std::size_t j=0;j<out.binding.ticket.count;++j)if(out.binding.ticket.points[j]==retained.point) {
                out.handles[j]=handle;out.observed=static_cast<std::uint8_t>(1U<<j);return out;
            }
        }
        return {};
    }
    [[nodiscard]] std::size_t pending_retirements() const noexcept {
        std::size_t count{};for(const auto& retained:retained_)count+=retained.used?1U:0U;return count;
    }
    [[nodiscard]] bool constructed(const Binding& binding,const Point& point,std::uint32_t handle) noexcept {
        if(!binding.epoch || handle==UINT32_MAX)return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& row=rows_[i];if(!row.active || row.binding!=binding)continue;
            for(std::size_t j=0;j<binding.ticket.count;++j)if(binding.ticket.points[j]==point) {
                const auto bit=static_cast<std::uint8_t>(1U<<j);
                if((row.constructed&bit) && row.handles[j]!=handle)return false;
                if(!retain(binding,point,handle)) {row.incomplete=true;return false;}
                row.handles[j]=handle;row.constructed|=bit;row.observed|=bit;
                row.retired&=static_cast<std::uint8_t>(~bit);return true;
            }
        }
        return false;
    }
    [[nodiscard]] bool interface_resolved(const Binding& binding,std::uint64_t guid) noexcept {
        for(std::size_t i=0;i<count_;++i) {
            auto& row=rows_[i];if(!row.active || row.binding!=binding)continue;
            for(std::size_t j=0;j<binding.ticket.count;++j)if(binding.ticket.points[j].guid==guid) {
                const auto bit=static_cast<std::uint8_t>(1U<<j);
                if(!(row.constructed&bit))return false;
                const bool changed=(row.interfaces&bit)==0;row.interfaces|=bit;return changed;
            }
        }
        return false;
    }
    [[nodiscard]] bool actor_retired(const Binding& binding,const Point& point,std::uint32_t handle) noexcept {
        if(!binding.epoch || epoch_==UINT64_MAX)return false;
        for(auto& retained:retained_)if(retained.used && retained.point==point && retained.handle==handle
            && retained.binding.ticket==binding.ticket) {
            Snapshot* current{};
            for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket==binding.ticket)current=&rows_[i];
            if(retained.binding!=binding && (!current || current->binding!=binding))return false;
            retained={};
            if(current)for(std::size_t j=0;j<binding.ticket.count;++j)if(binding.ticket.points[j]==point
                && current->handles[j]==handle) {
                const auto bit=static_cast<std::uint8_t>(1U<<j);
                current->constructed&=static_cast<std::uint8_t>(~bit);current->interfaces&=static_cast<std::uint8_t>(~bit);
                current->retired|=bit;current->binding.epoch=++epoch_;break;
            }
            return true;
        }
        return false;
    }
    [[nodiscard]] bool removing(const Binding& binding) noexcept {
        if(epoch_==UINT64_MAX)return false;
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding==binding) {
            rows_[i].constructed=0;rows_[i].interfaces=0;rows_[i].binding.epoch=++epoch_;return true;
        }
        return false;
    }
    void release(Owner owner) noexcept {
        for(std::size_t i=0;i<count_;++i)if(rows_[i].active && rows_[i].binding.ticket.owner==owner) {
            rows_[i].active=false;
            rows_[i].binding.epoch=epoch_==UINT64_MAX?0:++epoch_;
        }
    }
private:
    struct Retained final {Binding binding{};Point point{};std::uint32_t handle{};bool used{};};
    [[nodiscard]] bool retain(const Binding& binding,const Point& point,std::uint32_t handle) noexcept {
        Retained* available{};
        for(auto& retained:retained_) {
            if(!retained.used) {if(!available)available=&retained;continue;}
            if(retained.handle==handle)return retained.binding.ticket==binding.ticket && retained.point==point;
        }
        if(!available)return false;
        *available={binding,point,handle,true};return true;
    }
    // Bounded independently from population service capacity. Exhaustion never
    // evicts a resident identity and leaves the affected source gate closed.
    std::array<Retained,256> retained_{};
    std::array<Snapshot,8> rows_{};std::size_t count_{};std::uint64_t epoch_{};
};
[[nodiscard]] bool bind(const Ticket&) noexcept;
[[nodiscard]] Snapshot lookup(std::uint32_t list) noexcept;
[[nodiscard]] Snapshot retirement(std::uint32_t list) noexcept;
[[nodiscard]] Snapshot retirement(std::uint32_t list,std::uint32_t handle) noexcept;
[[nodiscard]] bool ready(const Ticket&) noexcept;
[[nodiscard]] bool constructed(const Binding&,const Point&,std::uint32_t handle) noexcept;
[[nodiscard]] bool interface_resolved(const Binding&,std::uint64_t guid) noexcept;
[[nodiscard]] bool removing(const Binding&) noexcept;
[[nodiscard]] bool actor_retired(const Binding&,const Point&,std::uint32_t handle) noexcept;
void release(Owner) noexcept;
}
