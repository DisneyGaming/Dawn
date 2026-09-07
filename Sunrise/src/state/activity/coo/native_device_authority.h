#pragma once
#include <bit>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
namespace sunrise::state::activity::coo::native_device {
[[nodiscard]] inline bool inactive_state(std::span<const std::byte,0x44> state) noexcept {
    const auto read32=[](const std::byte* b) noexcept { std::uint32_t v{};std::memcpy(&v,b,sizeof v);return v; };
    const auto read64=[](const std::byte* b) noexcept { std::uint64_t v{};std::memcpy(&v,b,sizeof v);return v; };
    const auto generation = read32(state.data());
    if (generation == 0U || generation >= 0x7FFFFFFFU
        || read32(state.data() + 4) != 0U
        || state[8] != std::byte{} || state[9] != std::byte{}
        || read32(state.data() + 0xC) != 0xFFFFFFFFU
        || read64(state.data() + 0x10) != 0xFFFF00FF811C9DC5ULL
        || read32(state.data() + 0x20) != 0U
        || read32(state.data() + 0x24) != 0U
        || read32(state.data() + 0x28) != 0U
        || read32(state.data() + 0x2C) != 0x3F800000U
        || state[0x30] != std::byte{}
        || read32(state.data() + 0x40) != 0U) { return false; }
    return true;
}
// 8080956A collection: count4, up to8 runtime selectors of stride0x30.
// 80809579 -> ABA810 -> 502770/5009C0 resolves one entity from a scoped reference.
// Each marcher source requests exactly one actor; no player selector is used.
template<class Writer> bool collection(Writer& w,std::uint32_t registry,std::span<const std::uint16_t> sources) noexcept {
    if(sources.size()>8 || !w.write(static_cast<std::uint32_t>(sources.size()),4)) { return false; }
    for(const auto source:sources) {
        if(!w.write(1,1) || !w.write(0x80809579U,32) || !w.write(1,2)
            || !w.write(registry,32) || !w.write(2,7) || !w.write(32768U+source,16)) { return false; }
    }
    return true;
}
// 8080954B linked collection is preferred by native9EF940 over dynamic selectors.
template<class Writer> bool linked_effect(Writer& w,std::uint32_t registry,std::uint16_t collection,bool enabled) noexcept {
    if(!w.write(0,1) || !w.write(enabled?0U:1U,1)) { return false; }
    for(unsigned i=0;i<4;++i) { if(!w.write(0x80000000U,32)) { return false; } }
    return w.write(registry,32) && w.write(35,7) && w.write(32768U+collection,16) && w.write(0,1);
}
template<class Writer> bool object(Writer& writer,std::uint32_t generation,bool active) noexcept {
    return writer.write(generation ^ 0x80000000U, 32) // decoded generation
        && writer.write(0x80000000U, 32)              // decoded candidate index 0
        && writer.write(active ? 1U : 0U, 1)
        && writer.write(0U, 1)                       // no placement override
        && writer.write(0x7FFFFFFFU, 32)              // retain authored auxiliary integer -1
        && writer.write(0x811C9DC5U, 32)             // canonical absent scoped ref
        && writer.write(0U, 7)                       // decoded type -1
        && writer.write(0x7FFFU, 16)                 // decoded index -1
        && writer.write(0U, 32) && writer.write(0U, 32) && writer.write(0U, 32)
        && writer.write(0U, 1)                       // auxiliary object flag off
        && writer.write(0U, 2);                      // zero dynamic component states
}

// Change only the position channel. Absent revisions (-1) preserve native power
// and lock defaults. Snap is used for a barrier that must already exist on arrival.
template<class Writer> bool position_only(Writer& writer,float position,std::int16_t revision,bool snap) noexcept {
    return writer.write(std::bit_cast<std::uint32_t>(position),32)
        && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(revision)+0x8000),16)
        && writer.write(snap?1U:0U,1)
        && writer.write(0x3F800000U,32) && writer.write(0x7FFFU,16) && writer.write(0,1)
        && writer.write(0,32) && writer.write(0x7FFFU,16) && writer.write(0,1);
}

template<class Writer> bool channels(Writer& writer,float position,std::uint16_t revision) noexcept {
    const auto channel=[&](float value,std::uint16_t rev) noexcept {
        return writer.write(std::bit_cast<std::uint32_t>(value),32)
            && writer.write(static_cast<std::uint32_t>(rev)+0x8000U,16)
            && writer.write(0U,1);
    };
    // Zero revisions leave native power1/lock0 untouched. Smooth native device
    // interpolation owns the animation; no host timing or manual model phase.
    return channel(position,revision) && channel(1.F,0) && channel(0.F,0);
}

}
