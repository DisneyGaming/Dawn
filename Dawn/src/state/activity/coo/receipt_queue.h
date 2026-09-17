#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

namespace dawn::state::activity::coo {
// Admission and native ownership checks precede push. Coalescing may retain
// only an equivalent adjacent receipt; it never replaces the earliest time.
// Overflow remains latched until the owner explicitly resets the queue.
template<class Receipt, std::size_t Capacity = 128>
class ReceiptQueue final {
    static_assert(Capacity > 0 && std::is_trivially_copyable_v<Receipt>);
public:
    [[nodiscard]] bool push(const Receipt& receipt) noexcept {
        if (overflow_) { return false; }
        if (count_ == Capacity) { overflow_ = true; return false; }
        values_[(head_ + count_) % Capacity] = receipt;
        ++count_;
        return true;
    }
    template<class Equivalent>
    [[nodiscard]] bool push(const Receipt& receipt, Equivalent equivalent) noexcept {
        if (overflow_) { return false; }
        if (const auto* previous = back(); previous && equivalent(*previous, receipt)) { return true; }
        return push(receipt);
    }
    [[nodiscard]] const Receipt* back() const noexcept {
        return count_ == 0 ? nullptr : &values_[(head_ + count_ - 1) % Capacity];
    }
    [[nodiscard]] bool pop(Receipt& receipt) noexcept {
        if (count_ == 0) { return false; }
        receipt = values_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return true;
    }
    void discard() noexcept { head_ = count_ = 0; }
    void reset() noexcept { *this = {}; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool overflowed() const noexcept { return overflow_; }
private:
    std::array<Receipt, Capacity> values_{};
    std::size_t head_{}, count_{};
    bool overflow_{};
};
} // namespace dawn::state::activity::coo
