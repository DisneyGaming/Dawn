#pragma once
#include "coo/dialogue_service.h"
#include "coo/presentation_cues.h"

namespace dawn::state::activity::omega_presentation {
inline constexpr std::uint32_t kDialogueBank = 0x80F1FD07U;
inline constexpr std::size_t kDialogueRows = 34;
inline constexpr auto kNoDialogue = coo::kNoDialogue;
inline constexpr std::array<std::uint32_t, 7> kObjectives{
    0xC252E306U, 0x1EBF4621U, 0x3517D4D5U, 0x31A51CEBU,
    0xA41DE99BU, 0x85A8F583U, 0xDF97334DU};

using Dialogue = coo::DialogueRow;
// Bank order, not playback order. Compound rows retain their native child sequencing/selection.
// Rows 1/3/4 belong to Ikora actors; row 23 also occurs in Osiris actor graphs.
inline constexpr std::array<Dialogue, kDialogueRows> kDialogue{{
    {0xAD60F465U, 5445, 1000, false}, {0x57477432U, 6083, 0, true},
    {0x730F03C7U, 1985, 0, false}, {0x47AF17F4U, 5051, 0, true},
    {0xB4C3F0B9U, 3218, 0, true}, {0x0ED8C762U, 0, 0, false},
    {0xAE2495ACU, 8093, 0, false}, {0x0E9C80BEU, 10990, 0, false},
    {0x08AE5FB8U, 0, 0, false}, {0xE878194AU, 6461, 0, false},
    {0x7BA4F101U, 0, 0, false}, {0xB2CF9D6EU, 0, 0, false},
    {0xAB0676A8U, 2019, 0, false}, {0xA558F78FU, 6222, 0, false},
    {0x94E09524U, 5761, 0, false}, {0x0294D229U, 4432, 0, false},
    {0xB8CE809FU, 6354, 0, false}, {0x9DA4C20AU, 0, 0, false},
    {0xCB7F171DU, 2301, 0, false}, {0xC0570578U, 0, 0, false},
    {0xD16ECB03U, 0, 0, false}, {0xC645267EU, 3538, 0, false},
    {0x202F7829U, 3320, 0, false}, {0xD4CADE1DU, 2486, 0, true},
    {0x1C653216U, 0, 0, false}, {0x6352D26CU, 4173, 0, false},
    {0xD453DB47U, 1712, 0, false}, {0xEB43430DU, 0, 0, false},
    {0xDC1E272EU, 0, 0, false}, {0xB88C4BB2U, 3201, 0, false},
    {0x349D2672U, 5489, 0, false}, {0x5F8E6160U, 2781, 0, false},
    {0x03622EEBU, 3529, 0, false}, {0x921B35F9U, 3852, 5000, false}
}};

enum class Landmark : std::uint8_t { lighthouse, tunnel, forestVista, forestExit, lair, arena };
enum class Encounter : std::uint8_t {
    defenses, deletion, osirisArrives, osirisHolds, arcReady, arcReminder,
    eyeVulnerable, pursuit, defeated, cinematic
};

inline constexpr std::array<coo::ObjectiveCueBinding, 5> kObjectiveCues{{
    {18, kObjectives[5]}, {21, kObjectives[5]}, {31, kObjectives[5]},
    {22, kObjectives[4]}, {32, kObjectives[4]}
}};
inline constexpr coo::DialogueDefinition kDialogueDefinition{kDialogueBank, kDialogue, kObjectiveCues};
inline constexpr std::uint8_t kFinalDialogueRow = 33;
inline constexpr std::array<std::uint8_t, 10> kEncounterStages{1,2,2,3,4,4,5,6,7,8};
struct RescuePresentation final { std::uint32_t definition; std::uint8_t cycle; };
inline constexpr std::array<RescuePresentation, 3> kRescuePresentation{{
    {0x80F479BFU, 1}, {0x80F479F5U, 2}, {0x80F47A08U, 3}
}};
namespace cues {
using coo::Operation;
using coo::PresentationAction;
using coo::PresentationCue;
inline constexpr PresentationAction kLandmark0[]{{Operation::dialogue, 0, 0}};
inline constexpr PresentationAction kLandmark1[]{{Operation::objective, kObjectives[1], 0}, {Operation::dialogue, 6, 1500}};
inline constexpr PresentationAction kLandmark2[]{{Operation::objective, kObjectives[1], 0}, {Operation::dialogue, 7, 0}};
inline constexpr PresentationAction kLandmark3[]{{Operation::objective, kObjectives[1], 0}, {Operation::dialogue, 9, 0}};
inline constexpr PresentationAction kLandmark4[]{{Operation::objective, kObjectives[2], 0}};
inline constexpr PresentationAction kLandmark5[]{{Operation::objective, kObjectives[3], 0}, {Operation::dialogue, 13, 0}};
inline constexpr PresentationCue kLandmark[]{
    {static_cast<std::uint32_t>(Landmark::lighthouse), 0, kLandmark0},
    {static_cast<std::uint32_t>(Landmark::tunnel), 0, kLandmark1},
    {static_cast<std::uint32_t>(Landmark::forestVista), 0, kLandmark2},
    {static_cast<std::uint32_t>(Landmark::forestExit), 0, kLandmark3},
    {static_cast<std::uint32_t>(Landmark::lair), 0, kLandmark4},
    {static_cast<std::uint32_t>(Landmark::arena), 0, kLandmark5}
};
inline constexpr PresentationAction kEncounter0[]{{Operation::objective, kObjectives[3], 0}};
inline constexpr PresentationAction kEncounter1[]{{Operation::dialogue, 14, 0}};
inline constexpr PresentationAction kEncounter2[]{{Operation::dialogue, 15, 0}};
inline constexpr PresentationAction kEncounter3[]{{Operation::dialogue, 16, 0}};
inline constexpr PresentationAction kEncounter4[]{{Operation::dialogue, 25, 0}, {Operation::dialogue, 26, 5840}};
inline constexpr PresentationAction kEncounter5[]{{Operation::dialogue, 30, 0}};
inline constexpr PresentationAction kEncounter6[]{{Operation::objective, kObjectives[5], 0}};
inline constexpr PresentationAction kEncounter7[]{{Operation::dialogue, 18, 0}};
inline constexpr PresentationAction kEncounter8[]{{Operation::dialogue, 21, 0}};
inline constexpr PresentationAction kEncounter9[]{{Operation::dialogue, 31, 0}};
inline constexpr PresentationAction kEncounter10[]{{Operation::objective, kObjectives[4], 0}};
inline constexpr PresentationAction kEncounter11[]{{Operation::dialogue, 22, 0}};
inline constexpr PresentationAction kEncounter12[]{{Operation::dialogue, 32, 0}};
inline constexpr PresentationAction kEncounter13[]{{Operation::objective, kObjectives[6], 0}, {Operation::dialogue, 29, 0}};
inline constexpr PresentationAction kEncounter14[]{{Operation::dialogue, kFinalDialogueRow, 0}};
inline constexpr PresentationCue kEncounter[]{
    {static_cast<std::uint32_t>(Encounter::defenses), 7, kEncounter0},
    {static_cast<std::uint32_t>(Encounter::deletion), 1, kEncounter1},
    {static_cast<std::uint32_t>(Encounter::osirisArrives), 7, kEncounter2},
    {static_cast<std::uint32_t>(Encounter::osirisHolds), 1, kEncounter3},
    {static_cast<std::uint32_t>(Encounter::osirisHolds), 2, kEncounter4},
    {static_cast<std::uint32_t>(Encounter::osirisHolds), 4, kEncounter5},
    {static_cast<std::uint32_t>(Encounter::arcReady), 7, kEncounter6},
    {static_cast<std::uint32_t>(Encounter::arcReady), 1, kEncounter7},
    {static_cast<std::uint32_t>(Encounter::arcReminder), 3, kEncounter8},
    {static_cast<std::uint32_t>(Encounter::arcReminder), 4, kEncounter9},
    {static_cast<std::uint32_t>(Encounter::eyeVulnerable), 7, kEncounter10},
    {static_cast<std::uint32_t>(Encounter::eyeVulnerable), 1, kEncounter11},
    {static_cast<std::uint32_t>(Encounter::eyeVulnerable), 4, kEncounter12},
    {static_cast<std::uint32_t>(Encounter::pursuit), 7, kEncounter13},
    {static_cast<std::uint32_t>(Encounter::defeated), 7, kEncounter14}
};
inline constexpr PresentationAction kRevealComplete[]{{Operation::dialogue, 12, 250}};
inline constexpr PresentationAction kFirstRescueFollowup[]{{Operation::dialogue, 16, 6400}};
} // namespace cues
} // namespace dawn::state::activity::omega_presentation
