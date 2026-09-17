#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::client::hooks::bootflow::eater_cranium_deferred {

/** Per-thread nested native-use queue. A scope belongs to one queue instance and
 * reset epoch, so another thread's queue or a pre-reset scope cannot drain it. */
template<class Event,std::size_t Capacity>
class Queue final {
public:
    struct Scope final {
        const Queue* owner{};
        std::uint64_t epoch{};
        std::uint64_t token{};
        bool outer{};
        explicit operator bool() const noexcept {return owner!=nullptr;}
    };

    Scope begin(bool active) noexcept {
        if(!active || depth_>=scopeTokens_.size()) return {};
        const bool outer=depth_==0;
        if(outer) count_=0;
        if(++nextToken_==0) ++nextToken_;
        scopeTokens_[depth_++]=nextToken_;
        return {this,epoch_,nextToken_,outer};
    }

    bool defer(const Event& event) noexcept {
        if(!depth_) return false;
        if(count_>=Capacity) return false;
        events_[count_++]=event;return true;
    }

    template<class Match>
    bool cancel(Match&& match) noexcept {
        std::size_t kept{};bool removed{};
        for(std::size_t i=0;i<count_;++i) {
            if(match(events_[i])) {removed=true;continue;}
            if(kept!=i) events_[kept]=events_[i];
            ++kept;
        }
        for(std::size_t i=kept;i<count_;++i) events_[i]={};
        count_=kept;return removed;
    }

    template<class Visit>
    bool finish(Scope scope,Visit&& visit) noexcept {
        if(scope.owner!=this || scope.epoch!=epoch_ || !depth_
            || scopeTokens_[depth_-1]!=scope.token) return false;
        scopeTokens_[depth_-1]=0;
        --depth_;
        if(!scope.outer) return true;
        if(depth_) {reset();return false;}
        const auto pending=events_;const auto count=count_;count_=0;events_={};
        for(std::size_t i=0;i<count;++i) visit(pending[i]);
        return true;
    }

    void reset() noexcept {
        events_={};scopeTokens_={};count_=depth_=0;
        if(++epoch_==0) ++epoch_;
    }

    std::size_t pending() const noexcept {return count_;}
    bool active() const noexcept {return depth_!=0;}

private:
    std::array<Event,Capacity> events_{};
    std::array<std::uint64_t,Capacity+1> scopeTokens_{};
    std::size_t count_{};
    unsigned depth_{};
    std::uint64_t epoch_{1};
    std::uint64_t nextToken_{};
};

} // namespace dawn::client::hooks::bootflow::eater_cranium_deferred
