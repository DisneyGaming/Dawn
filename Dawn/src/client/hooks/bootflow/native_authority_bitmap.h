#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dawn::client::hooks::bootflow::native_authority_bitmap {

// The native owner bitmap has one bit per 13-bit entity slot. A view is scoped
// to one worker tick: validate the image-backed range once, then read each live
// bit. Do not cache bit values or entity identities between ticks.
class View final {
public:
    static constexpr std::size_t kBytes=(1U<<13U)/8U;
    template<class Readable>
    [[nodiscard]] static View acquire(const std::byte* table,Readable&& readable) noexcept {
        return View{table && readable(table,kBytes)?table:nullptr};
    }
    [[nodiscard]] explicit operator bool() const noexcept {return table_!=nullptr;}
    [[nodiscard]] bool missing(std::uint32_t entity) const noexcept {
        if(!table_ || entity==UINT32_MAX)return false;
        std::uint32_t word{};
        std::memcpy(&word,table_+((entity&0x1FFFU)>>5U)*sizeof word,sizeof word);
        return (word&(1U<<(entity&31U)))==0;
    }
private:
    explicit View(const std::byte* table) noexcept:table_(table){}
    const std::byte* table_{};
};

} // namespace dawn::client::hooks::bootflow::native_authority_bitmap
