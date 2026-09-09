#pragma once
#include "native_catalog.h"
#include "mechanism_catalog.h"
#include "../coo/objective_service.h"
namespace sunrise::state::activity::deep_storage {
struct PlateBinding { coo::Asset source,volume;float chargeSeconds; };
// VIDEO estimates; native 815B8B3B owns progress and the completion latch.
inline constexpr PlateBinding kPlates[]{
    {{0xA13D8A45U,0x80B560D5U,4,4},{0xA13D8A45U,0x80B5604CU,60,12},5.F},
    {{0x59700FA7U,0x80B56868U,4,72},{0x59700FA7U,0x80B56378U,60,273},10.F},
    {{0x59700FA7U,0x80B56875U,4,74},{0x59700FA7U,0x80B56378U,60,280},10.F},
};
struct ScanBinding { coo::Asset source,link;std::uint32_t controllerDefinition;std::uint64_t guid;float defaultSeconds,sourceSeconds; };
inline constexpr ScanBinding kScans[]{
    {{0xA13D8A45U,0x80B5609EU,4,2},{0x4324A238U,0x80B56174U,65,0},0x8156EFA4U,0xCDFC784B0C4A9CF5ULL,kGhostScans[0].defaultDuration,kGhostScans[0].sourceDuration},
    {{0x59700FA7U,0x80B5645CU,4,95},{0xEA42F517U,0x80B5654EU,65,0},0x8157E6B1U,0x844920F35A95B55BULL,kGhostScans[1].defaultDuration,kGhostScans[1].sourceDuration},
};
// The native directive owns authored objective text. Active object markers use
// its scoped native source, never a fabricated position or a trigger centroid.
inline constexpr coo::MarkerTarget marker(std::uint32_t event) noexcept {
    if(event==kObjectives[1].event || event==kObjectives[2].event) {return {kScans[0].source,{}};}
    if(event==kObjectives[8].event) {return {kScans[1].source,{}};}
    return {};
}
float device_position(coo::Asset,bool) noexcept;
}
