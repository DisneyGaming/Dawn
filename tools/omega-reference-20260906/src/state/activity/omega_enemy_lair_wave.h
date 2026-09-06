#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../middleware/bap/activity_message/sense_update.h"
#include "omega_enemy_lair_catalog.h"

namespace sunrise::state::activity::omega_enemy_lair {

using SenseObject = middleware::bap::activity_message::sense_update::SenseObject;

/** Reflected 80807ECC delta. Unknown fields keep their native offsets/names.
 * +0C is associated native actors (4E5170), not a direct health query. +20 is
 * consumed requests: creation failure and deactivation both increment it. */
struct SpawnerDelta final {
    std::array<std::uint32_t,6> scalar{}; // +00,+04,+08,+0C,+10(raw7),+14
    std::array<std::int32_t,8> consumed{};
    std::array<std::uint8_t,24> scalar44{}; // quantized raw7, not world values
    std::uint32_t scalar44Present{};
    std::uint32_t revision{};
    std::uint8_t scalarPresent{};
    std::uint8_t consumedCount{};
    std::int8_t state18{};
    std::int8_t state19{};
    std::array<bool,3> flags{}; // +1A,+1B,+1C
    bool hasDelta{};
    bool consumedPresent{};
    bool scalar44ArrayPresent{};
};

namespace detail {
/** Reads captured native body chunks, including their right-aligned last chunk. */
class BodyReader final {
public:
    explicit BodyReader(const SenseObject& object) noexcept : object_(object) {}
    [[nodiscard]] bool read(unsigned width,std::uint32_t& value) noexcept {
        value=0;
        if(width>32 || position_>object_.bodyBits || width>object_.bodyBits-position_) { return false; }
        for(unsigned i=0;i<width;++i) {
            const auto chunk=position_/64U;
            const auto left=object_.bodyBits-chunk*64U;
            const auto chunkBits=left<64U ? left : 64U;
            std::uint64_t word{};
            if(chunk==0) { word=object_.bodyFirst; }
            else if(chunk==1) { word=object_.bodySecond; }
            else if(chunk==2) { word=object_.bodyThird; }
            else if(chunk-3U<object_.bodyTail.size()) { word=object_.bodyTail[chunk-3U]; }
            else { return false; }
            value=(value<<1U)|static_cast<std::uint32_t>((word>>(chunkBits-1U-position_%64U))&1U);
            ++position_;
        }
        return true;
    }
    [[nodiscard]] bool ended() const noexcept { return position_==object_.bodyBits; }
private:
    const SenseObject& object_;
    std::uint32_t position_{};
};
} // namespace detail

/** Complete schema consumption, without guessing body widths or retaining a
 * previous generation on malformed input. Does not inspect process memory. */
[[nodiscard]] inline bool decode_spawner_delta(const SenseObject& object,SpawnerDelta& result) noexcept {
    result={};
    if(object.slotType!=1 || object.bodyBits<33 || object.bodyBits>638) { return false; }
    SpawnerDelta delta{};
    detail::BodyReader reader{object};
    std::uint32_t value{};
    if(!reader.read(1,value)) { return false; }
    delta.hasDelta=value!=0;
    if(delta.hasDelta!=object.hasDelta) { return false; }
    if(delta.hasDelta) {
        constexpr std::array<unsigned,6> widths{31,31,31,6,7,31};
        for(unsigned i=0;i<widths.size();++i) {
            if(!reader.read(1,value)) { return false; }
            if(value!=0) {
                delta.scalarPresent|=static_cast<std::uint8_t>(1U<<i);
                if(!reader.read(widths[i],delta.scalar[i])) { return false; }
            }
        }
        if(!reader.read(2,value)) { return false; }
        delta.state18=static_cast<std::int8_t>(static_cast<int>(value)-1);
        if(!reader.read(3,value)) { return false; }
        delta.state19=static_cast<std::int8_t>(static_cast<int>(value)-1);
        for(auto& flag:delta.flags) { if(!reader.read(1,value)) { return false; } flag=value!=0; }
        if(!reader.read(1,value)) { return false; }
        delta.consumedPresent=value!=0;
        if(delta.consumedPresent) {
            if(!reader.read(4,value) || value>delta.consumed.size()) { return false; }
            delta.consumedCount=static_cast<std::uint8_t>(value);
            for(unsigned i=0;i<delta.consumedCount;++i) {
                if(!reader.read(32,value)) { return false; }
                // Native signed codec is biased by INT32_MIN; int64 avoids overflow.
                delta.consumed[i]=static_cast<std::int32_t>(static_cast<std::int64_t>(value)-2147483648LL);
            }
        }
        if(!reader.read(1,value)) { return false; }
        delta.scalar44ArrayPresent=value!=0;
        if(delta.scalar44ArrayPresent) {
            for(unsigned i=0;i<delta.scalar44.size();++i) {
                if(!reader.read(1,value)) { return false; }
                if(value!=0) {
                    delta.scalar44Present|=1U<<i;
                    if(!reader.read(7,value)) { return false; }
                    delta.scalar44[i]=static_cast<std::uint8_t>(value);
                }
            }
        }
    }
    if(!reader.read(32,delta.revision) || !reader.ended() || delta.revision!=object.revision) { return false; }
    result=delta;
    return true;
}

/** Delta mirror owned by exactly one run and authority generation. Absent
 * optional fields retain their value; an unobserved optional field stays unknown. */
struct SpawnerMirror final {
    std::array<std::uint32_t,6> scalar{};
    std::array<std::int32_t,8> consumed{};
    std::array<std::uint8_t,24> scalar44{};
    std::uint32_t scalar44Known{};
    std::uint32_t revision{};
    std::uint8_t scalarKnown{};
    std::uint8_t consumedCount{};
    std::int8_t state18{};
    std::int8_t state19{};
    std::array<bool,3> flags{};
    bool consumedKnown{};
    bool revisionKnown{};

    void merge(const SpawnerDelta& delta) noexcept {
        revision=delta.revision;revisionKnown=true;
        if(!delta.hasDelta) { return; }
        for(unsigned i=0;i<scalar.size();++i) {
            if((delta.scalarPresent&(1U<<i))!=0) { scalar[i]=delta.scalar[i]; }
        }
        scalarKnown|=delta.scalarPresent;
        state18=delta.state18;state19=delta.state19;flags=delta.flags;
        if(delta.consumedPresent) {
            consumed=delta.consumed;consumedCount=delta.consumedCount;consumedKnown=true;
        }
        for(unsigned i=0;i<scalar44.size();++i) {
            if((delta.scalar44Present&(1U<<i))!=0) { scalar44[i]=delta.scalar44[i]; }
        }
        scalar44Known|=delta.scalar44Present;
    }
};

/** Conservative receipt for a single category of loose native requests.
 * 'settled' proves observed admission followed by native retirement/consumption;
 * it does not prove death rather than forced deactivation. The host must not
 * withdraw/unload/replace the population before using this receipt for combat.
 * Member-bound actor lifetimes require their own member receipt. */
class LoosePopulationReceipt final {
public:
    [[nodiscard]] bool begin(std::uint16_t slot,std::uint32_t generation,
                             std::uint8_t requested) noexcept {
        *this={};
        const auto* spawner=find_spawner(slot);
        if(spawner==nullptr || !supported_by_encounter(*spawner) || spawner->memberSlot!=0
            || requested==0 || requested>63
            || generation>0x7FFFFFFFU) { return false; }
        slot_=slot;generation_=generation;requested_=requested;configured_=true;
        return true;
    }
    [[nodiscard]] bool observe(const SenseObject& object) noexcept {
        if(!configured_ || object.registryKey!=kRegistry || object.slotIndex!=slot_) { return false; }
        SpawnerDelta delta{};
        if(!decode_spawner_delta(object,delta)) { return false; }
        // Ignore retransmissions/stale revisions. Native revisions are uint32 serials.
        if(mirror_.revisionKnown && (delta.revision==mirror_.revision
            || delta.revision-mirror_.revision>=0x80000000U)) { return false; }
        if((delta.scalarPresent&1U)!=0) {
            if(delta.scalar[0]!=generation_) {
                // A newer native generation invalidates this request's mirror. An
                // omitted-generation delta must never inherit the former epoch.
                if(mirror_.revisionKnown) { configured_=false;mirror_={};peakAssociated_=0; }
                return false;
            }
        } else if((mirror_.scalarKnown&1U)==0) { return false; }
        mirror_.merge(delta);
        if((mirror_.scalarKnown&8U)!=0 && mirror_.scalar[3]>peakAssociated_) {
            peakAssociated_=mirror_.scalar[3];
        }
        return true;
    }
    [[nodiscard]] bool admitted() const noexcept { return configured_ && peakAssociated_>=requested_; }
    [[nodiscard]] bool settled() const noexcept {
        return admitted() && (mirror_.scalarKnown&8U)!=0 && mirror_.scalar[3]==0
            && mirror_.consumedKnown && mirror_.consumedCount==1
            && mirror_.consumed[0]>=requested_;
    }
    [[nodiscard]] const SpawnerMirror& mirror() const noexcept { return mirror_; }
    [[nodiscard]] std::uint32_t peak_associated() const noexcept { return peakAssociated_; }
    [[nodiscard]] std::uint32_t generation() const noexcept { return generation_; }
    [[nodiscard]] std::uint8_t requested() const noexcept { return requested_; }
    [[nodiscard]] std::uint16_t slot() const noexcept { return slot_; }
private:
    SpawnerMirror mirror_{};
    std::uint32_t generation_{};
    std::uint32_t peakAssociated_{};
    std::uint16_t slot_{};
    std::uint8_t requested_{};
    bool configured_{};
};

} // namespace sunrise::state::activity::omega_enemy_lair
