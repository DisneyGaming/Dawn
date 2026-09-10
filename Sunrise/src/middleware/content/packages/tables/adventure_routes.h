#pragma once
#include "activity_table.h"
#include <array>

namespace sunrise::middleware::content::packages::tables::adventure {
inline constexpr std::size_t kRootSlot=5;
inline constexpr std::uint32_t kClass=0x80805B8F;
inline constexpr std::size_t kMaximumChoices=16, kMaximumTokens=128;
struct Token final { std::uint32_t operation{}, argument{}; };
struct Choice final {
    // -1 is the native absent activity sentinel; it is not index 65535.
    std::int16_t activity{-1};
    std::array<Token,kMaximumTokens> tokens{};
    std::size_t tokenCount{};
};
struct Routes final {
    std::uint32_t selector{};
    std::array<Choice,kMaximumChoices> choices{};
    std::size_t count{};
};
enum class Result : std::uint8_t { found, absent, invalid };
namespace detail {
[[nodiscard]] inline bool array(std::span<const std::byte> bytes,std::size_t descriptor,
    std::uint32_t expected,std::size_t stride,std::size_t maximum,Array& value,bool allowEmpty=false) noexcept {
    std::uint64_t count{};
    std::int64_t relative{};
    if(allowEmpty && activities::read(bytes,descriptor,count) && count==0
        && activities::read(bytes,descriptor+8,relative)) {
        value={};return relative==0;
    }
    return find_array_at(bytes,descriptor,value) && value.elementClass==expected
        && value.count<=maximum && value.dataOffset<=bytes.size()
        && value.count<=(bytes.size()-value.dataOffset)/stride;
}
}
// Native consumer: C8B530 -> C8B5A0 (investment root+60/slot 5);
// 109D1C0 walks ordered 24-byte choices and evaluates their 80807D31 expressions.
// Decode only. A caller must not turn a syntactically valid choice into eligibility.
[[nodiscard]] inline Result lookup(std::span<const std::byte> bytes,std::uint32_t actualClass,
    std::uint32_t selector,Routes& output) noexcept {
    output={};
    Array groups{};
    if(actualClass!=kClass || selector==0 || selector==UINT32_MAX || selector==0x811C9DC5
        || !detail::array(bytes,8,0x80805B95,24,2048,groups)) return Result::invalid;
    std::size_t match{}, matches{};
    for(std::size_t i=0;i<groups.count;++i) {
        std::uint32_t key{};
        if(!activities::read(bytes,groups.dataOffset+i*24,key)) return Result::invalid;
        if(key==selector) {match=groups.dataOffset+i*24;++matches;}
    }
    if(!matches) return Result::absent;
    if(matches!=1) return Result::invalid;
    Array choices{};
    if(!detail::array(bytes,match+8,0x80805B97,24,kMaximumChoices,choices,true)) return Result::invalid;
    Routes result{};result.selector=selector;result.count=static_cast<std::size_t>(choices.count);
    for(std::size_t i=0;i<result.count;++i) {
        const auto row=choices.dataOffset+i*24;
        auto& choice=result.choices[i];
        Array tokens{};
        if(!activities::read(bytes,row+16,choice.activity)
            || choice.activity < -1
            || !detail::array(bytes,row,0x80807D31,8,kMaximumTokens,tokens,true)) return Result::invalid;
        choice.tokenCount=static_cast<std::size_t>(tokens.count);
        for(std::size_t j=0;j<choice.tokenCount;++j)
            if(!activities::read(bytes,tokens.dataOffset+j*8,choice.tokens[j])) return Result::invalid;
    }
    output=result;return Result::found;
}
} // namespace sunrise::middleware::content::packages::tables::adventure
