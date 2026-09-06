#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::state::build_data::scenarios {

/** Serialized class that owns the keyed Towerfall cue table. */
inline constexpr std::uint32_t kCueTableClass = 0x80804F72U;
/** Element classes measured on the outer record array and each record's value array. */
inline constexpr std::uint32_t kCueTableRecordClass = 0x80804F74U;
inline constexpr std::uint32_t kCueTableValueClass = 0x80804F76U;
/** Every value field is a typed hash with this measured type id. */
inline constexpr std::uint32_t kCueTableHashClass = 0x80B9EBCDU;
/** FNV basis reused by package data as the authored-absent hash. */
inline constexpr std::uint32_t kCueTableAbsentHash = 0x811C9DC5U;

inline constexpr std::size_t kCueTableHashCount = 4;
inline constexpr std::size_t kCueTableRecordCapacity = 64;
inline constexpr std::size_t kCueTableValueCapacity = 128;

struct CueTableHash final {
    std::uint32_t classId{};
    std::uint32_t value{};
};

struct CueTableValue final {
    std::array<CueTableHash, kCueTableHashCount> hashes{};
    std::uint32_t flags{};
};

struct CueTableRecord final {
    std::uint32_t key{};
    std::uint32_t domain{};
    std::uint64_t enabled{};
    std::uint64_t flags{};
    std::uint16_t firstValue{};
    std::uint16_t valueCount{};
};

struct CueTable final {
    std::array<CueTableRecord, kCueTableRecordCapacity> records{};
    std::array<CueTableValue, kCueTableValueCapacity> values{};
    std::size_t recordCount{};
    std::size_t valueCount{};
};

/**
 * Decodes the measured 0x80804F72 self-relative record table.
 * The routine validates both array classes, fixed strides, typed-hash classes, and all bounds.
 */
[[nodiscard]] bool decode_cue_table(std::span<const std::byte> blob,
                                    CueTable& output) noexcept;

} // namespace sunrise::state::build_data::scenarios
