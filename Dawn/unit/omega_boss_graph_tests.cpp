#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "client/hooks/bootflow/omega_boss_graph.h"

namespace graph = dawn::client::hooks::bootflow::omega_boss_graph;
void check(bool passed, const char* message) {
    if (!passed) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

graph::Owner owner() { return {1, 0x12345001, 0x5CF4200C, 7, 0, 3, 2000, 4, 5, 0, 0x63FAA2EA}; }

void queue_and_request() {
    const auto queue = graph::make_queue();
    check(graph::queue_matches(queue), "native owned queue matches");
    // Fixture follows independent AB35B0 stores and 4E2950 absent-reference stores.
    const std::array<unsigned char, 0x30> expected{
        1,0,0,0, 0,0,0,0, 9,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0x12,0x1A,0xB1,0xAF, 0x9F,0x37,0xD2,0x65,
        0,0,0,0, 0xC5,0x9D,0x1C,0x81, 0xFF,0,0xFF,0xFF, 0,0,0,0};
    check(std::memcmp(queue.data(), expected.data(), expected.size()) == 0,
          "queue agrees with native constructor and absent reference fixture");
    for (std::size_t i = expected.size(); i < queue.size(); ++i)
        check(queue[i] == std::byte{0}, "inactive command storage remains canonical zero");
    for (std::size_t i = 0; i < queue.size(); ++i) {
        auto changed = queue;
        changed[i] ^= std::byte{1};
        check(!graph::queue_matches(changed), "mutated queue cannot be owned by this bridge");
    }
    check(!graph::queue_matches(std::span(queue).first(0x807)), "truncated queue is rejected");
    const auto request = graph::make_condition_request();
    const std::array<unsigned char, 12> condition{
        0x12,0x1A,0xB1,0xAF, 0x9F,0x37,0xD2,0x65, 0x66,0xC8,0xF9,0xC0};
    check(std::memcmp(request.data(), condition.data(), condition.size()) == 0,
          "opcode 5E names exact group sequence condition");
    for (std::size_t i = 12; i < request.size(); ++i)
        check(request[i] == (i == 0x60 ? std::byte{0x5E} : std::byte{0}),
              "condition opcode and unused request bytes follow native branch");
}

void table_guards() {
    graph::EventTable table{}, parsed{};
    check(graph::parse_event_table(std::as_bytes(std::span{&table, 1}), parsed),
          "native empty table decodes");
    parsed.count = 3;
    check(!graph::parse_event_table(std::as_bytes(std::span{&table, 1}).first(0x83), parsed)
          && parsed.count == 3, "failed read preserves output and never synthesizes empty table");
    for (int count : {-1,17,0x7FFFFFFF}) {
        table.count = count;
        check(!graph::valid_table(table), "invalid native capacity is rejected");
    }
    table = {};
    table.count = 1;
    table.rows[0] = graph::kSummonTuple;
    for (int references : {-128,-1,0}) {
        table.rows[0].references = static_cast<std::int8_t>(references);
        check(!graph::expected_add(table, parsed) && !graph::expected_remove(table, parsed),
              "signed refcount corruption is never mutated");
    }
    table.rows[0] = graph::kSummonTuple;
    table.rows[0].references = 127;
    check(!graph::expected_add(table, parsed), "registration cannot wrap signed 127");
    check(graph::expected_remove(table, parsed) && parsed.rows[0].references == 126,
          "removal of an owned saturated count does not overflow");
    table.rows[1] = table.rows[0]; table.count = 2;
    check(!graph::valid_table(table), "duplicate tuples cannot establish one owned reference");
    table = {}; table.count = 16;
    for (int i = 0; i < 16; ++i) table.rows[i] = {std::uint32_t(i + 1), 1, 0, 1};
    check(!graph::expected_add(table, parsed), "full table cannot accept absent tuple");
    table.rows[7] = graph::kSummonTuple;
    check(graph::expected_add(table, parsed) && parsed.count == 16
          && parsed.rows[7].references == 2, "full table can increment existing tuple");
}

void lease_guards() {
    const auto bound = owner();
    const graph::EventTable empty{};
    graph::EventTable added{}, removed{};
    check(graph::expected_add(empty, added), "prepare native add receipt");
    graph::EventLease lease;
    check(!lease.begin_add(bound, empty, false, true), "event waits for close trigger");
    check(!lease.begin_add(bound, empty, true, false), "event waits for exact issued graph queue");
    auto invalid = bound; invalid.character = invalid.parent;
    check(!lease.begin_add(invalid, empty, true, true), "generic parent cannot substitute for character");
    for (std::int64_t offset : {-1LL, 0x2000001LL}) {
        invalid = bound; invalid.memberOffset = offset;
        check(!lease.begin_add(invalid, empty, true, true), "member outside verified datum bounds is rejected");
    }
    check(lease.begin_add(bound, empty, true, true), "one close event call is claimed");
    check(!lease.begin_add(bound, empty, true, true), "reentrant member tick cannot duplicate add");
    check(lease.finish_add(bound, added), "exact zero to one receipt owns one reference");
    check(!lease.begin_add(bound, added, true, true), "owned lease never adds again");
    auto revised = bound; ++revised.revision;
    check(lease.begin_remove(revised, added), "command replacement may release same character lease");
    check(!lease.begin_remove(revised, added), "reentrant remove is prohibited");
    check(graph::expected_remove(added, removed) && lease.finish_remove(revised, removed),
          "one exact decrement releases lease");
    check(lease.state() == graph::LeaseState::released
          && !lease.begin_add(bound, empty, true, true), "released command does not replay");

    // Same slot with a different full salt, actor, run or member generation is stale.
    for (int field = 0; field < 11; ++field) {
        graph::EventLease stale;
        check(stale.begin_add(bound, empty, true, true), "claim stale-owner fixture");
        auto changed = bound;
        switch (field) {
        case 0: ++changed.run; break; case 1: changed.member ^= 0x10000; break;
        case 2: changed.actor ^= 0x10000; break; case 3: ++changed.generation; break;
        case 4: ++changed.revision; break; case 5: changed.parent ^= 0x10000; break;
        case 6: changed.character ^= 0x10000; break; case 7: changed.animation ^= 0x10000; break;
        case 8: changed.biped ^= 0x10000; break; case 9: ++changed.memberOffset; break;
        default: changed.character = 0xFFFFFFFF; break;
        }
        check(!stale.finish_add(changed, added) && stale.state() == graph::LeaseState::uncertain,
              "changed identity invalidates successful-looking add receipt");
        check(!stale.begin_add(bound, empty, true, true)
              && !stale.begin_remove(bound, added), "uncertain mutation can never be retried or decremented");
    }

    for (int failure = 0; failure < 4; ++failure) {
        graph::EventLease uncertain;
        check(uncertain.begin_add(bound, empty, true, true), "claim uncertain receipt fixture");
        auto after = added;
        if (failure == 0) after = empty;
        if (failure == 1) after.rows[0].references = 2;
        if (failure == 2) { after.count = 2; after.rows[1] = {0xABC, 2, 0, 1}; }
        check(!uncertain.finish_add(bound, after, failure != 3),
              "missing extra or unreadable changes cannot establish ownership");
        check(!uncertain.begin_add(bound, empty, true, true), "uncertain add remains consumed");
    }

    graph::EventTable shared{}; shared.count = 2;
    shared.rows[0] = graph::kSummonTuple; shared.rows[1] = {0xABC, 2, 0, 5};
    graph::EventLease sharedLease;
    check(sharedLease.begin_add(bound, shared, true, true) && graph::expected_add(shared, added)
          && sharedLease.finish_add(bound, added), "existing external reference is shared safely");
    check(sharedLease.begin_remove(bound, added) && graph::expected_remove(added, removed)
          && sharedLease.finish_remove(bound, removed) && graph::same_table(shared, removed),
          "lease removal preserves external references and unrelated row");
    check(graph::expected_remove(shared, removed) && removed.count == 1
          && removed.rows[0] == shared.rows[1], "last-reference removal uses native swap-last ordering");

    graph::EventLease uncertainRemove;
    check(uncertainRemove.begin_add(bound, empty, true, true) && graph::expected_add(empty, added)
          && uncertainRemove.finish_add(bound, added) && uncertainRemove.begin_remove(bound, added),
          "claim uncertain removal fixture");
    check(!uncertainRemove.finish_remove(bound, added) && !uncertainRemove.begin_remove(bound, added),
          "unchanged removal receipt must never be retried");
    uncertainRemove.abandon();
    check(uncertainRemove.state() == graph::LeaseState::abandoned
          && !uncertainRemove.begin_add(bound, empty, true, true), "teardown cannot replay or mutate old actor");

    graph::EventLease recycledMember;
    check(recycledMember.begin_add(bound, empty, true, true)
          && graph::expected_add(empty, added) && recycledMember.finish_add(bound, added),
          "claim member-relative binding fixture");
    auto wrongComponent = bound; wrongComponent.memberOffset = 0xB58;
    check(!recycledMember.begin_remove(wrongComponent, added)
          && recycledMember.state() == graph::LeaseState::uncertain,
          "another component in the same datum cannot remove the original member lease");
}

int main() {
    queue_and_request();
    table_guards();
    lease_guards();
    std::puts("omega_boss_graph_tests: passed");
}
