#pragma once
#include "native_activity_clock.h"
#include "../../../state/activity/coo/executor.h"
#include "../../../middleware/bap/activity_message/native/capture_controller_authority.h"
#include <cstring>
#include <span>

namespace sunrise::server::runtime::activity::capture_feedback {
namespace wire=middleware::bap::activity_message::native::capture_controller;
namespace coo=state::activity::coo;
inline constexpr std::uint32_t kTickRva=0x1006F20;
inline constexpr std::size_t kSourceBytes=0x448,kControllerBytes=0x1D0;
enum class RunIdentity : std::uint8_t { activityIncarnation, activitySession };
[[nodiscard]] constexpr bool valid(RunIdentity value) noexcept {
    return value==RunIdentity::activityIncarnation || value==RunIdentity::activitySession;
}
[[nodiscard]] inline bool token_matches(const activity_clock::Domain& domain,
    const coo::Token& token,RunIdentity identity) noexcept {
    if(!domain || !valid(identity) || !token.incarnation)return false;
    const auto expected=identity==RunIdentity::activitySession
        ? domain.owner.sessionId : domain.owner.incarnation.value;
    return expected && token.run==expected;
}
struct Ticket final {
    activity_clock::Domain domain{};
    coo::Token token{};
    coo::Asset source{};
    std::uint32_t generation{},entityDefinition{},controllerDefinition{};
    std::int64_t sourceDefinitionOffset{},controllerDefinitionOffset{};
    activity_clock::wire::Configuration clockConfiguration{};
    wire::State requested{};
    std::uint64_t armEpoch{};
    RunIdentity runIdentity{RunIdentity::activityIncarnation};
};
[[nodiscard]] inline bool same(const Ticket& a,const Ticket& b) noexcept {
    return a.domain==b.domain && a.token==b.token && a.source==b.source
        && a.generation==b.generation && a.entityDefinition==b.entityDefinition
        && a.controllerDefinition==b.controllerDefinition && a.sourceDefinitionOffset==b.sourceDefinitionOffset
        && a.controllerDefinitionOffset==b.controllerDefinitionOffset
        && a.clockConfiguration.field0==b.clockConfiguration.field0
        && std::bit_cast<std::uint32_t>(a.clockConfiguration.timing)==std::bit_cast<std::uint32_t>(b.clockConfiguration.timing)
        && a.requested==b.requested && a.armEpoch==b.armEpoch && a.runIdentity==b.runIdentity;
}
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return static_cast<bool>(t.domain) && token_matches(t.domain,t.token,t.runIdentity)
        && t.source.registry && t.source.definition && t.source.type==4 && t.source.slot<=32767
        && t.generation && t.generation<=0x7FFFFFFF && t.entityDefinition && t.controllerDefinition
        && t.sourceDefinitionOffset>0 && t.controllerDefinitionOffset>0 && t.armEpoch
        && activity_clock::wire::valid(t.clockConfiguration) && t.clockConfiguration.timing>0
        && wire::valid(t.requested) && t.requested.active && t.requested.clock.running
        && t.requested.clock.rate>0;
}
struct Capture final {
    // Copied before original1006F20 begins; never reacquire a current ticket to
    // qualify a delayed callback from an earlier owner or command.
    Ticket ticket{};
    std::span<const std::byte> source,sourceDefinition,entity,scene,before,after,context;
    // sourceHandle is the indexed instance handle at source+20. authorityHandle
    // is the common component descriptor at source+160; it resolves back to the
    // source, not to a separate authority metadata object.
    std::uint32_t sourceHandle{UINT32_MAX},authorityHandle{UINT32_MAX};
    std::uint32_t entityHandle{UINT32_MAX},sceneHandle{UINT32_MAX},controllerHandle{UINT32_MAX};
    // Full salted context handles from the source's native lookup before/after.
    std::uint32_t sourceClockContext{UINT32_MAX},controllerClockContext{UINT32_MAX};
    std::uint32_t producerRva{};
    std::uint64_t sequence{};
};
struct Observation final {
    Ticket ticket{};
    std::uint64_t sequence{};
    std::uint32_t sourceHandle{},entityHandle{},controllerHandle{},clockContext{};
    float progress{},remainingSeconds{};
    bool completed{};
};
template<class T> [[nodiscard]] T field(std::span<const std::byte> bytes,std::size_t at) noexcept {
    T out{};if(at<=bytes.size() && sizeof out<=bytes.size()-at)std::memcpy(&out,bytes.data()+at,sizeof out);return out;
}
[[nodiscard]] inline wire::State state(std::span<const std::byte> c) noexcept {
    return {field<std::uint8_t>(c,0x30)!=0,
        {field<std::uint8_t>(c,0x38)!=0,field<std::uint64_t>(c,0x40),field<std::uint64_t>(c,0x48),
         field<std::uint64_t>(c,0x50),field<std::uint64_t>(c,0x58),field<std::uint64_t>(c,0x60),field<float>(c,0x68)},
        field<std::uint8_t>(c,0x70)!=0};
}
// Pure qualification only. This module does not poll, patch, call native code,
// start a timer, or transform elapsed wall time into a completion receipt.
[[nodiscard]] inline bool qualify(const Ticket& expected,const Capture& c,Observation& out) noexcept {
    if(!valid(expected) || !same(expected,c.ticket) || c.producerRva!=kTickRva || !c.sequence
        || c.source.size()!=kSourceBytes || c.sourceDefinition.size()!=0x70 || c.entity.size()!=0xC0
        || c.scene.size()!=0x10 || c.before.size()!=kControllerBytes || c.after.size()!=kControllerBytes
        || c.context.size()!=0x80 || c.sourceHandle==UINT32_MAX || c.authorityHandle==UINT32_MAX
        || c.entityHandle==UINT32_MAX || c.sceneHandle==UINT32_MAX || c.controllerHandle==UINT32_MAX
        || c.sourceClockContext==UINT32_MAX || c.sourceClockContext!=c.controllerClockContext)return false;
    if(field<std::uint32_t>(c.source,0)!=expected.source.definition || field<std::uint32_t>(c.source,4)!=0x80809928
        || field<std::int64_t>(c.source,8)!=expected.sourceDefinitionOffset
        || field<std::uint32_t>(c.source,0x20)!=c.sourceHandle
        || field<std::uint32_t>(c.source,0x160)!=c.authorityHandle || field<std::uint32_t>(c.source,0x164)!=0x80809927
        || field<std::uint32_t>(c.source,0x180)!=expected.generation || field<std::uint8_t>(c.source,0x188)!=1
        || field<std::uint32_t>(c.source,0x2F0)!=expected.generation
        || field<std::uint32_t>(c.source,0x444)!=c.entityHandle
        || field<std::uint32_t>(c.source,0x440)!=field<std::uint32_t>(c.entity,0)
        || field<std::uint32_t>(c.entity,0xC)!=c.entityHandle || (field<std::uint32_t>(c.entity,4)&4)
        || field<std::uint32_t>(c.entity,0x4C)!=c.sceneHandle
        || field<std::uint32_t>(c.scene,4)!=expected.entityDefinition)return false;
    if(field<std::uint32_t>(c.sourceDefinition,0)!=expected.source.definition
        || field<std::uint32_t>(c.sourceDefinition,4)!=0x80809927
        || field<std::uint32_t>(c.sourceDefinition,0x30)!=expected.source.registry
        || field<std::uint16_t>(c.sourceDefinition,0x34)!=4
        || field<std::uint16_t>(c.sourceDefinition,0x36)!=expected.source.slot
        || field<std::uint32_t>(c.sourceDefinition,0x48)!=0x8080992F
        || field<std::uint32_t>(c.sourceDefinition,0x38)!=expected.domain.bubble)return false;
    const auto& b=c.before;const auto& a=c.after;
    if(std::memcmp(b.data(),a.data(),0x30)!=0 || field<std::uint32_t>(a,0)!=expected.controllerDefinition
        || field<std::uint32_t>(a,4)!=0x80804FCB || field<std::int64_t>(a,8)!=expected.controllerDefinitionOffset
        || field<std::uint32_t>(a,0x24)!=c.controllerHandle || field<std::uint32_t>(a,0x2C)!=c.entityHandle
        || state(b)!=expected.requested || state(a)!=expected.requested)return false;
    if(field<std::uint8_t>(c.context,0x10)!=1 || field<std::uint32_t>(c.context,0x48)!=expected.domain.scenario
        || (field<std::uint8_t>(c.context,0x14)!=0)!=expected.clockConfiguration.field0
        || field<std::uint32_t>(c.context,0x18)!=std::bit_cast<std::uint32_t>(expected.clockConfiguration.timing)
        || field<std::uint64_t>(c.context,0x68)==UINT64_MAX)return false;
    const float progress=field<float>(a,0x1B8),remaining=field<float>(a,0x1BC);
    if(!std::isfinite(progress) || !std::isfinite(remaining) || progress<0 || progress>1 || remaining<0)return false;
    const bool completed=progress==1.0F && field<std::uint8_t>(b,0x79)==0 && field<std::uint8_t>(a,0x79)==1;
    out={expected,c.sequence,c.sourceHandle,c.entityHandle,c.controllerHandle,c.sourceClockContext,progress,remaining,completed};
    return true;
}
}
