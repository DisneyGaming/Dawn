#pragma once
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace mission_parameter_fixture {
inline bool numeric(std::string& document,std::string_view name,std::int64_t value) {
    const std::string key="\""+std::string(name)+"\"";
    const auto first=document.find(key);
    if(first==std::string::npos || document.find(key,first+key.size())!=std::string::npos) return false;
    auto begin=first+key.size();
    while(begin<document.size() && std::isspace(static_cast<unsigned char>(document[begin]))) ++begin;
    if(begin==document.size() || document[begin++]!=':') return false;
    while(begin<document.size() && std::isspace(static_cast<unsigned char>(document[begin]))) ++begin;
    auto end=begin;if(end<document.size() && document[end]=='-') ++end;
    const auto digits=end;while(end<document.size() && std::isdigit(static_cast<unsigned char>(document[end]))) ++end;
    if(end==digits || (end<document.size() && !std::isspace(static_cast<unsigned char>(document[end]))
        && document[end]!=',' && document[end]!='}')) return false;
    document.replace(begin,end-begin,std::to_string(value));return true;
}
}
