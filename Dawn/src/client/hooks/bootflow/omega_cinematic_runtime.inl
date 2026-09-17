// Included with the native reveal owner. Resource readiness does not consume start attempts.
void __fastcall cinematic_tick(std::byte* component) noexcept {
    const hooking::CallGate::Scope call(callGate);
    hooking::await_original(cinematicOriginal)(component);
    if (!call.accepts_side_effects() || !active()) return;
    observe_mission_ending(component);
    std::array<std::byte, 0x262> body{};
    const bool copied = copy_native(component, body.data(), body.size());
    const bool matches = copied && omega_reveal_source::matches(body, omega_reveal_source::kIntro);
    observe_tick(false, component, copied, matches);
    if (!matches) return;
    const auto run = state::activity::mission_run_generation();
    const auto progress = omega::presentation_progress(run);
    if (!omega_boss_spawn::eligible(true, progress)) return;
    const bool playing = read<std::uint8_t>(body.data(), 0x260) != 0;
    unsigned attempt{};
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) return;
        reset(run);
        if (runState.cinematicStarted) {
            if (playing) runState.cinematicObserved = true;
            if (!runState.cinematicFinished && runState.cinematicObserved && !playing) {
                runState.cinematicFinished = true;
                runState.cinematicCompletedAt = GetTickCount64();
                log("ev=omega_reveal stage=intro_stopped run=%llu native_playing=0", run);
            }
            return;
        }
        if (playing) {
            runState.cinematicStarted = runState.cinematicObserved = true;
            log("ev=omega_reveal stage=intro_already_playing run=%llu", run);
            return;
        }
        const auto now = GetTickCount64();
        // A real node-1 playback receipt primes the camera. No host animation timer.
        if (!runState.queueAccepted || !runState.flightSeen || runState.graphRetired
            || now < runState.nextCinematic || runState.cinematicAttempts >= 5) return;
        runState.nextCinematic = now + 2000;
    }
    const std::uint32_t cinematic = 0xA74B2200U;
    if (!native<void*(__fastcall*)(const std::uint32_t*)>(0xC4C1A0)(&cinematic)) {
        const std::lock_guard lock(mutex);
        if (runState.run == run && !runState.cinematicMissingLogged) {
            runState.cinematicMissingLogged = true;
            log("ev=omega_reveal stage=intro_wait run=%llu reason=resource_pending hash=A74B2200", run);
        }
        return;
    }
    {
        const std::lock_guard lock(mutex);
        // Waiting for registration is not a failed native start. Revalidate the
        // owner after the external lookup before reserving an actual attempt.
        if (runState.run != run || runState.cinematicStarted || !runState.queueAccepted
            || !runState.flightSeen || runState.graphRetired || runState.cinematicAttempts >= 5) return;
        attempt = ++runState.cinematicAttempts;
    }
    const bool started = native<bool(__fastcall*)(std::byte*)>(0x1069CC0)(component);
    std::uint8_t nativePlaying{};
    copy_value(component + 0x260, nativePlaying);
    const std::lock_guard lock(mutex);
    if (runState.run != run) return;
    runState.cinematicStarted = started;
    runState.cinematicObserved = nativePlaying != 0;
    log("ev=omega_reveal stage=intro_request run=%llu attempt=%u accepted=%u native_playing=%u "
        "hash=A74B2200 trigger=native_fly_receipt", run, attempt, started ? 1U : 0U,
        static_cast<unsigned>(nativePlaying));
}
