#pragma once
#include "../coo/mission_script.h"
#include "catalog.h"
namespace sunrise::state::activity::gateway {
inline constexpr coo::Asset kVanceScene{0xBA0B27A0U,0x80F46DE0U,43,5};
inline constexpr std::uint32_t kVanceAscentMs=22640, kVanceFinishMs=31000;
inline constexpr coo::Asset kModule{kRoot,kScenario,0,0};
inline constexpr coo::Asset kLanding{0x85742F3EU,0x80F470E5U,60,359};
inline constexpr coo::Asset kRecess{0x85742F3EU,0x80F470E5U,60,361};
inline constexpr coo::Asset kObjective{kRoot,0x80F47420U,68,0};
inline constexpr coo::Asset kDialogueAsset{kRoot,0x80F47426U,53,2};
inline constexpr coo::script::Capability kCapabilities[]{
    {"opening.module","composition",{coo::Operation::mechanic,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::requested}},
    {"opening.checked","composition",{coo::Operation::observation,{0x00000000U,0x00000000U,0,0},0U,coo::Wait::observed}},
    {"landing.entered","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,359},0U,coo::Wait::observed}},
    {"objective.find_gateway","opening",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},3369893117U,coo::Wait::requested}},
    {"dialogue.ikora_gateway","opening",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},1U,coo::Wait::nativeReady}},
    {"recess.entered","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,361},0U,coo::Wait::observed}},
    {"marchers.start","opening",{coo::Operation::mechanic,{0x986985D0U,0x80F46DB0U,0,0},10U,coo::Wait::requested}},
    {"recess.initial","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::requested}},
    {"recess.reinforce_trigger","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,367},1U,coo::Wait::observed}},
    {"recess.reinforcements","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},2U,coo::Wait::requested}},
    {"recess.cleared","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::completed}},
    {"recess.reinforcements_cleared","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},2U,coo::Wait::completed}},
    {"recess.cannons","opening",{coo::Operation::device,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::nativeReady}},
    {"shelf.entered","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,369},0U,coo::Wait::observed}},
    {"shelf.initial","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},3U,coo::Wait::requested}},
    {"shelf.reinforce_trigger","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,372},3U,coo::Wait::observed}},
    {"shelf.reinforcements","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},4U,coo::Wait::requested}},
    {"end.entered","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,375},0U,coo::Wait::observed}},
    {"end.initial","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},5U,coo::Wait::requested}},
    {"end.reinforce_trigger","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,379},5U,coo::Wait::observed}},
    {"end.reinforcements","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},6U,coo::Wait::requested}},
    {"end.cleared","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},5U,coo::Wait::completed}},
    {"end.reinforcements_cleared","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},6U,coo::Wait::completed}},
    {"end.cannon","opening",{coo::Operation::device,{0x986985D0U,0x80F46DB0U,0,0},2U,coo::Wait::requested}},
    {"dialogue.vance","opening",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},2U,coo::Wait::nativeReady}},
    {"lighthouse.landed","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,378},0U,coo::Wait::observed}},
    {"shelf.passed","opening",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,375},256U,coo::Wait::observed}},
    {"mainland.outskirts","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},7U,coo::Wait::requested}},
    {"mainland.center","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},8U,coo::Wait::requested}},
    {"objective.forest_gate","opening",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},4154833763U,coo::Wait::requested}},
    {"mainland.intro","opening",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,439},0U,coo::Wait::observed}},
    {"dialogue.forest_gate","opening",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},3U,coo::Wait::nativeReady}},
    {"mainland.cleared","opening",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},8U,coo::Wait::completed}},
    {"forest.approached","opening",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,441},0U,coo::Wait::observed}},
    {"dialogue.at_gate","opening",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},4U,coo::Wait::nativeReady}},
    {"forest.blocked","opening",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,448},0U,coo::Wait::observed}},
    {"dialogue.blocked","opening",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},5U,coo::Wait::nativeReady}},
    {"objective.bring_sagira","opening",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},4197838318U,coo::Wait::requested}},
    {"return.center","ending",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},9U,coo::Wait::requested}},
    {"return.outskirts","ending",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},10U,coo::Wait::requested}},
    {"objective.return","ending",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},3034087627U,coo::Wait::requested}},
    {"return.contact","ending",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,461},512U,coo::Wait::observed}},
    {"dialogue.descendants","ending",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},6U,coo::Wait::nativeReady}},
    {"finale.approached","ending",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,461},0U,coo::Wait::observed}},
    {"finale.wave_1","ending",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},11U,coo::Wait::completed}},
    {"finale.wave_2","ending",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},12U,coo::Wait::completed}},
    {"finale.wave_3","ending",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},13U,coo::Wait::completed}},
    {"finale.support","ending",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},14U,coo::Wait::requested}},
    {"module.expose","ending",{coo::Operation::mechanic,{0x4B946B28U,0x80F46F23U,4,32},20U,coo::Wait::requested}},
    {"objective.module","ending",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},2164099943U,coo::Wait::requested}},
    {"dialogue.module","ending",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},7U,coo::Wait::nativeReady}},
    {"module.destroyed","ending",{coo::Operation::observation,{0x4B946B28U,0x80F46F23U,4,32},513U,coo::Wait::observed}},
    {"lighthouse.unlock","ending",{coo::Operation::mechanic,{0x4B946B28U,0x80F46F23U,4,32},21U,coo::Wait::requested}},
    {"objective.enter","ending",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},2970444885U,coo::Wait::requested}},
    {"dialogue.timelines","ending",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},8U,coo::Wait::nativeReady}},
    {"lighthouse.entered","ending",{coo::Operation::observation,{0xBA0B27A0U,0x80F46DCDU,60,11},0U,coo::Wait::observed}},
    {"dialogue.old_place","ending",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},9U,coo::Wait::nativeReady}},
    {"objective.vance","ending",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},1915741729U,coo::Wait::requested}},
    {"vance.approached","ending",{coo::Operation::observation,{0xBA0B27A0U,0x80F46DCDU,60,13},0U,coo::Wait::observed}},
    {"dialogue.come_closer","ending",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},10U,coo::Wait::nativeReady}},
    {"vance.scene","ending",{coo::Operation::scene,{0xBA0B27A0U,0x80F46DE0U,43,5},1U,coo::Wait::nativeReady}},
    {"vance.ascent_cue","ending",{coo::Operation::observation,kVanceScene,kVanceAscentMs,coo::Wait::observed}},
    {"lighthouse.raise","ending",{coo::Operation::device,{0xBA0B27A0U,0x80F46DD0U,23,0},1U,coo::Wait::requested}},
    {"lighthouse.light","ending",{coo::Operation::device,{0xBA0B27A0U,0x80F46DD3U,23,1},1U,coo::Wait::requested}},
    {"vance.ending_cue","ending",{coo::Operation::observation,kVanceScene,kVanceFinishMs,coo::Wait::observed}},
    {"mission.finish","ending",{coo::Operation::mechanic,{0x986985D0U,0x80F46DB0U,0,0},30U,coo::Wait::requested}},
};
inline constexpr coo::script::ModuleCapability kModules[]{{"opening",{kModule,1}}};
inline constexpr coo::script::FactCapability kFacts[]{{"opening.checked",0}};
inline constexpr coo::script::Profile kProfile{"gateway.ending.v2","otherMissions",coo::Schema::otherMissions,
    kCapabilities,kModules,kFacts,kDialogue,kObjectives,{},{}};
// Opening contract is retained; ending has its own executor incarnation.
struct ContractStep final { std::uint32_t dependencies; std::span<const std::string_view> commands; };
inline constexpr std::string_view kStep0[]{"landing.entered"};
inline constexpr std::string_view kStep1[]{"objective.find_gateway","dialogue.ikora_gateway"};
inline constexpr std::string_view kStep2[]{"recess.entered"};
inline constexpr std::string_view kStep3[]{"marchers.start"};
inline constexpr std::string_view kStep4[]{"recess.initial"};
inline constexpr std::string_view kStep5[]{"recess.reinforce_trigger"};
inline constexpr std::string_view kStep6[]{"recess.reinforcements"};
inline constexpr std::string_view kStep7[]{"recess.cleared","recess.reinforcements_cleared"};
inline constexpr std::string_view kStep8[]{"recess.cannons"};
inline constexpr std::string_view kStep9[]{"shelf.entered"};
inline constexpr std::string_view kStep10[]{"shelf.initial"};
inline constexpr std::string_view kStep11[]{"shelf.reinforce_trigger"};
inline constexpr std::string_view kStep12[]{"shelf.reinforcements"};
inline constexpr std::string_view kStep13[]{"shelf.passed"};
inline constexpr std::string_view kStep14[]{"end.entered"};
inline constexpr std::string_view kStep15[]{"end.initial"};
inline constexpr std::string_view kStep16[]{"end.reinforce_trigger"};
inline constexpr std::string_view kStep17[]{"end.reinforcements"};
inline constexpr std::string_view kStep18[]{"end.cleared","end.reinforcements_cleared"};
inline constexpr std::string_view kStep19[]{"end.cannon","dialogue.vance"};
inline constexpr std::string_view kStep20[]{"lighthouse.landed"};
inline constexpr std::string_view kStep21[]{"mainland.outskirts","mainland.center","objective.forest_gate"};
inline constexpr std::string_view kStep22[]{"mainland.intro"};
inline constexpr std::string_view kStep23[]{"dialogue.forest_gate"};
inline constexpr std::string_view kStep24[]{"mainland.cleared"};
inline constexpr std::string_view kStep25[]{"forest.approached"};
inline constexpr std::string_view kStep26[]{"dialogue.at_gate"};
inline constexpr std::string_view kStep27[]{"forest.blocked"};
inline constexpr std::string_view kStep28[]{"dialogue.blocked"};
inline constexpr std::string_view kStep29[]{"objective.bring_sagira"};
inline constexpr ContractStep kContract[]{
    {0U,kStep0},
    {1U,kStep1},
    {2U,kStep2},
    {1U,kStep3},
    {12U,kStep4},
    {16U,kStep5},
    {32U,kStep6},
    {64U,kStep7},
    {128U,kStep8},
    {256U,kStep9},
    {512U,kStep10},
    {1024U,kStep11},
    {2048U,kStep12},
    {4096U,kStep13},
    {8192U,kStep14},
    {16384U,kStep15},
    {32768U,kStep16},
    {65536U,kStep17},
    {131072U,kStep18},
    {262144U,kStep19},
    {524288U,kStep20},
    {1048576U,kStep21},
    {2097152U,kStep22},
    {4194304U,kStep23},
    {2097152U,kStep24},
    {25165824U,kStep25},
    {33554432U,kStep26},
    {67108864U,kStep27},
    {134217728U,kStep28},
    {268435456U,kStep29},
};
inline constexpr std::string_view kEndingStep0[]{"return.center","return.outskirts","objective.return"};
inline constexpr std::string_view kEndingStep1[]{"return.contact"};
inline constexpr std::string_view kEndingStep2[]{"dialogue.descendants"};
inline constexpr std::string_view kEndingStep3[]{"finale.approached"};
inline constexpr std::string_view kEndingStep4[]{"finale.wave_1"};
inline constexpr std::string_view kEndingStep5[]{"finale.wave_2"};
inline constexpr std::string_view kEndingStep6[]{"finale.wave_3","finale.support"};
inline constexpr std::string_view kEndingStep7[]{"module.expose","objective.module","dialogue.module"};
inline constexpr std::string_view kEndingStep8[]{"module.destroyed"};
inline constexpr std::string_view kEndingStep9[]{"lighthouse.unlock","objective.enter","dialogue.timelines"};
inline constexpr std::string_view kEndingStep10[]{"lighthouse.entered"};
inline constexpr std::string_view kEndingStep11[]{"dialogue.come_closer","objective.vance"};
inline constexpr std::string_view kEndingStep12[]{"vance.approached"};
inline constexpr std::string_view kEndingStep13[]{"dialogue.old_place"};
inline constexpr std::string_view kEndingStep14[]{"vance.scene"};
inline constexpr std::string_view kEndingStep15[]{"vance.ascent_cue"};
inline constexpr std::string_view kEndingStep16[]{"lighthouse.raise","lighthouse.light"};
inline constexpr std::string_view kEndingStep17[]{"vance.ending_cue"};
inline constexpr std::string_view kEndingStep18[]{"mission.finish"};
inline constexpr ContractStep kEndingContract[]{
    {0U,kEndingStep0},
    {1U,kEndingStep1},
    {2U,kEndingStep2},
    {1U,kEndingStep3},
    {8U,kEndingStep4},
    {16U,kEndingStep5},
    {32U,kEndingStep6},
    {64U,kEndingStep7},
    {128U,kEndingStep8},
    {256U,kEndingStep9},
    {512U,kEndingStep10},
    {1024U,kEndingStep11},
    {1024U,kEndingStep12},
    {2048U,kEndingStep13},
    {12288U,kEndingStep14},
    {16384U,kEndingStep15},
    {32768U,kEndingStep16},
    {16384U,kEndingStep17},
    {196612U,kEndingStep18},
};
[[nodiscard]] bool valid_document(const coo::script::Views& views) noexcept;
} // namespace sunrise::state::activity::gateway
