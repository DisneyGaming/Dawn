#include "state/activity/coo/omega_projection.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "middleware/encoding/bit_writer.h"

namespace coo = sunrise::state::activity::coo;
namespace omega = coo::omega;
namespace wire = omega::wire;
namespace present = sunrise::state::activity::omega_presentation;
namespace fight = sunrise::state::activity::omega_first_lair;
namespace ending = sunrise::state::activity::omega_ending;
unsigned checks{};
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while(false)

struct Services final : coo::Services {
    std::vector<coo::Command> published, retired;
    std::size_t refuse{SIZE_MAX};
    bool publish(const coo::Command& command) noexcept override {
        if (published.size() == refuse) { return false; }
        published.push_back(command); return true;
    }
    void cancel(const coo::Command& command) noexcept override { retired.push_back(command); }
};

constexpr std::array<coo::CommandSpec, 2> opening{{
    {coo::Operation::scene, {0x4786C0E0, 0x80EC0FA8, 1, 0}},
    {coo::Operation::dialogue, {0xD00142CF, 0x80F1FD07, 53, 0}}
}};
constexpr std::array<coo::CommandSpec, 1> combat{{{coo::Operation::population, {0x0040BF06, 1, 18, 0}}}};
constexpr std::array<coo::CommandSpec, 1> end{{{coo::Operation::cinematic, {0x3A6CE17A, 0x80F47BCB, 6, 0}}}};
constexpr std::array<coo::Step, 3> steps{{
    {"scene plus speech", 0, opening}, {"concurrent combat", 0, combat}, {"join", 3, end}
}};
constexpr coo::Definition definition{"contract fixture", coo::Schema::omegaArchive, steps};

void receipt(coo::Executor& executor, std::size_t step, std::size_t command, coo::Milestone event) {
    CHECK(executor.enqueue({executor.token(step, command), event}));
}

void contracts() {
    Services services;
    coo::Executor e;
    CHECK(e.start(definition, 7));
    CHECK(!e.enqueue({e.token(0, 0), coo::Milestone::completed})); // Not requested.
    CHECK(!e.start(definition, 8));
    e.update(services);
    CHECK(services.published.size() == 3);
    CHECK(e.diagnostics().active == 3);
    CHECK(!e.step_state(0).commands[0].ready);
    receipt(e, 0, 0, coo::Milestone::completed);
    e.update(services);
    CHECK(!e.step_state(0).commands[0].completed); // Completion cannot invent readiness.
    auto stale = e.token(0, 0); ++stale.run;
    CHECK(!e.enqueue({stale, coo::Milestone::nativeReady}));
    stale = e.token(0, 0); ++stale.incarnation;
    CHECK(!e.enqueue({stale, coo::Milestone::nativeReady}));
    receipt(e, 0, 0, coo::Milestone::nativeReady);
    receipt(e, 0, 0, coo::Milestone::nativeReady);
    receipt(e, 0, 0, coo::Milestone::completed);
    CHECK(!e.step_state(0).commands[0].ready); // Enqueue never progresses inline.
    e.update(services);
    CHECK(e.diagnostics().duplicates == 1);
    CHECK(e.step_state(0).phase == coo::StepPhase::active); // Still waiting on speech.
    receipt(e, 1, 0, coo::Milestone::nativeReady);
    receipt(e, 1, 0, coo::Milestone::completed);
    e.update(services);
    CHECK(services.published.size() == 3); // Join waits for both branches.
    receipt(e, 0, 1, coo::Milestone::nativeReady);
    receipt(e, 0, 1, coo::Milestone::completed);
    const auto old = e.token(0, 1);
    e.update(services);
    CHECK(services.published.size() == 4);
    CHECK(e.diagnostics().active == 4);
    CHECK(!e.enqueue({old, coo::Milestone::completed}));
    receipt(e, 2, 0, coo::Milestone::nativeReady);
    receipt(e, 2, 0, coo::Milestone::completed);
    e.update(services);
    CHECK(e.diagnostics().phase == coo::Phase::complete);
    CHECK(!e.start(definition, 7)); // Live service leases require teardown.
    e.cancel(services);
    CHECK(services.retired.size() == 4);
    CHECK(services.retired.front().token.step == 2);
    e.cancel(services);
    CHECK(services.retired.size() == 4); // Exactly once cleanup.
    CHECK(e.start(definition, 7));
    e.update(services);
    CHECK(!e.enqueue({old, coo::Milestone::completed})); // Reused external run id.
    e.cancel(services);

    coo::Executor overflow;
    Services sink;
    CHECK(overflow.start(definition, 1)); overflow.update(sink);
    for (std::size_t i = 0; i < coo::Executor::kQueueSize; ++i) {
        receipt(overflow, 0, 0, coo::Milestone::nativeReady);
    }
    CHECK(!overflow.enqueue({overflow.token(0, 0), coo::Milestone::completed}));
    overflow.update(sink);
    CHECK(overflow.diagnostics().failure == coo::Failure::queueOverflow);
    CHECK(sink.retired.size() == 3);
    CHECK(overflow.diagnostics().complete == 0);

    coo::Executor failed;
    Services refused; refused.refuse = 1;
    CHECK(failed.start(definition, 2)); failed.update(refused);
    CHECK(failed.diagnostics().failure == coo::Failure::publication);
    CHECK(refused.retired.size() == 1);
    Services native;
    CHECK(failed.start(definition, 3)); failed.update(native);
    receipt(failed, 0, 0, coo::Milestone::failed); failed.update(native);
    CHECK(failed.diagnostics().failure == coo::Failure::native);
    CHECK(native.retired.size() == 3);

    auto invalid = definition; invalid.schema = coo::Schema::unspecified;
    CHECK(!coo::Executor::valid(invalid));
    auto invalidSteps = steps; invalidSteps[0].dependencies = 4;
    invalid = definition; invalid.steps = invalidSteps;
    CHECK(!coo::Executor::valid(invalid));
    invalidSteps = steps; invalidSteps[1].name = invalidSteps[0].name;
    CHECK(!coo::Executor::valid(invalid));
    const coo::Asset key{0x2763EC97, 12, 17, 0};
    std::array<coo::Asset, 1200> catalog{}; catalog.back() = key;
    CHECK(coo::resolve(catalog, key) == &catalog.back());
    catalog[4] = key;
    CHECK(coo::resolve(catalog, key) == nullptr);
    CHECK(coo::resolve(catalog, {1, 2, 3, 4}) == nullptr);
}

void scheduling() {
    constexpr std::array<coo::CommandSpec, 1> request{{
        {coo::Operation::mechanic, {}, 1, coo::Wait::requested}}};
    constexpr std::array<coo::CommandSpec, 1> observation{{
        {coo::Operation::observation, {}, 1, coo::Wait::observed}}};
    const std::array<coo::Step, 4> graph{{
        {"request", 0, request}, {"native receipt", 1, observation},
        {"next request", 2, request}, {"final request", 4, request}}};
    const coo::Definition fixture{"scheduling", coo::Schema::otherMissions, graph};
    Services services;
    coo::Executor e;
    CHECK(!e.update_pending());
    CHECK(e.start(fixture, 1));
    CHECK(e.update_pending());
    e.update(services);
    CHECK(!e.update_pending()); // Waiting on a real native observation.
    CHECK(e.enqueue({e.token(1, 0), coo::Milestone::observed}));
    CHECK(e.update_pending());
    e.update(services);
    CHECK(e.diagnostics().queued == 0);
    CHECK(e.update_pending()); // A requested-only successor still needs to join.
    e.update(services);
    CHECK(e.update_pending());
    e.update(services);
    CHECK(e.diagnostics().phase == coo::Phase::complete);
    CHECK(services.published.size() == graph.size());
    CHECK(!e.update_pending());
    e.cancel(services);
    CHECK(!e.update_pending());
    CHECK(e.start(fixture, 2));
    Services refused; refused.refuse = 0;
    e.update(refused);
    CHECK(e.diagnostics().phase == coo::Phase::failed);
    CHECK(!e.update_pending());
}

struct Controllers final : omega::Controllers {
    omega::Frame frame;
    std::array<bool, 8> observed{};
    std::vector<unsigned> calls;
    present::Presentation presentation(const omega::Input&) noexcept override { calls.push_back(0); return frame.presentation; }
    fight::Authority encounter(std::uint64_t, std::uint32_t generation, bool) noexcept override {
        CHECK(generation == frame.presentation.bossGeneration); calls.push_back(1); return frame.encounter;
    }
    void request_ending(std::uint64_t, bool) noexcept override { calls.push_back(2); }
    ending::Authority ending(const omega::Input&) noexcept override { calls.push_back(3); return frame.ending; }
    std::array<bool, 8> facts(std::uint64_t, const omega::Frame&) noexcept override { return observed; }
};

// Encode actual projected fields, not object padding, over every authored type
// and source slot used by the existing archive protocol fixture.
void same_wire(const omega::Frame& a, const omega::Frame& b) {
    wire::Snapshot left{}, right{};
    left.archiveOmega = right.archiveOmega = true;
    left.omegaSceneAuthority = right.omegaSceneAuthority = true;
    left.omegaDialogueArm = right.omegaDialogueArm = true;
    omega::project(a, left); omega::project(b, right);
    constexpr std::array<std::uint32_t, 12> registries{
        0x4786C0E0, 0xD00142CF, 0x2763EC97, 0xF4D0E0B2, 0x95FB2E01, 0x99BD2FEB,
        0x0040BF06, 0x0040BF05, 0x0040BF03, 0x3A6CE17A, 0x30A025E8, 0x0040BF04};
    constexpr std::array<std::uint8_t, 14> types{1, 2, 4, 6, 11, 13, 17, 18, 23, 30, 35, 43, 68, 70};
    std::array<std::byte, 8192> x{}, y{};
    for (auto registry : registries) for (auto type : types) for (std::uint16_t slot = 0; slot < 150; ++slot) {
        const auto width = wire::auth_body_bits(left, registry, type, slot, type == 13);
        CHECK(width == wire::auth_body_bits(right, registry, type, slot, type == 13));
        if (width == 0) { continue; }
        sunrise::middleware::encoding::bits::Writer wx(x), wy(y);
        CHECK(wire::write_auth_body(wx, left, registry, type, slot, type == 13));
        CHECK(wire::write_auth_body(wy, right, registry, type, slot, type == 13));
        CHECK(wx.bit_count() == wy.bit_count());
        std::size_t bytes{}, rightBytes{};
        CHECK(wx.finish(bytes) && wy.finish(rightBytes) && bytes == rightBytes);
        CHECK(std::equal(x.begin(), x.begin() + bytes, y.begin()));
    }
}

void adapter_parity() {
    coo::MissionRuntime legacy, executor;
    CHECK(executor.select(42, true)); // Intake can latch before the in-world seed.
    CHECK(executor.select(42, false));
    CHECK(!legacy.select(42, false));
    CHECK(!legacy.select(42, true));
    Controllers left, right;
    for (unsigned phase = 0; phase < 12; ++phase) {
        omega::Frame frame;
        frame.presentation.bossGeneration = 7;
        frame.presentation.generations.fill(phase + 1);
        frame.presentation.activeRow = static_cast<std::uint8_t>(phase * 2);
        frame.presentation.objective = present::kObjectives[phase % present::kObjectives.size()];
        frame.presentation.intro = {7, phase == 3};
        frame.encounter.generation = frame.encounter.crownGeneration = 7;
        frame.encounter.loose.fill(static_cast<std::uint8_t>(phase % 3));
        frame.encounter.anchor = frame.encounter.cannon = phase > 2;
        frame.encounter.cycle = static_cast<std::uint8_t>(phase / 4 + 1);
        frame.encounter.chargeEnabled = phase % 2 == 0;
        frame.encounter.endingRequested = phase >= 10;
        frame.encounter.crownRestricted = phase < 10;
        frame.encounter.rescueMarkerReadyMask = 0x7FF;
        frame.encounter.transitCreated = 0xAA5555AA;
        frame.ending.token = {42, 20, 1, 7};
        frame.ending.revision = phase + 1;
        frame.ending.bookendState = frame.ending.arrived = phase >= 10;
        frame.ending.play = frame.ending.started = phase == 10;
        frame.ending.complete = phase == 11;
        left.frame = right.frame = frame;
        left.calls.clear(); right.calls.clear();
        for (std::size_t i = 0; i < right.observed.size(); ++i) { right.observed[i] = phase > i + 2; }
        const omega::Input input{42, 1000U * phase, 120, phase > 1, true};
        auto direct = input; direct.executor = false;
        same_wire(legacy.update(omega::kMission, direct, left), executor.update(omega::kMission, input, right));
        CHECK(left.calls == right.calls);
        CHECK(right.calls == (phase >= 10 ? std::vector<unsigned>{0,1,2,3} : std::vector<unsigned>{0,1,3}));
    }
    CHECK(executor.diagnostics().phase == coo::Phase::complete);
    // Mid-run toggles cannot create two publishers or switch implementations.
    static_cast<void>(executor.update(omega::kMission, {42, 50000, 120, true, false}, right));
    CHECK(executor.selected());
    static_cast<void>(legacy.update(omega::kMission, {42, 50000, 120, true, true}, left));
    CHECK(!legacy.selected());
    const auto incarnation = executor.diagnostics().incarnation;
    executor.reset(); right.observed = {};
    static_cast<void>(executor.update(omega::kMission, {42, 1, 120, false, true}, right));
    CHECK(executor.diagnostics().incarnation > incarnation);
    CHECK(executor.diagnostics().complete == 1); // Old observations cleared.
    const auto before = right.calls.size();
    static_cast<void>(executor.update(omega::kMission, {0, 1, 120, false, true}, right));
    CHECK(right.calls.size() == before);
    static_cast<void>(executor.update(omega::kMission, {43, 1, 120, false, false}, right));
    CHECK(!executor.selected());
}


void deep_restriction_wire() {
    wire::Snapshot snapshot{};snapshot.deep_storage.enabled=true;snapshot.deep_storage.spawnGeneration=7;
    const auto field=[](const auto& bytes,std::size_t bit,unsigned width) {
        std::uint32_t value{};for(unsigned i=0;i<width;++i) {value=(value<<1)|((std::to_integer<unsigned>(bytes[(bit+i)/8])>>(7-(bit+i)%8))&1U);}return value;
    };
    for(bool restricted:{false,true,false}) {
        snapshot.deep_storage.restricted=restricted;
        for(const auto type:std::array<std::uint8_t,2>{17,35}) {
            std::array<std::byte,4096> bytes{};sunrise::middleware::encoding::bits::Writer w(bytes);
            const std::uint16_t slot=type==17?3U:1U;
            CHECK(wire::write_auth_body(w,snapshot,0x4786C0E0U,type,slot,false));
            CHECK(w.bit_count()==wire::auth_body_bits(snapshot,0x4786C0E0U,type,slot,false));
            if(type==17) {CHECK(field(bytes,72,32)==0x80000000U+(restricted?19U:0U));}
            else {CHECK(field(bytes,0,1)==static_cast<unsigned>(restricted));}
        }
    }
    // Disabling the restriction restores the native unrestricted lifetime ordinal.
    std::array<std::byte,4096> bytes{};sunrise::middleware::encoding::bits::Writer w(bytes);
    CHECK(wire::write_auth_body(w,snapshot,0x4786C0E0U,17,3,false));CHECK(field(bytes,72,32)==0x80000000U);
}

int main() {
    contracts(); scheduling(); adapter_parity(); deep_restriction_wire();
    std::printf("PASS: %u checks; concurrent joins, stale receipts, teardown, and 12 adapter wire phases\n", checks);
}
