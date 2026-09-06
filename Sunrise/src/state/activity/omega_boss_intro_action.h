#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::state::activity::omega_presentation {

inline constexpr std::uint32_t kBossMemberDefinition = 0x80F4756DU;
inline constexpr std::uint32_t kBossIntroGroup = 0xAFB11A12U;
inline constexpr std::uint32_t kBossIntroSequence = 0x65D2379FU;
inline constexpr std::uint32_t kBossSummonEvent = 0xC0F9C866U;

/** Native 80807F72 queue copied by A9C960, not a wire packet. Runtime kind 9
 * differs from authored spawn-action kind 12. The executable's A889E0 lookup
 * resolves this pair against 80F4519A to group 0 / sequence 1. Its graph
 * retains the default one-second hold, flies in, then waits for the summon
 * event. The old 2BA99C93 sequence continued into the wipe animation.
 * Kind 9 ignores the third hash; it cannot select a starting graph node. */
[[nodiscard]] constexpr std::array<std::byte,0x808> boss_intro_action() noexcept {
    std::array<std::byte,0x808> queue{};
    const auto put = [&queue](std::size_t offset,std::uint32_t value) constexpr {
        for (unsigned i=0;i<4;++i) { queue[offset+i]=std::byte((value>>(i*8))&0xFFU); }
    };
    put(0,1);                           // one 0x40-byte command at +8
    queue[8]=std::byte{9};               // named animation; default condition 0
    put(0x18,kBossIntroGroup);            // 80807F76 payload starts at command+10
    put(0x1C,kBossIntroSequence);
    put(0x20,0x811C9DC5U);
    put(0x24,0x811C9DC5U);               // absent scoped target
    queue[0x28]=std::byte{0xFF};
    queue[0x2A]=queue[0x2B]=std::byte{0xFF};
    // Payload+14 target mode and +15 marker retain native default-fill zero.
    return queue;
}

/** A replaced native queue cancels this bridge's event lease even if its
 * authority revision did not change. Original A9C960 copies these exact fields. */
[[nodiscard]] constexpr bool boss_intro_action_active(
    std::span<const std::byte> queue,std::int32_t head) noexcept {
    if(head!=0 || queue.size()<0x20 || queue[8]!=std::byte{9}) { return false; }
    const auto read=[queue](std::size_t offset) constexpr {
        std::uint32_t value{};
        for(unsigned i=0;i<4;++i) { value|=std::to_integer<std::uint32_t>(queue[offset+i])<<(i*8); }
        return value;
    };
    return read(0)==1 && read(0x18)==kBossIntroGroup && read(0x1C)==kBossIntroSequence;
}

/** Opcode 5E is a synchronous, reference-counted native animation event.
 * C620F0 adds one reference; C693F0 removes exactly one matching reference. */
[[nodiscard]] constexpr std::array<std::byte,128> boss_summon_request() noexcept {
    std::array<std::byte,128> request{};
    const std::array hashes{kBossIntroGroup,kBossIntroSequence,kBossSummonEvent};
    for(std::size_t h=0;h<hashes.size();++h) {
        for(unsigned i=0;i<4;++i) { request[h*4+i]=std::byte((hashes[h]>>(i*8))&0xFFU); }
    }
    request[0x60]=std::byte{0x5E};
    return request;
}

/** Matching native C693F0 removal of the named group-0/sequence-1 request.
 * This clears its selector through native10D35D0. The motion arena is retired
 * on a subsequent native update; callers must observe that before teleport. */
[[nodiscard]] constexpr std::array<std::byte,128> boss_intro_stop_request() noexcept {
    std::array<std::byte,128> request{};
    const std::array hashes{kBossIntroGroup,kBossIntroSequence};
    for(std::size_t h=0;h<hashes.size();++h) {
        for(unsigned i=0;i<4;++i) { request[h*4+i]=std::byte((hashes[h]>>(i*8))&0xFFU); }
    }
    request[0x60]=std::byte{0x5D};
    return request;
}

struct BossSummonEventState {
    bool valid{};
    unsigned count{},refs{};
    [[nodiscard]] constexpr bool can_add() const noexcept {
        // Native C717E0 compares the reference byte as signed. Never increase
        // 127 to 128: its removal branch would erase every owner's reference.
        return valid && refs<127 && (refs!=0 || count<16);
    }
};

/** Read the native 16-entry animation-event table, including exact group and
 * sequence identity. A wildcard/another event belongs to its existing owner. */
[[nodiscard]] constexpr BossSummonEventState boss_summon_event_state(
    std::span<const std::byte> component) noexcept {
    if(component.size()<0xB8) { return {}; }
    const auto read=[component](std::size_t offset,unsigned width) constexpr {
        std::uint32_t value{};
        for(unsigned i=0;i<width;++i) { value|=std::to_integer<std::uint32_t>(component[offset+i])<<(i*8); }
        return value;
    };
    BossSummonEventState state{true,read(0x34,4),0};
    if(state.count>16) { return {}; }
    for(unsigned i=0;i<state.count;++i) {
        const auto offset=0x38+i*8;
        if(read(offset,4)==kBossSummonEvent && read(offset+4,2)==1 && read(offset+6,1)==0) {
            if(state.refs!=0 || read(offset+7,1)==0 || read(offset+7,1)>127) { return {}; }
            state.refs=read(offset+7,1);
        }
    }
    return state;
}

} // namespace sunrise::state::activity::omega_presentation
