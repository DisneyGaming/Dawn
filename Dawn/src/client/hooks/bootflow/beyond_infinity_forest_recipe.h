#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace dawn::client::hooks::bootflow::beyond_forest {
inline constexpr std::uint32_t kConfiguration=0x80F4D0F1U;
inline constexpr std::size_t kWorkerBytes=0x9BD;
// The lookup uses definition class80804EF6; its returned address contains the
// paired runtime class80804EF7. Verified in package80F460FA and the live run.
inline constexpr bool sensor_identity(std::uint32_t tag,std::uint32_t kind,std::int64_t offset) noexcept {
    return tag==0x80F460FAU && kind==0x80804EF7U && offset==0xD68;
}
template<class T> T read(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};std::memcpy(&value,bytes.data()+offset,sizeof value);return value;
}
inline bool matches(std::span<const std::byte> worker) noexcept {
    return worker.size()>=kWorkerBytes && read<std::uint32_t>(worker,4)==0x80804FECU
        && read<std::uint32_t>(worker,0x96C)==kConfiguration;
}
inline constexpr std::uint32_t seed(std::uint64_t run,std::uint32_t generation,std::uint8_t pass) noexcept {
    auto value=run^(static_cast<std::uint64_t>(generation)<<32)^(0x9E3779B97F4A7C15ULL*pass);
    value=(value^(value>>30))*0xBF58476D1CE4E5B9ULL;
    value=(value^(value>>27))*0x94D049BB133111EBULL;
    const auto result=static_cast<std::uint32_t>(value^(value>>31))&0x7FFFFFFFU;
    return result?result:1U;
}
// 80F4D0F2 / 80804FED @17D8 supplies A's anchors. FFE820 copies them to
// 970..993. FF2F80 uses these as east/west/north/south edges of the grid.
// Forest A uses nine 15m cells per anchor row. Its fixed Past and Future
// corridors both meet row0 at height0; the generic prefab defaults are not
// mission endpoints. The active flag initially opens only the entrance gate.
inline bool prepare(std::span<std::byte> worker,std::uint32_t layoutSeed,std::uint8_t pass) noexcept {
    if(!matches(worker) || !layoutSeed || layoutSeed>0x7FFFFFFFU || pass<1 || pass>2) { return false; }
    const std::uint32_t mask=0;
    std::memcpy(worker.data()+0x948,&mask,4);std::memcpy(worker.data()+0x94C,&layoutSeed,4);
    constexpr std::array<std::size_t,4> columns{0x970,0x979,0x981,0x989};
    constexpr std::array<std::size_t,4> weights{0x974,0x97C,0x984,0x98C};
    constexpr std::array<std::size_t,4> starts{0x978,0x980,0x988,0x990};
    const std::array<std::int8_t,4> coordinates=pass==1?std::array<std::int8_t,4>{-1,0,1,-1}:std::array<std::int8_t,4>{0,0,-1,-1};
    constexpr std::array<std::int8_t,4> heights{0,0,1,0};
    const auto entrance=pass==1?2U:1U,target=pass==1?1U:0U;
    for(std::size_t i=0;i<columns.size();++i) {
        const float weight=i==target?1.F:0.F;
        std::memcpy(worker.data()+columns[i],&coordinates[i],1);
        std::memcpy(worker.data()+columns[i]+1,&heights[i],1);
        std::memcpy(worker.data()+weights[i],&weight,4);
        worker[starts[i]]=i==entrance?std::byte{1}:std::byte{0};
    }
    return true;
}
// Only command fields in the decoded authority record are written. The native
// tick owns effective state, regeneration, encounter completion and gateways.
inline bool enable(std::span<std::byte> record,bool on) noexcept {
    if(record.size()<0x2E) { return false; }
    record[0x2C]=static_cast<std::byte>(read<std::uint8_t>(record,0x2C)|8U);
    record[0x2D]=on?std::byte{1}:std::byte{0};return true;
}
}
