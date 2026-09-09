#pragma once
#include "../../../src/middleware/bap/activity_message/activity_clock_state_encoder.h"
#include <array>
#include <limits>
namespace beyond_clock_state_fixture {
inline void run(void (*check)(bool,const char*)) {
    namespace clock=sunrise::middleware::bap::activity_message::clock_state;
    std::array<std::byte,5> bytes{};std::size_t written{};
    check(clock::encode(clock::kRunning,bytes,written) && written==5,
        "native clock notification fits its 33-bit schema");
    const std::array<std::byte,5> running{std::byte{0x1F},std::byte{0xC0},std::byte{},std::byte{},std::byte{}};
    check(bytes==running,"clock running payload preserves flag zero and encodes IEEE rate one MSB first");
    check(clock::encode({true,1.F},bytes,written) && bytes[0]==std::byte{0x9F} && bytes[1]==std::byte{0xC0},
        "clock flag occupies the first bit before the float");
    check(clock::encode({false,0.F},bytes,written) && bytes==std::array<std::byte,5>{},
        "zero clock rate retains native paused payload");
    check(clock::encode({false,.5F},bytes,written) && bytes[0]==std::byte{0x1F} && bytes[1]==std::byte{0x80},
        "clock fractional rate uses raw IEEE float representation");
    bytes.fill(std::byte{0xA5});const auto original=bytes;
    for(const float rate:{-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        check(!clock::encode({false,rate},bytes,written) && written==0 && bytes==original,
            "invalid clock rates leave output unchanged");
    }
    check(!clock::encode(clock::kRunning,std::span(bytes).first<4>(),written) && written==0 && bytes==original,
        "short clock output leaves output unchanged");
}
}
