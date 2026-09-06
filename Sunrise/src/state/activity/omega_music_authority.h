#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../middleware/encoding/bit_writer.h"

namespace sunrise::state::activity::omega_music {

inline constexpr std::uint32_t kRegistry=0x82FB58B7U;
inline constexpr std::uint32_t kDefinition=0x80F47BD7U;
inline constexpr std::uint32_t kSchema=0x80804F58U;
inline constexpr std::uint32_t kNeutralKey=0x811C9DC5U;
inline constexpr std::size_t kCandidateCount=19;
inline constexpr std::size_t kReferenceCapacity=128;
inline constexpr std::size_t kAuthorityBits=7223;
inline constexpr std::size_t kAuthorityBytes=(kAuthorityBits+7U)/8U;
inline constexpr std::uint32_t kCandidateMask=(1U<<kCandidateCount)-1U;

[[nodiscard]] constexpr bool is_sensor(std::uint32_t key,std::uint8_t type,
                                       std::uint16_t slot) noexcept {
    return key==kRegistry && type==11 && slot==1;
}

/** Authored candidate identities in priority order, not mission chronology.
 * The native selector chooses the highest qualifying ordinal. These are not
 * Wwise event IDs and never replace a reference's registry key on the wire. */
inline constexpr std::array<std::uint32_t,kCandidateCount> kCandidateKeys{{
    0xB7EF9E4AU,0xCC9D4AA0U,0xDEA7CE73U,0x59EF8D18U,0xEBDA8BDBU,
    0xEFD979EFU,0x653635A2U,0xAEE7F815U,0x042E4DDDU,0x22F98E82U,
    0x4F781BF9U,0x5F1F1B11U,0x9A90DB0EU,0xB7362D8FU,0x1722E4F9U,
    0xFA6536F3U,0x470BDE8CU,0xF186BD75U,0x926A1630U,
}};

/** 80809C42 is55wirebits, not an eight-byte memory copy. */
struct Reference final {
    std::uint32_t registry{kNeutralKey};
    std::int8_t type{-1};
    std::int16_t index{-1};
    constexpr bool operator==(const Reference&) const noexcept=default;
};

[[nodiscard]] constexpr bool neutral(const Reference& ref) noexcept {
    return ref.registry==kNeutralKey && ref.type==-1 && ref.index==-1;
}

[[nodiscard]] constexpr bool valid_reference(const Reference& ref) noexcept {
    return neutral(ref) || (ref.registry!=0 && ref.registry!=kNeutralKey
        && ref.type>=0 && ref.type<=126 && ref.index>=0);
}

[[nodiscard]] constexpr bool valid_guard(const Reference& ref) noexcept {
    if(neutral(ref)) { return true; }
    return valid_reference(ref) && (ref.type==5 || ref.type==33 || ref.type==34
        || ref.type==57 || ref.type==60);
}

/** Omega has one selector group with19 candidates. Clear its mask to relinquish
 * music; authored silence candidates2/18 are distinct selected bank states.
 * A valid guard is evaluated while its derived bit is unset. Native DEF9F0
 * retains a qualified bit until the host clears that candidate's mask bit.
 * The readiness reference gates native reconciliation; leave it neutral unless
 * a specific authored readiness dependency has been proved. */
struct Authority final {
    std::uint32_t activeCandidates{};
    std::array<Reference,kCandidateCount> guards{};
    Reference readiness{};
    constexpr bool operator==(const Authority&) const noexcept=default;
};

[[nodiscard]] constexpr bool valid(const Authority& authority) noexcept {
    if((authority.activeCandidates&~kCandidateMask)!=0 || !valid_reference(authority.readiness)) {
        return false;
    }
    for(const auto& guard:authority.guards) {
        if(!valid_guard(guard)) { return false; }
    }
    return true;
}

/** Explicit host selection. No mission phase-to-candidate mapping is inferred. */
[[nodiscard]] constexpr bool select(Authority& authority,std::size_t candidate) noexcept {
    if(candidate>=kCandidateCount) { return false; }
    authority.activeCandidates=std::uint32_t{1}<<candidate;
    return true;
}

[[nodiscard]] inline bool write_reference(middleware::encoding::bits::Writer& writer,
                                          const Reference& ref) noexcept {
    return writer.write(ref.registry,32)
        && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.type)+1),7)
        && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.index)+0x8000),16);
}

/** Full fixed native authority body. No presence prefix or event command belongs
 * inside this schema. The caller supplies the normal type11 authority envelope. */
[[nodiscard]] inline bool write(middleware::encoding::bits::Writer& writer,
                                const Authority& authority) noexcept {
    if(!valid(authority)) { return false; }
    const auto start=writer.bit_count();
    if(!writer.write(authority.activeCandidates,32) || !writer.write(0,32)
        || !writer.write(0,32) || !writer.write(0,32)) { return false; }
    for(std::size_t i=0;i<kReferenceCapacity;++i) {
        if(!write_reference(writer,i<kCandidateCount?authority.guards[i]:Reference{})) { return false; }
    }
    return write_reference(writer,authority.readiness) && writer.bit_count()-start==kAuthorityBits;
}

} // namespace sunrise::state::activity::omega_music
