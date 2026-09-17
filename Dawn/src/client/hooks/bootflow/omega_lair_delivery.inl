// Runs only in the existing native source tick. Never retains a runtime pointer
// or edits the sync pool's pending/dirty flags. Native apply owns all scheduling.
void deliver_lair_source(std::byte* component, const hooking::CallGate::Scope& call) noexcept {
    namespace delivery = omega_lair_delivery;
    std::array<std::byte, delivery::kComponentBytes> before{};
    // Reject other sources before copying the larger component.
    std::uint32_t tag{};
    if (!copy_value(component, tag)) return;
    bool known = false;
    for (const auto& source : delivery::authority::kSources) known |= tag == source.definition;
    if (!known || !copy_native(component, before.data(), before.size())) return;
    const auto* source = delivery::source(before);
    if (!source) return;
    const auto run = state::activity::mission_run_generation();
    const auto progress = omega::presentation_progress(run);
    if (!omega_boss_spawn::eligible(true, progress)) return;
    const auto wave = omega::lair_start::prepare(run, omega::boss_generation(run, progress));
    if (!wave.leftStarted) return;
    const auto index = source->slot - 3U;
    {
        const std::lock_guard lock(mutex);
        reset(run);
        if (runState.lairDeliveryClaimed[index]) return;
    }
    // +4E2630 proves component+170 is this source's sync object handle.
    // +9FEC30 resolves its authority storage, also verified in the live capture.
    const auto handle = read<std::uint32_t>(before.data(), 0x170);
    auto* object = resolve_handle(handle);
    std::array<std::byte, 0x70> objectBytes{};
    if (!copy_native(object, objectBytes.data(), objectBytes.size())
        || read<std::uint32_t>(objectBytes.data(), 0xC) != 0x80807EC9U) return;
    using Resolve = const std::byte*(__fastcall*)(std::byte*, std::uint32_t) noexcept;
    const auto* runtime = native<Resolve>(0x9FEC30)(object, delivery::kBodyBytes);
    std::array<std::byte, delivery::kBodyBytes> body{};
    if (!copy_native(runtime, body.data(), body.size())
        || !delivery::pending(*source, wave.generation, wave.leftStarted, objectBytes, body)) return;
    if (delivery::adopted(before, body)) return;
    // Recheck live identities and bytes before claiming the one native mutation.
    std::array<std::byte, delivery::kComponentBytes> current{};
    std::array<std::byte, delivery::kBodyBytes> freshBody{};
    if (!call.accepts_side_effects() || state::activity::mission_run_generation() != run
        || resolve_handle(handle) != object
        || !copy_native(component, current.data(), current.size())
        || delivery::source(current) != source
        || read<std::uint32_t>(current.data(), 0x170) != handle
        || !copy_native(runtime, freshBody.data(), freshBody.size()) || freshBody != body
        || delivery::adopted(current, body)) return;
    {
        const std::lock_guard lock(mutex);
        if (runState.run != run || runState.lairDeliveryClaimed[index]
            || !call.accepts_side_effects() || state::activity::mission_run_generation() != run) return;
        runState.lairDeliveryClaimed[index] = true;
    }
    // +4E8FB0 consumes {schema, padding, authority pointer}; +4A6340 returns
    // that pointer. Supply the validated local copy, preserving the decoded body.
    struct Message { std::uint32_t schema, padding; const std::byte* body; };
    static_assert(offsetof(Message, body) == 8 && sizeof(Message) == 16);
    const Message message{0x80807EC9U, 0, body.data()};
    native<void(__fastcall*)(std::byte*, const Message*) noexcept>(0x4E8FB0)(component, &message);
    std::array<std::byte, delivery::kComponentBytes> after{};
    const bool confirmed = copy_native(component, after.data(), after.size())
        && delivery::source(after) == source && delivery::adopted(after, body);
    log("ev=omega_lair_start stage=native_delivery run=%llu slot=%u generation=%u "
        "object=%08X requested=2 adopted=%u pending=%d,%d tactical_row=%d mutation=native_apply_once",
        run, source->slot, wave.generation, handle, confirmed ? 1U : 0U,
        read<std::int32_t>(after.data(), 0x268), read<std::int32_t>(after.data(), 0x26C),
        read<std::int32_t>(after.data(), 0x5FC));
}
