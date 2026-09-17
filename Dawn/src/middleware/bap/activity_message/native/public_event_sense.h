#pragma once
#include <cstddef>
#include <cstdint>

namespace dawn::middleware::bap::activity_message::native::public_event_sense {
inline constexpr std::uint32_t kSchema=0x80804F56;
struct Output final { std::uint32_t revision{}; bool root{},value{}; };
// 80804F56 at executable reflection RVA 3918258 has exactly one bool.
// Preserve it as a raw observation. Consumer BF73D0 can set this flag when a
// monitored native identity vanishes; it is not an event-success/death receipt.
// The enclosing native sense codec appends revision32 after the optional root.
template<class Reader>
[[nodiscard]] bool read(Reader& reader,Output& output,std::size_t& width) noexcept {
    Output result{};
    const auto before=reader.remaining_bits();
    std::uint64_t value{};
    if (!reader.read(1,value)) return false;
    result.root=value!=0;
    if (result.root) {
        if (!reader.read(1,value)) return false;
        result.value=value!=0;
    }
    if (!reader.read(32,value)) return false;
    result.revision=static_cast<std::uint32_t>(value);
    width=before-reader.remaining_bits(); output=result; return true;
}
} // namespace dawn::middleware::bap::activity_message::native::public_event_sense
