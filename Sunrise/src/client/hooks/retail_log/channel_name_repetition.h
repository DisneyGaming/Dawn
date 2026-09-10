#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "../../../core/logging/repetition.h"

namespace sunrise::client::hooks::retail_log {

/** Only the observed empty/unknown display-label churn on an unchanged endpoint qualifies. */
[[nodiscard]] inline bool is_placeholder_name_change(std::string_view text) noexcept {
    constexpr std::string_view prefix = "networking:channel: Channel name change from ";
    constexpr std::string_view unknown = "<UNKNOWN PEER NAME>";
    if (!text.starts_with(prefix)) {
        return false;
    }
    text.remove_prefix(prefix.size());
    const std::size_t separator = text.find(" to ");
    if (separator == std::string_view::npos) {
        return false;
    }
    const std::string_view previous = text.substr(0, separator);
    const std::string_view next = text.substr(separator + 4);
    const std::size_t hash = previous.find('#');
    // HASHMARK is literal text in the native log format, not in the stored channel name.
    const std::size_t marker = next.find(" HASHMARK");
    if (hash == std::string_view::npos || marker == std::string_view::npos || hash == 0
        || previous.substr(0, hash) != next.substr(0, marker)) {
        return false;
    }
    const std::string_view address = previous.substr(0, hash);
    if (address.find(':') == std::string_view::npos
        || address.find_first_not_of("0123456789.:") != std::string_view::npos) {
        return false;
    }
    const std::string_view oldLabel = previous.substr(hash + 1);
    const std::string_view newLabel = next.substr(marker + 9);
    return (oldLabel == unknown && newLabel.empty())
           || (oldLabel.empty() && newLabel == " <UNKNOWN PEER NAME>");
}

/** Each exact direction/site/endpoint gets its first line and a periodic count summary. */
class ChannelNameRepetition {
public:
    [[nodiscard]] core::log::RepetitionReport observe(std::int32_t site,
                                                     std::string_view text,
                                                     std::uint64_t now) noexcept {
        if (!is_placeholder_name_change(text) || text.size() >= Key{}.text.size()) {
            if (text.starts_with("networking:channel: Channel name change from ")) {
                observations_.clear();
            }
            return {};
        }
        Key key{};
        key.site = site;
        text.copy(key.text.data(), text.size());
        auto* counter = observations_.find_or_insert(key);
        return counter == nullptr ? core::log::RepetitionReport{} : counter->observe(now);
    }

private:
    struct Key {
        std::int32_t site{};
        std::array<char, 320> text{};
        bool operator==(const Key&) const = default;
    };
    core::log::ObservationTable<Key, core::log::RepetitionCounter, 16> observations_{};
};

} // namespace sunrise::client::hooks::retail_log
