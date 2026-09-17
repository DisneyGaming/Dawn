#pragma once
#include <array>
#include <cstdint>
namespace dawn::client::hooks::bootflow::garden_ending_scene_path {
struct Ref {std::uint32_t tag{},kind{};std::uint64_t offset{};};
struct Cue {bool valid{},gotHim{},closing{};};
struct Node {std::uint64_t offset{},definition{};std::uint32_t kind{};};
inline constexpr std::array<Node,4> nodes{{
    {0x11F0,0x4BF8,0x808062F6U}, // "and... got him"
    {0x1760,0x4DD8,0x808062F6U}, // final closing line
    {0x1930,0x4E78,0x80806285U}, // authored three-second closing delay
    {0x1A90,0x4EF8,0x808062E4U}, // closing branch terminal
}};
template<class Read> Cue probe(Read& read,std::uintptr_t root,std::uint32_t handle) noexcept {
    if(root<0x10000 || root>UINTPTR_MAX-0x4458 || handle==UINT32_MAX)return {};
    Ref ref{};std::uint32_t self{},owner{};std::uint64_t count{};
    if(!read.value(root,ref) || ref.tag!=0x80F44F11U || ref.kind!=0x80806384U || ref.offset!=0x4458
        || !read.value(root+0x24,self) || self!=handle || !read.value(root+0x2c,owner) || owner==UINT32_MAX
        || !read.value(root+0x38,count) || count!=22)return {};
    std::array<std::uint32_t,nodes.size()> states{};
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto n=nodes[i];const auto a=root+n.offset;
        std::uint32_t parent{},kind{};std::uint64_t offset{};
        if(!read.value(a,ref) || ref.tag!=0x80F44F11U || ref.kind!=n.kind || ref.offset!=n.definition
            || !read.value(a+0x28,parent) || parent!=handle || !read.value(a+0x2c,kind) || kind!=n.kind-1U
            || !read.value(a+0x30,offset) || offset!=n.offset
            || !read.value(a+0x98,states[i]) || states[i]>2)return {};
    }
    for(std::size_t i=0;i<nodes.size();++i) {
        std::uint32_t after{};
        if(!read.value(root+nodes[i].offset+0x98,after) || after!=states[i])return {};
    }
    return {true,states[0]==1,states[1]==2 && states[2]==2 && states[3]==2};
}
// Input 16 starts Sagira's interruption speech. Its native completion edges
// own the following pause, scream and closing cue; keep that chain intact.
enum class Reaction {unavailable,arm,held};
template<class Read> Reaction reaction(Read& read,std::uintptr_t root,std::uint32_t handle) noexcept {
    if(root<0x10000 || root>UINTPTR_MAX-0x4458 || handle==UINT32_MAX)return Reaction::unavailable;
    constexpr std::uint64_t node=0x1590,signal=node+0xD0;
    Ref ref{},input{};std::uint32_t self{},parent{},kind{},state{},counter{},fallback{},count{};
    std::uint64_t offset{};std::int64_t relative{};
    if(!read.value(root,ref) || ref.tag!=0x80F44F11U || ref.kind!=0x80806384U || ref.offset!=0x4458
        || !read.value(root+0x24,self) || self!=handle
        || !read.value(root+node,ref) || ref.tag!=0x80F44F11U || ref.kind!=0x808062F6U || ref.offset!=0x4D38
        || !read.value(root+node+0x28,parent) || parent!=handle
        || !read.value(root+node+0x2C,kind) || kind!=0x808062F5U
        || !read.value(root+node+0x30,offset) || offset!=node
        || !read.value(root+node+0x98,state) || state!=0
        || !read.value(root+0x68,count) || count<=16 || count>256
        || !read.value(root+0x70,relative) || relative<0 || relative>0x20000
        || !read.value(root+0x80+static_cast<std::uintptr_t>(relative)+16*16,input)
        || input.tag!=handle || input.kind!=0x80806388U || input.offset!=signal
        || !read.value(root+signal+0x60,counter) || counter>1
        || !read.value(root+signal+0x50,fallback) || fallback!=UINT32_MAX)return Reaction::unavailable;
    std::array<std::uint32_t,6> callback{};
    if(!read.value(root+signal+0x20,callback) || callback[0]!=0x80806342U || callback[1]!=0
        || callback[3]!=0x808062F5U || callback[4]!=node || callback[5]!=0)return Reaction::unavailable;
    if(callback[2]==handle && counter==0)return Reaction::arm;
    if(callback[2]==UINT32_MAX && counter==1)return Reaction::held;
    return Reaction::unavailable;
}

}
