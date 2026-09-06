#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace sunrise::state::activity::coo {
enum class Admission : std::uint8_t { ignored, accepted, overflow };

// The mission selects cohorts and native summon timing. This ledger owns
// admission, salted identity retention, verified deaths, and clearance joins.
template<class Receipt, std::size_t Groups, std::size_t MaximumPopulation>
class PopulationService final {
    static_assert(Groups > 0 && MaximumPopulation > 0 && MaximumPopulation <= UINT8_MAX);
    static_assert(std::is_trivially_copyable_v<Receipt>);
public:
    void enable(std::size_t index) noexcept { if (index < Groups) { enabled_[index] = true; } }
    [[nodiscard]] bool enabled(std::size_t index) const noexcept { return index < Groups && enabled_[index]; }
    template<class Catalog>
    [[nodiscard]] bool source_enabled(const Catalog& catalog, std::uint16_t source, std::uint32_t registry) const noexcept {
        if (catalog.size() != Groups) { return false; }
        for (std::size_t i = 0; i < Groups; ++i) {
            if (catalog[i].source == source && catalog[i].registry == registry && enabled_[i]) { return true; }
        }
        return false;
    }
    template<class Catalog>
    [[nodiscard]] Admission admit(const Catalog& catalog, const Receipt& receipt,
                                  std::uint64_t run, std::uint32_t generation) noexcept {
        if (!receipt.valid() || receipt.run != run || receipt.generation != generation
            || !source_enabled(catalog, receipt.source, receipt.registry)) { return Admission::ignored; }
        for (std::size_t i = 0; i < Groups; ++i) for (std::uint8_t n = 0; n < counts_[i]; ++n) {
            if (actors_[i][n].receipt.actor == receipt.actor) { return Admission::ignored; }
        }
        for (std::size_t i = 0; i < Groups; ++i) {
            const auto& group = catalog[i];
            if (group.source == receipt.source && group.registry == receipt.registry && enabled_[i]
                && counts_[i] < group.count) {
                if (counts_[i] >= MaximumPopulation) { return Admission::overflow; }
                actors_[i][counts_[i]++].receipt = receipt;
                return Admission::accepted;
            }
        }
        // Excess actors from a nonblocking cohort cannot become a kill barrier.
        for (std::size_t i = 0; i < Groups; ++i) {
            const auto& group = catalog[i];
            if (group.source == receipt.source && group.registry == receipt.registry && enabled_[i] && !group.required) {
                return Admission::ignored;
            }
        }
        return Admission::overflow;
    }
    [[nodiscard]] bool died(const Receipt& receipt, std::uint64_t run, std::uint32_t generation) noexcept {
        if (!receipt.valid() || receipt.run != run || receipt.generation != generation) { return false; }
        for (std::size_t i = 0; i < Groups; ++i) for (std::uint8_t n = 0; n < counts_[i]; ++n) {
            if (actors_[i][n].receipt == receipt) {
                if (actors_[i][n].dead) { return false; }
                actors_[i][n].dead = true; return true;
            }
        }
        return false;
    }
    [[nodiscard]] bool cleared(std::size_t index, std::uint8_t requested) const noexcept {
        if (index >= Groups || !enabled_[index] || counts_[index] != requested) { return false; }
        for (std::uint8_t n = 0; n < counts_[index]; ++n) { if (!actors_[index][n].dead) { return false; } }
        return true;
    }
private:
    struct Actor final { Receipt receipt{}; bool dead{}; };
    std::array<std::array<Actor, MaximumPopulation>, Groups> actors_{};
    std::array<std::uint8_t, Groups> counts_{};
    std::array<bool, Groups> enabled_{};
};
} // namespace sunrise::state::activity::coo
