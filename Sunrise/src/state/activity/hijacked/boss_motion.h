#pragma once
#include "../omega_boss_teleport_action.h"
#include <cstring>
namespace sunrise::state::activity::hijacked::boss_motion {
// Installed 80B421AA point slots54/57, then 80B420E0 native boss spawn.
inline constexpr std::array<std::array<float,3>,3> kDestinations{{
    {283.8000183F,307.6000061F,-89.8000031F},
    {400.3700867F,359.0768738F,-96.6716232F},
    {466.3000183F,331.3999939F,-95.4000015F}}};
// 80F2FD3F group0 (1F992208), sequence6 (CBFDCA32).
inline std::optional<omega_boss_teleport::Request> request(std::uint8_t stage) noexcept {
    if(stage>=kDestinations.size()) {return std::nullopt;}
    auto result=omega_boss_teleport::encode_request(kDestinations[stage]);
    (*result)[0]=std::byte{};(*result)[4]=std::byte{6};return result;
}
inline bool arrived(std::uint8_t stage,const std::array<float,3>& point) noexcept {
    if(stage>=kDestinations.size()) {return false;}
    float distance{};
    for(std::size_t i=0;i<point.size();++i) {
        if(!std::isfinite(point[i])) {return false;}
        const auto delta=point[i]-kDestinations[stage][i];distance+=delta*delta;
    }
    return distance<=1.F;
}

struct Landing {
    std::array<float,4> destination{};
    std::uint32_t sequence{};
    bool active{};
};
// 10C6AF0 records the sequence at+94. Native10D0020 projects its provisional
// destination onto the world, adds the native height offset and stores+B0;
// 10C7273 then copies that adjusted point into the actual motion request.
// The authored point can differ by several metres from this native destination.
inline Landing landing(std::span<const std::byte> selector) noexcept {
    Landing out{};
    if(selector.size()<0xC0) {return out;}
    out.active=selector[0x90]!=std::byte{};
    std::memcpy(&out.sequence,selector.data()+0x94,sizeof out.sequence);
    std::memcpy(out.destination.data(),selector.data()+0xB0,sizeof out.destination);
    return out;
}
// User-requested generous allowance for native landing correction and Hydra drift.
inline constexpr float kArrivalRadius=15.F;
struct LandingEvidence {
    std::array<float,4> previous{};
    bool activeSeen{};
};
inline void retain_landing(LandingEvidence& evidence,const Landing& current) noexcept {
    if(current.sequence==6 && current.active) {evidence.activeSeen=true;}
}
inline bool teleport_arrived(bool issued,bool activeSeen,const std::array<float,4>& previous,
                             const Landing& current,const std::array<float,3>& actual) noexcept {
    if(!issued || current.active || current.sequence!=6 || current.destination[3]!=1.F) {return false;}
    for(std::size_t i=0;i<previous.size();++i) {if(!std::isfinite(previous[i]) || !std::isfinite(current.destination[i])) {return false;}}
    // A stopped old selector is not an acknowledgement of a new request. Retain
    // either native activation or a newly resolved destination for this lease.
    if(!activeSeen && current.destination==previous) {return false;}
    float distance{};
    for(std::size_t i=0;i<actual.size();++i) {
        if(!std::isfinite(actual[i]) || !std::isfinite(current.destination[i])) {return false;}
        const auto delta=actual[i]-current.destination[i];distance+=delta*delta;
    }
    return distance<=kArrivalRadius*kArrivalRadius;
}
inline bool teleport_arrived(bool issued,const LandingEvidence& evidence,
                             const Landing& current,const std::array<float,3>& actual) noexcept {
    // The endpoint can continue changing after dispatch. Always use its latest
    // native value; an intermediate correction must never poison this request.
    return teleport_arrived(issued,evidence.activeSeen,evidence.previous,current,actual);
}
}
