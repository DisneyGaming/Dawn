#pragma once
#include <array>
#include <cstdint>
namespace dawn::state::activity::omega::rescue {
inline constexpr std::uint32_t kRegistry=0x99BD2FEB;
struct Source { std::uint16_t slot; std::uint32_t definition; };
struct Scene { std::uint16_t slot; std::uint32_t definition,selector; std::uint8_t count; std::array<std::uint16_t,8> sources; };
// Extracted from packaged 80F47B1D definitions. Selectorless loops and debug scenes are excluded.
inline constexpr std::array<Source,24> sources{{
    {0,0x80F4799D},
    {5,0x80F479B0},
    {6,0x80F479B4},
    {7,0x80F479B8},
    {8,0x80F479BC},
    {38,0x80F47A18},
    {39,0x80F47A1C},
    {40,0x80F47A20},
    {41,0x80F47A24},
    {42,0x80F47A28},
    {43,0x80F47A2C},
    {44,0x80F47A30},
    {45,0x80F47A34},
    {77,0x80F47A95},
    {78,0x80F47A99},
    {79,0x80F47A9D},
    {80,0x80F47AA1},
    {85,0x80F47AB1},
    {86,0x80F47AB5},
    {87,0x80F47AB9},
    {88,0x80F47ABD},
    {93,0x80F47ACE},
    {94,0x80F47AD2},
    {95,0x80F47AD5},
}};
inline constexpr std::array<Scene,11> scenes{{
    {9,0x80F479BF,0x80EC0DD0,5,{0,5,6,7,8}},
    {27,0x80F479F5,0x80EC0DD5,1,{0}},
    {33,0x80F47A08,0x80EC0DD5,1,{38}},
    {46,0x80F47A37,0x80EC0DCB,8,{38,40,41,42,43,44,45,39}},
    {66,0x80F47A73,0x80EC0DB6,0,{}},
    {67,0x80F47A76,0x80EC0DBD,4,{77,78,79,80}},
    {68,0x80F47A79,0x80EC0DB9,0,{}},
    {81,0x80F47AA4,0x80EC0DB9,0,{}},
    {82,0x80F47AA7,0x80EC0DBD,4,{85,86,87,88}},
    {83,0x80F47AAA,0x80EC0DB2,0,{}},
    {84,0x80F47AAD,0x80EC0DC5,3,{93,94,95}},
}};
inline const Scene* scene(std::uint16_t slot) noexcept { for(const auto& row:scenes) if(row.slot==slot) return &row; return nullptr; }
inline const Source* source(std::uint16_t slot) noexcept { for(const auto& row:sources) if(row.slot==slot) return &row; return nullptr; }
}
