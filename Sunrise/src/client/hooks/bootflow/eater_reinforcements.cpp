#include "eater_reinforcements.h"
#include "eater_reinforcement_selection.h"
#include "gateway_native_read.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../state/activity/eater_of_worlds/runtime.h"
#include "../../../core/logging/log.h"
#include <atomic>
#include <cstdio>

namespace sunrise::client::hooks::bootflow::eater_reinforcements {
namespace {
namespace selection = eater_reinforcement_selection;
namespace mission = state::activity::eater_of_worlds;
using Select = void(__fastcall*)(std::uint32_t*, std::int32_t, std::byte*) noexcept;
hooking::CallGate gate;
hooking::detour::Handle hook{};
std::atomic<Select> original{};
std::atomic_uint32_t applied{}, rejected{};
std::uintptr_t image{};
constexpr std::uintptr_t kSelectionRva = 0x4E34C0U;
constexpr std::array<unsigned char, 16> kPrefix{
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,
    0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};

bool resolve(gateway_native::Read& read, const selection::NativeReference& ref,
             std::uintptr_t& target, std::uint32_t& entity) noexcept {
    std::uintptr_t base{};
    // All supported wrappers are inside their resource. Reject negative or oversized offsets.
    if(ref.offset < 0 || ref.offset > 0x100000 || !read.resolve(ref.handle, base)
       || base > UINTPTR_MAX - static_cast<std::uintptr_t>(ref.offset)) { return false; }
    target = base + static_cast<std::uintptr_t>(ref.offset);
    std::uint32_t marker{};
    return target >= 0x10000 && read.value(target - 4U, marker) && marker == ref.kind
        && read.value(target, entity) && entity != UINT32_MAX;
}

bool exact_source(gateway_native::Read& read, std::uintptr_t source,
                  std::uint32_t tag) noexcept {
    selection::NativeReference ref{};
    if(!read.value(source, ref) || ref.handle != tag || ref.kind != 0x8080948FU
       || ref.offset != selection::kSourceDefinitionOffset) { return false; }
    std::uintptr_t definition{};
    std::uint32_t identity{}, registry{};
    std::uint8_t type{};
    std::uint16_t slot{};
    return selection::source_slot(tag) != UINT16_MAX
        && resolve(read, ref, definition, identity) && identity == tag
        && read.value(definition + 0x30U, registry) && registry == 0x686321C8U
        && read.value(definition + 0x34U, type) && type == 1U
        && read.value(definition + 0x36U, slot) && slot == selection::source_slot(tag);
}

selection::Donor donor(gateway_native::Read& read, mission::reinforcements::Kind kind) noexcept {
    const auto spec = selection::donor_spec(kind);
    std::uintptr_t target{};
    std::uint32_t entity{}, identity{}, weight{};
    std::uint8_t flags{};
    const bool resolved = kind != mission::reinforcements::Kind::invalid
        && spec.reference.kind == selection::kSelectionReferenceKind
        && resolve(read, spec.reference, target, entity) && entity == spec.entity
        && read.value(target - 0x18U, identity) && identity == spec.choiceIdentity
        && read.value(target - 0x14U, weight) && weight == 1U
        && read.value(target - 0x10U, flags) && flags == spec.choiceFlags;
    return {spec, resolved};
}

void report(bool success, const char* reason, std::uint64_t run,
            std::uint16_t slot, std::uint32_t generation, std::uint32_t entity) noexcept {
    auto& counter = success ? applied : rejected;
    if(counter.fetch_add(1U, std::memory_order_relaxed) >= 16U) { return; }
    std::array<char, 256> line{};
    const int n = std::snprintf(line.data(), line.size(),
        "ev=eater_reinforcement_selection stage=selected run=%llu source=%u generation=%u "
        "entity=%08X result=%s reason=%s",
        static_cast<unsigned long long>(run), static_cast<unsigned>(slot), generation,
        entity, success ? "applied" : "passthrough", reason);
    if(n > 0 && static_cast<std::size_t>(n) < line.size()) {
        core::log::write(core::log::Channel::client,
            success ? core::log::Level::info : core::log::Level::warn,
            {line.data(), static_cast<std::size_t>(n)});
    }
}

bool store(std::byte* output, const selection::SelectedMember& value) noexcept {
    __try { std::memcpy(output, &value, sizeof value); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

__declspec(noinline) void substitute(std::uintptr_t source, std::int32_t category,
                                     std::byte* output) noexcept {
    // This callback can run inside authority publication. Use its lock-free run token.
    const auto run = mission::native_roster_run();
    if(!run || category != 0) { return; }
    gateway_native::Read read{image};
    std::uint32_t tag{}, generation{};
    if(!read.value(source, tag)) { return; }
    const auto slot = selection::source_slot(tag);
    if(slot == UINT16_MAX) { return; }
    selection::SelectedMember selected{};
    if(!exact_source(read, source, tag) || !read.value(source + 0x244U, generation)
       || !read.value(reinterpret_cast<std::uintptr_t>(output), selected)) {
        report(false, "source_identity", run, slot, generation, UINT32_MAX); return;
    }
    std::uintptr_t target{};
    std::uint32_t entity{};
    if(selected.reference.kind != selection::kSelectionReferenceKind
       || !resolve(read, selected.reference, target, entity)) {
        report(false, "selected_reference", run, slot, generation, UINT32_MAX); return;
    }
    const auto replacement = donor(read, mission::reinforcements::select(slot, 0U));
    const auto result = selection::apply({run, tag, generation, category, entity}, replacement, selected);
    if(result == selection::Result::native) { return; }
    if(result != selection::Result::substituted) {
        report(false, "selection_validation", run, slot, generation, entity); return;
    }
    std::uint32_t currentGeneration{};
    if(!gate.accepting() || mission::native_roster_run() != run
       || !read.value(source + 0x244U, currentGeneration) || currentGeneration != generation
       || !exact_source(read, source, tag) || !store(output, selected)) {
        report(false, "final_recheck", run, slot, generation, entity); return;
    }
    report(true, "native_choice", run, slot, generation, replacement.spec.entity);
}

__declspec(noinline) void __fastcall select(std::uint32_t* source, std::int32_t category,
                                           std::byte* output) noexcept {
    hooking::CallGate::Scope scope(gate);
    const auto fn = hooking::await_original(original);
    fn(source, category, output);
    // +4E2E80 copies this complete member into its durable queue after we return.
    // Insertion rules, ship seats, transforms, and actor lifetime remain native.
    if(scope.accepts_side_effects()) { substitute(reinterpret_cast<std::uintptr_t>(source), category, output); }
}
bool idle() noexcept { return gate.idle(); }
}

__declspec(noinline) bool install() noexcept {
    if(original.load(std::memory_order_acquire)) { return gate.accepting(); }
    gate.quiesce();
    image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    gateway_native::Read read{image};
    std::array<unsigned char, 16> bytes{};
    if(!image || !read.value(image + kSelectionRva, bytes) || bytes != kPrefix) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
            "ev=eater_reinforcements stage=install result=prefix_mismatch"); return false;
    }
    if(!hooking::detour::install({reinterpret_cast<void*>(image + kSelectionRva),
                                reinterpret_cast<void*>(&select)}, hook)) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
            "ev=eater_reinforcements stage=install result=attach_fail"); return false;
    }
    hooking::publish_original(original, reinterpret_cast<Select>(hook.original));
    gate.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
        "ev=eater_reinforcements stage=install result=ok boundary=4E34C0 roster=2_colossi,5_psions,3_incendiors");
    return true;
}
void quiesce() noexcept { gate.quiesce(); }
bool uninstall() noexcept {
    gate.quiesce();
    if(!original.load(std::memory_order_acquire)) { return true; }
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&select)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&substitute)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(hook, entries, &idle) != hooking::detour::UninstallResult::removed) { return false; }
    original.store(nullptr, std::memory_order_release);
    image = 0;
    applied.store(0U, std::memory_order_relaxed);
    rejected.store(0U, std::memory_order_relaxed);
    return true;
}
}
