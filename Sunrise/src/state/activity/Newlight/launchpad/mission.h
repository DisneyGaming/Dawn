#pragma once
#include "native_catalog.h"
#include <initializer_list>

namespace sunrise::state::activity::newlight::launchpad {
enum class Section : std::uint8_t { exterior,lights,breach,divide,hangar,count };
struct Checkpoint {std::uint32_t region,spawnSet;};
// Installed cosmo_launchpad spawn sets. Select only after reaching the section;
// this changes the lifetime's death/respawn target, never its travel handshake.
inline constexpr Checkpoint kCheckpoints[]{
    {24,0xEAEC2335U},{0,0x68935472U},{0,0x2EA8FB98U},{8,0x10FF7418U},{16,0x573574C2U}};
static_assert(std::size(kCheckpoints)==static_cast<std::size_t>(Section::count));
enum class Mechanic : std::uint32_t { next=1,arm,light,rifle,shotgun,rocket,ketch,scanShip,finish,ghostLights,ghostRifle,ghostDismiss,shutter,assault };
enum class Event : std::uint32_t { used=1,granted,shipFound,ghostLightsComplete,lightsOn,ghostAtLights,ketchStarted,assaultComplete,flareStarted,firstVandalNear };
inline constexpr std::uint32_t kAssaultTarget=13;
inline constexpr coo::Asset kRifle=asset(kBreach,4,5),kCache=asset(kBreach,4,6),kPowerCache=asset(kHangar,4,0);
inline constexpr coo::Asset kWalkerCache=asset(kDivide,4,12);
struct AmbushCue {std::uint16_t actor,source,sound;};
inline constexpr AmbushCue kAmbushCues[]{{33,32,78},{47,46,79},{35,34,81},{59,58,80}};
inline constexpr coo::Asset kFlare=asset(kKetch,4,2);
inline constexpr coo::Asset kPickups[]{kRifle,kCache,kPowerCache,kWalkerCache};
enum class Cohort : std::uint8_t { nests,firstVandal,firstFight,arena,cache,corridor,garage,garageExit,raiders,walker,hangar,backup,corridorRear,corridorMelee,hallDrop,garageDefender,corridorDefender,count };
struct Member {std::uint32_t registry;std::uint16_t slot;};
template<std::uint32_t Key,std::size_t N> consteval auto members(const std::uint16_t (&slots)[N]) {
    std::array<Member,N> out{};for(std::size_t i=0;i<N;++i) {out[i]={Key,slots[i]};}return out;
}
inline constexpr auto kNests=members<kBreach>({10,19});
inline constexpr auto kFirstVandal=members<kBreach>({29});
// The C wall/ceiling entrances share volume138. The next drop is at volume139;
// delaying either until a later crossing puts its request behind the player.
inline constexpr auto kFirstFight=members<kBreach>({30,32});
inline constexpr auto kHallDrop=members<kBreach>({34});
inline constexpr auto kArena=members<kBreach>({36,38});
inline constexpr auto kCacheFight=members<kBreach>({53});
inline constexpr auto kCorridor=members<kBreach>({41,46});
inline constexpr auto kGarage=members<kBreach>({54,58,60});
inline constexpr auto kGarageDefender=members<kBreach>({62});
inline constexpr auto kCorridorDefender=members<kBreach>({44});
inline constexpr auto kGarageExit=members<kBreach>({64});
// Tank-minion source 10 is an emission source, not a placed infantry squad.
inline constexpr auto kRaiders=members<kDivide>({6,7,8,9});
inline constexpr auto kWalker=members<kDivide>({1});
inline constexpr auto kHangarFight=members<kHangar>({8,9,10});
inline constexpr auto kBackup=members<kHangar>({11,12});
inline constexpr auto kCorridorRear=members<kBreach>({52});
inline constexpr auto kCorridorMelee=members<kBreach>({48,50});
struct CohortBinding {std::span<const Member> members;};
inline constexpr CohortBinding kCohorts[]{
    {kNests},{kFirstVandal},{kFirstFight},{kArena},{kCacheFight},{kCorridor},
    {kGarage},{kGarageExit},{kRaiders},{kWalker},{kHangarFight},{kBackup},{kCorridorRear},{kCorridorMelee},{kHallDrop},{kGarageDefender},{kCorridorDefender}};

struct GraphStorage {std::array<coo::CommandSpec,224> commands{};std::array<coo::Step,100> steps{};std::size_t commandCount{},stepCount{};};
struct Graph {
    coo::Definition definition{};
    void name(std::string_view) noexcept;
    std::uint32_t add(std::string_view,std::uint32_t,std::initializer_list<coo::CommandSpec>) noexcept;
private:friend struct Mission;GraphStorage* storage_{};std::size_t first_{};
};
struct Mission {
    GraphStorage storage{};std::array<Graph,static_cast<std::size_t>(Section::count)> phases{};
    Mission() noexcept;Mission(const Mission&)=delete;Mission& operator=(const Mission&)=delete;
    bool valid() const noexcept;
};
const Mission& mission() noexcept;
}
