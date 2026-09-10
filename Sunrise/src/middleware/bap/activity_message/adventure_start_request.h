#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../activity_host_manager/request/selection/internal.h"

namespace sunrise::middleware::bap::activity_message::adventure_start {

inline constexpr std::uint32_t kMessageType=11;
inline constexpr std::uint32_t kSchema=0x80808698;
inline constexpr std::uint32_t kDescriptorSchema=0x80808716;
namespace descriptor=activity_host_manager::request::selection;

// The type-11 body is exactly one descriptor, without the service-6 protobuf,
// opcode, request-kind or trailing request flag. Preserve the full native bits.
struct Request final {
    descriptor::ActivityManagerSelection selection{};
    std::uint64_t account{};
    std::uint64_t nonce{};
    std::uint8_t revision{};
    bool hasAccount{};
    bool hasNonce{};
};

// This parser does not decide Adventure/account eligibility and does not launch
// anything. The caller must bind the request to its authenticated live owner.
[[nodiscard]] inline bool parse(std::span<const std::byte> bytes,Request& output) noexcept {
    output={};
    if(bytes.empty() || bytes.size()>descriptor::kActivityManagerDescriptorCapacity) return false;
    Request parsed{};
    encoding::bits::Reader reader(bytes);
    if(!descriptor::parse(reader,parsed.selection)) return false;
    const auto consumed=bytes.size()*8-reader.remaining_bits();
    if(reader.remaining_bits()>7) return false;
    std::uint64_t value{};
    if(reader.remaining_bits()!=0 && (!reader.read(static_cast<std::uint8_t>(reader.remaining_bits()),value) || value!=0)) return false;
    if(!descriptor::copy_bits(bytes,0,consumed,parsed.selection.descriptorBits,
                             parsed.selection.descriptorBitLength)) return false;

    // The common service-6 parser intentionally skips identities and revision.
    // Read their exact schema fields for owner/nonce/replay validation by State.
    encoding::bits::Reader prefix(bytes);
    if(!prefix.skip(28) || !descriptor::skip_optional(prefix,9)
       || !descriptor::read_presence(prefix,parsed.hasAccount)) return false;
    if(parsed.hasAccount && !prefix.read(64,parsed.account)) return false;
    if(!descriptor::read_presence(prefix,parsed.hasNonce)) return false;
    if(parsed.hasNonce && !prefix.read(64,parsed.nonce)) return false;
    std::uint64_t skulls{};
    if(!prefix.read(5,skulls) || skulls>descriptor::kActivityManagerSkullCapacity
       || !prefix.skip(static_cast<std::size_t>(skulls)*9) || !prefix.read(8,value)) return false;
    parsed.revision=static_cast<std::uint8_t>(value);

    // A host transaction must not reinterpret a malformed suffix as a valid
    // package prefix. Require a terminated canonical native 40-byte name.
    if(parsed.selection.hasPackageName) {
        encoding::bits::Reader name(bytes);
        if(!name.skip(parsed.selection.packageNameBitOffset)) return false;
        bool ended=false;
        for(std::size_t i=0;i<parsed.selection.packageName.size();++i) {
            if(!name.read(8,value)) return false;
            const auto character=static_cast<std::uint8_t>(value+128U);
            if(character==0) { ended=true; continue; }
            if(ended || !((character>='a' && character<='z')
                         || (character>='0' && character<='9') || character=='_')) return false;
        }
        if(!ended) return false;
        // 8080872A is 0x84 bytes: a count plus 32 four-byte entries.
        // The shared inspection parser walks wider counts, but replaying them
        // into the native fixed-size descriptor is not a valid host response.
        if(!descriptor::skip_optional(name,32) || !name.skip(1)) return false;
        bool present{};
        if(!descriptor::read_presence(name,present)) return false;
        if(present) {
            if(!name.read(1,value) || !name.skip(static_cast<std::size_t>(value)*96)) return false;
        }
        if(!descriptor::read_presence(name,present)) return false;
        if(present && (!name.read(6,value) || value>32)) return false;
    }
    output=parsed;
    return true;
}
} // namespace sunrise::middleware::bap::activity_message::adventure_start
