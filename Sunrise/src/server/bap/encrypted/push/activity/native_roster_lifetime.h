#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::server::bap::encrypted::push::activity::roster_lifetime {

inline constexpr std::size_t kTopCapacity=256;
inline constexpr std::size_t kBlockCapacity=64;
inline constexpr std::size_t kBlockKeyCapacity=96;
inline constexpr std::size_t kRetainedKeyCapacity=kTopCapacity+kBlockCapacity*kBlockKeyCapacity;

struct Identity final {
    std::uint64_t owner{},incarnation{},epochLo{},epochHi{};
    std::uint32_t scenario{};
    [[nodiscard]] constexpr bool operator==(const Identity&) const noexcept=default;
};

template<std::size_t Capacity> struct List final {
    std::array<std::uint32_t,Capacity> keys{};
    std::array<std::uint8_t,Capacity> presence{},states{};
    std::size_t count{};
};

struct Block final { std::uint32_t bubble{};List<kBlockKeyCapacity> entries{}; };

/** Dedicated phase-1 mirror. Group bodies and their ordering are not stored here.
 * Local identity is (bubble, key): a shared registry has an independent ordinal,
 * presence and state in each bubble. Global keys remain separate from locals.
 * States are complete wire bytes, including the encoder's 0x80 bias. */
struct State final {
    Identity identity{};
    List<kTopCapacity> top{};
    std::array<Block,kBlockCapacity> blocks{};
    std::size_t blockCount{};
};

struct WireList final {
    std::span<const std::uint32_t> keys{};
    std::span<const std::uint8_t> presence{},states{};
};
struct WireBlock final { std::uint32_t bubble{};WireList entries{}; };
struct DesiredList final {
    std::span<const std::uint32_t> keys{};
    /** Empty means present. Omission of a key is not a removal request. */
    std::span<const std::uint8_t> presence{};
};
struct DesiredBlock final { std::uint32_t bubble{};DesiredList entries{}; };
struct Request final {
    Identity identity{};
    std::uint32_t currentBubble{};
    std::uint8_t initialStateByte{};
    DesiredList top{};
    std::span<const DesiredBlock> blocks{};
};

enum class Result : std::uint8_t {
    ready,invalidIdentity,identityMismatch,invalidBubble,invalidView,invalidState,
    capacity,duplicateKey,duplicateBubble,keyScopeChanged,dormantChange,unknownRemoval,
    invalidPrior
};

template<std::size_t N> [[nodiscard]] constexpr WireList view(const List<N>& list) noexcept {
    if(list.count>N)return {};
    return {std::span(list.keys).first(list.count),std::span(list.presence).first(list.count),
            std::span(list.states).first(list.count)};
}

namespace detail {
[[nodiscard]] constexpr bool valid_identity(const Identity& id) noexcept {
    // The caller separately qualifies epoch availability. Zero and all-ones
    // are valid opaque native epochs; only equality binds this retained mirror.
    return id.owner!=0 && id.incarnation!=0 && id.scenario!=0 && id.scenario!=UINT32_MAX;
}
[[nodiscard]] constexpr bool valid_state(std::uint8_t value) noexcept {return value>=0x80;}

[[nodiscard]] constexpr Result desired(const DesiredList& value,std::size_t maximum) noexcept {
    if(value.keys.size()>maximum)return Result::capacity;
    if(!value.presence.empty() && value.presence.size()!=value.keys.size())return Result::invalidView;
    for(const auto present:value.presence)if(present>1)return Result::invalidView;
    for(std::size_t i=0;i<value.keys.size();++i) {
        if(value.keys[i]==0 || value.keys[i]==UINT32_MAX)return Result::invalidView;
        for(std::size_t j=0;j<i;++j)if(value.keys[i]==value.keys[j])return Result::duplicateKey;
    }
    return Result::ready;
}
[[nodiscard]] constexpr Result wire(const WireList& value,std::size_t maximum) noexcept {
    const auto result=desired({value.keys,value.presence},maximum);
    if(result!=Result::ready)return result;
    if(value.presence.size()!=value.keys.size() || value.states.size()!=value.keys.size())
        return Result::invalidView;
    for(const auto state:value.states)if(!valid_state(state))return Result::invalidState;
    return Result::ready;
}
template<std::size_t N> [[nodiscard]] constexpr bool contains(const List<N>& list,
    std::uint32_t key) noexcept {
    for(std::size_t i=0;i<list.count;++i)if(list.keys[i]==key)return true;
    return false;
}
[[nodiscard]] constexpr bool in_bubbles(const State& state,std::uint32_t key) noexcept {
    for(std::size_t b=0;b<state.blockCount;++b)
        if(contains(state.blocks[b].entries,key))return true;
    return false;
}
template<std::size_t N> constexpr void copy(List<N>& to,const WireList& from) noexcept {
    to.count=from.keys.size();
    for(std::size_t i=0;i<to.count;++i) {
        to.keys[i]=from.keys[i];to.presence[i]=from.presence[i];to.states[i]=from.states[i];
    }
}
template<std::size_t N> [[nodiscard]] constexpr Result apply(List<N>& target,
    const DesiredList& desiredList,std::uint8_t initialState,bool active) noexcept {
    for(std::size_t i=0;i<desiredList.keys.size();++i) {
        const auto key=desiredList.keys[i];
        const auto presence=desiredList.presence.empty()?std::uint8_t{1}:desiredList.presence[i];
        std::size_t slot{};
        while(slot<target.count && target.keys[slot]!=key)++slot;
        if(slot==target.count) {
            if(presence==0)return Result::unknownRemoval;
            if(target.count==N)return Result::capacity;
            // Receive3CCE50 stores a dormant addition without dispatch. Native
            // slice-load42F1B0 -> C79B40 ->3CBE50 constructs its present keys
            // on later entry. No state transition is needed at that time.
            target.keys[slot]=key;target.presence[slot]=1;target.states[slot]=initialState;
            ++target.count;
        } else {
            if(!active && target.presence[slot]!=presence)return Result::dormantChange;
            target.presence[slot]=presence;
            // Equal presence/state preserves native objects. A tombstone's later
            // 0->1 presence edge creates anew; it needs no fabricated state bump.
        }
    }
    return Result::ready;
}
} // namespace detail

/** Validate a retained mirror before using any embedded counts as bounds. */
[[nodiscard]] constexpr Result validate(const State& state) noexcept {
    if(!detail::valid_identity(state.identity))return Result::invalidIdentity;
    if(state.top.count>kTopCapacity || state.blockCount>kBlockCapacity)return Result::capacity;
    auto result=detail::wire(view(state.top),kTopCapacity);
    if(result!=Result::ready)return result;
    std::size_t retained=state.top.count;
    for(std::size_t b=0;b<state.blockCount;++b) {
        const auto& block=state.blocks[b];
        if(block.bubble>=kBlockCapacity)return Result::invalidBubble;
        if(block.entries.count>kBlockKeyCapacity)return Result::capacity;
        for(std::size_t prior=0;prior<b;++prior)
            if(state.blocks[prior].bubble==block.bubble)return Result::duplicateBubble;
        result=detail::wire(view(block.entries),kBlockKeyCapacity);
        if(result!=Result::ready)return result;
        retained+=block.entries.count;
        if(retained>kRetainedKeyCapacity)return Result::capacity;
        for(std::size_t i=0;i<block.entries.count;++i) {
            const auto key=block.entries.keys[i];
            if(detail::contains(state.top,key))return Result::duplicateKey;
        }
    }
    return Result::ready;
}

/** Adopt the exact already-committed warm-up bytes, never a newly selected set.
 * This records a mirror, not native construction/readiness. Failure leaves out intact. */
[[nodiscard]] inline Result seed(const Identity& identity,const WireList& top,
    std::span<const WireBlock> blocks,State& out) noexcept {
    if(!detail::valid_identity(identity))return Result::invalidIdentity;
    if(blocks.size()>kBlockCapacity)return Result::capacity;
    auto result=detail::wire(top,kTopCapacity);
    if(result!=Result::ready)return result;
    State candidate{};candidate.identity=identity;detail::copy(candidate.top,top);
    candidate.blockCount=blocks.size();
    for(std::size_t b=0;b<blocks.size();++b) {
        result=detail::wire(blocks[b].entries,kBlockKeyCapacity);
        if(result!=Result::ready)return result;
        candidate.blocks[b].bubble=blocks[b].bubble;
        detail::copy(candidate.blocks[b].entries,blocks[b].entries);
    }
    result=validate(candidate);
    if(result==Result::ready)out=candidate;
    return result;
}

/** Pure candidate publication. Ordinals never compact or change scope.
 * New dormant entries may be appended for native construction on bubble entry.
 * Existing dormant entries remain byte-identical: presence changes fail closed.
 * Explicit removal/reactivation is supported only for globals/current bubble.
 * The caller must commit this value with the corresponding encoded publication. */
[[nodiscard]] inline Result plan(const State& prior,const Request& request,State& out) noexcept {
    if(validate(prior)!=Result::ready)return Result::invalidPrior;
    if(!detail::valid_identity(request.identity))return Result::invalidIdentity;
    if(prior.identity!=request.identity)return Result::identityMismatch;
    if(request.currentBubble>=kBlockCapacity)return Result::invalidBubble;
    if(!detail::valid_state(request.initialStateByte))return Result::invalidState;
    if(request.blocks.size()>kBlockCapacity)return Result::capacity;
    auto result=detail::desired(request.top,kTopCapacity);
    if(result!=Result::ready)return result;
    State candidate=prior;
    for(const auto key:request.top.keys) {
        if(detail::in_bubbles(prior,key))return Result::keyScopeChanged;
    }
    result=detail::apply(candidate.top,request.top,request.initialStateByte,true);
    if(result!=Result::ready)return result;
    for(std::size_t b=0;b<request.blocks.size();++b) {
        const auto& block=request.blocks[b];
        if(block.bubble>=kBlockCapacity)return Result::invalidBubble;
        for(std::size_t p=0;p<b;++p)
            if(request.blocks[p].bubble==block.bubble)return Result::duplicateBubble;
        result=detail::desired(block.entries,kBlockKeyCapacity);
        if(result!=Result::ready)return result;
        for(const auto key:block.entries.keys) {
            if(detail::contains(candidate.top,key))return Result::keyScopeChanged;
        }
        std::size_t slot{};
        while(slot<candidate.blockCount && candidate.blocks[slot].bubble!=block.bubble)++slot;
        const bool active=block.bubble==request.currentBubble;
        if(slot==candidate.blockCount) {
            if(slot==kBlockCapacity)return Result::capacity;
            candidate.blocks[slot].bubble=block.bubble;++candidate.blockCount;
        }
        result=detail::apply(candidate.blocks[slot].entries,block.entries,request.initialStateByte,active);
        if(result!=Result::ready)return result;
    }
    result=validate(candidate);
    if(result==Result::ready)out=candidate;
    return result;
}

} // namespace sunrise::server::bap::encrypted::push::activity::roster_lifetime
