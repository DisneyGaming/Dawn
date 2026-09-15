#include "server/runtime/activity/cue_presentation_service.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>

namespace cue = sunrise::server::runtime::activity::cue_presentation;
namespace wire = cue::wire;
using Owner = cue::Owner;

unsigned checks{};
void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL %u: %s\n", checks, message);
        std::exit(1);
    }
}

constexpr Owner kOwner{42, {7}};
constexpr std::uint64_t kBoot = 9;

wire::Request presentation(std::int32_t variant = 0) {
    wire::Request result{};
    result.registry = 100;
    result.event = 200;
    result.variant = variant;
    result.scope = UINT32_MAX;
    return result;
}

cue::Action action(std::uint32_t id, wire::Request request,
                   cue::TimerOperation timer = cue::TimerOperation::absent,
                   std::uint64_t duration = 0,
                   std::uint32_t evidence = 0,
                   std::optional<std::int64_t> maximumVariant = std::nullopt,
                   std::optional<std::int64_t> progressTarget = std::nullopt) {
    return {id, request, timer, duration, evidence, maximumVariant, progressTarget};
}

cue::Command command(const cue::Service& service, std::uint64_t request,
                     std::uint32_t actionId, std::uint64_t clock = 1) {
    return {kOwner, kBoot, service.revision(), request, actionId, 0, clock};
}

cue::PresentationUpdate update(const cue::Service& service, std::uint64_t request,
                               std::uint32_t actionId,
                               std::optional<std::int32_t> variant = std::nullopt,
                               std::optional<wire::Progress> progress = std::nullopt,
                               Owner owner = kOwner, std::uint64_t boot = kBoot,
                               std::uint64_t expectedRevision = 0) {
    return {owner, boot, expectedRevision ? expectedRevision : service.revision(), request,
            actionId, variant, progress};
}

void legacy_actions_remain_unchanged() {
    cue::Service service;
    auto initial = presentation();
    initial.hasProgress = true;
    initial.progress = {2, 5};
    const std::array actions{cue::Action{1, initial, cue::TimerOperation::absent, 0, 0}};
    check(service.begin(kOwner, kBoot, actions), "legacy action begins");
    check(service.request(command(service, 1, 1)) == cue::Result::accepted,
          "legacy command publishes");
    const auto before = *service.presentation();
    const auto revision = service.revision();
    check(service.update(update(service, 2, 1, 1)) == cue::Result::unsupported,
          "legacy action rejects variant update");
    check(service.update(update(service, 3, 1, std::nullopt, wire::Progress{1, 10}))
              == cue::Result::unsupported,
          "legacy action rejects progress update");
    check(service.revision() == revision && *service.presentation() == before,
          "rejected legacy updates do not mutate state");
}

void valid_dynamic_updates_change_only_allowed_fields() {
    cue::Service service;
    auto initial = presentation();
    initial.hasProgress = true;
    initial.progress = {0, 100};
    const std::array actions{action(1, initial, cue::TimerOperation::absent, 0, 0, 2, 100)};
    check(service.begin(kOwner, kBoot, actions), "dynamic action begins");
    check(service.request(command(service, 1, 1)) == cue::Result::accepted,
          "dynamic action publishes");
    auto expected = *service.presentation();
    expected.variant = 2;
    expected.progress = {42, 100};
    check(service.update(update(service, 2, 1, 2, wire::Progress{42, 100}))
              == cue::Result::accepted,
          "valid variant and progress update accepted");
    check(*service.presentation() == expected, "update changes only variant and progress");
    check(service.current_action() == 1, "dynamic update retains current action");
}

void pause_counter_update_retains_timer() {
    cue::Service service;
    const std::array actions{
        action(1, presentation(), cue::TimerOperation::start, 100, 0, std::nullopt, 100),
        action(2, presentation(), cue::TimerOperation::pause, 0, 0, std::nullopt, 100)};
    check(service.begin(kOwner, kBoot, actions), "timer actions begin");
    check(service.request(command(service, 1, 1, 100)) == cue::Result::accepted,
          "timer starts");
    check(service.request(command(service, 2, 2, 130)) == cue::Result::accepted,
          "timer pauses");
    const auto paused = *service.presentation();
    check(paused.hasTimer && !paused.timer.advancing && paused.timer.remaining == 70
              && paused.timer.elapsed == 30 && paused.timer.anchor == 130,
          "pause establishes expected timer state");
    auto expected = paused;
    expected.hasProgress = true;
    expected.progress = {55, 100};
    check(service.update(update(service, 3, 2, std::nullopt, wire::Progress{55, 100}))
              == cue::Result::accepted,
          "paused counter update accepted");
    check(*service.presentation() == expected, "counter update retains paused timer exactly");
    check(!service.presentation()->timer.advancing && service.presentation()->timer.anchor == 130,
          "counter update does not unpause or re-anchor timer");
}

void update_identity_and_ordering_are_checked() {
    cue::Service service;
    const std::array actions{action(1, presentation(), cue::TimerOperation::absent, 0, 0, 2)};
    check(service.begin(kOwner, kBoot, actions), "identity test begins");
    check(service.request(command(service, 1, 1)) == cue::Result::accepted,
          "identity test publishes");
    const auto accepted = update(service, 2, 1, 1);
    check(service.update(accepted) == cue::Result::accepted, "identity update accepted");
    check(service.update(update(service, 2, 1, 1)) == cue::Result::duplicate,
          "duplicate update rejected");
    check(service.update(update(service, 3, 1, 1, std::nullopt, kOwner, kBoot, 2))
              == cue::Result::stale,
          "stale revision rejected");
    check(service.update(update(service, 4, 1, 1, std::nullopt,
                                Owner{43, {7}}, kBoot)) == cue::Result::stale,
          "wrong owner epoch rejected");
    check(service.update(update(service, 5, 99, 1)) == cue::Result::stale,
          "wrong expected action rejected");
}

void bounds_and_boundaries_are_checked() {
    {
        cue::Service service;
        const std::array actions{action(1, presentation(), cue::TimerOperation::absent, 0, 0, -1)};
        check(!service.begin(kOwner, kBoot, actions), "negative variant bound rejected");
    }
    {
        cue::Service service;
        const std::array actions{action(1, presentation(2), cue::TimerOperation::absent, 0, 0, 1)};
        check(!service.begin(kOwner, kBoot, actions), "initial variant above bound rejected");
    }
    {
        cue::Service service;
        const std::array actions{action(1, presentation(), cue::TimerOperation::absent, 0, 0, 1, 0)};
        check(!service.begin(kOwner, kBoot, actions), "zero progress target rejected");
    }
    {
        cue::Service service;
        const std::array actions{action(1, presentation(), cue::TimerOperation::absent, 0, 0,
                                         1, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1)};
        check(!service.begin(kOwner, kBoot, actions), "progress target above wire bound rejected");
    }
    {
        cue::Service service;
        const std::array actions{action(1, presentation(), cue::TimerOperation::absent, 0, 0, 0)};
        check(service.begin(kOwner, kBoot, actions), "zero variant boundary begins");
        check(service.request(command(service, 1, 1)) == cue::Result::accepted,
              "zero variant boundary publishes");
        check(service.update(update(service, 2, 1, 0)) == cue::Result::unchanged,
              "zero variant boundary remains valid");
    }
    {
        cue::Service service;
        constexpr auto maximum = std::numeric_limits<std::int32_t>::max();
        auto initial = presentation();
        initial.hasProgress = true;
        initial.progress = {0, maximum};
        const std::array actions{action(1, initial, cue::TimerOperation::absent, 0, 0, maximum, maximum)};
        check(service.begin(kOwner, kBoot, actions), "maximum wire boundaries begin");
        check(service.request(command(service, 1, 1)) == cue::Result::accepted,
              "maximum wire boundaries publish");
        check(service.update(update(service, 2, 1, maximum, wire::Progress{maximum, maximum}))
                  == cue::Result::accepted,
              "maximum variant and progress are accepted");
        const auto revision = service.revision();
        check(service.update(update(service, 3, 1, std::nullopt, wire::Progress{-1, maximum}))
                  == cue::Result::invalidUpdate,
              "negative progress rejected");
        check(service.update(update(service, 4, 1, std::nullopt, wire::Progress{maximum, maximum - 1}))
                  == cue::Result::invalidUpdate,
              "current above target rejected");
        check(service.update(update(service, 5, 1, std::nullopt, wire::Progress{0, 99}))
                  == cue::Result::invalidUpdate,
              "undeclared progress target rejected");
        check(service.update(update(service, 6, 1, -1)) == cue::Result::invalidUpdate,
              "negative variant rejected");
        check(service.revision() == revision, "invalid boundary updates do not consume revision");
    }
}

int main() {
    legacy_actions_remain_unchanged();
    valid_dynamic_updates_change_only_allowed_fields();
    pause_counter_update_retains_timer();
    update_identity_and_ordering_are_checked();
    bounds_and_boundaries_are_checked();
    std::printf("PASS %u checks\n", checks);
}
