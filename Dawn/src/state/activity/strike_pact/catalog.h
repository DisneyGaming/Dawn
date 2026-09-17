// Tree of Probabilities (strike_pact) native identities. Recovered from the installed
// scenario 80F54AE7 through the Dawn activity SDK export and the live roster
// publications of the accepted Dawn host build (registry keys per object tag).
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "../coo/dialogue_service.h"
#include "../coo/native_combatant_authority.h"
namespace dawn::state::activity::strike_pact {
inline constexpr std::uint32_t kScenario=0x80F54AE7U;
// Scenario root presentation group (object 80F55229): directive 68/0, music 11/1, dialogue 53/2.
inline constexpr std::uint32_t kRoot=0x361407B2U, kRootTag=0x80F55229U;
// Lighthouse opening mission group (object 80F551DE).
inline constexpr std::uint32_t kOpening=0xCC7A090DU, kOpeningTag=0x80F551DEU;
// Lighthouse teleporter group (object 80F551EB): teleport 4/0.
inline constexpr std::uint32_t kTeleport=0x75A69C5AU, kTeleportTag=0x80F551EBU;
// Lighthouse area group (object 80F55205): tunnel triggers, forest portal point 47/8.
inline constexpr std::uint32_t kLighthouse=0xF8D0DADCU, kLighthouseTag=0x80F55205U;
inline constexpr int kRegion=120;
inline constexpr std::uint8_t kBubble=15;
inline constexpr std::uint32_t kOpeningSpawnSet=0x0E1523FEU;
// The dialogue bank handle is validated at dispatch time from the native component; the
// adapter accepts the bank the strike's own type-53 component reports.
inline constexpr std::uint32_t kBank=0x80F1FFB6U;

struct Point final { float x,y,z; };
struct Volume final { std::uint32_t registry; std::uint16_t slot; std::string_view name; Point min,max; };
// Type-60 trigger boxes from the SDK export (authored bounds; the SDK carries no polygon).
inline constexpr Volume kVolumes[]{
    {kOpening,73,"pt_initial_spawns",{76.6999969F,78.0335388F,73.0999985F},{142.272583F,447.930908F,113.099998F}},
    {kOpening,71,"pt_near_gate",{226.400009F,126.124161F,68.3000031F},{366.443848F,369.668823F,108.300003F}},
    {kLighthouse,10,"pt_see_gate",{239.523392F,189.994949F,68.F},{408.654327F,319.5F,108.000008F}},
    {kLighthouse,5,"pt_enter_tunnel",{-828.385193F,-597.70929F,-12.7609653F},{-792.50885F,-569.332642F,7.23903608F}},
    {kLighthouse,11,"pt_teleported",{-849.F,-553.792114F,-24.F},{-760.367981F,-374.419037F,-4.F}},
};

// One authored squad source. Counts are the SDK member categories; the second category is
// the named member (sq_cabal_anchor's Gladiator). Cohorts are the accepted Dawn stages.
struct Spawn final {
    std::uint16_t source; std::uint32_t registry; std::uint8_t loose,second,count,categories,cohort;
    bool required; std::int8_t tacticalRow; std::uint16_t tacticalSlot; std::string_view name;
};
inline constexpr std::uint8_t kLastCohort=3;
// count = loose + second: the actors the shared population ledger expects per source.
inline constexpr std::array<Spawn,13> kSpawns{{
    {1,kOpening,1,0,1,1,1,false,-1,0,"sq_intro_fight[0]"},
    {2,kOpening,1,0,1,1,1,false,-1,0,"sq_intro_fight[1]"},
    {4,kOpening,1,1,2,2,2,false,-1,0,"sq_cabal_vanguard[0]"},
    {5,kOpening,1,0,1,1,2,false,-1,0,"sq_cabal_vanguard[1]"},
    {6,kOpening,1,1,2,2,2,false,-1,0,"sq_cabal_vanguard[2]"},
    {17,kOpening,1,0,1,1,2,false,-1,0,"sq_mixed_center[0]"},
    {21,kOpening,1,0,1,1,2,false,-1,0,"sq_mixed_center[4]"},
    {24,kOpening,1,1,2,2,2,false,-1,0,"sq_mixed_center[7]"},
    {11,kOpening,1,1,2,2,3,true,-1,0,"sq_cabal_rearguard[0]"},
    {13,kOpening,1,1,2,2,3,true,-1,0,"sq_cabal_rearguard[2]"},
    // The turret's placement is native; its unreliable spawn never blocks the gate.
    {16,kOpening,1,0,1,1,3,false,-1,0,"sq_basilisk"},
    {25,kOpening,1,0,1,1,3,true,-1,0,"sq_vex_harassers[0]"},
    // Named defender: zero loose members, one named Gladiator bound through the type-2
    // sensor (slot 15), bootstrapped into the authored staircase task row 4 of obj 3/0.
    {14,kOpening,0,1,1,2,3,true,4,0,"sq_cabal_anchor"},
}};
static_assert([] { for(const auto& s:kSpawns) { if(s.count!=s.loose+s.second) { return false; } } return true; }());
inline constexpr std::uint16_t kAnchorSource=14, kGladiatorSensor=15, kShieldWall=27, kAudience=64;
inline constexpr std::uint16_t kObjectiveSlot=0; // obj_pact_lighthouse, type 3
[[nodiscard]] constexpr const Spawn* spawn(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& s:kSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr;
}
[[nodiscard]] constexpr std::uint8_t expected_actors(const Spawn& s) noexcept { return s.count; }
[[nodiscard]] constexpr coo::native_combatant::TacticalGroup tactical_group(const Spawn& s) noexcept {
    if(s.tacticalRow<0) { return {}; }
    return {s.registry,s.tacticalSlot,s.tacticalRow};
}
struct EnemyReceipt final {
    std::uint64_t run{}; std::uint32_t actor{},owner{},generation{}; std::uint16_t source{}; std::uint32_t registry{};
    // The registry is not pinned here. The strike drives five authored objects across four
    // regions, and the only table that knows which (registry, source) pairs are its own is the
    // combined kAllSpawns, which is declared after this type. Pinning the opening's registry made
    // every Forest, Chase, ledge and boss actor unadmittable, so none of those cohorts could ever
    // report ready or cleared and the run stalled at the first Forest kill gate. The real filter
    // is PopulationService::admit, which refuses any receipt whose source is not an enabled row
    // of the catalog it is given.
    bool valid() const noexcept { return run!=0 && actor!=UINT32_MAX && owner!=UINT32_MAX && generation!=0 && registry!=0; }
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};

// Directive bank events (SDK mission.Directive name hashes).
inline constexpr std::uint32_t kApproachGateway=0xDA162298U, kTraverseForest=0x489890F3U, kTrackCabal=0x97DFF425U,
    kDisableBarrier=0xEA50F954U, kEliminateHostiles=0x3D6350FAU, kMountUp=0xE133A090U, kFindLeader=0x574D4C17U,
    kFindMap=0xF611984AU, kEvadeVex=0x59365CA0U, kDefeatThuun=0xD1ECAA7BU, kAccessMap=0xF52F2E37U;
inline constexpr std::uint32_t kObjectives[]{kApproachGateway,kTraverseForest,kTrackCabal,kDisableBarrier,kEliminateHostiles,
    kMountUp,kFindLeader,kFindMap,kEvadeVex,kDefeatThuun,kAccessMap};

// Dialogue rows (SDK mission.DialogueDefinition selectors). Durations are not exported by the
// SDK; a uniform window keeps the shared spacing arbitration conservative.
inline constexpr std::uint32_t kRowMs=7000;
inline constexpr coo::DialogueRow kDialogueRows[]{
    {0xB35F543CU,kRowMs,0,false}, // row 0 Ikora: Cabal loose in the Forest
    {0x94358F13U,kRowMs,0,false}, // row 1 Red Legion survivors
    {0,0,0,false},                // row 2
    {0,0,0,false},                // row 3
    {0xC04A5765U,kRowMs,0,false}, // row 4 tunnel
    {0x6A30D732U,kRowMs,0,false}, // row 5 forest arrival
    {0xC18E182AU,kRowMs,0,false}, // row 6 first Forest area
    {0,0,0,false},{0,0,0,false},{0,0,0,false}, // rows 7-9
    {0x81D654A2U,kRowMs,0,false}, // row 10 chase entry
    {0x110D4DBFU,kRowMs,0,false}, // row 11 mount up
    {0,0,0,false},                // row 12
    {0x4B4B0C95U,kRowMs,0,false}, // row 13 sparrow jump
    {0,0,0,false},{0,0,0,false},  // rows 14-15
    {0x5276621BU,kRowMs,0,false}, // row 16
    {0x274C8AD6U,kRowMs,0,false}, // row 17 ledge
    {0x52AE09A9U,kRowMs,0,false}, // row 18 ledge final
    {0xA40140B4U,kRowMs,0,false}, // row 19 see tree
    {0x84D90F6BU,kRowMs,0,false}, // row 20 reveal
    {0xCE4E6824U,kRowMs,0,false}, // row 21
    {0xE5B7C166U,kRowMs,0,false}, // row 22 room 1
    {0,0,0,false},                // row 23
    {0xE10541E0U,kRowMs,0,false}, // row 24 room 2
    {0x56B56FBCU,kRowMs,0,false}, // row 25
    {0xED18D16AU,kRowMs,0,false}, // row 26
    {0xD1DD2887U,kRowMs,0,false}, // row 27 Thuun dead
    {0xB61406F2U,kRowMs,0,false}, // row 28 finale
    {0x8C046411U,kRowMs,0,false}, // row 29
};
inline constexpr coo::DialogueDefinition kDialogue{kBank,kDialogueRows,{}};
} // namespace dawn::state::activity::strike_pact
