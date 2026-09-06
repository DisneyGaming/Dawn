// Shared by read-only diagnostics and the bounded startup helper.
// No value is written and no progression receipt is emitted here.
bool read_animation_glow(const graph::Owner& owner,float& value) noexcept {
    namespace eye=omega_boss_eye_diagnostics;
    auto* parent=resolve_handle(owner.parent);
    std::array<std::byte,0x1474> before{},after{};
    if (!copy_native(parent,before.data(),before.size()) || !eye::animation_parent(before,owner)) return false;
    const auto relative=eye::field<std::uintptr_t>(before,0x1330);
    const auto address=reinterpret_cast<std::uintptr_t>(parent)+0x1340+relative
        +eye::kAnimationGlowProvider*0x30;
    std::array<std::byte,0x30> provider{};
    if (!copy_native(reinterpret_cast<const void*>(address),provider.data(),provider.size())
        || !eye::animation_glow_provider(provider)
        || address+0x10+eye::field<std::uintptr_t>(provider,0x10)!=reinterpret_cast<std::uintptr_t>(parent)) return false;
    // Original4FE170 reads precisely this float; it does not evaluate or mutate.
    value=eye::field<float>(provider,0x20);
    return std::isfinite(value) && resolve_handle(owner.parent)==parent
        && copy_native(parent,after.data(),after.size()) && eye::animation_parent(after,owner)
        && eye::field<std::uintptr_t>(after,0x1330)==relative;
}

