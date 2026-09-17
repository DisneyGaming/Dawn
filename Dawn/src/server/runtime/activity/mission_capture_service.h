#pragma once
#include "../../../middleware/bap/activity_message/native/capture_controller_authority.h"
#include "mission_device_pose_service.h"
#include <cstring>
#include <span>

namespace dawn::server::runtime::activity::mission_capture {
namespace wire=middleware::bap::activity_message::native::capture_controller;
// Server-owned command state. Time controls the native timer request only;
// completion still requires an authenticated original 1006F20 observation.
struct Publication final {
    wire::State state{};
    std::uint32_t revision{};
    bool published{};
    float presentationPosition{};
    mission_device_pose::Publication pose{};
};
[[nodiscard]] inline bool update(Publication& out,std::uint32_t revision,bool running,
    bool completed,std::uint64_t duration,std::uint64_t now) noexcept {
    if(!revision || !duration || duration>(static_cast<std::uint64_t>(INT32_MAX)*673200U/1000U) || now>=UINT64_MAX-duration)return false;
    // Retain the exact native completed pose and anchor across departure.
    if(completed && out.published)return true;
    if(out.published && out.revision==revision && out.state.clock.running==running)return true;
    out.state=running?wire::State{true,{true,0,duration,0,duration,now,1.F},false}:wire::State{};
    out.revision=revision;out.published=true;
    return true;
}
// Read-only receipt qualification against the exact server publication. This
// accepts no callback from an earlier occupancy revision or timer command.
[[nodiscard]] inline bool matches(const Publication& expected,std::uint32_t revision,
    std::span<const std::byte> bytes) noexcept {
    if(!expected.published || expected.revision!=revision || bytes.size()!=0x48)return false;
    const auto field=[&]<class T>(std::size_t offset) {T out{};std::memcpy(&out,bytes.data()+offset,sizeof out);return out;};
    const wire::State actual{field.template operator()<std::uint8_t>(0)!=0,
        {field.template operator()<std::uint8_t>(8)!=0,field.template operator()<std::uint64_t>(0x10),
         field.template operator()<std::uint64_t>(0x18),field.template operator()<std::uint64_t>(0x20),
         field.template operator()<std::uint64_t>(0x28),field.template operator()<std::uint64_t>(0x30),
         field.template operator()<float>(0x38)},field.template operator()<std::uint8_t>(0x40)!=0};
    return actual==expected.state;
}
}
