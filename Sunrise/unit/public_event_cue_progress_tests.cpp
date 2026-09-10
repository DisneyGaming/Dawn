#include "middleware/bap/activity_message/native/adventure_cue_authority.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace cue = sunrise::middleware::bap::activity_message::native::cue;
namespace bits = sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool result, const char* message) {
    ++checks;
    if (!result) { std::cerr << message << '\n'; std::exit(1); }
}
template<class T> void save(const std::filesystem::path& path, const T& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    check(bool(stream), "fixture export");
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    cue::Request baseline{0xC8229B2B, 0x00D1C5B9, 107, 0, 15};
    std::array<std::byte, 601> original{};
    std::ifstream fixture(argv[2], std::ios::binary);
    fixture.read(reinterpret_cast<char*>(original.data()), original.size());
    check(bool(fixture), "existing native fixture read");
    std::array<std::byte, 601> encoded{};
    bits::Writer baselineWriter(encoded);
    check(cue::write(baselineWriter, baseline) && encoded == original, "absent progress preserves existing wire bytes");
    unsigned index{};
    for (std::uint8_t ring = 0; ring < 3; ++ring) {
        for (const auto progress : {cue::Progress{0, 100}, cue::Progress{1, 18}, cue::Progress{9, 18},
                                   cue::Progress{18, 18}, cue::Progress{INT32_MAX, INT32_MAX}}) {
            for (const bool clear : {false, true}) {
                auto request = baseline;
                request.ring = ring;
                request.hasProgress = true;
                request.progress = progress;
                request.clear = clear;
                check(cue::valid(request), "valid bounded progress request");
                bits::Writer writer(encoded);
                check(cue::write(writer, request) && writer.bit_count() == cue::kBits, "constant native record width");
                const auto decoded = cue::decoded(request);
                for (unsigned row = 0; row < 3; ++row) {
                    std::array<std::int32_t, 4> actual{};
                    std::memcpy(actual.data(), decoded.data() + 0x58 + row * 0xF8, sizeof(actual));
                    const std::array<std::int32_t, 4> expected = row == ring && !clear
                        ? std::array<std::int32_t, 4>{progress.current, progress.target, -1, -1}
                        : std::array<std::int32_t, 4>{-1, -1, -1, -1};
                    check(actual == expected, "only active ring receives requested progress");
                }
                const auto stem = output / ("case-" + std::to_string(index++));
                save(stem.string() + ".body", encoded);
                save(stem.string() + ".expected", decoded);
                for (std::size_t size = 0; size < encoded.size(); ++size) {
                    bits::Writer shortWriter(std::span(encoded).first(size));
                    check(!cue::write(shortWriter, request), "truncated output rejected");
                }
            }
        }
    }
    for (const auto progress : {cue::Progress{-1, 100}, cue::Progress{0, 0}, cue::Progress{0, -1},
                               cue::Progress{101, 100}, cue::Progress{INT32_MIN, INT32_MAX}}) {
        auto request = baseline; request.hasProgress = true; request.progress = progress;
        auto writer = bits::Writer::measuring();
        check(!cue::valid(request) && !cue::write(writer, request) && writer.bit_count() == 0,
              "invalid progress emits no record");
    }
    std::cout << "PASS " << checks << " checks; exported " << index << " original-decoder cases\n";
}
