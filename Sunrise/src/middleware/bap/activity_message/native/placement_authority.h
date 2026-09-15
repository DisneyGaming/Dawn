#pragma once
#include "retained_authority_scope.h"
#include "../../../encoding/bit_writer.h"
#include "public_event_interaction_authority.h"
#include "capture_controller_authority.h"
#include "generic_device_authority.h"
#include <array>
#include <optional>
#include <bit>
#include <cmath>

namespace sunrise::middleware::bap::activity_message::native::placement {
// Schema 8080992F. The 253-bit compatibility tuple keeps the established raw
// sentinels, transform-override flag1 and one absent override record used by
// Mercury/Omega portals. It is not the authored-transform contract. Requests
// with an explicit positive generation instead use the reflected252-bit form:
// native source generation, authored transform and an empty override list.
inline constexpr std::size_t kActiveBits=253;
inline constexpr std::size_t kAuthoredActiveBits=252;
[[nodiscard]] constexpr bool valid_generation(std::uint32_t generation) noexcept {
    return generation<=0x7FFFFFFFU;
}
template<class Writer> [[nodiscard]] bool write_active(Writer& writer,
    interaction::Mode mode=interaction::Mode::unchanged) noexcept {
    if(!interaction::valid(mode))return false;
    return writer.write(0,32) && writer.write(1,32)
        && writer.write(1,1) && writer.write(1,1) && writer.write(0,32)
        && writer.write(0x811C9DC5U,32) && writer.write(0,7) && writer.write(32767,16)
        && writer.write(0,32) && writer.write(0,32) && writer.write(0,32)
        && writer.write(0,1) && writer.write(1,2) && interaction::write_record(writer,mode);
}
struct Position final {float x{},y{},z{};};
struct Request final {
    std::uint32_t registry{};std::uint16_t slot{};std::uint8_t bubble{};
    interaction::Mode interactionMode{interaction::Mode::unchanged};
    // Zero retains the established compatibility tuple. Positive generations
    // opt into the reflected source contract needed by deferred placements.
    std::uint32_t generation{};
    std::optional<capture_controller::State> capture{};
    // Positive-generation authored sources can withdraw their native child.
    // The default preserves every existing active authority byte.
    bool active{true};
    // Native auth+9 and cached XYZ at +20. With an absent reference this
    // replaces translation only; the authored rotation and W remain intact.
    std::optional<Position> position{};
    std::optional<generic_device::State> pose{};
};
[[nodiscard]] constexpr std::size_t body_bits(const Request& request) noexcept {
    if(!interaction::valid(request.interactionMode) || !valid_generation(request.generation)
        || (!request.active && !request.generation)
        || (request.position && (!request.generation || !std::isfinite(request.position->x)
            || !std::isfinite(request.position->y) || !std::isfinite(request.position->z)))
        || (request.capture && (!request.generation || !capture_controller::valid(*request.capture))))return 0;
    if(request.pose && (!request.generation || !generic_device::valid(*request.pose)))return 0;
    if(request.pose)return kAuthoredActiveBits+generic_device::kRecordBits
        +(request.capture?capture_controller::kRecordBits:0)
        +(request.interactionMode==interaction::Mode::unchanged?0:1+interaction::kPayloadBits);
    if(request.capture)return kAuthoredActiveBits+capture_controller::kRecordBits
        +(request.interactionMode==interaction::Mode::unchanged?0:1+interaction::kPayloadBits);
    if(request.generation && request.interactionMode==interaction::Mode::unchanged)return kAuthoredActiveBits;
    return kActiveBits+(request.interactionMode==interaction::Mode::unchanged?0:interaction::kPayloadBits);
}
template<class Writer> [[nodiscard]] bool write(Writer& writer,const Request& request) noexcept {
    if(!body_bits(request))return false;
    if(request.generation) {
        // Reflected 8080992F: f0/f1/f4 are signed32 with INT_MIN wire bias.
        // Native9F2F30 requires committed < generation for deferred sources;
        // raw wire0 decodes INT_MIN and can never advance a committed0 source.
        // Auth+9=false retains the candidate's authored transform (9EFBC0).
        const bool overrides=request.interactionMode!=interaction::Mode::unchanged;
        const auto count=(overrides?1U:0U)+(request.capture?1U:0U)+(request.pose?1U:0U);
        return writer.write(0x80000000U+request.generation,32) && writer.write(0x80000000U,32)
            && writer.write(request.active?1U:0U,1) && writer.write(request.position?1U:0U,1) && writer.write(0x80000000U,32)
            && writer.write(0x811C9DC5U,32) && writer.write(0,7) && writer.write(32767,16)
            && writer.write(request.position?std::bit_cast<std::uint32_t>(request.position->x):0U,32)
            && writer.write(request.position?std::bit_cast<std::uint32_t>(request.position->y):0U,32)
            && writer.write(request.position?std::bit_cast<std::uint32_t>(request.position->z):0U,32)
            && writer.write(0,1) && writer.write(count,2)
            && (!overrides || interaction::write_record(writer,request.interactionMode))
            && (!request.capture || capture_controller::write_record(writer,*request.capture))
            && (!request.pose || generic_device::write_record(writer,*request.pose));
    }
    return write_active(writer,request.interactionMode);
}
struct Batch final { std::array<Request,64> entries{};std::size_t count{}; };
[[nodiscard]] inline const Request* find(const Batch& batch,std::uint32_t key,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(type!=4 || batch.count>batch.entries.size()) return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].registry==key && batch.entries[i].slot==slot) return &batch.entries[i];
    return nullptr;
}
template<class Roster>
[[nodiscard]] bool valid(const Batch& batch,const Roster& roster,std::uint32_t region) noexcept {
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size()) return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!request.registry || request.registry==UINT32_MAX || request.slot>32767 || request.bubble>63
            || !body_bits(request)
            || !authority_scope::valid(roster,request.registry,request.bubble,region)) return false;
        for(std::size_t j=0;j<i;++j)
            if(batch.entries[j].registry==request.registry && batch.entries[j].slot==request.slot) return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& row=roster.groups[g];if(row.key!=request.registry) continue;
            ++groups;
            if(row.slotTypes.size()!=row.slotIndices.size() || row.slotFlags.size()!=row.slotIndices.size()) return false;
            for(std::size_t s=0;s<row.slotIndices.size();++s) if(row.slotIndices[s]==request.slot) {
                if(row.slotTypes[s]!=4 || (row.slotFlags[s]&2)==0) return false;
                ++slots;
            }
        }
        if(groups!=1 || slots!=1) return false;
    }
    return true;
}
}
