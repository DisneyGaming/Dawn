#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <bit>
#include <cmath>

namespace sunrise::middleware::bap::activity_message::native::cue {
// 80804F67 directive authority with optional readiness reference and C4 timer.
inline constexpr std::size_t kBits=4802,kDecodedBytes=0x300;
inline constexpr std::uint32_t kAbsent=0x811C9DC5U;
struct Reference final {
    std::uint32_t registry{kAbsent};
    std::uint8_t type{UINT8_MAX};
    std::uint16_t slot{UINT16_MAX};
    friend bool operator==(const Reference&,const Reference&)=default;
};
[[nodiscard]] constexpr bool valid(const Reference& ref) noexcept {
    return ref==Reference{} || (ref.registry && ref.registry!=UINT32_MAX && ref.registry!=kAbsent
        && ref.type<=126 && ref.slot<=32767);
}
struct Timer final {
    bool advancing{};
    std::uint64_t minimum{},maximum{0x134F00C00000ULL},elapsed{},remaining{},anchor{UINT64_MAX};
    float rate{1.0F};
    friend bool operator==(const Timer&,const Timer&)=default;
};
// Native signed current/target fields at active ring +48/+4C. An absent
// progress command preserves the original -1 sentinels for other directives.
struct Progress final {
    std::int32_t current{}, target{};
    friend bool operator==(const Progress&, const Progress&) = default;
};
[[nodiscard]] constexpr bool valid(const Progress& p) noexcept {
    return p.target > 0 && p.current >= 0 && p.current <= p.target;
}
// Original 4C9200 initializes maximum from 31,536,000 seconds using float32
// conversion (35F080). It is a generic upper bound, not the run duration.
[[nodiscard]] inline bool valid(const Timer& t) noexcept {
    return std::isfinite(t.rate) && t.minimum<=t.elapsed && t.elapsed<=t.maximum
        && t.maximum!=UINT64_MAX && t.remaining!=UINT64_MAX && t.anchor!=UINT64_MAX;
}
// Only the proven native type4 placement reference is exposed here. The
// bubble qualifies descriptor admission; it is not serialized into the reference.
struct PlacementTarget final {
    std::uint32_t registry{};std::uint16_t slot{};std::uint8_t bubble{};
    friend bool operator==(const PlacementTarget&,const PlacementTarget&)=default;
};
[[nodiscard]] constexpr bool valid(const PlacementTarget& t) noexcept {
    return t.registry ? t.registry!=UINT32_MAX && t.registry!=kAbsent && t.slot<=32767 && t.bubble<64
                      : t.slot==0 && t.bubble==0;
}
struct Request final {
    std::uint32_t registry{},event{};
    std::uint16_t slot{};
    std::uint8_t ring{};
    // Native registry admission scope; not an additional authority wire field.
    // UINT32_MAX preserves the existing top-level directive contract.
    std::uint32_t scope{UINT32_MAX};
    std::int32_t variant{};
    bool hasTimer{};
    Timer timer{};
    // Native top-level reference at decoded offset0, separate from per-ring
    // navigation targets. Default absent retains existing authority bytes.
    Reference readiness{};
    std::array<PlacementTarget,4> navigation{};
    // Retain the active ring index while publishing its native absent event.
    // event/variant retain the exact manager identity being deactivated.
    bool clear{};
    bool hasProgress{};
    Progress progress{};
    // Native second top-level reference: the type71 participant presentation
    // sensor read by1008B40. Independent of type70 readiness and navigation.
    Reference publicEvent{};
    friend bool operator==(const Request&,const Request&)=default;
};
struct Batch final {std::array<Request,4> entries{};std::size_t count{};};
[[nodiscard]] inline bool valid(const Request& request) noexcept {
    for(std::size_t i=0;i<request.navigation.size();++i) {
        const auto& target=request.navigation[i];if(!valid(target))return false;
        for(std::size_t j=0;j<i;++j)
            if(target.registry && target.registry==request.navigation[j].registry
                && target.slot==request.navigation[j].slot)return false;
    }
    return request.registry && request.registry!=UINT32_MAX && request.registry!=kAbsent
        && request.event && request.event!=UINT32_MAX && request.event!=kAbsent
        && request.slot<=32767 && request.ring<3
        && (request.scope==UINT32_MAX || request.scope<=63)
        && request.variant>=0 && (!request.hasTimer || valid(request.timer)) && valid(request.readiness)
        && (!request.hasProgress || valid(request.progress)) && valid(request.publicEvent)
        && (request.publicEvent==Reference{} || (request.publicEvent.registry==request.registry && request.publicEvent.type==71));
}
[[nodiscard]] inline const Request* find(const Batch& b,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=68 || b.count>b.entries.size())return nullptr;
    for(std::size_t i=0;i<b.count;++i)if(b.entries[i].registry==key && b.entries[i].slot==slot)return &b.entries[i];
    return nullptr;
}
template<class Roster> [[nodiscard]] bool valid(const Batch& b,const Roster& roster) noexcept {
    if(b.count>b.entries.size() || roster.groupCount>roster.groups.size() || roster.topLevelGroupCount>roster.groupCount)return false;
    for(std::size_t i=0;i<b.count;++i) {
        const auto& request=b.entries[i];if(!valid(request))return false;
        for(std::size_t j=0;j<i;++j)
            if(b.entries[j].registry==request.registry && b.entries[j].slot==request.slot)return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& group=roster.groups[g];if(group.key!=request.registry)continue;
            if((request.scope==UINT32_MAX ? g>=roster.topLevelGroupCount : g<roster.topLevelGroupCount)
                || group.slotTypes.size()!=group.slotIndices.size()
                || group.slotFlags.size()!=group.slotIndices.size())return false;
            ++groups;
            for(std::size_t s=0;s<group.slotIndices.size();++s)if(group.slotIndices[s]==request.slot) {
                if(group.slotTypes[s]!=68 || (group.slotFlags[s]&2)==0)return false;
                ++slots;
            }
        }
        unsigned admissions{};
        for(const auto& block:roster.bubbleSubBlocks)
            for(std::size_t k=0;k<block.keys.size();++k)if(block.keys[k]==request.registry) {
                if(request.scope==UINT32_MAX || block.bubble!=request.scope
                    || (!block.presence.empty() && (block.presence.size()!=block.keys.size() || block.presence[k]!=1)))return false;
                ++admissions;
            }
        if(request.scope!=UINT32_MAX && admissions!=1)return false;
        if(groups!=1 || slots!=1)return false;
        for(const auto& target:request.navigation)if(target.registry) {
            unsigned targetGroups{},targetSlots{},targetAdmissions{};
            for(std::size_t g=0;g<roster.groupCount;++g) {
                const auto& group=roster.groups[g];if(group.key!=target.registry)continue;
                if(g<roster.topLevelGroupCount || group.slotTypes.size()!=group.slotIndices.size()
                    || group.slotFlags.size()!=group.slotIndices.size())return false;
                ++targetGroups;
                for(std::size_t s=0;s<group.slotIndices.size();++s)if(group.slotIndices[s]==target.slot) {
                    if(group.slotTypes[s]!=4 || !(group.slotFlags[s]&2))return false;
                    ++targetSlots;
                }
            }
            for(const auto& block:roster.bubbleSubBlocks)for(std::size_t k=0;k<block.keys.size();++k)
                if(block.keys[k]==target.registry) {
                    if(block.bubble!=target.bubble || (!block.presence.empty()
                        && (block.presence.size()!=block.keys.size() || block.presence[k]!=1)))return false;
                    ++targetAdmissions;
                }
            if(targetGroups!=1 || targetSlots!=1 || targetAdmissions!=1)return false;
        }
        for(const auto& reference:std::array{request.readiness,request.publicEvent})if(reference!=Reference{}) {
            // The first bounded capability references the same exact admitted
            // registry. Cross-registry lifetime/region transfer is not inferred.
            if(reference.registry!=request.registry)return false;
            unsigned targets{};
            for(std::size_t g=0;g<roster.groupCount;++g) {
                const auto& group=roster.groups[g];if(group.key!=reference.registry)continue;
                for(std::size_t s=0;s<group.slotIndices.size();++s)
                    if(group.slotIndices[s]==reference.slot) {
                        if(group.slotTypes[s]!=reference.type || (group.slotFlags[s]&1)==0)return false;
                        ++targets;
                    }
            }
            if(targets!=1)return false;
        }
    }
    return true;
}
template<class Writer> [[nodiscard]] bool write(Writer& w,const Request& request) noexcept {
    if(!valid(request))return false;
    const auto ref=[&]() {return w.write(kAbsent,32) && w.write(0,7) && w.write(32767,16);};
    if(request.readiness==Reference{}) {if(!ref())return false;}
    else if(!w.write(request.readiness.registry,32) || !w.write(static_cast<unsigned>(request.readiness.type)+1U,7)
        || !w.write(static_cast<unsigned>(request.readiness.slot)+32768U,16))return false;
    if(request.publicEvent==Reference{}) {if(!ref())return false;}
    else if(!w.write(request.publicEvent.registry,32) || !w.write(static_cast<unsigned>(request.publicEvent.type)+1U,7)
        || !w.write(static_cast<unsigned>(request.publicEvent.slot)+32768U,16))return false;
    for(std::size_t i=0;i<3;++i) {
        const bool active=i==request.ring && !request.clear;
        const bool timed=active && request.hasTimer;
        if(!w.write(active?request.event:kAbsent,32)
            || !w.write(0x80000000U+static_cast<std::uint32_t>(active?request.variant:0),32)
            || !w.write(active?1U:0U,2) || !w.write(timed && request.timer.advancing,1))return false;
        // Original decoder proof: high 32-bit lane first in the bitstream.
        // Use two width-32 calls so Writer::write(value,64) lane conventions
        // cannot silently swap asymmetric timestamp or duration values.
        const auto scalar64=[&](std::uint64_t value) {
            return w.write(value>>32,32) && w.write(static_cast<std::uint32_t>(value),32);
        };
        if(timed) {
            const auto& t=request.timer;
            if(!scalar64(t.minimum) || !scalar64(t.maximum) || !scalar64(t.elapsed)
                || !scalar64(t.remaining) || !scalar64(t.anchor) || !w.write(std::bit_cast<std::uint32_t>(t.rate),32))return false;
        } else {
            for(unsigned j=0;j<5;++j)if(!scalar64(UINT64_MAX))return false;
            if(!w.write(0,32))return false;
        }
        for(unsigned j=0;j<4;++j) {
            const auto value = active && request.hasProgress && j < 2
                ? (j == 0 ? request.progress.current : request.progress.target) : -1;
            if(!w.write(0x80000000U + static_cast<std::uint32_t>(value),32))return false;
        }
        if(!w.write(1,2) || !ref() || !w.write(1,3))return false;
        for(unsigned j=0;j<4;++j) {
            const auto& target=request.navigation[j];
            if(active && target.registry) {
                if(!w.write(target.registry,32) || !w.write(5,7) || !w.write(target.slot+32768U,16))return false;
            } else if(!ref())return false;
            if(!ref())return false;
            for(unsigned k=0;k<4;++k)if(!w.write(kAbsent,32))return false;
            if(!w.write(0,1))return false;
        }
    }
    return w.write(static_cast<unsigned>(request.ring)+1U,3);
}
using Decoded=std::array<std::byte,kDecodedBytes>;
// Canonical original-decoder image with zero-filled storage. The decoder leaves
// 141 padding bytes untouched; they cannot be presumed zero in a native packet.
[[nodiscard]] inline Decoded decoded(const Request& request) noexcept {
    Decoded result{};
    if(!valid(request))return result;
    const auto put=[&](std::size_t offset,auto value) {std::memcpy(result.data()+offset,&value,sizeof(value));};
    const auto ref=[&](std::size_t offset) {put(offset,kAbsent);result[offset+4]=std::byte{255};
        result[offset+6]=result[offset+7]=std::byte{255};};
    ref(0);ref(8);
    if(request.readiness!=Reference{}) {
        put(0,request.readiness.registry);put(4,request.readiness.type);put(6,request.readiness.slot);
    }
    if(request.publicEvent!=Reference{}) {
        put(8,request.publicEvent.registry);put(12,request.publicEvent.type);put(14,request.publicEvent.slot);
    }
    for(std::size_t i=0;i<3;++i) {
        const auto at=0x10+i*0xF8;put(at,i==request.ring && !request.clear?request.event:kAbsent);
        put(at+4,i==request.ring && !request.clear?request.variant:0);
        result[at+8]=i==request.ring && !request.clear?std::byte{}:std::byte{255};
        for(unsigned j=0;j<5;++j)put(at+0x18+j*8,UINT64_MAX);
        if(i==request.ring && !request.clear && request.hasTimer) {
            const auto& t=request.timer;result[at+0x10]=t.advancing?std::byte{1}:std::byte{};
            put(at+0x18,t.minimum);put(at+0x20,t.maximum);put(at+0x28,t.elapsed);
            put(at+0x30,t.remaining);put(at+0x38,t.anchor);put(at+0x40,t.rate);
        }
        for(unsigned j=0;j<4;++j)put(at+0x48+j*4,UINT32_MAX);
        if(i==request.ring && !request.clear && request.hasProgress) {
            put(at+0x48,request.progress.current);put(at+0x4C,request.progress.target);
        }
        ref(at+0x5C);
        for(unsigned j=0;j<4;++j) {
            const auto t=at+0x68+j*0x24;ref(t);ref(t+8);
            const auto& target=request.navigation[j];
            if(i==request.ring && !request.clear && target.registry) {
                put(t,target.registry);result[t+4]=std::byte{4};put(t+6,target.slot);
            }
            for(unsigned k=0;k<4;++k)put(t+16+k*4,kAbsent);
        }
    }
    put(0x2F8,static_cast<std::uint32_t>(request.ring));return result;
}
[[nodiscard]] constexpr bool decoder_writes(std::size_t index) noexcept {
    if(index>=kDecodedBytes)return false;
    if(index<0x10)return index%8!=5;
    if(index>=0x2F8)return index<0x2FC;
    const auto at=(index-0x10)%0xF8;
    if(at<9 || at==0x10 || (at>=0x18 && at<0x44) || (at>=0x48 && at<0x59) || at==0x64)return true;
    if(at>=0x5C && at<0x64)return at!=0x61;
    if(at>=0x68) {const auto target=(at-0x68)%0x24;return target<=0x20 && target!=5 && target!=13;}
    return false;
}
[[nodiscard]] inline bool matches_fields(std::span<const std::byte> bytes,const Request& request) noexcept {
    if(bytes.size()!=kDecodedBytes || !valid(request))return false;
    const auto expected=decoded(request);
    for(std::size_t i=0;i<bytes.size();++i)if(decoder_writes(i) && bytes[i]!=expected[i])return false;
    return true;
}
[[nodiscard]] constexpr std::uint32_t manager_id(std::uint32_t event,std::int32_t variant=0) noexcept {
    for(unsigned i=0;i<4;++i)event=(event*0x1000193U)^((static_cast<std::uint32_t>(variant)>>(8*i))&255U);
    return event;
}
}
