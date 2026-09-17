#pragma once
#include "frame.h"
namespace dawn::state::activity::strike_bond {
inline constexpr coo::Asset kEndingScene{0xC80A735BU,0x80F4749BU,43,5};
inline constexpr coo::Asset kEndingApproach{0xC80A735BU,0x80F474B7U,31,14};
inline constexpr std::uint32_t kEndingApproachInput=0x4BFF2327U,kEndingScanInput=0xDF677EBCU,kPanoptesAppears=0x325934D5U;
struct EndingSpeechReceipt {
    coo::Generation owner{};std::uint32_t generation{},weak{UINT32_MAX},serial{UINT32_MAX};
};
struct EndingPresentation {
    bool approached{},panoptes{},gotHimStarted{};
    std::uint32_t weak{UINT32_MAX},serial{UINT32_MAX};std::uint64_t gotHimAt{},panoptesAt{},reactionAt{};
    bool reactionHeld{},reactionPending{};
    static constexpr std::uint64_t kInterruptionDelayMs=6000;
    void appearance(std::uint64_t now) noexcept {if(!panoptes) {panoptes=true;panoptesAt=now;}}
    bool capture_ready(std::uint64_t now) const noexcept {
        return panoptes && now>=panoptesAt && now-panoptesAt>=kInterruptionDelayMs;
    }
    bool reaction(const EndingSpeechReceipt& receipt,bool fired,std::uint64_t now) noexcept {
        if(!gotHimStarted || receipt.weak!=weak || receipt.serial!=serial)return false;
        if(!fired) {reactionHeld=true;return true;}
        if(!reactionHeld)return false;
        if(!reactionPending) {reactionPending=true;reactionAt=now;}
        return now>=reactionAt && now-reactionAt>=kInterruptionDelayMs;
    }
    // First phrase of row17. Calibrate this short offset against listening;
    // its clock begins at native speech activation, never interaction completion.
    static constexpr std::uint64_t kGotHimPhraseMs=2000;
    bool speech(const EndingSpeechReceipt& receipt,std::uint8_t state,std::uint64_t now) noexcept {
        if(state!=1 || gotHimStarted || receipt.weak==UINT32_MAX || receipt.serial==UINT32_MAX) return false;
        weak=receipt.weak;serial=receipt.serial;gotHimAt=now;gotHimStarted=true;return true;
    }
    bool tree_ready(std::uint64_t now) const noexcept {return gotHimStarted && now>=gotHimAt && now-gotHimAt>=kGotHimPhraseMs;}
};
namespace ending_object {
struct Component {std::uint32_t definition{},kind{},self{UINT32_MAX},entity{UINT32_MAX};std::uint64_t offset{};};
inline constexpr std::uint32_t kind(coo::Asset a) noexcept {
    return a==coo::Asset{0xC80A735BU,0x80F47492U,4,2}?0x80803910U
        :a==coo::Asset{0xC80A735BU,0x80F47498U,4,4}?0x808036CFU:0U;
}
inline bool retained(coo::Generation owner,coo::Asset source,const NativeState& desired,
    std::uint32_t applied,std::uint32_t committed,std::uint32_t entity,Component component) noexcept {
    const auto expected=kind(source);
    return expected && owner.valid() && owner.value<32766
        && desired.managed && desired.desired && desired.prepared && desired.active
        && applied==desired.generation && applied==owner.value+1U && committed==owner.value
        && entity!=UINT32_MAX && component.entity==entity && component.self!=UINT32_MAX
        && component.kind==expected && component.definition==(expected==0x80803910U?0x80C7069BU:0x80FCD6C9U)
        && component.offset==(expected==0x80803910U?0xA78U:0x2158U);
}
}
}
