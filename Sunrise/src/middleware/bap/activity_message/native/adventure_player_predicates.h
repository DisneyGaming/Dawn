#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <optional>
namespace sunrise::middleware::bap::activity_message::native::player_predicates {
// Embedded808094E1 in native player participation80804F30. These are authored
// predicate names on one admitted player, not player identities or account flags.
struct Set final {
    std::array<std::uint32_t,32> hashes{};std::size_t count{};
    friend bool operator==(const Set&,const Set&)=default;
};
[[nodiscard]] constexpr bool valid(const Set& set) noexcept {
    if(set.count>set.hashes.size())return false;
    for(std::size_t i=0;i<set.count;++i)for(std::size_t j=0;j<i;++j)if(set.hashes[i]==set.hashes[j])return false;
    return true;
}
// Preserve existing service-owned names and order; failed additions are atomic.
[[nodiscard]] constexpr bool unite(Set& set,std::span<const std::uint32_t> names) noexcept {
    if(!valid(set))return false;auto result=set;
    for(const auto name:names) {
        bool present{};for(std::size_t i=0;i<result.count;++i)if(result.hashes[i]==name)present=true;
        if(present)continue;
        if(result.count==result.hashes.size())return false;
        result.hashes[result.count++]=name;
    }
    set=result;return true;
}
// Legacy service intent keeps its position; the generic set adds names without
// erasing or duplicating it. Overflow rejects the complete candidate.
[[nodiscard]] constexpr std::optional<Set> compose(const Set& set,bool legacy,std::uint32_t legacyName) noexcept {
    if(!valid(set))return {};
    Set result{};
    if(legacy && !unite(result,{&legacyName,1}))return {};
    if(!unite(result,std::span(set.hashes).first(set.count)))return {};
    return result;
}
[[nodiscard]] constexpr std::size_t body_bits(const Set& set) noexcept {return valid(set)?6+32*set.count:0;}
template<class Writer> [[nodiscard]] bool write(Writer& writer,const Set& set) noexcept {
    if(!valid(set) || !writer.write(set.count,6))return false;
    for(std::size_t i=0;i<set.count;++i)if(!writer.write(set.hashes[i],32))return false;
    return true;
}
}
