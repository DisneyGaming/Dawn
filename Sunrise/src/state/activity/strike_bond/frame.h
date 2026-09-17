#pragma once
#include "bindings.h"
#include "forest_binding.h"
#include "tethers.h"
#include "../coo/campaign_scan.h"
#include "boss_cycle.h"
#include "ending_flow.h"
#include "../coo/lifecycle_service.h"
#include "../coo/object_service.h"
#include "../coo/objective_service.h"
#include <bitset>
#include <optional>

namespace sunrise::state::activity::strike_bond {
// Forest C's encounter condition tables use these four faction selectors;
// their authored weighted groups select the actual inhabitants of each island.
struct HashSwitch {std::uint32_t key,value;};
inline constexpr HashSwitch kForestHashSwitches[]{
    {0x67AF9045U,0x050C5D2EU},{0x0D979BCDU,0x050C5D2EU},
    {0xAD3780EEU,0x050C5D2EU},{0x89567586U,0x050C5D2EU},
};
struct EnemyReceipt {
    std::uint64_t run{};std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept {return run && generation && registry && actor!=UINT32_MAX && owner!=UINT32_MAX;}
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
struct LensReceipt {
    coo::Generation owner{};coo::Asset asset{};std::uintptr_t source{};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},health{UINT32_MAX};
    bool valid() const noexcept {return owner.valid() && asset.registry && source>=0x10000 && entity!=UINT32_MAX && serial!=UINT32_MAX && health!=UINT32_MAX;}
    friend bool operator==(const LensReceipt&,const LensReceipt&)=default;
};
struct NativeState {std::uint32_t generation{};float position{};bool managed{},desired{},prepared{},active{},acknowledged{};};
struct SceneCommand {std::uint32_t generation{};bool stop{};std::uint8_t eventCount{};std::array<std::uint32_t,32> events{};};
struct Frame {
    EndingFlow endingFlow{};
    bool campaign{};coo::CampaignScan scan{};
    bool enabled{},checked{},finished{},ending{},coverEnabled{},populationFault{},restricted{},bossFighting{},bossDead{},forestGenerated{};
    std::uint8_t bossStage{},section{},activeRow{coo::kNoDialogue},musicCandidate{UINT8_MAX};
    /** Frozen source-template selector copied from the exact launch activity. */
    std::uint8_t enemyVariant{};
    std::uint32_t spawnGeneration{},revision{},objective{},generatorSeed{},checkpointSpawnSet{};
    int region{-1},checkpointSliceSet{-1};std::uint64_t gameplayClockTicks{},endEpoch{};
    std::array<std::uint32_t,std::size(kDialogueRows)> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<std::uint8_t,std::size(kSpawns)> taskPlusOne{};
    std::array<SceneCommand,std::size(kScenes)> scenes{};
    std::bitset<std::size(kLenses)> lensExposed{},lensDestroyed{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
    BossCycle bossCycle{};bool bossPlatformSnap{},bossPlatformAccepted{};
};
// Ordered reconstruction candidates from the issue report. Native score ordinals
// were not recovered; these values require listening verification in a fresh run.
inline constexpr std::uint8_t kMusicAbsent=UINT8_MAX,kMusicLighthouse=2,kMusicForest=4,kMusicPast=6,
    kMusicSpire=8,kMusicBossIntro=10,kMusicBossSecond=12,kMusicBossThird=13,kMusicBossDead=15;
inline constexpr std::uint8_t music_for_region(int region) noexcept {
    return region==120?kMusicLighthouse:region==80?kMusicForest:region==8?kMusicPast
        :region==136?kMusicSpire:kMusicAbsent;
}
inline bool raise_music(Frame& f,std::uint8_t candidate) noexcept {
    if(candidate>=128 || (f.musicCandidate!=kMusicAbsent && candidate<=f.musicCandidate)) return false;
    f.musicCandidate=candidate;return true;
}
inline constexpr auto kObjectBindings=[] {
    std::array<coo::ObjectBinding,[] {std::size_t n{};for(const auto& a:kAssets) if(a.asset.type==4) ++n;return n;}()> out{};
    std::size_t i{};for(const auto& a:kAssets) if(a.asset.type==4) out[i++]={a.asset,{},0.F,true};return out;
}();
inline constexpr std::size_t object_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<kObjectBindings.size();++i) if(kObjectBindings[i].source==a) return i;
    return kObjectBindings.size();
}
struct Cohort {std::uint32_t registry;std::uint16_t source;std::uint8_t count;bool required;};
inline constexpr auto kCohorts=[] {
    std::array<Cohort,std::size(kSpawns)> out{};
    // Each authored category requests one actor. Keep the exact admitted death
    // ledger while ignoring surplus, non-owned actors from optional populations.
    for(std::size_t i=0;i<out.size();++i) out[i]={kSpawns[i].asset.registry,kSpawns[i].asset.slot,kSpawns[i].count,false};
    return out;
}();
/**
 * Barrier devices whose authored position runs the other way round.
 *
 * Type-23 position is the authored motion value, not a presence flag: what 0 and 1 mean belongs to
 * each object's own behaviour graph, which is why deep_storage carries a table like this one and
 * why native initialisation leaves every device at position 0. The security-module barriers author
 * it inverted - 0 is the barrier standing, 1 retracts it. Driving it like the Tree of
 * Probabilities shield walls (1 present, 0 removed) cleared the barrier on arrival and put it back
 * the instant the lens died, so breaking the cube placed a block around the transporter instead of
 * opening it. Confirmed live: the four barrier devices of pf_block[0] were driven to 0.0 at
 * t=215656, 62 ms after `lens_destroyed value=65`, which is when the block appeared.
 *
 * The lens device of the same prefab is deliberately absent: it carries the three-valued
 * shielded / exposed / retired presentation (1 / .75 / 0) and that mapping already works - the
 * lenses expose and break correctly.
 */
[[nodiscard]] inline bool inverted_barrier(coo::Asset a) noexcept {
    if(a.type!=23) { return false; }
    // Only d_vex_block, the barrier body itself. Its three neighbours in the prefab - d_laser_in,
    // d_laser_out and d_shield - use the ordinary sense, 1 present and 0 removed: inverting them
    // too turned the shield off on arrival and raised it when the block was destroyed, which is
    // backwards. The barrier is the one device in this prefab authored the other way.
    // pf_block[0..4] is an eight-slot prefab repeating from 65; d_vex_block is base+4.
    if(a.registry==0xC95ECB1AU && a.slot>=69 && a.slot<=101 && (a.slot-65)%8==4) { return true; }
    // pf_tower_block, the same prefab in the Spire: lens 263, d_vex_block 264.
    if(a.registry==0x2CB86C0FU && a.slot==264) { return true; }
    return false;
}
[[nodiscard]] inline constexpr bool animated_cover(coo::Asset a) noexcept {
    // The roof's four eight-block layouts must interpolate between endpoints.
    // Native DF6C70 skips the movement-start branch when snap is enabled.
    return a.registry==0x2CB86C0FU && a.type==23 && a.slot>=76 && a.slot<=169 && (a.slot-76)%3==0;
}
inline constexpr coo::Asset kProbabilityTreeDevice{0xC80A735BU,0x80F474AEU,23,11};
[[nodiscard]] inline constexpr bool animated_position(coo::Asset a) noexcept {
    // Dendron's platform graph80F4598F samples its motion from device_position.
    // Snapping device173 to1 skips the authored60-second phase transition.
    // Campaign ending platforms use 80C22861: increasing position starts the
    // phase-in effect that reveals their solid meshes. Snap skips that effect.
    const bool endingPlatform=a.registry==0xB9395B1BU && a.type==23 && a.slot<=8;
    return a==kProbabilityTreeDevice || endingPlatform || animated_cover(a)
        || (a.registry==0x2CB86C0FU && a.type==23 && a.slot==173);
}
inline float device_position(const Frame& f,coo::Asset a) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets)) return 0.F;
    // 80F4ACA0 authors three phases: 0 is hidden, .5 reveals the tree,
    // and 1 shuts it down. The reveal must interpolate through its effect cue.
    // Confirmed in the campaign live test; an on/off 1/0 mapping skipped it.
    if(a==kProbabilityTreeDevice) return !f.native[i].managed?0.F:f.native[i].active?.5F:1.F;
    if(inverted_barrier(a)) { return f.native[i].active?0.F:1.F; }
    const auto l=lens_index(a);
    // Both 80F48031 (guardians) and 80F56863 (Dendron) have these native ranges.
    if(l<std::size(kLenses) && a.type==23) return f.lensDestroyed[l] || !f.native[i].active?0.F:f.lensExposed[l]?.75F:1.F;
    return f.native[i].position;
}
// Exact prefab linkage recovered from the native selectors. Type 26 is the
// actor shield effect; type 34 resolves its one authored Minotaur source.
struct GolemBinding {std::uint32_t registry;std::uint16_t source,tether,collection;std::size_t lens;};
inline constexpr GolemBinding kGolems[]{
    {0xC95ECB1AU,105,109,167,5},{0xC95ECB1AU,121,125,168,6},
    {0x2CB86C0FU,174,178,294,8},{0x2CB86C0FU,182,186,295,9},
    {0x2CB86C0FU,190,194,296,10},{0x2CB86C0FU,198,202,297,11},
    {0x2CB86C0FU,206,210,298,12},{0x2CB86C0FU,214,218,299,13},
    {0x2CB86C0FU,244,248,274,14},
};
inline constexpr const GolemBinding* golem(std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    for(const auto& g:kGolems) if(g.registry==key && ((type==26 && g.tether==slot) || (type==34 && g.collection==slot))) return &g;
    return nullptr;
}
inline constexpr bool route_guardian(coo::Asset source) noexcept {
    return source.type==1 && ((source.registry==0xC95ECB1AU && (source.slot==105 || source.slot==121))
        || (source.registry==0x2CB86C0FU && source.slot==244));
}
// Stage is the number of completed guardian pairs, independent of Lua graph index.
inline float boss_floor(const Frame& f) noexcept {return f.bossStage==0?2.F/3.F:f.bossStage==1?1.F/3.F:0.F;}
inline bool boss_blocked(const Frame& f,float fraction) noexcept {
    // Dying must remain eligible for the authored death clip's native kill event.
    return !f.bossFighting || f.bossDead || f.bossCycle.mode==BossMode::opening
        || f.bossCycle.mode==BossMode::parking || f.bossCycle.mode==BossMode::dormant
        || (f.bossStage<2 && fraction<=boss_floor(f));
}
struct Request {coo::Generation owner{};Frame frame{};};
inline constexpr coo::Asset kBossPlatform{0x2CB86C0FU,0x80F5493BU,4,172};
struct BossRequest {coo::Generation owner{};EnemyReceipt enemy{};Frame frame{};coo::ObjectReceipt platform{};};
struct LensRequest {coo::Generation owner{};LensReceipt lens{};std::uint32_t generation{};std::size_t index{};bool enabled{},vulnerable{},destroyed{};};
inline std::optional<float> tether_visibility(const TetherBinding& b,const Request& current,
    const LensRequest& lens,coo::Generation owner) noexcept {
    if(!owner.valid() || current.owner!=owner || lens.owner!=owner || lens.index!=b.lens
        || !current.frame.enabled) return std::nullopt;
    const auto& source=current.frame.native[asset_index(b.source)];
    if(!source.managed) return std::nullopt;
    return source.active && source.desired && lens.enabled && !lens.destroyed && !current.frame.bossDead?1.F:0.F;
}
}
