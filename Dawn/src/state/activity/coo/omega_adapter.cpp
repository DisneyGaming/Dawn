#include <Windows.h>
#include "omega_adapter.h"
#include "omega_script.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <array>
#include <cstdio>
#include <mutex>

namespace dawn::state::activity::coo::omega {
namespace {
std::mutex mutex;
MissionRuntime runtime;
std::uint32_t loggedActive{UINT32_MAX};
Phase loggedPhase{Phase::idle};
std::uint64_t loggedIncarnation{};

class NativeControllers final : public Controllers {
    omega_presentation::Presentation presentation(const Input& input) noexcept override {
        return omega_presentation::snapshot(input.run, input.now, input.region, input.entrance, input.executor);
    }
    omega_first_lair::Authority encounter(std::uint64_t run, std::uint32_t generation, bool executorOwned) noexcept override {
        return omega_first_lair::authority(run, generation, executorOwned);
    }
    void request_ending(std::uint64_t run, bool executorOwned) noexcept override {
        const auto status = omega_first_lair::status(run);
        if (status.enabled && status.token.valid()) {
            static_cast<void>(omega_ending::request({status.boss.run, status.boss.actionEpoch,
                status.boss.actor, status.boss.generation}, executorOwned));
        }
    }
    omega_ending::Authority ending(const Input& input) noexcept override {
        return omega_ending::authority(input.run, GetTickCount64());
    }
    std::array<bool, 8> facts(std::uint64_t run, const Frame& frame) noexcept override {
        const auto nav = omega_presentation::navigation();
        const auto status = omega_first_lair::status(run);
        const bool route = nav.enabled && nav.run == run;
        const bool fight = status.enabled && !status.failed && status.boss.run == run;
        const bool finalDeath = fight && frame.encounter.endingRequested;
        const bool ended = frame.ending.token.run == run && frame.ending.token.origin == omega_ending::Origin::encounter
            && frame.ending.complete && !frame.ending.failed;
        return {route && nav.landmark >= omega_presentation::Landmark::tunnel,
            route && (nav.forestComplete || nav.landmark >= omega_presentation::Landmark::lair),
            fight && status.island == 4, fight && status.cycle >= 2,
            fight && status.cycle >= 3, finalDeath, ended,
            ended && omega_ending::handoff_status(run) == omega_ending::Handoff::queued};
    }
};
NativeControllers controllers;

// File IO happens once, on mission selection, never in a native observer.
// An invalid script latches a failed definition for all executor consumers.
bool ensure_script() noexcept {
    static std::once_flag once;
    static std::unique_ptr<script::Document> document;
    static const script::Views invalid{};
    std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};
            std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&ensure_script),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(size==0 || size>=path.size()) { error="cannot resolve DLL-relative scripts path"; }
            else { document=script::Document::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"omega.lua",error); }
        } catch(const std::exception& exception) { error=exception.what(); }
        const bool loaded=document&&document->activate();
        if(!loaded) { static_cast<void>(script::publish(invalid)); }
        std::array<char,768> line{};
        const auto length=loaded?std::snprintf(line.data(),line.size(),
            "ev=coo_script mission=omega result=loaded format=lua graphs=%zu fnv1a64=%016llX path=Dawn/scripts/omega.lua reload=next_process",
            document->views().graphs.size(), static_cast<unsigned long long>(document->fingerprint())):
            std::snprintf(line.data(),line.size(),"ev=coo_script mission=omega result=failed path=Dawn/scripts/omega.lua reason=\"%.*s\"",
                static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data());
        if(length>0 && static_cast<std::size_t>(length)<line.size()) {
            core::log::write(core::log::Channel::server,loaded?core::log::Level::info:core::log::Level::error,{line.data(),static_cast<std::size_t>(length)});
        }
    });
    const auto* views=script::current();return views&&views->valid;
}
}

bool select(std::uint64_t run, bool requested) noexcept {
    const std::lock_guard lock(mutex);
    if (run != mission_run_generation()) { return false; }
    const bool selected=runtime.select(run, requested);
    if(selected) { static_cast<void>(ensure_script()); }
    return selected;
}
Frame update(const Input& input) noexcept {
    const std::lock_guard lock(mutex);
    if (input.run != mission_run_generation() || !mission_seed_armed() || omega_authority_quiesced()) { return {}; }
    const bool selected=runtime.select(input.run,input.executor);
    if(selected && !ensure_script()) { return {}; }
    const auto& definition=selected?script::mission(kMission):kMission;
    const auto result = runtime.update(definition, input, controllers);
    if (runtime.selected()) {
        const auto d = runtime.diagnostics();
        if (loggedIncarnation != d.incarnation || loggedActive != d.active || loggedPhase != d.phase) {
            std::array<char, 384> line{};
            std::string_view waiting = d.phase == Phase::complete ? "complete" : "none";
            for (std::size_t i = 0; i < definition.sequence.steps.size(); ++i) {
                if ((d.active & (std::uint32_t{1} << i)) != 0) { waiting = definition.sequence.steps[i].name; break; }
            }
            const auto size = std::snprintf(line.data(), line.size(),
                "ev=coo_executor mission=omega mode=composition run=%llu incarnation=%llu phase=%u active=%08X complete=%08X failure=%u waiting=\"%.*s\"",
                static_cast<unsigned long long>(d.run), static_cast<unsigned long long>(d.incarnation),
                static_cast<unsigned>(d.phase), d.active, d.complete, static_cast<unsigned>(d.failure),
                static_cast<int>(waiting.size()), waiting.data());
            if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
                core::log::write(core::log::Channel::server, core::log::Level::info,
                    {line.data(), static_cast<std::size_t>(size)});
            }
            loggedIncarnation = d.incarnation; loggedActive = d.active; loggedPhase = d.phase;
        }
    }
    return result;
}
void reset() noexcept {
    const std::lock_guard lock(mutex);
    runtime.reset();
    loggedActive = UINT32_MAX; loggedPhase = Phase::idle; loggedIncarnation = 0;
}
} // namespace dawn::state::activity::coo::omega
