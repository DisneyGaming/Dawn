// SPDX-License-Identifier: GPL-3.0-only
// Adapted from Sundial by KyleThmpsn. See vendor/sundial/NOTICE.md.
#include "localized_strings.h"
#include <limits>

namespace dawn::state::editor::strings {
namespace {
void utf8(std::string& out, std::uint32_t c) {
    if (c <= 0x7FU) out.push_back(static_cast<char>(c));
    else if (c <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (c >> 6U)));
        out.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
    } else if (c <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | (c >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((c >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (c >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((c >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((c >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
    }
}
bool append_shifted(std::span<const std::byte> bytes, std::uint16_t shift, std::string& out) {
    for (std::size_t i = 0; i < bytes.size();) {
        const auto first = std::to_integer<std::uint8_t>(bytes[i++]);
        std::uint32_t code = first;
        unsigned count = 0;
        if (first >= 0xF0U && first <= 0xF4U) { code &= 7; count = 3; }
        else if (first >= 0xE0U && first <= 0xEFU) { code &= 15; count = 2; }
        else if (first >= 0xC2U && first <= 0xDFU) { code &= 31; count = 1; }
        else if (first >= 0x80U) return false;
        if (count > bytes.size() - i) return false;
        for (unsigned n = 0; n < count; ++n) {
            const auto next = std::to_integer<std::uint8_t>(bytes[i++]);
            if ((next & 0xC0U) != 0x80U) return false;
            code = (code << 6U) | (next & 0x3FU);
        }
        if ((count == 1 && code < 0x80U) || (count == 2 && code < 0x800U)
            || (count == 3 && code < 0x10000U) || code > 0x10FFFFU
            || (code >= 0xD800U && code <= 0xDFFFU)) return false;
        const auto shifted = code + shift;
        if (shifted <= 0x10FFFFU && !(shifted >= 0xD800U && shifted <= 0xDFFFU)) code = shifted;
        if (code != 0) utf8(out, code);
    }
    return true;
}
}
bool decode(std::span<const std::byte> header, std::span<const std::byte> data,
            std::unordered_map<std::uint32_t, std::string>& output) {
    namespace tables = middleware::content::packages::tables;
    tables::Array hashes{}, parts{}, combos{};
    if (!tables::find_array_at(header, 8, hashes) || !tables::find_array_at(data, 8, parts)
        || !tables::find_array_at(data, 0x48, combos) || hashes.count != combos.count
        || hashes.count > (header.size() - hashes.dataOffset) / 4
        || parts.count > (data.size() - parts.dataOffset) / 0x20
        || combos.count > (data.size() - combos.dataOffset) / 0x10) return false;
    std::unordered_map<std::uint32_t, std::string> decoded;
    for (std::size_t i = 0; i < combos.count; ++i) {
        const auto combo = combos.dataOffset + i * 0x10;
        std::size_t first{}; std::int64_t count{};
        if (!relative(data, combo, first) || !read(data, combo + 8, count) || count < 0
            || first < parts.dataOffset || (first - parts.dataOffset) % 0x20 != 0
            || first > parts.dataOffset + parts.count * 0x20
            || static_cast<std::uint64_t>(count) > (parts.dataOffset + parts.count * 0x20 - first) / 0x20) return false;
        std::string value;
        for (std::size_t p = 0; p < static_cast<std::size_t>(count); ++p) {
            const auto part = first + p * 0x20;
            std::size_t start{}; std::uint16_t length{}, shift{};
            if (!relative(data, part + 8, start) || !read(data, part + 0x14, length)
                || !read(data, part + 0x18, shift) || length > data.size() - start
                || value.size() + length > 65536
                || !append_shifted(data.subspan(start, length), shift, value)) return false;
        }
        std::uint32_t hash{};
        if (!read(header, hashes.dataOffset + i * 4, hash)) return false;
        decoded.emplace(hash, std::move(value));
    }
    output = std::move(decoded);
    return true;
}
}
