#pragma once
#include <cstdint>
#include <array>
#include <cstddef>
#include <span>
#include "../../../middleware/encoding/bit_reader.h"
namespace dawn::state::activity::coo::native_player_trigger {
inline constexpr std::uint32_t kIncident=6685;
inline constexpr std::size_t kAuthBits=129,kPayloadBytes=53;
struct Receipt final { std::uint32_t registry{},object{};std::int16_t slot{-1};std::int8_t type{-1}; };
struct Request final { std::uint32_t registry{};std::uint16_t slot{};std::uint8_t bubble{};std::uint64_t generation{}; };
struct Batch final { std::array<Request,8> entries{};std::size_t count{}; };
/** Schema 8080879F carries the firing type-31 ClientRef after its common incident header. */
[[nodiscard]] inline bool decode(std::span<const std::byte> payload,Receipt& result) noexcept {
    result={};if(payload.size()!=kPayloadBytes) { return false; }
    middleware::encoding::bits::Reader reader(payload);
    std::uint64_t registry{},type{},slot{},object{},padding{};
    if(!reader.skip(335) || !reader.read(32,registry) || !reader.read(7,type)
        || !reader.read(16,slot) || !reader.read(32,object) || !reader.read(2,padding)
        || padding!=0 || reader.remaining_bits()!=0) { return false; }
    result={static_cast<std::uint32_t>(registry),static_cast<std::uint32_t>(object),
        static_cast<std::int16_t>(static_cast<int>(slot)-32768),
        static_cast<std::int8_t>(static_cast<int>(type)-1)};
    return result.registry!=0 && result.registry!=0x811C9DC5U && result.type==31 && result.slot>=0;
}
/** Arms the authored sensor; its native volume supplies the eventual incident. */
template<class Writer> [[nodiscard]] bool arm(Writer& writer,std::uint64_t generation) noexcept {
    return generation!=0 && writer.write(1,1) && writer.write(generation,64) && writer.write(0,64);
}
template<class Roster> [[nodiscard]] bool valid(const Batch& batch,const Roster& roster,
    std::uint32_t region) noexcept {
    (void)region;
    if(batch.count>batch.entries.size() || roster.groupCount>roster.groups.size())return false;
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& request=batch.entries[i];
        if(!request.registry || request.slot>32767 || request.bubble>63 || !request.generation)return false;
        unsigned groups{},slots{};
        for(std::size_t g=0;g<roster.groupCount;++g) {
            const auto& group=roster.groups[g];
            if(group.key!=request.registry)continue;
            ++groups;
            if(group.slotTypes.size()!=group.slotIndices.size()
                || group.slotFlags.size()!=group.slotIndices.size())return false;
            for(std::size_t s=0;s<group.slotIndices.size();++s)
                if(group.slotIndices[s]==request.slot) {
                    if(group.slotTypes[s]!=31 || (group.slotFlags[s]&2)==0)return false;
                    ++slots;
                }
        }
        if(groups!=1 || slots!=1)return false;
    }
    return true;
}
inline bool append(Batch& output,std::uint32_t registry,std::uint16_t slot,std::uint8_t bubble,
    std::uint64_t generation) noexcept {
    if(!registry || !generation || bubble>63 || output.count>output.entries.size())return false;
    for(std::size_t i=0;i<output.count;++i)
        if(output.entries[i].registry==registry && output.entries[i].slot==slot)return false;
    if(output.count==output.entries.size())return false;
    output.entries[output.count++]={registry,slot,bubble,generation};return true;
}
inline const Request* find(const Batch& batch,std::uint32_t registry,std::uint16_t slot) noexcept {
    if(batch.count>batch.entries.size())return nullptr;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].registry==registry && batch.entries[i].slot==slot)return &batch.entries[i];
    return nullptr;
}
}
