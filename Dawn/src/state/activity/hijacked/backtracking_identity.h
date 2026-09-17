#pragma once
#include <cstddef>
#include <cstdint>
namespace dawn::state::activity::hijacked {
struct BacktrackingPart {
    std::uint16_t row{};std::uint32_t facet{UINT32_MAX},net{UINT32_MAX},parent{UINT32_MAX};
    friend bool operator==(const BacktrackingPart&,const BacktrackingPart&)=default;
};
enum class NativeAncestry : std::uint8_t {unrelated,descendant,invalid};
// Native salted parent links, rather than equal gameentity IDs, own attachments.
// The reader must verify allocation, manager ownership, row salt and local mode.
template<class ReadParent>
NativeAncestry native_ancestry(std::uint32_t root,std::uint32_t parent,ReadParent read) noexcept {
    if(root==UINT32_MAX) {return NativeAncestry::invalid;}
    for(unsigned depth=0;depth<32;++depth) {
        if(parent==UINT32_MAX) {return NativeAncestry::unrelated;}
        if(parent==root) {return NativeAncestry::descendant;}
        std::uint32_t next{};if(!read(parent,next) || next==parent) {return NativeAncestry::invalid;}
        parent=next;
    }
    return NativeAncestry::invalid;
}
inline bool native_part_matches(const BacktrackingPart& expected,std::uint8_t kind,std::int8_t owner,
    std::uint32_t facet,std::uint32_t net,std::uint32_t parent) noexcept {
    return expected.row<1024 && expected.facet!=UINT32_MAX && expected.net!=UINT32_MAX
        && kind==0 && (owner==-1 || owner==-2) && facet==expected.facet && net==expected.net && parent==expected.parent;
}
}
