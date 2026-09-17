#include <array>
#include <cstring>
#include <iostream>
#include <span>

#include "client/hooks/bootflow/omega_forest_recipe.h"
#include "middleware/crypto/random_bytes.h"
#include "state/activity/runtime.h"

namespace {
namespace recipe = dawn::client::hooks::bootflow::omega_forest;
namespace activity = dawn::state::activity;
int failures = 0;
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << __FILE__ << ':' << line << ": " << expression << '\n';
        ++failures;
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
template <typename T> T read(const std::byte* data) {
    T value{};
    std::memcpy(&value, data, sizeof value);
    return value;
}

void packed_recipe_and_write_boundaries() {
    std::array<std::byte, 0x9C0> worker;
    worker.fill(std::byte{0xCD});
    const auto before = worker;
    CHECK(recipe::apply_cached_recipe(worker, 1234567));
    CHECK(read<std::uint32_t>(worker.data() + 0x948) == 0U);
    CHECK(read<std::uint32_t>(worker.data() + 0x94C) == 1234567U);
    // These offsets come from the native consumer, independently of the recipe constants.
    constexpr std::array<std::size_t, 4> columns{0, 9, 17, 25};
    constexpr std::array<std::size_t, 4> weights{4, 12, 20, 28};
    constexpr std::array<std::size_t, 4> opened{8, 16, 24, 32};
    constexpr std::array<int, 4> expectedColumns{-1, -1, 2, 1};
    constexpr std::array<int, 4> expectedHeights{2, 0, 0, 2};
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(read<std::int8_t>(worker.data() + 0x970 + columns[i]) == expectedColumns[i]);
        CHECK(read<std::int8_t>(worker.data() + 0x971 + columns[i]) == expectedHeights[i]);
        CHECK(read<float>(worker.data() + 0x970 + weights[i]) == (i == 3 ? 1.0F : 0.0F));
        CHECK(read<std::uint8_t>(worker.data() + 0x970 + opened[i]) == (i == 2 ? 1 : 0));
    }
    for (std::size_t i = 0; i < worker.size(); ++i) {
        const bool owned = (i >= 0x948 && i < 0x950) || (i >= 0x970 && i < 0x994);
        if (!owned) CHECK(worker[i] == before[i]);
    }
    const auto once = worker;
    CHECK(recipe::apply_cached_recipe(worker, 1234567));
    CHECK(worker == once);
    CHECK(!recipe::apply_cached_recipe(worker, 0));
    CHECK(!recipe::apply_cached_recipe(worker, 0x80000000U));
    CHECK(!recipe::apply_cached_recipe(std::span{worker}.first(0x993), 7));
    CHECK(worker == once);
}

void omega_scope_and_solver_arguments() {
    // Captured from the 2026-09-05 failed live run: worker+4 is the definition
    // class 80804FEC. 80805017 belongs to the resolved component, not this field.
    CHECK(recipe::matches(true, 0x80804FECU, 0x80F4E6E7U));
    CHECK(!recipe::matches(true, 0x80805017U, 0x80F4E6E7U));
    CHECK(!recipe::matches(false, 0x80804FECU, 0x80F4E6E7U));
    CHECK(!recipe::matches(true, 0x80804EF6U, 0x80F4E6E7U));
    CHECK(!recipe::matches(true, 0x80804FECU, 0x80F4E721U));
    const auto omega = recipe::solver_inputs(true, false, 0.12F, 0.16F, 0.8F, 0.9F);
    CHECK(omega.first == 0.0F && omega.second == 0.0F);
    const auto other = recipe::solver_inputs(false, false, 0.12F, 0.16F, 0.8F, 0.9F);
    CHECK(other.first == 0.12F && other.second == 0.16F);
    const auto otherDebug = recipe::solver_inputs(false, true, 0.12F, 0.16F, 0.8F, 0.9F);
    CHECK(otherDebug.first == 0.12F && otherDebug.second == 0.16F);
    const auto explicitDebug = recipe::solver_inputs(true, true, 0.12F, 0.16F, 0.8F, 0.9F);
    CHECK(explicitDebug.first == 0.8F && explicitDebug.second == 0.9F);
}

void run_lifecycle_and_relocated_workers() {
    recipe::RunSeed cache;
    unsigned draws = 0;
    const auto draw = [&draws](std::uint32_t& entropy) noexcept {
        ++draws;
        entropy = 0; // Zero and repeated entropy must still produce positive run seeds.
        return true;
    };
    activity::note_world_phase(activity::WorldPhase::idle);
    const auto firstRun = activity::mission_run_generation();
    activity::note_world_phase(activity::WorldPhase::transitioning);
    activity::note_world_phase(activity::WorldPhase::arrived);
    std::uint32_t firstSeed = 0;
    CHECK(cache.get(firstRun, draw, firstSeed));
    CHECK(firstSeed > 0 && firstSeed <= 0x7FFFFFFFU);
    std::array<std::byte, 0x9C0> original{};
    CHECK(recipe::apply_cached_recipe(original, firstSeed));
    for (int hop = 0; hop < 3; ++hop) {
        activity::note_world_phase(activity::WorldPhase::transitioning);
        activity::note_world_phase(activity::WorldPhase::arrived);
        CHECK(activity::mission_run_generation() == firstRun);
        std::uint32_t retained = 0;
        CHECK(cache.get(activity::mission_run_generation(), draw, retained));
        std::array<std::byte, 0x9C0> reconstructed{};
        CHECK(recipe::apply_cached_recipe(reconstructed, retained));
        CHECK(retained == firstSeed && reconstructed == original);
    }
    CHECK(draws == 1);
    activity::note_world_phase(activity::WorldPhase::idle);
    const auto nextRun = activity::mission_run_generation();
    CHECK(nextRun == firstRun + 1);
    activity::note_world_phase(activity::WorldPhase::idle);
    CHECK(activity::mission_run_generation() == nextRun);
    activity::note_world_phase(activity::WorldPhase::transitioning);
    activity::note_world_phase(activity::WorldPhase::arrived);
    std::uint32_t secondSeed = 0;
    CHECK(cache.get(nextRun, draw, secondSeed));
    CHECK(secondSeed > 0 && secondSeed != firstSeed && draws == 2);

    std::uint32_t unavailable = 99;
    CHECK(!cache.get(nextRun + 1, [](std::uint32_t&) noexcept { return false; }, unavailable));
    CHECK(unavailable == 0);
    CHECK(cache.get(nextRun + 1, [](std::uint32_t& entropy) noexcept {
        entropy = 0xFFFFFFFFU;
        return true;
    }, unavailable));
    CHECK(unavailable == 0x7FFFFFFFU);
    CHECK(cache.get(nextRun + 2, [](std::uint32_t& entropy) noexcept {
        entropy = 0xFFFFFFFFU;
        return true;
    }, unavailable));
    CHECK(unavailable == 1U);
}

void system_random_source() {
    recipe::RunSeed cache;
    std::uint32_t seed = 0;
    CHECK(cache.get(1, [](std::uint32_t& entropy) noexcept {
        return dawn::middleware::crypto::random::fill(
            std::as_writable_bytes(std::span{&entropy, 1U}));
    }, seed));
    CHECK(seed > 0 && seed <= 0x7FFFFFFFU);
}
} // namespace

int main() {
    packed_recipe_and_write_boundaries();
    omega_scope_and_solver_arguments();
    run_lifecycle_and_relocated_workers();
    system_random_source();
    if (failures) return 1;
    std::cout << "Omega forest recipe, scope, seed and lifecycle checks passed\n";
    return 0;
}
