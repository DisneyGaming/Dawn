#pragma once
#include <array>
#include <cstdint>

namespace sunrise::server::runtime::activity::mercury::faction_battle_evidence {
// Installed-package facts, not a registered gameplay capability. The incident
// outbound payload and owner-correlated display receipt need qualification.
inline constexpr std::uint32_t kScenario=0x80F4696A,kAnnouncementHash=0xA2DD920A;
inline constexpr std::uint32_t kStringContainer=0x80B9E37E,kEnglishStrings=0x80B34700;
inline constexpr std::uint16_t kStringOrdinal=25,kIncidentIndex=4852,kPresentationRow=287;
inline constexpr std::uint32_t kIncidentTable=0x80B9E5BF,kPresentationTable=0x80B9E5E3;
inline constexpr std::uint32_t kIncidentPayloadSchema=0x808087F1;
inline constexpr std::uint16_t kIncidentPresentationList=157;
inline constexpr std::uint32_t kAnnouncementRecipientPredicate=0x1124697D,kAnnouncementChannel=0x811C9DC5;
// Scenario+0x80 names this installed catalog. These hashes identify authored
// event families; they do not authorize activation or specify timer units.
inline constexpr std::uint32_t kPatrolCatalog=0x80F55240;
inline constexpr std::uint32_t kFullPublicEventFamily=0x68D50CE5,kSilentSkirmishFamily=0x22D671D8;
inline constexpr char kAnnouncementUtf8[]="The enemy is moving against each other\xE2\x80\xA6";
struct Lead final {std::uint32_t object{},registry{};std::uint8_t bubble{},descriptors{};};
inline constexpr std::array<Lead,3> kChestSkirmishLeads{{
    {0x80F5BFA1,0x0F075D5A,15,25},{0x80F5BFF4,0x0F075D59,15,25},{0x80F5E047,0x0F075D58,15,25}
}};
// These own a chest, one boss source, one fodder source and one resistance
// source. Catalog 80F55240 rows 1/2 group their keys with other silent skirmishes.
// Their six source variants repeat the same weighted minor/major/miniboss
// templates. No recovered controller ties them to the announcement or proves
// the user's escalating faction battle. Do not register them as that encounter.
enum class Gate : std::uint8_t {
    encounterIdentity,opposingCohorts,nativeHostility,waveConditions,
    nativeAnnouncementDelivery,schedulingAndBudget,completionAndRetirement,sourceRenewal
};
inline constexpr std::array<Gate,8> kUnresolved{
    Gate::encounterIdentity,Gate::opposingCohorts,Gate::nativeHostility,Gate::waveConditions,
    Gate::nativeAnnouncementDelivery,Gate::schedulingAndBudget,Gate::completionAndRetirement,Gate::sourceRenewal
};
inline constexpr bool kActivationSupported=false;
} // namespace sunrise::server::runtime::activity::mercury::faction_battle_evidence
