#pragma once
#include "controller.h"
#include "../../../../client/hooks/bootflow/coo_native_components.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"

namespace sunrise::state::activity::newlight::launchpad::cache {
namespace gn=client::hooks::bootflow::gateway_native;
struct Binding {
    coo::ObjectReceipt object{};std::uintptr_t component{};std::uint32_t self{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
// 80805FB2 is a loot cache, not the 80804FB2 deposit consumer. D10B30
// latches +2D0 only for the local interacting player; D119F0 sets +2D1
// after its opening animation reaches the authored loot cue. +2E0 is open.
// These local latches are not copied by the cache's D11060 network apply.
template<class Read> bool sample(Read& read,std::uintptr_t source,const Request& req,
    std::size_t pickup,Binding& out) noexcept {
    if(pickup<1 || pickup>=std::size(kPickups) || !req.owner.valid() || !req.frame.enabled || req.frame.finished
        || req.frame.cinematic.phase!=cinematics::Phase::gameplay) {return false;}
    const auto& p=req.frame.pickups[pickup];const auto& wanted=req.frame.native[asset_index(kPickups[pickup])];
    if(!p.armed || p.used || !p.binding.valid() || !wanted.active || !wanted.prepared
        || p.binding.owner!=coo::Generation{req.owner.run,wanted.generation}) {return false;}
    const auto* asset=find(kPickups[pickup].registry,4,kPickups[pickup].slot);
    gn::Ref header{};gn::Weak entity{},again{};std::uint32_t generation{},committed{},bundle{},parent{};
    std::uint8_t active{},local{},looted{},opened{};std::uintptr_t row{},component{},resolved{};
    if(!read.value(source,header) || header.handle!=asset->asset.definition || header.kind!=0x80809928U || header.offset!=asset->offset
        || !read.value(source+0x180,generation) || generation!=wanted.generation
        || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || entity!=gn::Weak{p.binding.serial,p.binding.entity}
        || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !client::hooks::bootflow::coo_native::component<Read,1024>(read,bundle,entity.handle,0x80805FB2U,component)
        || !read.value(component,header) || header.handle!=(pickup==1?0x80C44F51U:pickup==2?0x80C44F8EU:0x80C45000U)
        || header.kind!=0x80805FB2U || header.offset!=0x3E0) {return false;}
    Binding b{p.binding,component};
    if(!read.value(component+0x24,b.self) || !read.resolve(b.self,resolved) || resolved!=component
        || !read.value(component+0x2C,parent) || parent!=entity.handle
        || !read.value(component+0x2D0,local) || local!=1
        || !read.value(component+0x2D1,looted) || looted!=1
        || !read.value(component+0x2E0,opened) || opened!=1
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again)
        || !read.value(source+0x180,generation) || generation!=wanted.generation
        || !read.value(source+0x2F0,committed) || committed!=generation) {return false;}
    out=b;return true;
}
}
