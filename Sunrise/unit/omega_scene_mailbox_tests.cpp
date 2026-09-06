#include <barrier>
#include <cstdint>
#include <iostream>
#include <thread>

#include "state/activity/omega/omega_scene_mailbox.h"

namespace {

using sunrise::state::activity::SceneGeneration;
using sunrise::state::activity::omega::retirement_mask;
using sunrise::state::activity::omega::SceneRetirement;
using sunrise::state::activity::omega::SceneRetirementMailbox;

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;

    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void absent_and_unrepresentable_generations_fail_closed() {
    SceneRetirementMailbox absent;
    const SceneGeneration zero{};
    const SceneGeneration first{1U};
    const SceneGeneration tooLarge{SceneRetirementMailbox::kMaximumMailboxGeneration + 1U};

    CHECK(!absent.schedule(zero, SceneRetirement::observation0));
    CHECK(!absent.schedule(first, SceneRetirement::observation0));
    CHECK(absent.consume(zero) == 0U);
    CHECK(absent.consume(first) == 0U);
    CHECK(absent.pending(first) == 0U);
    CHECK(!absent.advance_to(zero, first));
    CHECK(!absent.advance_to(first, SceneGeneration{2U}));

    SceneRetirementMailbox overflowed{tooLarge};
    CHECK(overflowed.pending(tooLarge) == 0U);
    CHECK(!overflowed.schedule(tooLarge, SceneRetirement::observation0));
    CHECK(!overflowed.advance_to(tooLarge, SceneGeneration{tooLarge.value + 1U}));
}

void schedule_requires_an_exact_generation_and_coalesces_duplicates() {
    const SceneGeneration current{17U};
    SceneRetirementMailbox mailbox{current};

    CHECK(mailbox.is_lock_free());
    CHECK(!mailbox.schedule(SceneGeneration{16U}, SceneRetirement::observation0));
    CHECK(!mailbox.schedule(SceneGeneration{18U}, SceneRetirement::observation0));
    CHECK(!mailbox.schedule(current, static_cast<SceneRetirement>(0U)));
    CHECK(!mailbox.schedule(current, static_cast<SceneRetirement>(3U)));
    CHECK(!mailbox.schedule(current, static_cast<SceneRetirement>(8U)));
    CHECK(mailbox.schedule(current, SceneRetirement::observation0));
    CHECK(!mailbox.schedule(current, SceneRetirement::observation0));
    CHECK(mailbox.schedule(current, SceneRetirement::observation2));
    CHECK(mailbox.pending(current)
          == (retirement_mask(SceneRetirement::observation0)
              | retirement_mask(SceneRetirement::observation2)));
    CHECK(mailbox.pending(SceneGeneration{16U}) == 0U);
    CHECK(mailbox.pending(SceneGeneration{18U}) == 0U);
}

void consume_only_clears_the_matching_generation() {
    const SceneGeneration current{41U};
    SceneRetirementMailbox mailbox{current};

    CHECK(mailbox.schedule(current, SceneRetirement::observation1));
    CHECK(mailbox.consume(SceneGeneration{40U}) == 0U);
    CHECK(mailbox.consume(SceneGeneration{42U}) == 0U);
    CHECK(mailbox.pending(current) == retirement_mask(SceneRetirement::observation1));
    CHECK(mailbox.consume(current) == retirement_mask(SceneRetirement::observation1));
    CHECK(mailbox.pending(current) == 0U);
    CHECK(mailbox.consume(current) == 0U);
}

void advancing_generation_cannot_inherit_predecessor_retirements() {
    const SceneGeneration oldGeneration{70U};
    const SceneGeneration newGeneration{71U};
    SceneRetirementMailbox mailbox{oldGeneration};

    CHECK(mailbox.schedule(oldGeneration, SceneRetirement::observation0));
    CHECK(mailbox.advance_to(oldGeneration, newGeneration));
    CHECK(mailbox.pending(oldGeneration) == 0U);
    CHECK(mailbox.pending(newGeneration) == 0U);
    CHECK(mailbox.consume(newGeneration) == 0U);

    CHECK(!mailbox.schedule(oldGeneration, SceneRetirement::observation1));
    CHECK(mailbox.pending(newGeneration) == 0U);
    CHECK(mailbox.schedule(newGeneration, SceneRetirement::observation2));
    CHECK(mailbox.consume(newGeneration) == retirement_mask(SceneRetirement::observation2));
}

void generation_transition_is_strictly_monotonic_and_bounded() {
    SceneRetirementMailbox mailbox{SceneGeneration{9U}};

    CHECK(!mailbox.advance_to(SceneGeneration{8U}, SceneGeneration{10U}));
    CHECK(!mailbox.advance_to(SceneGeneration{9U}, SceneGeneration{9U}));
    CHECK(!mailbox.advance_to(SceneGeneration{9U}, SceneGeneration{8U}));
    CHECK(mailbox.advance_to(SceneGeneration{9U}, SceneGeneration{20U}));
    CHECK(!mailbox.advance_to(SceneGeneration{9U}, SceneGeneration{21U}));

    const SceneGeneration maximum{SceneRetirementMailbox::kMaximumMailboxGeneration};
    SceneRetirementMailbox bounded{maximum};
    CHECK(bounded.schedule(maximum, SceneRetirement::observation0));
    CHECK(!bounded.advance_to(
        maximum, SceneGeneration{SceneRetirementMailbox::kMaximumMailboxGeneration + 1U}));
    CHECK(bounded.consume(maximum) == retirement_mask(SceneRetirement::observation0));
}

void racing_old_schedule_and_advance_never_marks_the_successor() {
    constexpr std::uint64_t kIterations = 10'000U;
    SceneRetirementMailbox mailbox{SceneGeneration{1U}};
    SceneGeneration oldGeneration{1U};
    bool advanceResult = false;
    std::barrier phase{3};

    std::thread scheduler([&] {
        for (std::uint64_t iteration = 0; iteration < kIterations; ++iteration) {
            phase.arrive_and_wait();
            (void)mailbox.schedule(oldGeneration, SceneRetirement::observation0);
            phase.arrive_and_wait();
        }
    });

    std::thread advancer([&] {
        for (std::uint64_t iteration = 0; iteration < kIterations; ++iteration) {
            phase.arrive_and_wait();
            advanceResult =
                mailbox.advance_to(oldGeneration, SceneGeneration{oldGeneration.value + 1U});
            phase.arrive_and_wait();
        }
    });

    for (std::uint64_t iteration = 0; iteration < kIterations; ++iteration) {
        advanceResult = false;
        phase.arrive_and_wait();
        phase.arrive_and_wait();

        const SceneGeneration newGeneration{oldGeneration.value + 1U};
        CHECK(advanceResult);
        CHECK(mailbox.pending(newGeneration) == 0U);
        CHECK(mailbox.consume(newGeneration) == 0U);
        CHECK(!mailbox.schedule(oldGeneration, SceneRetirement::observation1));
        CHECK(mailbox.pending(newGeneration) == 0U);
        CHECK(mailbox.schedule(newGeneration, SceneRetirement::observation2));
        CHECK(mailbox.consume(newGeneration) == retirement_mask(SceneRetirement::observation2));
        oldGeneration = newGeneration;
    }

    scheduler.join();
    advancer.join();
}

} // namespace

int main() {
    absent_and_unrepresentable_generations_fail_closed();
    schedule_requires_an_exact_generation_and_coalesces_duplicates();
    consume_only_clears_the_matching_generation();
    advancing_generation_cannot_inherit_predecessor_retirements();
    generation_transition_is_strictly_monotonic_and_bounded();
    racing_old_schedule_and_advance_never_marks_the_successor();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " Scene mailbox check(s) failed\n";
        return 1;
    }

    std::cout << "all Scene mailbox checks passed\n";
    return 0;
}
