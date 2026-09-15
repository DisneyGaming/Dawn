#pragma once
#include "../../../account/account_state.h"
#include <cstddef>
#include <span>
#include <array>

namespace sunrise::state::activity::newlight::launchpad::quest {
inline constexpr std::uint32_t kEscapeCosmodrome=0xD267D4BB;
// Installed 86657 quest line: only the introduction is advanced by this mission.
inline constexpr std::array<std::uint32_t,6> kSteps{
    kEscapeCosmodrome,0x219C5A6F,0x8B7F6365,0xC259C4CF,0xE34E77B4,0x3D3AAB9C};
inline constexpr std::array<std::int32_t,6> kTracking{100,120,130,140,150,160};
inline constexpr std::array<std::uint32_t,2> kEscapeRewards{0xA3E834D7U,0x0E3D4ED7U}; // Arcadia, Wayfarer Zero
inline constexpr std::array<account::inventory::EquipmentSlot,2> kEscapeSlots{
    account::inventory::EquipmentSlot::ship,account::inventory::EquipmentSlot::vehicle};
struct Currency {std::uint32_t hash;std::int32_t quantity;};
inline constexpr Currency kEscapeCurrencies[]{{3159615086U,2500},{2817410917U,800}};
struct Introduction {std::int16_t vendor,interaction,sale;std::uint32_t reward;};
inline constexpr std::array<Introduction,3> kIntroductions{{
    {22,11,250,0x6412DC58}, // Banshee-44: Bayesian MSu
    {16,15,222,0xC0E76945}, // Zavala: First Strike
    {17,16,28,0x2E0C1F9D}, // Ikora: Explore the EDZ
}};
inline int step(const CharacterState& character) noexcept {
    if(character.inventory.count>character.inventory.values.size()) {return -1;}
    int found=-1;
    for(std::size_t i=0;i<character.inventory.count;++i) {
        const auto& item=character.inventory.values[i];
        for(std::size_t s=0;s<kSteps.size();++s) if(!item.postmaster && item.definitionHash==kSteps[s] && item.quantity>0) {
            if(found!=-1) {return -1;}found=static_cast<int>(s);
        }
    }
    return found;
}
inline int accepted_step(std::int16_t vendor,std::int16_t interaction,std::int16_t reply,
                         std::int32_t selection) noexcept {
    if(reply!=0) {return -1;}
    for(std::size_t i=0;i<kIntroductions.size();++i) {
        const auto& intro=kIntroductions[i];
        if(vendor==intro.vendor && interaction==intro.interaction
            && (selection==-1 || selection==intro.sale)) {return static_cast<int>(i)+2;}
    }
    return -1;
}

// Start-destination row 0 selects activity 1 from f753 AND NOT f1041.
// Native mapping 81319322/+40 maps those slots to character-object bytes 20/59.
// Owning the first Pursuit opts only that Guardian into the native sign-in launch.
inline void project_start(const CharacterState& character,std::span<std::byte> flags) noexcept {
    if(flags.size()<=59 || character.inventory.count>character.inventory.values.size()) {return;}
    const int current=step(character);
    if(current<0) {return;}
    // Ikora hands the Guardian over to ordinary play at Become Legend. Retain
    // opening completion, but release New Light's forced-start restriction.
    flags[20]=current<5?std::byte{2}:std::byte{};
    flags[59]=current==0?std::byte{}:std::byte{2};
}
template<class CharacterObject> inline void project(const CharacterState& character,CharacterObject& object) noexcept {
    const auto current=step(character);if(current<0) {return;}
    project_start(character,object.acquiredFlags);
    // 81319094/8E/8F introduction expressions read these exact flag slots.
    // Character override arrays are consumed by native 540650; the existing
    // Family-5 bit-0 arm (FA7FE0, context vtable +68) enables this per-character bank.
    object.unlockFlagCount=4;
    object.unlockFlags[0]={20483,2,0};
    for(int i=0;i<3;++i) object.unlockFlags[i+1]={static_cast<std::int16_t>(20484+i),
        static_cast<std::uint8_t>(current==i+2?2:0),0};
    object.unlockValueCount=6;
    object.unlockValues[0]={12740,0,kTracking[current]};
    for(int i=0;i<5;++i) object.unlockValues[i+1]={static_cast<std::int16_t>(12741+i),0,current>i?1:0};
}
}
