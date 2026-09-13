// Tree of Probabilities (strike_pact) presentation identities: everything the mission says,
// shows and remembers across all four held regions. Recovered from the scenario 80F54AE7 SDK
// export (mission.DialogueDefinition, mission.Directive, mission.Slot, mission.states) and from
// the proven mission graph that already runs this strike (scripts strike_pact.lua,
// strike_pact/route.lua, strike_pact/boss.lua, strike_pact/music.lua).
//
// The opening catalog in catalog.h owns the same dialogue rows and directive hashes for the
// region-120 slice alone. This header restates them for the whole mission in a nested namespace
// so both can be included in one translation unit; the static_asserts below pin the two together
// so a later edit to either cannot drift.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "../coo/dialogue_service.h"
#include "../coo/objective_service.h"
#include "catalog.h"
namespace sunrise::state::activity::strike_pact::presentation {
// The section catalogs live in the parent namespace; these cross-checks name it.
namespace pact = sunrise::state::activity::strike_pact;

// ---------------------------------------------------------------------------------------------
// Publication surfaces. All three presentation components hang off the scenario root group.
// ---------------------------------------------------------------------------------------------
// Native reflected schemas and their fixed authority widths, read from the SDK slot records:
// the directive sensor is 4802 bits (80804F67), the dialogue sensor 19767 (80804F77), and the
// music sensor 7223 (80804F58) - the same selector body omega_music_authority.h already encodes.
inline constexpr coo::Asset kDirectiveAsset{kRoot,kRootTag,68,0};  // m_directive_sensor
inline constexpr coo::Asset kDialogueAsset{kRoot,kRootTag,53,2};   // m_dialog_sensor
inline constexpr coo::Asset kMusicAsset{kRoot,kRootTag,11,1};      // m_music_sensor
inline constexpr std::uint32_t kDirectiveSchema=0x80804F67U,kDialogueSchema=0x80804F77U,kMusicSchema=0x80804F58U;
inline constexpr std::size_t kDirectiveBits=4802,kDialogueBits=19767,kMusicBits=7223;

// ---------------------------------------------------------------------------------------------
// 1. Dialogue. mission.DialogueDefinition.M_DIALOG_SENSOR is a sparse row -> selector map; the
// runtime indexes rows directly, so unauthored rows stay present as zero-selector placeholders.
// A zero duration is what makes a placeholder unplayable: DialogueService::enqueue rejects it,
// which is why the placeholders need no separate "authored" flag.
// The export carries no clip lengths, so one conservative window is shared by every row; it is
// only the spacing arbitration's guess at when the voice line has finished.
// No row is sceneOwned: boss.lua plays rows 20 and 21 itself around the Minotaur scene rather
// than letting the scene drive them.
// ---------------------------------------------------------------------------------------------
inline constexpr std::uint32_t kRowMs=7000;
inline constexpr coo::DialogueRow kDialogueRows[]{
    {0xB35F543CU,22201U,0,false}, // 0  strike_pact.lua start_dialogue - opening, first pt_initial_spawns/pt_near_gate entry
    {0x94358F13U,9644U,0,false}, // 1  strike_pact.lua "pact.dialogue.gate" - opening, pt_near_gate
    {0,0,0,false},                // 2  unauthored
    {0,0,0,false},                // 3  unauthored
    {0xC04A5765U,11076U,0,false}, // 4  strike_pact.lua "pact.dialogue.tunnel" - opening, pt_enter_tunnel
    {0x6A30D732U,13067U,0,false}, // 5  strike_pact.lua "pact.dialogue.forest" - Forest, pt_begin
    {0xC18E182AU,13467U,0,false}, // 6  strike_pact.lua on_event_generator_state - Forest, first generated area completed
    {0,0,0,false},                // 7  unauthored
    {0,0,0,false},                // 8  unauthored
    {0,0,0,false},                // 9  unauthored
    {0x81D654A2U,9262U,0,false}, // 10 route.lua stage "enter_chase"
    {0x110D4DBFU,9258U,0,false}, // 11 route.lua firstroom_cleared
    {0,0,0,false},                // 12 unauthored
    {0x4B4B0C95U,2253U,0,false}, // 13 route.lua stage "sparrow_jump"
    {0,0,0,false},                // 14 unauthored
    {0,0,0,false},                // 15 unauthored
    {0x5276621BU,5823U,0,false}, // 16 authored; the proven graph never plays it
    {0x274C8AD6U,16540U,0,false}, // 17 route.lua route.initialize - ledge arrival (region 0)
    {0x52AE09A9U,3786U,0,false}, // 18 route.lua stage "ledge_final" (PM_LEDGE_FINAL)
    {0xA40140B4U,10497U,0,false}, // 19 route.lua stage "see_tree"
    {0x84D90F6BU,4121U,0,false}, // 20 boss.lua reveal - Minotaur scene activation
    {0xCE4E6824U,2368U,0,false}, // 21 boss.lua on_scene_finished
    {0xE5B7C166U,9319U,0,false}, // 22 boss.lua start_room1
    {0,0,0,false},                // 23 unauthored
    {0xE10541E0U,2646U,0,false}, // 24 boss.lua retreat into room 2
    {0x56B56FBCU,6612U,0,false}, // 25 boss.lua rooms[2].cue - the room-2 gauntlet
    {0xED18D16AU,2423U,0,false}, // 26 boss.lua rooms[3].cue - retreat into room 3
    {0xD1DD2887U,16010U,0,false}, // 27 boss.lua defeat
    {0xB61406F2U,3588U,0,false}, // 28 authored; the proven graph never plays it
    {0x8C046411U,16599U,0,false}, // 29 authored; the proven graph never plays it
};
inline constexpr std::size_t kDialogueRowCount=std::size(kDialogueRows);
// mission.DialogueCue maps CUE_n -> n, so a cue ordinal is already this array's index.
[[nodiscard]] constexpr bool authored_row(std::uint8_t row) noexcept {
    return row<kDialogueRowCount && kDialogueRows[row].selector!=0;
}
// The dialogue bank handle is not exported; it is validated at dispatch from the strike's own
// type-53 component, exactly as catalog.h's kBank does.
inline constexpr coo::DialogueDefinition kDialogue{kBank,kDialogueRows,{}};

// ---------------------------------------------------------------------------------------------
// 2. Directives. Every mission.Directive row is element 0 of one bank; its id "directive/0002767e"
// and its slot_row agree, so the bank tag is the row.
// ---------------------------------------------------------------------------------------------
inline constexpr std::uint32_t kDirectiveBank=0x0002767EU;
static_assert(kDirectiveBank==161406U); // mission.Directive slot_row, identical on all eleven rows
inline constexpr std::uint32_t kApproachGateway=0xDA162298U,kTraverseForest=0x489890F3U,kTrackCabal=0x97DFF425U,
    kDisableBarrier=0xEA50F954U,kEliminateHostiles=0x3D6350FAU,kMountUp=0xE133A090U,kFindLeader=0x574D4C17U,
    kFindMap=0xF611984AU,kEvadeVex=0x59365CA0U,kDefeatThuun=0xD1ECAA7BU,kAccessMap=0xF52F2E37U;
inline constexpr std::uint32_t kObjectives[]{kApproachGateway,kTraverseForest,kTrackCabal,kDisableBarrier,
    kEliminateHostiles,kMountUp,kFindLeader,kFindMap,kEvadeVex,kDefeatThuun,kAccessMap};
// Publication points, carried verbatim from the proven graph. Objective order follows the retail
// reference recording (route.lua): the Forest keeps Traverse until the Chase publishes Eliminate
// Hostiles; the Chase moves to Mount Up at the first-room clear and to Find the Cabal Leader on
// the sparrow run, which then holds through the traps and the ledge.
struct DirectiveUse final { std::uint32_t event; std::string_view symbol,where; };
inline constexpr DirectiveUse kDirectiveUses[]{
    {kApproachGateway,"APPROACH_THE_INFINITE_FOREST_GATEWAY","strike_pact.lua initialize_presentation, opening before the gate opens"},
    {kTraverseForest,"TRAVERSE_THE_INFINITE_FOREST","strike_pact.lua update_gate and pt_enter_tunnel; route.lua forest steps 2-3"},
    {kTrackCabal,"TRACK_THE_CABAL","strike_pact.lua initialize_forest, Forest arrival (audience only, no target)"},
    {kDisableBarrier,"DISABLE_THE_BARRIER","route.lua forest step 1 - see_portal trigger or the forest_end monitor"},
    {kEliminateHostiles,"ELIMINATE_HOSTILES","route.lua chase step 1 - Chase arrival and enter_chase"},
    {kMountUp,"MOUNT_UP_AND_MOVE_QUICKLY","route.lua chase step 2 - firstroom_cleared"},
    {kFindLeader,"FIND_THE_CABAL_LEADER","route.lua chase step 3 (sparrow_run/sparrow_jump) and bomb step 1 (ledge arrival)"},
    {kFindMap,"FIND_THE_MAP_OF_THE_INFINITE_FOREST","route.lua bomb step 2 (ledge_final); boss.lua on_player_trigger before the prefight clears"},
    {kEvadeVex,"EVADE_VEX_DEFENSES","boss.lua room 2, targeting ap_room3"},
    {kDefeatThuun,"DEFEAT_VALUS_THUUN","boss.lua show_objective at fight start and room 3"},
    // Authored for the terminal at the end of the strike; the proven graph never publishes it.
    {kAccessMap,"ACCESS_THE_MAP",{}},
};
static_assert(std::size(kDirectiveUses)==std::size(kObjectives));

// ---------------------------------------------------------------------------------------------
// 3. Music. Sections are candidate ordinals in the authored music sensor, taken from music.lua.
// music.select is monotonic - it drops any section at or below the one already published - so a
// replayed region or a reattach can never walk the score backwards.
// ---------------------------------------------------------------------------------------------
inline constexpr std::uint8_t kMusicForest=2,kMusicChaseFight=5,kMusicBombStart=8,kMusicBossIntro=10,
    kMusicBossRoom2=12,kMusicBossRoom3=14,kMusicBossDead=16;
struct MusicSection final { std::uint8_t candidate; std::uint16_t region; std::string_view name,where; };
inline constexpr MusicSection kMusicSections[]{
    {kMusicForest,72,"FOREST","route.lua route.initialize - Forest arrival"},
    {kMusicChaseFight,16,"CHASE_FIGHT","route.lua stage enter_chase"},
    {kMusicBombStart,0,"BOMB_START","route.lua route.initialize - ledge arrival"},
    {kMusicBossIntro,0,"BOSS_INTRO","boss.lua reveal"},
    {kMusicBossRoom2,0,"BOSS_ROOM2_FIGHT","boss.lua start_room 2"},
    {kMusicBossRoom3,0,"BOSS_ROOM3_FIGHT","boss.lua start_room 3"},
    {kMusicBossDead,0,"BOSS_DEAD","boss.lua defeat"},
};
[[nodiscard]] constexpr bool advances_music(std::int16_t published,std::uint8_t section) noexcept {
    return static_cast<std::int32_t>(section)>published; // music.lua: (variable or -1) >= section -> ignore
}
static_assert([] { std::int16_t last=-1; for(const auto& s:kMusicSections) { if(!advances_music(last,s.candidate)) { return false; } last=static_cast<std::int16_t>(s.candidate); } return true; }());
// UNRESOLVED: the music sensor's candidate name hashes. music.lua notes that these ordinals match
// choice hashes in the authored sensor, but the SDK export publishes only the M_MUSIC_SENSOR slot
// record - it carries no candidate table - so the hashes behind ordinals 2/5/8/10/12/14/16 are
// not recoverable here and only the ordinals are carried.

// ---------------------------------------------------------------------------------------------
// 4. Regions the mission holds. mission.states gives region_index, slice_set_index and the state
// hash; the group registry that publishes each region's engagement audience owns it. The state's
// own key ordinal is the bubble, and region_index is eight times that bubble in every exported
// row - the two are separate fields, not one derived from the other.
// A death respawns on the lifetime's spawn set, which otherwise stays the opening's: a bubble
// with no set of its own has no spawn location, so each region selects one on arrival (route.lua).
// ---------------------------------------------------------------------------------------------
struct Region final {
    std::string_view state,name;
    std::uint32_t stateHash,registry,tag,arrivalSpawnSet;
    std::uint16_t regionIndex,sliceSetIndex,audience;
    std::uint8_t bubble;
};
inline constexpr Region kRegions[]{
    // The opening never calls set_checkpoint: it keeps the lifetime's starting set, the one
    // catalog.h already records, which is why route.lua's comment about it holding over matters.
    {"STATE_80F54AE7_000F_0000_80F54AE2","opening",0x02F12469U,kOpening,kOpeningTag,kOpeningSpawnSet,120,120,64,15},
    {"STATE_80F54AE7_0009_0000_80F54ADC","forest",0xA522CBE0U,0x2763EC91U,0x80F550B8U,0xFEB62946U,72,72,28,9},
    // The Chase selects nothing on arrival; its first checkpoint waits for the enter_chase volume
    // so a fly-in cannot strand the respawn on a set the player has not reached yet.
    {"STATE_80F54AE7_0002_0000_80F54AD5","chase",0x95E28335U,0x588E5FB9U,0x80F54FFEU,0U,16,16,93,2},
    {"STATE_80F54AE7_0000_0000_80F54AD3","ledge_and_boss",0xDA8AE171U,0xA5F083B5U,0x80F54E07U,0x3360C6BEU,0,0,176,0},
};
static_assert([] { for(const auto& r:kRegions) { if(r.regionIndex!=r.sliceSetIndex || r.regionIndex!=8U*r.bubble) { return false; } } return true; }());
[[nodiscard]] constexpr const Region* region(std::uint16_t index) noexcept {
    for(const auto& r:kRegions) { if(r.regionIndex==index) { return &r; } } return nullptr;
}
// Every respawn set the proven graph selects, in the order a run reaches them. The hashes are
// mission decisions from route.lua and boss.lua; the SDK export contains no spawn-set table.
struct Checkpoint final { std::uint16_t region; std::uint32_t spawnSet; std::string_view where; };
inline constexpr Checkpoint kCheckpoints[]{
    {72,0xFEB62946U,"route.lua route.initialize - Forest entrance"},
    {72,0x54A5A748U,"route.lua stage see_portal - Forest exit"},
    {16,0xC803F1CCU,"route.lua stage enter_chase"},
    {16,0x4DA08186U,"route.lua firstroom_cleared"},
    {0,0x3360C6BEU,"route.lua route.initialize - ledge"},
    {0,0x23BAC4D9U,"boss.lua on_player_trigger - pt_reached_boss"},
    {0,0x61D967C9U,"boss.lua start_room 2"},
    {0,0x61D967C8U,"boss.lua start_room 3"},
};

// Where each authored cue is spoken. Every one of these is played at most once per run in the
// proven graph, so a re-entered volume or a reattach must not re-offer a row already spoken.
struct CuePoint final { std::uint8_t row; std::uint16_t region; std::string_view where; };
inline constexpr CuePoint kCuePoints[]{
    {0,120,"strike_pact.lua start_dialogue"},          {1,120,"strike_pact.lua pt_near_gate"},
    {4,120,"strike_pact.lua pt_enter_tunnel"},         {5,72,"strike_pact.lua pt_begin"},
    {6,72,"strike_pact.lua on_event_generator_state"}, {10,16,"route.lua enter_chase"},
    {11,16,"route.lua firstroom_cleared"},             {13,16,"route.lua sparrow_jump"},
    {17,0,"route.lua route.initialize - ledge"},       {18,0,"route.lua ledge_final"},
    {19,0,"route.lua see_tree"},                       {20,0,"boss.lua reveal"},
    {21,0,"boss.lua on_scene_finished"},               {22,0,"boss.lua start_room1"},
    {24,0,"boss.lua retreat to room 2"},               {25,0,"boss.lua room 2"},
    {26,0,"boss.lua retreat to room 3"},               {27,0,"boss.lua defeat"},
};
static_assert([] { for(const auto& c:kCuePoints) { if(!authored_row(c.row) || !region(c.region)) { return false; } } return true; }());

// ---------------------------------------------------------------------------------------------
// 5. Navigation targets. Every directive marker the proven graph publishes resolves to an
// authored type-47 point; the strike authors 21 of them and the graph aims at these eight.
// The only non-point target is the live boss actor SQ_BOSS_VAL_THOOUN (type 2, slot 47 of
// 0xA5F083B5), which boss.lua follows through his scene and room changes instead of a fixed
// position - it is deliberately absent from this table.
// ---------------------------------------------------------------------------------------------
inline constexpr std::uint16_t kNavPointType=47;
struct NavPoint final { std::uint32_t registry,tag; std::uint16_t slot; std::string_view name,use; };
inline constexpr NavPoint kNavPoints[]{
    {kLighthouse,kLighthouseTag,8,"ap_forest_portal","strike_pact.lua show_directive - the opening's gateway marker, dropped once through the tunnel"},
    {0xA9350228U,0x80F550C9U,5,"ap_forest_portal","route.lua forest default target"},
    {0x7E558786U,0x80F550DAU,7,"ap_forest_portal","route.lua stage leaving_forest - the far end of the exit tunnel"},
    {0x8E64DB66U,0x80F55029U,4,"ap_fight_room","route.lua chase target while Eliminate Hostiles is published"},
    {0xFBD01A06U,0x80F55018U,6,"ap_boss_room","route.lua chase default target"},
    {0x73CBF939U,0x80F54E12U,1,"ap_boss_room","route.lua bomb default target"},
    // The SDK never recovered aliases for these two; slot_0007 is ap_room1 and slot_0002 ap_room3.
    {0xC6F46FAFU,0x80F54E1DU,7,"slot_0007","boss.lua on_player_trigger - the room-1 approach"},
    {0xC6F46FAFU,0x80F54E1DU,2,"slot_0002","boss.lua Evade Vex defenses - the room-3 gauntlet"},
};
// The four locator words of an unused marker slot must carry the packaged FNV absent sentinel.
// Zero is a legal locator value, so a zeroed quad reads as a supplied position and feeds the
// native marker selector false origin candidates instead of leaving the slot empty.
inline constexpr std::array<std::uint32_t,4> kAbsentLocator{0x811C9DC5U,0x811C9DC5U,0x811C9DC5U,0x811C9DC5U};
[[nodiscard]] constexpr coo::MarkerTarget marker(const NavPoint& point) noexcept {
    return {{point.registry,point.tag,kNavPointType,point.slot},kAbsentLocator};
}
[[nodiscard]] constexpr const NavPoint* nav_point(std::uint32_t registry,std::uint16_t slot) noexcept {
    for(const auto& p:kNavPoints) { if(p.registry==registry && p.slot==slot) { return &p; } } return nullptr;
}
// coo::MarkerTarget::valid() is not constexpr, so its predicate is restated here rather than
// called: a published marker needs a real registry and a slot the native reference can encode.
static_assert([] { for(const auto& p:kNavPoints) { if(p.registry==0 || kNavPointType>=127 || p.slot>=32768) { return false; } } return true; }());

// The opening slice restated here must stay identical to catalog.h's.
static_assert(kRowMs==pact::kRowMs && kDialogueRowCount==std::size(pact::kDialogueRows));
static_assert([] { for(std::size_t i=0;i<kDialogueRowCount;++i) { if(kDialogueRows[i].selector!=pact::kDialogueRows[i].selector) { return false; } } return true; }());
static_assert(std::size(kObjectives)==std::size(pact::kObjectives));
static_assert([] { for(std::size_t i=0;i<std::size(kObjectives);++i) { if(kObjectives[i]!=pact::kObjectives[i]) { return false; } } return true; }());
static_assert(kRegions[0].regionIndex==pact::kRegion && kRegions[0].bubble==pact::kBubble
    && kRegions[0].audience==pact::kAudience && kRegions[0].arrivalSpawnSet==pact::kOpeningSpawnSet);
} // namespace sunrise::state::activity::strike_pact::presentation
