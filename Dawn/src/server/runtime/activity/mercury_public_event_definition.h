#pragma once
#include "mercury_public_event_registries.h"
#include "public_event_placement_adapter.h"

namespace dawn::server::runtime::activity::mercury::public_events {
namespace coo = state::activity::coo;
// A development-only native rally placement probe, not a scheduled public event.
// Its graph stays pending until a separately qualified placement-ready receipt.
inline constexpr coo::Asset kRallyFlag{0x85C38F77,0x80F5BF33,4,0};
inline constexpr coo::CommandSpec kRallyCommands[]{
    {coo::Operation::device,kRallyFlag,1,coo::Wait::nativeReady},
};
inline constexpr coo::Step kRallySteps[]{{"place_authored_rally_flag",0,kRallyCommands}};
inline constexpr coo::ReceiptBinding kRallyReceipts[]{{"rally.placement.ready",0,0}};
inline constexpr coo::Definition kRallyGraph{
    "public_event_rally_placement_probe",coo::Schema::otherMissions,kRallySteps,kRallyReceipts};
inline constexpr public_event::Definition kRallyProbeDefinition{
    "mercury.vex_crossroads.rally_probe",std::span(kRegistries).first(1),&kRallyGraph,
    nullptr,nullptr,nullptr,{}};
inline constexpr placement::Capability kRallyPlacement{&kRegistries[0],0};
// Payload80F5BF33 has one authored visual808099D8 at descriptor+0x520;
// native runtime header uses definition body+0x4C8 (80809928).
// The sole visual80BEF9E6 contains exactly one80804FB0 controller815ABCED.
// NativeF32820 starts it blocked; its80804FB1 setup has no scoped override. The source's
// reflected80804FB8 mode2 command enables the original interaction after the
// placement receipt. NativeF32CD0/F33480 retain eligibility and grant handling.
inline constexpr public_event::placement_feedback::Definition kRallyFeedback{0x4C8,1,true,0x815ABCED,0x388};
inline constexpr std::uint32_t kRallyEntity=0x80BEF9E6,kRallyInteraction=0x815ABCED,
    kRallyInteractionEvent=0xE882D22F,kRallyGrantPattern=0x80BEF944;

// Presentation table 80804F72, read from the installed packages. Native event
// selectors are known; their lifecycle/counter values are not inferred here.
struct DirectiveEvidence final { std::uint32_t event{},title{},description{},progress{}; };
inline constexpr std::uint32_t kCrossroadsDirectiveBank=0x80F5E35B;
inline constexpr std::uint32_t kRallyDirectiveBank=0x80FD3320;
inline constexpr std::uint32_t kIncomingEvent=0x76B10BE7;
inline constexpr std::array<DirectiveEvidence,8> kDirectives{{
    {0x00D1C5B9,0x50EFBC4D,0x290979DD,0x6267DC7C},
    {0x00D1C5BA,0x50EFBC4D,0x290979DE,0xD0A18DC4},
    {0xDA3C9201,0x50EFBC4D,0x290979DE,0xD0A18DC4},
    {0x0730D92A,0x50EFBC4D,0x290979DF,0x811C9DC5},
    {0x27252151,0x50EFBC4D,0x290979DF,0x811C9DC5},
    {0xB4C1948A,0x50EFBC4D,0x290979D8,0x811C9DC5},
    {0xA6D37988,0x50EFBC4D,0x290979D8,0x811C9DC5},
    {0x72636B56,0x50EFBC4D,0x1654946F,0x811C9DC5},
}};
struct SourceEvidence final {
    std::uint16_t slot{},primaryRule{},fallbackRule{};
    std::uint32_t redirect{},definition{};
};
// All 17 source definitions have typed 8080948F body at 1832. Rule references
// occur at +1984/+1992. This table deliberately has no counts or tactical rows.
// It cannot be passed to population::Service without a recovered policy/binding.
inline constexpr std::array<SourceEvidence,17> kSources{{
    {44,174,174,0x80F5E3A5,0x80F5E3A4},{45,174,174,0x80F5E3A8,0x80F5E3A7},
    {46,239,239,0x80F5E3AB,0x80F5E3AA},{47,239,239,0x80F5E3AE,0x80F5E3AD},
    {48,244,244,0x80F5E3B1,0x80F5E3B0},{49,246,246,0x80F5E3B4,0x80F5E3B3},
    {50,250,250,0x80F5E3B7,0x80F5E3B6},{51,251,251,0x80F5E3C5,0x80F5E3C4},
    {52,252,252,0x80F5E44C,0x80F5E44B},{53,253,253,0x80F5E44F,0x80F5E44E},
    {54,255,255,0x80F5E452,0x80F5E451},{55,254,254,0x80F5E455,0x80F5E454},
    {56,254,254,0x80F5E458,0x80F5E457},{57,172,172,0x80F5E45B,0x80F5E45A},
    {59,256,256,0x80F5E45E,0x80F5E45D},{60,132,132,0x80F5E461,0x80F5E460},
    {61,166,166,0x80F5E464,0x80F5E463},
}};
} // namespace dawn::server::runtime::activity::mercury::public_events
