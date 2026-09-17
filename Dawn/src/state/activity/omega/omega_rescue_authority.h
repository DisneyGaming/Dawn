#pragma once
#include "omega_rescue_catalog.h"
#include "omega_mission_state.h"
#include "omega_ikora_authority.h"

namespace dawn::state::activity::omega::rescue {
inline const mission::SceneCommand* command(const mission::Snapshot& snapshot,std::uint16_t slot) noexcept {
    for(const auto& value:snapshot.scenes) if(value.generation && value.slot==slot) return &value;
    return nullptr;
}
inline bool requested(const mission::Snapshot& snapshot,std::uint16_t slot) noexcept {
    for(const auto& value:snapshot.scenes) if(value.generation) {
        const auto* row=scene(value.slot);
        if(row) for(unsigned i=0;i<row->count;++i) if(row->sources[i]==slot) return true;
    }
    return false;
}
inline std::size_t bits(const mission::Snapshot& snapshot,std::uint32_t key,
                        std::uint8_t type,std::uint16_t slot) noexcept {
    if(key!=kRegistry || !snapshot.generation) return 0;
    if(type==1 && source(slot)) return 641;
    if(type!=43) return 0;
    const auto* row=scene(slot);if(!row) return 0;
    const auto* value=command(snapshot,slot);
    return 74U+55U*row->count+32U*(value?value->eventCount:0U);
}
template<class Writer> bool write(Writer& writer,const mission::Snapshot& snapshot,
    std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!bits(snapshot,key,type,slot)) return false;
    if(type==1) return ikora::write_source(writer,snapshot.generation,requested(snapshot,slot));
    const auto& row=*scene(slot);const auto* value=command(snapshot,slot);
    if(value && (value->generation>0x7FFFFFFFU || value->eventCount>value->events.size())) return false;
    // The first word is a biased signed generation. The selector is owned by
    // the packaged Scene definition; placing its asset hash here is incorrect.
    bool ok=writer.write(value?0x80000000U+value->generation:0x7FFFFFFFU,32)
        && writer.write(value&&value->stop?1U:0U,1) && writer.write(row.count,4);
    for(unsigned i=0;ok && i<row.count;++i)
        ok=writer.write(kRegistry,32) && writer.write(2,7) && writer.write(0x8000U+row.sources[i],16);
    ok=ok && writer.write(value?value->generation:0U,31) && writer.write(value?value->eventCount:0U,6);
    if(value) for(unsigned i=0;ok && i<value->eventCount;++i) ok=writer.write(value->events[i],32);
    return ok;
}
}
