#include "cue_table_decoder.h"

#include <cstring>

#include "../../../middleware/content/packages/tables/definition_index_table.h"

namespace sunrise::state::build_data::scenarios {
namespace {

namespace tables = middleware::content::packages::tables;

constexpr std::size_t kRootDescriptorOffset = 8;
constexpr std::size_t kRecordStride = 40;
constexpr std::size_t kRecordKeyOffset = 0;
constexpr std::size_t kRecordDomainOffset = 4;
constexpr std::size_t kRecordEnabledOffset = 8;
constexpr std::size_t kRecordValueDescriptorOffset = 16;
constexpr std::size_t kRecordFlagsOffset = 32;
constexpr std::size_t kValueStride = 36;
constexpr std::size_t kTypedHashStride = 8;
constexpr std::size_t kValueFlagsOffset = kCueTableHashCount * kTypedHashStride;

template <typename Value>
[[nodiscard]] bool read(std::span<const std::byte> blob,
                        std::size_t offset,
                        Value& output) noexcept {
    if (offset > blob.size() || blob.size() - offset < sizeof output) {
        return false;
    }
    std::memcpy(&output, blob.data() + offset, sizeof output);
    return true;
}

[[nodiscard]] bool range_fits(std::span<const std::byte> blob,
                              std::size_t offset,
                              std::uint64_t count,
                              std::size_t stride) noexcept {
    return count <= blob.size() / stride && offset <= blob.size() - count * stride;
}

} // namespace

bool decode_cue_table(std::span<const std::byte> blob, CueTable& output) noexcept {
    output = {};
    tables::Array records{};
    if (!tables::find_array_at(blob, kRootDescriptorOffset, records)
        || records.elementClass != kCueTableRecordClass
        || records.count > output.records.size()
        || !range_fits(blob, records.dataOffset, records.count, kRecordStride)) {
        return false;
    }

    for (std::uint64_t recordOrdinal = 0; recordOrdinal < records.count; ++recordOrdinal) {
        const std::size_t recordOffset =
            records.dataOffset + static_cast<std::size_t>(recordOrdinal) * kRecordStride;
        tables::Array values{};
        std::uint64_t enabled = 0;
        std::uint64_t flags = 0;
        CueTableRecord record{};
        if (!read(blob, recordOffset + kRecordKeyOffset, record.key)
            || !read(blob, recordOffset + kRecordDomainOffset, record.domain)
            || !read(blob, recordOffset + kRecordEnabledOffset, enabled)
            || !read(blob, recordOffset + kRecordFlagsOffset, flags)
            || !tables::find_array_at(
                blob, recordOffset + kRecordValueDescriptorOffset, values)
            || values.elementClass != kCueTableValueClass || values.count == 0
            || values.count > output.values.size() - output.valueCount
            || !range_fits(blob, values.dataOffset, values.count, kValueStride)) {
            output = {};
            return false;
        }
        record.enabled = enabled;
        record.flags = flags;
        record.firstValue = static_cast<std::uint16_t>(output.valueCount);
        record.valueCount = static_cast<std::uint16_t>(values.count);

        for (std::uint64_t valueOrdinal = 0; valueOrdinal < values.count; ++valueOrdinal) {
            const std::size_t valueOffset =
                values.dataOffset + static_cast<std::size_t>(valueOrdinal) * kValueStride;
            CueTableValue value{};
            for (std::size_t hash = 0; hash < value.hashes.size(); ++hash) {
                const std::size_t hashOffset = valueOffset + hash * kTypedHashStride;
                if (!read(blob, hashOffset, value.hashes[hash].classId)
                    || !read(blob,
                             hashOffset + sizeof(std::uint32_t),
                             value.hashes[hash].value)
                    || value.hashes[hash].classId != kCueTableHashClass) {
                    output = {};
                    return false;
                }
            }
            if (!read(blob, valueOffset + kValueFlagsOffset, value.flags)) {
                output = {};
                return false;
            }
            output.values[output.valueCount++] = value;
        }
        output.records[output.recordCount++] = record;
    }
    return output.recordCount != 0;
}

} // namespace sunrise::state::build_data::scenarios
