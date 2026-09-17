#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "client/hooks/bootflow/omega_forest_recipe.h"
#include "client/hooks/bootflow/omega_forest_scope.h"
#include "client/hooks/bootflow/native_authority_bitmap.h"

namespace forest = dawn::client::hooks::bootflow::omega_forest;
namespace {
unsigned checks{};
unsigned failures{};
void check(bool passed, const char* description) {
    ++checks;
    if (!passed) {
        ++failures;
        std::printf("FAIL: %s\n", description);
    }
}
#define CHECK_AUTHORITY(expression) check(static_cast<bool>(expression), #expression)
void authority_bitmap_checks_are_batched() {
    using View=dawn::client::hooks::bootflow::native_authority_bitmap::View;
    std::array<std::uint32_t,256> words{};
    const auto* bytes=reinterpret_cast<const std::byte*>(words.data());
    unsigned queries{};
    const auto view=View::acquire(bytes,[&](const std::byte* at,std::size_t size) noexcept {
        ++queries;return at==bytes && size==sizeof words;
    });
    CHECK_AUTHORITY(view);CHECK_AUTHORITY(queries==1);
    for(std::uint32_t slot=0;slot<8192;++slot) {
        const auto handle=0x12340000U|slot;
        CHECK_AUTHORITY(view.missing(handle));
        words[slot/32]|=1U<<(slot%32);
        CHECK_AUTHORITY(!view.missing(handle));
    }
    CHECK_AUTHORITY(queries==1); // No per-owner memory-permission queries.
    CHECK_AUTHORITY(!view.missing(UINT32_MAX));
    words[255]&=~(1U<<31);
    CHECK_AUTHORITY(view.missing(0x12341FFFU)); // A cleared live bit is not cached.
    const auto denied=View::acquire(bytes,[](const std::byte*,std::size_t) noexcept {return false;});
    CHECK_AUTHORITY(!denied);CHECK_AUTHORITY(!denied.missing(0));
    const auto absent=View::acquire(nullptr,[&](const std::byte*,std::size_t) noexcept {
        ++queries;return true;
    });
    CHECK_AUTHORITY(!absent);CHECK_AUTHORITY(queries==1);
}
#undef CHECK_AUTHORITY

}

int main() {
    authority_bitmap_checks_are_batched();
    // Regression: Haunted #78 reached native slice 104, whose pending bubble bit is 13.
    // Its otherwise valid-looking forest mask must never authorize an Omega seed override.
    constexpr std::array<std::string_view, 7> otherPackages{
        "infinite_abyss", "mission_abyss_intro", "mercury_freeroam", "",
        "mission_scot_extra", "MISSION_SCOT", "mission_sco"};
    constexpr std::array<std::uint64_t, 5> verdicts{
        0U, 1U, 0x100U, 0x123456789ABCDE00ULL, 0x123456789ABCDE01ULL};
    for (const auto package : otherPackages) {
        for (const bool active : {false, true}) {
            for (const bool joined : {false, true}) {
                const bool scoped = forest::legacy_mutation_allowed(active, joined, package);
                check(!scoped, "non-Omega activity cannot authorize legacy forest mutation");
                for (const auto nativeVerdict : verdicts) {
                    check(forest::legacy_seed_verdict(scoped, nativeVerdict, 0x2000U)
                              == nativeVerdict,
                          "Haunted bubble-13 seed gate preserves complete native result");
                }
            }
        }
    }
    check(!forest::legacy_mutation_allowed(true, false, "mission_scot"),
          "configured Omega default without committed join cannot authorize mutation");
    check(!forest::legacy_mutation_allowed(false, true, "mission_scot"),
          "stale joined Omega in idle world cannot authorize mutation");
    const bool omega = forest::legacy_mutation_allowed(true, true, "mission_scot");
    check(omega, "committed active Omega retains its legacy behavior");

    for (unsigned bit = 0; bit < 32U; ++bit) {
        const std::uint32_t mask = 1U << bit;
        const bool forestBubble = bit >= 8U && bit <= 14U;
        check(forest::legacy_seed_verdict(omega, 0U, mask) == (forestBubble ? 1U : 0U),
              "Omega fallback retains exact original forest-bubble range");
        for (const auto nativeVerdict : verdicts) {
            check(forest::legacy_seed_verdict(false, nativeVerdict, mask) == nativeVerdict,
                  "unscoped seed gate preserves native result for every bubble");
            if (static_cast<std::uint8_t>(nativeVerdict) != 0U) {
                check(forest::legacy_seed_verdict(omega, nativeVerdict, mask) == nativeVerdict,
                      "native successful result is never replaced even in Omega");
            }
        }
    }
    check(forest::legacy_seed_verdict(omega, 0U, 0U) == 0U, "empty pending set retains native wait");
    check(forest::legacy_seed_verdict(omega, 0U, 0x7F00U) == 1U, "all Omega forest bubbles retain fallback");
    check(forest::legacy_seed_verdict(omega, 0U, 0xFF00U) == 0U, "Lighthouse mixed with forest retains native wait");
    check(forest::legacy_seed_verdict(omega, 0U, 0xFFFFFFFFU) == 0U, "unknown/mixed pending set retains native wait");

    // Existing recipe/solver/owner-authority code still has a second content-family gate.
    std::array<std::byte, forest::kWorkerPrefixSize> worker{};
    std::memcpy(worker.data() + 4U, &forest::kWorkerDefinitionClass, sizeof(std::uint32_t));
    std::memcpy(worker.data() + 0x96CU, &forest::kForestD, sizeof(std::uint32_t));
    check(forest::matches(worker, "mission_scot"), "proven Omega Forest-D worker still matches");
    check(!forest::matches(worker, "infinite_abyss"), "Haunted context rejects even cached Omega worker bytes");
    constexpr std::uint32_t hauntedPieceSet = 0x8150A904U;
    std::memcpy(worker.data() + 0x96CU, &hauntedPieceSet, sizeof hauntedPieceSet);
    check(!forest::matches(worker, "mission_scot"), "installed Haunted generator family cannot receive Omega recipe");

    std::printf("Haunted forest legacy scope: %u checks, %u failures\n", checks, failures);
    return failures == 0U ? 0 : 1;
}
