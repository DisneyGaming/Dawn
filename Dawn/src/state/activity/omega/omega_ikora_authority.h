#pragma once

#include <cstddef>
#include <cstdint>

namespace dawn::state::activity::omega::ikora {
inline constexpr std::uint32_t kRegistry = 0xD00142CFU;
inline constexpr std::uint32_t kSceneGenerationWire = 0x80EC0F96U;
inline constexpr std::uint32_t kPortalRequest = 0xC7ECAA77U;
inline constexpr std::uint32_t kLatticeRelease = 0x792AAA50U;
inline constexpr std::size_t kSourceBits = 641U;
inline constexpr std::size_t kSceneBits = 129U;
inline constexpr std::size_t kGateBits = 147U;

[[nodiscard]] constexpr bool source_slot(std::uint32_t key, std::uint8_t type,
                                         std::uint16_t slot) noexcept {
    return key == kRegistry && type == 1U && slot == 0U;
}
[[nodiscard]] constexpr bool scene_slot(std::uint32_t key, std::uint8_t type,
                                        std::uint16_t slot) noexcept {
    return key == kRegistry && type == 43U && slot == 1U;
}
[[nodiscard]] constexpr bool gate_slot(std::uint32_t key, std::uint8_t type,
                                       std::uint16_t slot) noexcept {
    return key == kRegistry && type == 23U && slot == 16U;
}

template<class Writer>
[[nodiscard]] bool absent_reference(Writer& writer) noexcept {
    return writer.write(0x811C9DC5U, 32) && writer.write(0, 7)
        && writer.write(0x7FFFU, 16);
}

/** Native 80807EC9 source; authored inline placement, Scene-request mode 1.
 * The one source owns both the orb actor and the following animation wrappers. */
template<class Writer>
[[nodiscard]] bool write_source(Writer& writer, std::uint32_t generation = 1, bool requested = true) noexcept {
    if (!generation || generation > 0x7FFFFFFFU) return false;
    bool ok = writer.write(1, 1) && absent_reference(writer)
        && writer.write(1, 1) && absent_reference(writer)
        && writer.write(1, 1) && writer.write(0, 3)
        && writer.write(1, 1) && writer.write(1, 4)
        && writer.write(0x80000000U + (requested ? 1U : 0U), 32)
        && writer.write(1, 1) && writer.write(0, 4)
        && writer.write(1, 1)
        && writer.write(1, 3) && writer.write(1, 2)
        && writer.write(1, 3) && writer.write(1, 2) && writer.write(1, 3)
        && writer.write(1, 1) && writer.write(generation, 31)
        && writer.write(1, 1) && writer.write(0, 32)
        && writer.write(1, 1) && writer.write(0x811C9DC5U, 32);
    for (unsigned reference = 0; ok && reference < 4; ++reference)
        ok = writer.write(1, 1) && absent_reference(writer);
    return ok && writer.write(1, 1) && writer.write(0, 31)
        && writer.write(1, 1) && writer.write(0, 31)
        && writer.write(1, 1) && writer.write(1, 6)
        && writer.write(1, 1) && writer.write(1, 5)
        && writer.write(1, 1) && writer.write(0, 31)
        && writer.write(1, 2) && writer.write(2, 3) // retirement0, request mode1
        && writer.write(1, 1) && writer.write(0x811C9DC5U, 32);
}

/** 8080626B: generation, stop, sources, source revision, retained external events.
 * B41330 delivers newly present C7 once; retaining it does not restart the graph. */
template<class Writer>
[[nodiscard]] bool write_scene(Writer& writer, bool portalRequested) noexcept {
    return writer.write(kSceneGenerationWire, 32) && writer.write(0, 1)
        && writer.write(1, 4) && writer.write(kRegistry, 32)
        && writer.write(2, 7) && writer.write(0x8000U, 16)
        && writer.write(1, 31) && writer.write(portalRequested ? 1U : 0U, 6)
        && (!portalRequested || writer.write(kPortalRequest, 32));
}

/** Only position changes. Native power1/lock0 revisions stay absent (-1). */
template<class Writer>
[[nodiscard]] bool write_gate(Writer& writer, bool released) noexcept {
    return writer.write(released ? 0U : 0x3F800000U, 32)
        && writer.write(released ? 0x8002U : 0x8001U, 16)
        && writer.write(released ? 0U : 1U, 1)
        && writer.write(0x3F800000U, 32) && writer.write(0x7FFFU, 16)
        && writer.write(0, 1)
        && writer.write(0, 32) && writer.write(0x7FFFU, 16)
        && writer.write(0, 1);
}
} // namespace dawn::state::activity::omega::ikora
