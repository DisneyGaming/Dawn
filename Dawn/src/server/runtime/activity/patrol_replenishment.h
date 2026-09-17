#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::server::runtime::activity::patrol_replenishment {
// One credit per confirmed dead-and-retired actor. A streamed-out survivor is
// not a casualty. Retain individual deadlines so later deaths cannot postpone
// earlier replacements. Counts are per native category, not weighted choices.
struct Counts final {
    std::array<std::uint8_t,2> lane{};
    [[nodiscard]] constexpr unsigned total() const noexcept {return unsigned(lane[0])+lane[1];}
};
class Credits final {
public:
    [[nodiscard]] bool observe(std::uint32_t generation,std::uint64_t now,
        std::array<std::size_t,2> casualties,std::uint32_t cooldown=30000) noexcept {
        if(!generation || !cooldown || now>UINT64_MAX-cooldown)return false;
        if(generation_ && (generation<generation_ || now<lastNow_))return false;
        if(generation!=generation_) {*this={};generation_=generation;}
        if(casualties[0]<seen_[0] || casualties[1]<seen_[1])return false;
        std::size_t free{};for(const auto& debt:debts_)free+=debt.due==0;
        const auto first=casualties[0]-seen_[0],second=casualties[1]-seen_[1];
        if(first>free || second>free-first)return false;
        for(std::uint8_t category=0;category<2;++category) {
            auto count=casualties[category]-seen_[category];
            for(auto& debt:debts_)if(count && !debt.due) {debt={now+cooldown,category};--count;}
        }
        seen_=casualties;lastNow_=now;return true;
    }
    [[nodiscard]] Counts due(std::uint64_t now) const noexcept {
        Counts result{};
        if(now<lastNow_)return result;
        for(const auto& debt:debts_)if(debt.due && now>=debt.due)++result.lane[debt.category];
        return result;
    }
    [[nodiscard]] bool commit(std::uint64_t now,Counts count) noexcept {
        const auto ready=due(now);
        if(!count.total() || count.lane[0]>ready.lane[0] || count.lane[1]>ready.lane[1])return false;
        for(auto& debt:debts_)if(debt.due && now>=debt.due && count.lane[debt.category]) {
            --count.lane[debt.category];debt={};
        }
        return true;
    }
private:
    struct Debt final {std::uint64_t due{};std::uint8_t category{};};
    std::array<Debt,64> debts_{};
    std::array<std::size_t,2> seen_{};
    std::uint64_t lastNow_{};
    std::uint32_t generation_{};
};
template<class CountsType>
[[nodiscard]] constexpr bool casualties(const CountsType& counts,std::size_t& output) noexcept {
    if(counts.failed || counts.dead>counts.admitted || counts.alive>counts.resident
        || counts.resident>counts.admitted || counts.resident-counts.alive>counts.dead)return false;
    output=counts.dead-(counts.resident-counts.alive);return true;
}
// Native counters are nonnegative signed32. Keep the per-step request bounded,
// but do not mistake lifetime quota for simultaneous population. Never wrap a
// counter or reset a source generation while surviving actors remain.
[[nodiscard]] constexpr bool fits(unsigned first,unsigned second,Counts additions) noexcept {
    return first && additions.total() && additions.total()<=63
        && first<=unsigned(INT32_MAX)-additions.lane[0]
        && second<=unsigned(INT32_MAX)-additions.lane[1];
}
template<class Service,class Ledger>
[[nodiscard]] bool ready(Credits& credits,std::uint64_t now,std::uint32_t cooldown,
    const Service& service,std::size_t index,const Ledger& ledger,bool twoCategories,
    bool nativePending,Counts intended,Counts& output) noexcept {
    output={};std::array<std::size_t,2> deadRetired{},alive{};
    const auto all=ledger.counts();
    if(twoCategories) {
        if constexpr(requires {ledger.counts(std::uint8_t{});}) {
        const auto first=ledger.counts(0),second=ledger.counts(1);
        // Incomplete category attribution is not permission to refill the wrong
        // species or a dormant second lane. Whole-source renewal remains usable.
        if(first.admitted+second.admitted!=all.admitted)return true;
        if(!casualties(first,deadRetired[0]) || !casualties(second,deadRetired[1]))return false;
        alive={first.alive,second.alive};
        } else return true;
    } else {if(!casualties(all,deadRetired[0]))return false;alive[0]=all.alive;}
    if(!credits.observe(service.generation(index),now,deadRetired,cooldown))return false;
    if(nativePending || !all.alive)return true;
    const auto* observed=service.observation(index);
    if(!observed || !observed->consumedKnown || observed->consumedCount!=(twoCategories?2:1))return true;
    for(unsigned lane=0;lane<(twoCategories?2U:1U);++lane)
        if(observed->consumed[lane]<0 || static_cast<std::size_t>(observed->consumed[lane])<deadRetired[lane])return true;
    auto additions=credits.due(now);
    for(unsigned lane=0;lane<(twoCategories?2U:1U);++lane) {
        const auto requested=lane?service.second_target(index):service.target(index);
        if(!requested){additions.lane[lane]=0;continue;}
        const auto consumed=static_cast<std::size_t>(observed->consumed[lane]);
        // Already-issued replacements count against the intended live ceiling,
        // even before they finish spawning. An over-admitted singleton never
        // earns another leader while its surviving leader still occupies it.
        const auto outstanding=requested>consumed+alive[lane]?requested-consumed-alive[lane]:0;
        const auto occupied=alive[lane]+outstanding;
        const auto vacancies=intended.lane[lane]>occupied?intended.lane[lane]-occupied:0;
        if(additions.lane[lane]>vacancies)additions.lane[lane]=static_cast<std::uint8_t>(vacancies);
    }
    if(fits(service.target(index),service.second_target(index),additions))output=additions;
    return true;
}
}
