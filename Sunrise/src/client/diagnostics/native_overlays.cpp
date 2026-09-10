#include "native_overlays.h"

#include <Windows.h>
#include <intrin.h>

#include <array>
#include <bit>
#include <cstddef>
#include <span>

#include "../executable/image.h"
#include "../memory/current_process_memory.h"
#include "native_overlay_ownership.h"

namespace sunrise::client::diagnostics::native_overlays {
namespace {

/** Exact reader bytes include the RIP-relative reference to the controlled byte. */
struct Descriptor final {
    const char* name;
    const char* description;
    std::uintptr_t flag;
    std::uintptr_t reader;
    std::array<std::uint8_t, 16> signature;
};

constexpr std::array<Descriptor, static_cast<std::size_t>(Overlay::count)> kDescriptors{{
    {"Camera coordinates",
     "Native camera XYZ position text.",
     0x281329E,
     0x110B8C6,
     {0x80, 0x3D, 0xD1, 0x79, 0x70, 0x01, 0x00, 0x48, 0x8B, 0xD9, 0x0F, 0x84, 0xBD, 0x00, 0x00,
      0x00}},
    {"Activity instances",
     "Activity and instance state reported by the game.",
     0x30506DC,
     0x16CF08C,
     {0x80, 0x3D, 0x49, 0x16, 0x98, 0x01, 0x00, 0x0F, 0x84, 0xC0, 0x0E, 0x00, 0x00, 0x8B, 0x15,
      0xC9}},
    {"Session transitions",
     "Native transition state and timing.",
     0x30506DD,
     0x16CFF59,
     {0x80, 0x3D, 0x7D, 0x07, 0x98, 0x01, 0x00, 0x0F, 0x84, 0x26, 0x02, 0x00, 0x00, 0x8B, 0x15,
      0xEC}},
    {"Transition detail",
     "Adds session-manager detail while Session transitions is enabled.",
     0x30506DB,
     0x16CFF9F,
     {0x80, 0x3D, 0x35, 0x07, 0x98, 0x01, 0x00, 0x74, 0x1B, 0xE8, 0xB3, 0xE8, 0x75, 0xFF, 0x4C,
      0x8B}},
    {"Bubble hosts and slice readiness",
     "Native host, slice state and readiness records.",
     0x30507E5,
     0x16D09B5,
     {0x80, 0x3D, 0x29, 0xFE, 0x97, 0x01, 0x00, 0x0F, 0x84, 0x5E, 0x02, 0x00, 0x00, 0x8B, 0x15,
      0xD0}},
    {"Native retail log",
     "The game's most recent log events; unavailable during native cleanup.",
     0x30507E2,
     0x16D128E,
     {0x80, 0x3D, 0x4D, 0xF5, 0x97, 0x01, 0x00, 0x4C, 0x8B, 0xB4, 0x24, 0xA8, 0x0A, 0x00, 0x00,
      0x0F}},
    {"Activity status",
     "Current activity identity when a native session is connected.",
     0x30506D0,
     0x16CD8C0,
     {0x80, 0x3D, 0x09, 0x2E, 0x98, 0x01, 0x00, 0x4C, 0x8B, 0xF8, 0x48, 0x89, 0x45, 0xC8, 0x74,
      0x56}},
    {"Network channels",
     "Native channel-manager connection states.",
     0x30506D7,
     0x16CE714,
     {0x80, 0x3D, 0xBC, 0x1F, 0x98, 0x01, 0x00, 0x48, 0x8D, 0x3D, 0x56, 0x4C, 0x9A, 0x01, 0x44,
      0x0F}},
    {"Network sessions",
     "Session connections, ports and NAT status.",
     0x30506D8,
     0x16CE9B8,
     {0x80, 0x3D, 0x19, 0x1D, 0x98, 0x01, 0x00, 0x4C, 0x8D, 0x2D, 0x16, 0x1B, 0x4D, 0x00, 0x41,
      0xBF}},
    {"Session members",
     "Native local and remote membership records.",
     0x30506D9,
     0x16CEEB3,
     {0x80, 0x3D, 0x1F, 0x18, 0x98, 0x01, 0x00, 0x0F, 0x28, 0xB4, 0x24, 0x90, 0x0A, 0x00, 0x00,
      0x0F}},
    {"Inactivity status",
     "Configured timeouts and observed inactivity, for inspection.",
     0x30506DF,
     0x16D0C20,
     {0x80, 0x3D, 0xB8, 0xFA, 0x97, 0x01, 0x00, 0x0F, 0x84, 0x31, 0x02, 0x00, 0x00, 0x8B, 0x15,
      0x45}},
    {"Matchmaking status",
     "Native interface and matchmaking state.",
     0x30507E0,
     0x16D018C,
     {0x80, 0x3D, 0x4D, 0x06, 0x98, 0x01, 0x00, 0x0F, 0x84, 0xD5, 0x03, 0x00, 0x00, 0xE8, 0xD2,
      0x7A}},
    {"Connection quality",
     "Native session probes, responses and QoS statistics.",
     0x30507E1,
     0x16D0E61,
     {0x80, 0x3D, 0x79, 0xF9, 0x97, 0x01, 0x00, 0x4C, 0x8B, 0xBC, 0x24, 0xA0, 0x0A, 0x00, 0x00,
      0x4C}},
    {"Voice status",
     "Native voice connection diagnostics.",
     0x30507E3,
     0x16D1392,
     {0x80, 0x3D, 0x4A, 0xF4, 0x97, 0x01, 0x00, 0x48, 0x8B, 0xBC, 0x24, 0xE0, 0x0A, 0x00, 0x00,
      0x48}},
    {"Connection repair status",
     "Native timeout and repair history, for inspection.",
     0x30507E4,
     0x16D056E,
     {0x80, 0x3D, 0x6F, 0x02, 0x98, 0x01, 0x00, 0x0F, 0x84, 0x3A, 0x04, 0x00, 0x00, 0xE8, 0xB0,
      0x46}},
    {"Build information",
     "Native build and session diagnostic text.",
     0x280F110,
     0xE84AB9,
     {0x80, 0x3D, 0x50, 0xA6, 0x98, 0x01, 0x00, 0x0F, 0x84, 0xED, 0x01, 0x00, 0x00, 0xE8, 0x25,
      0x5F}},
}};

/** The qualified CMP must address this descriptor's exact flag, not an adjacent byte. */
[[nodiscard]] constexpr bool valid_descriptors() noexcept {
    for (const auto& entry : kDescriptors) {
        const auto& bytes = entry.signature;
        const std::uint32_t displacement = static_cast<std::uint32_t>(bytes[2]) |
                                           (static_cast<std::uint32_t>(bytes[3]) << 8) |
                                           (static_cast<std::uint32_t>(bytes[4]) << 16) |
                                           (static_cast<std::uint32_t>(bytes[5]) << 24);
        if (bytes[0] != 0x80 || bytes[1] != 0x3D || bytes[6] != 0 ||
            entry.reader + 7 + displacement != entry.flag) {
            return false;
        }
    }
    return true;
}
static_assert(valid_descriptors());

struct Control final {
    detail::Ownership ownership{};
    const char* lastError{};
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::array<Control, kDescriptors.size()> g_controls{};
std::uintptr_t g_base{};
executable::ExecutableImage g_image{};

struct Lock final {
    Lock() noexcept { AcquireSRWLockExclusive(&g_lock); }
    ~Lock() noexcept { ReleaseSRWLockExclusive(&g_lock); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};

[[nodiscard]] const Descriptor* descriptor(Overlay overlay) noexcept {
    const auto index = static_cast<std::size_t>(overlay);
    return index < kDescriptors.size() ? &kDescriptors[index] : nullptr;
}

/** The main executable's PE section map is cached; no switch is set during discovery. */
[[nodiscard]] bool image_ready() noexcept {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (base == 0) {
        return false;
    }
    if (base != g_base) {
        g_controls = {};
        g_image = {};
        g_base = base;
    }
    return g_image.count != 0 || executable::inspect(std::bit_cast<std::byte*>(base), g_image);
}

[[nodiscard]] bool executable_range(std::uintptr_t address, std::size_t size) noexcept {
    for (std::size_t index = 0; index < g_image.count; ++index) {
        const auto section = g_image.sections[index];
        const auto start = reinterpret_cast<std::uintptr_t>(section.data());
        if (address >= start && address - start <= section.size() &&
            size <= section.size() - (address - start)) {
            return true;
        }
    }
    return false;
}

/** Never change page protection to make an unavailable switch writable. */
[[nodiscard]] bool writable_flag(std::uintptr_t address) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(std::bit_cast<const void*>(address), &region, sizeof region) !=
            sizeof region ||
        region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
        reinterpret_cast<std::uintptr_t>(region.AllocationBase) != g_base ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = region.Protect & 0xFF;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
}

/** Signature checks run before showing an enabled control and again before each write. */
[[nodiscard]] Snapshot inspect(const Descriptor& entry) noexcept {
    Snapshot result{};
    if (!image_ready() || g_base > UINTPTR_MAX - entry.flag ||
        g_base > UINTPTR_MAX - entry.reader) {
        result.reason = "Native overlays are unavailable for this executable.";
        return result;
    }
    std::array<std::uint8_t, 16> actual{};
    if (!executable_range(g_base + entry.reader, actual.size()) ||
        !memory::read_current_process(nullptr, g_base + entry.reader,
                                      std::as_writable_bytes(std::span{actual})) ||
        actual != entry.signature) {
        result.reason = "Native reader does not match this game build.";
        return result;
    }
    if (!writable_flag(g_base + entry.flag) ||
        !memory::read_current_process(
            nullptr, g_base + entry.flag,
            std::as_writable_bytes(std::span{&result.value, std::size_t{1}}))) {
        result.reason = "Native display switch is unavailable.";
        return result;
    }
    result.available = true;
    return result;
}

/** A one-byte CAS refuses a stale UI value and preserves an intervening native edit. */
[[nodiscard]] bool exchange(std::uintptr_t address, std::uint8_t expected,
                            std::uint8_t desired) noexcept {
    __try {
        const char prior = _InterlockedCompareExchange8(std::bit_cast<volatile char*>(address),
                                                        std::bit_cast<char>(desired),
                                                        std::bit_cast<char>(expected));
        return std::bit_cast<std::uint8_t>(prior) == expected;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

const char* display_name(Overlay overlay) noexcept {
    const auto* entry = descriptor(overlay);
    return entry != nullptr ? entry->name : "Unknown native overlay";
}

const char* description(Overlay overlay) noexcept {
    const auto* entry = descriptor(overlay);
    return entry != nullptr ? entry->description : "";
}

Snapshot snapshot(Overlay overlay) noexcept {
    const auto* entry = descriptor(overlay);
    if (entry == nullptr) {
        return {false, 0, "Unknown native overlay.", nullptr};
    }
    const Lock lock;
    Snapshot result = inspect(*entry);
    auto& control = g_controls[static_cast<std::size_t>(overlay)];
    if (result.available) {
        control.ownership.observe(result.value);
    }
    result.lastError = control.lastError;
    return result;
}

bool set_enabled(Overlay overlay, std::uint8_t expected, bool enabled) noexcept {
    const auto* entry = descriptor(overlay);
    if (entry == nullptr) {
        return false;
    }
    const Lock lock;
    const Snapshot current = inspect(*entry);
    auto& control = g_controls[static_cast<std::size_t>(overlay)];
    if (!current.available) {
        control.lastError = current.reason;
        return false;
    }
    control.ownership.observe(current.value);
    if (current.value != expected) {
        control.lastError = "Native state changed; use the updated switch to try again.";
        return false;
    }
    const std::uint8_t desired = enabled ? 1 : 0;
    if (desired != current.value) {
        if (!exchange(g_base + entry->flag, expected, desired)) {
            // An intervening change loses our earlier cleanup claim as well.
            control.ownership.owned = false;
            control.lastError = "Native state changed or the switch could not be updated.";
            return false;
        }
        control.ownership.written({current.value, desired});
    }
    control.lastError = nullptr;
    return true;
}

void shutdown() noexcept {
    const Lock lock;
    for (std::size_t index = 0; index < kDescriptors.size(); ++index) {
        auto& ownership = g_controls[index].ownership;
        if (!ownership.owned) {
            continue;
        }
        const Snapshot current = inspect(kDescriptors[index]);
        if (current.available) {
            ownership.observe(current.value);
            if (ownership.owned) {
                (void)exchange(g_base + kDescriptors[index].flag, ownership.last,
                               ownership.original);
            }
        }
    }
    g_controls = {};
    g_image = {};
    g_base = 0;
}

} // namespace sunrise::client::diagnostics::native_overlays
