// Shared 9F19F0 owner: forward native authority before mission preparation.
__declspec(noinline) void __fastcall portal_visual_apply(std::byte* component,
                                                         const std::byte* packet) noexcept {
    const auto original = hooking::await_original(g_portalApplyOriginal);
    portal_effect_observe("authority_before", 0, component);
    if (original) original(component, packet);
    omega_reveal_native::observe_mission_device(component);
    portal_effect_observe("authority_after", 0, component);
}
