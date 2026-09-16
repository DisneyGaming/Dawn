#pragma once
#include <cstddef>
#include <cstdint>
#include "omega_teardown_native.h"

namespace sunrise::client::hooks::bootflow::native_property_list {
using Source=omega_teardown_native::Source;
using omega_teardown_native::read;
inline constexpr std::uint32_t kAbsent=UINT32_MAX;
inline constexpr std::size_t kGroups=128;
struct Group {
    std::uint32_t key{},head{kAbsent},tail{kAbsent},flags{};
    friend bool operator==(const Group&,const Group&)=default;
};
static_assert(sizeof(Group)==16);
struct Pool {
    std::uintptr_t metadata{},elements{},bits{};
    std::uint32_t mask{},kind{};
    std::uint16_t capacity{};
};
// The list's own allocator, not a global table number (which changes with load order).
// Layout and handle reconstruction are native 4DB4F0 / 34E830. Nodes are 0x88 bytes,
// with salt at +80 and intrusive previous/next at +74/+78.
inline bool pool(const Source& source,std::uintptr_t list,Pool& out) noexcept {
    std::uintptr_t header{},bitmap{};
    std::uint32_t stride{},saltOffset{},bitCount{};
    return read(source,list+0x808,out.metadata) && out.metadata!=0
        && read(source,out.metadata,header) && header!=0
        && read(source,out.metadata+8,out.elements) && out.elements>=0x10000
        && read(source,out.metadata+0x1C,saltOffset) && saltOffset==0x80
        && read(source,out.metadata+0x20,stride) && stride==0x88
        && read(source,out.metadata+0x24,out.mask)
        && read(source,out.metadata+0x34,out.kind)
        && read(source,header+0x1C,out.capacity) && out.capacity>0 && out.capacity<=8192
        && read(source,header+8,bitmap) && bitmap!=0
        && read(source,bitmap+8,bitCount) && bitCount>=out.capacity && bitCount<=8192
        && read(source,bitmap+0x10,out.bits) && out.bits>=0x10000
        && out.elements<=UINTPTR_MAX-static_cast<std::uintptr_t>(out.capacity)*0x88;
}
inline std::uint32_t handle(const Pool& p,std::uint32_t serial,std::uint32_t index) noexcept {
    const auto salt=serial&p.mask;
    const auto high=(p.kind&0x40000000U)!=0
        ? (((salt&7U)|0xFFFFFFF0U)<<14)|(p.kind&0x3FFFU)
        : ((salt&0xFFU)<<10)|(p.kind&0x3FFU);
    return (high<<13)|index;
}
inline bool allocated(const Source& source,const Pool& p,std::uint32_t index,bool& live) noexcept {
    std::uint32_t bits{};
    if(index>=p.capacity || !read(source,p.bits+(index/32U)*4U,bits))return false;
    live=(bits&(1U<<(index%32U)))!=0;return true;
}
enum class State { valid, retired, blocked };
struct Node {std::uint32_t key{},previous{},next{};};
inline bool node(const Source& source,const Pool& p,std::uint32_t datum,Node& out) noexcept {
    const auto index=datum&0x1FFFU;
    bool live{};std::uint32_t serial{};
    if(datum==kAbsent || !allocated(source,p,index,live) || !live)return false;
    const auto address=p.elements+index*0x88ULL;
    return read(source,address+0x80,serial) && handle(p,serial,index)==datum
        && read(source,address,out.key) && read(source,address+0x74,out.previous)
        && read(source,address+0x78,out.next);
}
inline State inspect(const Source& source,const Pool& p,const Group& group) noexcept {
    auto datum=group.head;auto previous=kAbsent;std::uint32_t visited{};
    bool valid=true;
    while(datum!=kAbsent) {
        Node current{};
        if(++visited>p.capacity || !node(source,p,datum,current)
            || current.key!=group.key || current.previous!=previous) {valid=false;break;}
        previous=datum;datum=current.next;
    }
    if(valid && previous==group.tail)return State::valid;
    // A broken chain is not proof that its members died. Remove a cached group
    // only after independently checking EVERY allocated slot in its owning pool.
    // Never follow a freed node's next pointer, modify pool slots, or free actors.
    for(std::uint32_t i=0;i<p.capacity;++i) {
        bool live{};std::uint32_t key{};
        if(!allocated(source,p,i,live))return State::blocked;
        if(live && (!read(source,p.elements+i*0x88ULL,key) || key==group.key))return State::blocked;
    }
    return State::retired;
}
template<class Update,class Erase,class Report>
void run(const Source& source,std::uintptr_t list,bool allowRemoval,
         Update&& update,Erase&& erase,Report&& report) noexcept {
    std::int32_t count{};Pool owner{};
    if(!read(source,list,count) || count<0 || count>static_cast<std::int32_t>(kGroups)) {
        report("invalid_count",0);return;
    }
    if(count==0)return;
    if(!pool(source,list,owner)) {report("unreadable_pool",0);return;}
    for(std::int32_t i=0;i<count;) {
        const auto address=list+4+static_cast<std::uintptr_t>(i)*sizeof(Group);
        Group group{},again{};
        if(!read(source,address,group)) {report("unreadable_group",0);return;}
        const auto state=inspect(source,owner,group);
        std::int32_t now{};std::uintptr_t metadata{};
        if(!read(source,list,now) || now!=count
            || !read(source,list+0x808,metadata) || metadata!=owner.metadata
            || !read(source,address,again) || again!=group) {report("owner_changed",group.key);return;}
        if(state==State::retired && allowRemoval) {
            if(!erase(i,count)) {report("removal_failed",group.key);return;}
            --count;report("retired_group_removed",group.key);continue;
        }
        if(state==State::valid)update(address);
        else report("damaged_group_blocked",group.key);
        ++i;
    }
}
}
