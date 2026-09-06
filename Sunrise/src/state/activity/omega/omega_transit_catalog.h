#pragma once
#include "omega_mission_devices.h"
namespace sunrise::state::activity::omega::transit {
enum class Role { ringCore,ringFx,bridge,portal,finalCore,finalFx,finalDisk,destination,charge,sink,endFx,eyeFront,eyeBack,eyeReturn };
struct Source { std::uint32_t registry,definition;std::uint16_t slot,gate;std::uint8_t cycle;Role role;std::int8_t preparation; };
inline constexpr std::array<Source,43> sources{{
    {0x0040BF06,0x80F47690,28,0,1,Role::ringCore,0},
    {0x0040BF06,0x80F47693,29,0,1,Role::ringCore,1},
    {0x0040BF06,0x80F47696,30,0,1,Role::ringCore,2},
    {0x0040BF06,0x80F47699,31,34,1,Role::ringFx,-1},
    {0x0040BF06,0x80F4769C,32,35,1,Role::ringFx,-1},
    {0x0040BF06,0x80F4769F,33,36,1,Role::ringFx,-1},
    {0x0040BF06,0x80F47684,24,26,1,Role::bridge,-1},
    {0x0040BF06,0x80F47687,25,27,1,Role::bridge,-1},
    {0x0040BF06,0x80F4766F,17,0,1,Role::portal,-1},
    {0x0040BF05,0x80F47769,28,0,2,Role::ringCore,3},
    {0x0040BF05,0x80F4776C,29,0,2,Role::ringCore,4},
    {0x0040BF05,0x80F4776F,30,0,2,Role::ringCore,5},
    {0x0040BF05,0x80F47772,31,34,2,Role::ringFx,-1},
    {0x0040BF05,0x80F47775,32,35,2,Role::ringFx,-1},
    {0x0040BF05,0x80F47778,33,36,2,Role::ringFx,-1},
    {0x0040BF05,0x80F4775D,24,26,2,Role::bridge,-1},
    {0x0040BF05,0x80F47760,25,27,2,Role::bridge,-1},
    {0x0040BF05,0x80F47715,0,0,2,Role::portal,-1},
    {0x0040BF03,0x80F47884,20,23,3,Role::bridge,-1},
    {0x0040BF03,0x80F47887,21,24,3,Role::bridge,-1},
    {0x0040BF03,0x80F4788A,22,25,3,Role::bridge,-1},
    {0x0040BF03,0x80F478A5,31,0,3,Role::portal,-1},
    {0x95FB2E01,0x80F475AC,22,0,3,Role::finalCore,6},
    {0x95FB2E01,0x80F475AF,23,24,3,Role::finalFx,-1},
    {0x95FB2E01,0x80F475B5,25,26,3,Role::finalDisk,-1},
    {0x95FB2E01,0x80F47576,4,0,1,Role::destination,-1},
    {0x95FB2E01,0x80F47579,5,0,1,Role::destination,-1},
    {0x95FB2E01,0x80F4757C,6,0,1,Role::destination,-1},
    {0x95FB2E01,0x80F4757F,7,0,3,Role::destination,-1},
    {0x95FB2E01,0x80F47582,8,0,3,Role::destination,-1},
    {0x95FB2E01,0x80F47585,9,0,3,Role::destination,-1},
    {0x0040BF06,0x80F47672,18,0,1,Role::charge,-1},
    {0x0040BF06,0x80F47678,20,0,1,Role::sink,-1},
    {0x0040BF06,0x80F47675,19,21,1,Role::endFx,-1},
    {0x0040BF05,0x80F47718,1,0,2,Role::charge,-1},
    {0x0040BF05,0x80F4771E,3,0,2,Role::sink,-1},
    {0x0040BF05,0x80F4771B,2,4,2,Role::endFx,-1},
    {0x0040BF03,0x80F47848,0,0,3,Role::charge,-1},
    {0x0040BF03,0x80F4784E,2,0,3,Role::sink,-1},
    {0x0040BF03,0x80F4784B,1,3,3,Role::endFx,-1},
    {0x95FB2E01,0x80F475BB,27,29,1,Role::eyeFront,-1},
    {0x95FB2E01,0x80F475BE,28,0,1,Role::eyeBack,-1},
    {0x95FB2E01,0x80F475C4,30,0,1,Role::eyeReturn,-1},
}};
inline const Source* find(std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    for(const auto& row:sources) if(row.registry==registry && ((type==4 && row.slot==slot) || (type==23 && row.gate && row.gate==slot))) return &row;
    return nullptr;
}
}
