#include "state/activity/open_world_member_observations.h"
#include "state/activity/coo/open_world_member_catalog.h"
#include <cstdio>
#include <memory>
#include <cstdlib>

namespace members = dawn::state::activity::open_world_members;
namespace events = dawn::state::activity::native_population;
namespace {
unsigned checks{};
void check(bool value, int line) {
    ++checks;
    if (!value) { std::printf("FAIL line %d\n", line); std::exit(1); }
}
#define CHECK(value) check((value), __LINE__)
events::Lease lease() {
    const dawn::state::activity::ActivityInstanceKey owner{42, {7}};
    return {owner, {42, 91, 7, {0x74337EDDU, 0x80F5B68EU, 1, 1}, 1}, 15};
}
events::Event admitted(const events::Lease& owner, std::uint32_t actor = 0x1001) {
    return {owner, {owner.source, actor, actor + 0x1000}, 0x3001, events::Kind::admitted};
}
members::Observation observation(const events::Event& event, std::uint64_t nonce = 3) {
    return {{event.lease, nonce}, event.actor, event.sourceHandle, {0x80F5B68EU, 0x1234U, 128}};
}
void exact_identity() {
    members::Mailbox<2> mailbox;
    const auto event = admitted(lease()); const auto value = observation(event);
    CHECK(mailbox.offer(value) == members::Intake::stored);
    CHECK(mailbox.offer(value) == members::Intake::duplicate && mailbox.size() == 1);
    members::Observation output;
    auto wrong = event; ++wrong.actor.entity;
    CHECK(!mailbox.take(value.binding, wrong, output));
    wrong = event; ++wrong.actor.actor;
    CHECK(!mailbox.take(value.binding, wrong, output));
    wrong = event; ++wrong.sourceHandle;
    CHECK(!mailbox.take(value.binding, wrong, output));
    wrong = event; wrong.kind = events::Kind::died;
    CHECK(!mailbox.take(value.binding, wrong, output));
    auto rebound = value.binding; ++rebound.nonce;
    CHECK(!mailbox.take(rebound, event, output));
    rebound = value.binding; ++rebound.lease.source.generation;
    CHECK(!mailbox.take(rebound, event, output));
    CHECK(mailbox.size() == 1 && mailbox.take(value.binding, event, output));
    CHECK(output.member == value.member && !output.conflicted && mailbox.size() == 0);
    CHECK(!mailbox.take(value.binding, event, output));
}
void ambiguity_and_release() {
    members::Mailbox<2> mailbox;
    const auto event = admitted(lease()); auto value = observation(event);
    CHECK(mailbox.offer(value) == members::Intake::stored);
    ++value.member.offset;
    CHECK(mailbox.offer(value) == members::Intake::conflict);
    members::Observation output;
    CHECK(mailbox.take(value.binding, event, output) && output.conflicted);
    value = observation(event); value.binding.nonce = 0;
    CHECK(mailbox.offer(value) == members::Intake::invalid);
    value = observation(event); value.actor.owner.source.type = 2;
    CHECK(mailbox.offer(value) == members::Intake::invalid);
    value = observation(event); value.binding.lease.activity.sessionId = 43;
    CHECK(mailbox.offer(value) == members::Intake::invalid);
    CHECK(mailbox.offer(observation(event)) == members::Intake::stored);
    CHECK(mailbox.offer(observation(admitted(lease(), 0x2001))) == members::Intake::stored);
    CHECK(mailbox.offer(observation(admitted(lease(), 0x3001))) == members::Intake::evicted);
    mailbox.release({43, {7}}); CHECK(mailbox.size() == 2);
    mailbox.release(event.lease.activity); CHECK(mailbox.size() == 0);
}
void independent_authority() {
    auto authorityStorage = std::make_unique<events::Mailbox>();
    auto& authority = *authorityStorage;
    const auto owner = lease(); CHECK(authority.bind(owner));
    const auto creation = authority.begin_creation();
    auto event = admitted(owner); event.actor.birthNonce = creation.nonce;
    events::Receipt receipt;
    CHECK(authority.stage(creation, event, receipt) == events::StageResult::staged);
    members::Mailbox<1> diagnostic;
    CHECK(diagnostic.offer(observation(admitted(owner, 0x5001))) == members::Intake::stored);
    auto value = observation(event); value.binding = receipt;
    CHECK(diagnostic.offer(value) == members::Intake::evicted);
    CHECK(!authority.overflow() && authority.provisional(receipt, event));
    CHECK(authority.admit(receipt, event) == events::AdmitResult::admitted);
    std::array<events::Event, 2> received{};
    CHECK(authority.drain(owner.activity, received) == 1);
    CHECK(received[0].actor == event.actor && !authority.overflow());
    CHECK(!authority.pending(owner.activity));
}
void synchronized_lifecycle() {
    const auto event = admitted(lease()); const auto value = observation(event);
    members::Observation output;
    CHECK(!members::enabled());
    members::capture(value.binding, event, value.member);
    CHECK(members::losses() == 0 && members::take(value.binding, event, output) == members::Take::inactive);
    members::start(); CHECK(members::enabled());
    members::capture(value.binding, event, value.member);
    CHECK(members::take(value.binding, event, output) == members::Take::found && output.member == value.member);
    for (std::uint32_t i = 0; i < 257; ++i) {
        const auto item = admitted(lease(), 0x4000 + i);
        members::capture({item.lease, 3}, item, value.member);
    }
    CHECK(members::losses() == 1);
    members::release(event.lease.activity);
    members::capture(value.binding, event, value.member);
    CHECK(members::take(value.binding, event, output) == members::Take::found);
    members::stop(); CHECK(!members::enabled());
    CHECK(members::take(value.binding, event, output) == members::Take::inactive);
    members::start(); CHECK(members::losses() == 0);
    CHECK(members::take(value.binding, event, output) == members::Take::missing);
    members::stop();
}
void busy_retry_join() {
    auto authorityStorage = std::make_unique<events::Mailbox>();
    auto& authority = *authorityStorage; const auto owner = lease(); CHECK(authority.bind(owner));
    auto event = admitted(owner); const auto creation = authority.begin_creation();
    event.actor.birthNonce = creation.nonce;
    events::Receipt receipt;
    CHECK(authority.stage(creation, event, receipt) == events::StageResult::staged);
    auto filler = event; filler.kind = events::Kind::died;
    for (unsigned i = 0; i < events::kEventCapacity; ++i) CHECK(authority.submit(filler, receipt));
    members::Mailbox<2> diagnostic; auto value = observation(event); value.binding = receipt;
    CHECK(diagnostic.offer(value) == members::Intake::stored);
    CHECK(authority.admit(receipt, event) == events::AdmitResult::busy);
    CHECK(diagnostic.offer(value) == members::Intake::duplicate);
    CHECK(diagnostic.size() == 1 && !authority.overflow());
    auto receivedStorage = std::make_unique<std::array<events::Event, events::kEventCapacity>>();
    auto& received = *receivedStorage;
    CHECK(authority.drain(owner.activity, received) == events::kEventCapacity);
    CHECK(authority.admit(receipt, event) == events::AdmitResult::admitted);
    CHECK(authority.drain(owner.activity, received) == 1);
    members::Observation output;
    CHECK(diagnostic.take(authority.capture(owner), received[0], output));
    CHECK(output.member == value.member && diagnostic.size() == 0);
}
void exact_catalog() {
    CHECK(members::kSources.size() == 1692);
    std::size_t choices{};
    for (const auto& source : members::kSources) {
        CHECK(source.first == choices && source.count > 0 && source.count <= 1776);
        for (std::size_t i = 0; i < source.count; ++i) {
            const auto& choice = members::kChoices[source.first + i];
            CHECK(members::lookup(source.resource, source.registry, source.source, choice.memberOffset) == &choice);
            CHECK(!members::lookup(0, source.registry, source.source, choice.memberOffset));
            CHECK(!members::lookup(source.resource, 0, source.source, choice.memberOffset));
            CHECK(!members::lookup(source.resource, source.registry, UINT16_MAX, choice.memberOffset));
            CHECK(!members::lookup(source.resource, source.registry, source.source, INT64_MIN));
            CHECK(choice.weight > 0 && choice.variant < 6 && choice.category < 8 && choice.choice < 128);
        }
        choices += source.count;
    }
    CHECK(choices == members::kChoices.size() && choices == 16512);
}
void terminal_and_superseded() {
    auto authorityStorage = std::make_unique<events::Mailbox>();
    auto& authority = *authorityStorage; const auto owner = lease(); CHECK(authority.bind(owner));
    auto event = admitted(owner); const auto creation = authority.begin_creation();
    event.actor.birthNonce = creation.nonce;
    events::Receipt receipt;
    CHECK(authority.stage(creation, event, receipt) == events::StageResult::staged);
    members::start(); authority.release(owner.activity); members::release(owner.activity);
    members::capture(receipt, event, observation(event).member); // delayed, now-stale callback
    CHECK(authority.admit(receipt, event) == events::AdmitResult::rejected);
    members::discard(receipt, event);
    members::Observation output;
    CHECK(members::take(receipt, event, output) == members::Take::missing);
    members::stop();
    members::Mailbox<2> mailbox; const auto stale = observation(event);
    CHECK(mailbox.offer(stale) == members::Intake::stored);
    auto next = stale; ++next.binding.nonce; ++next.binding.lease.source.generation;
    next.actor.owner = next.binding.lease.source;
    std::size_t discarded{};
    CHECK(mailbox.offer(next, &discarded) == members::Intake::stored && discarded == 1);
    CHECK(mailbox.size() == 1 && !mailbox.take(stale.binding, event, output));
    mailbox.release_source(stale.binding.lease.source); CHECK(mailbox.size() == 1);
    mailbox.release_source(next.binding.lease.source); CHECK(mailbox.size() == 0);
    unsigned attempts{};
    CHECK(!members::detail::try_bounded([&attempts]() noexcept { ++attempts; return false; }));
    CHECK(attempts == 8);
    attempts = 0;
    CHECK(members::detail::try_bounded([&attempts]() noexcept { return ++attempts == 3; }));
    CHECK(attempts == 3);
}
}
int main() {
    exact_identity(); ambiguity_and_release(); independent_authority(); synchronized_lifecycle(); busy_retry_join();
    exact_catalog(); terminal_and_superseded();
    std::printf("PASS %u diagnostic member mailbox checks\n", checks);
}
