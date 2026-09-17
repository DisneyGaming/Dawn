// Included by the production callback fixture. The authored action tables,
// original fullbody producer, cached scalar evaluator, native clock and renderer
// clip collector all execute. Only environment/math services and the upstream
// scalar-cache producer are modeled; no expected output row is synthesized.
bool native_false() noexcept { return false; }
std::uint32_t native_zero() noexcept { return 0; }
void native_cookie(std::uintptr_t) noexcept {}
float native_interpolate() noexcept { return 0.F; }
void* native_copy(void* to, const void* from, std::size_t count) noexcept { return std::memcpy(to, from, count); }
void* native_clear(void* to, int value, std::size_t count) noexcept { return std::memset(to, value, count); }
float native_mod(float value, float divisor) noexcept { return std::fmod(value, divisor); }
bool native_refs_equal(const void* left, const void* right) noexcept { return std::memcmp(left, right, 12) == 0; }
void* native_pose(void* out, const void*) noexcept { std::memset(out, 0, 32); return out; }
void native_pose_angles(const void*, float* first, float* second) noexcept { *first = *second = 0.F; }

void setup_native_fullbody(const char* nativeImage) {
    // Copy full function ranges, not split unwind records.
    for (const auto& range : std::array{
        std::array{0x104B7A0U, 0x904U}, std::array{0x104CFC0U, 0x174U},
        std::array{0x104B600U, 0x65U},
        std::array{0x104C800U, 0x1C3U},
        std::array{0x104CE80U, 0x134U}, std::array{0x10512D0U, 0x11U},
        std::array{0x1050220U, 0x34U}, std::array{0x1050F10U, 0x42U},
        std::array{0x1050F60U, 0xE2U}, std::array{0x104E3E0U, 0xEBU},
        std::array{0x10474A0U, 0x70U}, std::array{0x1047510U, 0x80U},
        std::array{0x104CC70U, 0x14DU}, std::array{0x1047A20U, 0x15CU},
        std::array{0x1047B80U, 0x15FU},
        std::array{0x1048C50U, 0x5CU},
        std::array{0x1047590U, 0x22AU}, std::array{0x1047380U, 0x8CU},
        std::array{0x1049F70U, 0x8CU}, std::array{0x104E720U, 0x2AEU},
        std::array{0x104A410U, 0xAEU}, std::array{0x104B750U, 0x4AU},
        std::array{0x104B250U, 0x8AU}, std::array{0x104C9D0U, 0x1D4U},
        std::array{0x104A700U, 0x160U}, std::array{0xC84EF0U, 0x32U},
        std::array{0xC886C0U, 0x1C9U}, std::array{0xC85A50U, 0xC6U},
        std::array{0xC85BB0U, 0x6FU}, std::array{0xC84E40U, 0x64U},
        std::array{0x1040990U, 0xC2U}, std::array{0x1040A60U, 0x15AU},
        std::array{0x187CB30U, 0x51U}})
        copy_file(nativeImage, image + range[0], range[1], range[0]);
    for (const auto rva : {0x1BD39CCU, 0x1BCCA60U, 0x1BA2B7CU, 0x1BAB014U})
        copy_file(nativeImage, image + rva, 16, rva);
    fixture_thunk(0x187C480, reinterpret_cast<void*>(&native_cookie));
    // Adjacent six-byte import veneers cannot hold the generic twelve-byte thunk.
    for (const auto& item : std::array{
        std::pair{std::uintptr_t{0x187E862}, reinterpret_cast<void*>(&native_clear)},
        std::pair{std::uintptr_t{0x187E85C}, reinterpret_cast<void*>(&native_copy)}}) {
        const auto cell = item.first == 0x187E862 ? 0x187EF00U : 0x187EF08U;
        image[item.first] = std::byte{0xFF}; image[item.first + 1] = std::byte{0x25};
        put(image, item.first + 2, static_cast<std::int32_t>(cell - item.first - 6));
        put(image, cell, item.second);
    }
    fixture_thunk(0x187E9B2, reinterpret_cast<void*>(&native_mod));
    fixture_thunk(0x36A100, reinterpret_cast<void*>(&native_interpolate));
    fixture_thunk(0x3B6800, reinterpret_cast<void*>(&native_false));
    fixture_thunk(0x104B160, reinterpret_cast<void*>(&native_false));
    fixture_thunk(0x6260781, reinterpret_cast<void*>(&native_zero));
    fixture_thunk(0x584DFF2, reinterpret_cast<void*>(&native_zero));
    fixture_thunk(0x384ED0, reinterpret_cast<void*>(&native_pose));
    fixture_thunk(0x38B400, reinterpret_cast<void*>(&native_pose_angles));
    fixture_thunk(0x4A48E0, reinterpret_cast<void*>(&native_refs_equal));
    DWORD old{};
    for (auto rva : {0x1047000U, 0x1048000U, 0x1049000U, 0x104A000U, 0x104B000U, 0x104C000U,
        0x104D000U, 0x104E000U, 0x1050000U, 0x1051000U, 0x1040000U, 0xC84000U, 0xC88000U, 0xC85000U,
        0x187C000U, 0x187E000U, 0x36A000U, 0x3B6000U, 0x6260000U, 0x584D000U,
        0x384000U, 0x38B000U, 0x4A4000U})
        check(VirtualProtect(image + rva, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "execute original fullbody pipeline");
    FlushInstructionCache(GetCurrentProcess(), image, 0x6270000);
}
std::byte* left_group() noexcept { return fullbodyBody + 0x1600 + 2 * combat::kActionGroupBytes; }

bool native_fullbody_producer(std::byte* body, const void* baseFrame, float dt,
                             std::uint8_t mode, const void*, void*) noexcept {
    // The native graph-input/cache producer is represented by its proven
    // scalar31 value. Original E720 reads its cached float4 branch unchanged.
    auto* state = body + 0x1C0;
    put(state, 0x30, std::int32_t{16});
    put(state, 0x38, std::uintptr_t{0x1000 - 0x1C0 - 0x48});
    put(state, 0x48, std::uintptr_t{0x1200 - 0x1C0 - 0x58});
    for (unsigned i = 0; i < 16; ++i) put(body, 0x1000 + i * 4, -dt);
    auto* row = scalarBody + 0x58 + read<std::ptrdiff_t>(scalarBody, 0x58) + 0x10 + 31 * 16;
    std::memcpy(body + 0x1200 + 14 * 16, row, 16);
    std::memcpy(body + 0x1200 + 15 * 16, row - 16, 16);
    std::array<std::byte, 32> spatial{};
    using Original = bool(__fastcall*)(std::byte*, const void*, float, std::uint8_t, const void*, void*) noexcept;
    return reinterpret_cast<Original>(image + 0x104B7A0)(body, baseFrame, dt, mode, spatial.data(), nullptr);
}
void prepare_native_fullbody() {
    put(fullbodyBody, 0xB29, std::uint8_t{1});
    put(fullbodyBody, 0xB20, 1.F);
    put(fullbodyBody, 0xB2B, std::uint8_t{0xFF});
    put(worldRows + (entityHandle & 0x1FFF) * 0x100, 0x3C, UINT32_MAX);
    // Six groups have already been allocated using the authored count.
    std::memset(fullbodyBody + 0x1600, 0, 6 * combat::kActionGroupBytes);
    put(fullbodyBody, 0x500, std::int32_t{6});
    fullBodyOriginal.store(native_fullbody_producer);
}
void render_output_test() {
    std::array<std::byte, 0x400> renderer{};
    put(renderer.data(), 0x20, std::int32_t{6});
    put(renderer.data(), 0x28, std::uintptr_t{0x40 - 0x38});
    using Copy = void(__fastcall*)(std::byte*, std::byte*) noexcept;
    reinterpret_cast<Copy>(image + 0x104A700)(fullbodyBody, renderer.data());
    auto* group = renderer.data() + 0x40 + 2 * combat::kActionGroupBytes;
    check(std::memcmp(group, left_group(), combat::kActionGroupBytes) == 0,
        "original104A700 publishes the observed custom output to renderer state");
    alignas(16) std::array<std::byte, 0x400> collected{};
    using Collect = void(__fastcall*)(const std::byte*, const std::byte*, float, std::byte*) noexcept;
    reinterpret_cast<Collect>(image + 0x1040990)(resolve_handle(observation::kBankAsset), group, 1.F, collected.data());
    check(read<std::int32_t>(collected.data(), 0x108) == 1
        && read<std::uint32_t>(collected.data(), 0x10C) == combat::kLeftClip
        && read<float>(collected.data(), 8) > 0.99F,
        "original render collector resolves actual output index20 to weighted80F4518B");
}
