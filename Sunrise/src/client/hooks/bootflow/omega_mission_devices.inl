void observe_mission_eye_object(std::byte* component,const omega::mission::Snapshot& snapshot) noexcept {
    if(snapshot.command.cycle<1 || snapshot.command.cycle>2) return;
    std::array<std::byte,0x448> body{},after{};
    if(!copy_native(component,body.data(),body.size())) return;
    for(const auto& row:omega::transit::sources) {
        if(row.role!=omega::transit::Role::eyeFront && row.role!=omega::transit::Role::eyeBack && row.role!=omega::transit::Role::eyeReturn) continue;
        if(!omega::transit::status(snapshot,row).active
            || !omega_reveal_source::matches(body,{row.definition,0x80809928U,0x4C8})) continue;
        omega_cannon_delivery::Reference reference;
        const auto generation=omega::transit::generation(snapshot,row);
        if(!mission_source_identity(component,body,0x160,0x80809927U,reference)
            || read<std::uint32_t>(body.data(),0x180)!=generation
            || read<std::uint8_t>(body.data(),0x188)!=1) return;
        std::uint32_t entity=UINT32_MAX;
        native<std::uint32_t*(__fastcall*)(const void*,std::uint32_t*) noexcept>(0x352310)(body.data()+0x440,&entity);
        if(entity==UINT32_MAX || entity!=read<std::uint32_t>(body.data(),0x444)
            || !copy_native(component,after.data(),after.size()) || after!=body) return;
        const std::byte* worlds{};std::uint32_t stride{},identity{};
        if(!copy_value(image+0x1F93428,worlds) || !worlds || !copy_value(image+0x1F93430,stride)
            || stride<0xE0 || stride>0x1000 || !copy_value(worlds+(entity&0x1FFF)*stride+0xC,identity) || identity!=entity) return;
        if(omega::mission::runtime::receipt([&](auto& s){return s.eye_object(snapshot.command.token,row.slot,generation,reference.member,entity);}))
            log("ev=omega_mission stage=eye_object_created run=%llu epoch=%u cycle=%u slot=%u generation=%u source=%08X entity=%08X receipt=native_source_child",
                snapshot.command.token.boss.run,snapshot.command.token.epoch,snapshot.command.cycle,row.slot,generation,reference.member,entity);
        return;
    }
}
void observe_mission_device(std::byte* component) noexcept {
    const hooking::CallGate::Scope call(callGate);
    if(!call.accepts_side_effects() || !active()) return;
    observe_mission_arc_source(component);
    const auto run=state::activity::mission_run_generation();
    const auto snapshot=omega::mission::runtime::snapshot(run);
    if(!snapshot.generation) return;
    observe_mission_eye_object(component,snapshot);
    std::array<std::byte,0x1A0> body{};
    if(!copy_native(component,body.data(),body.size())) return;
    omega_cannon_delivery::Reference identity;
    if(!mission_source_identity(component,body,0x160,0x80809927U,identity)) return;
    const auto self=identity.member;
    for(const auto& core:omega::transit::sources) {
        if(core.preparation<0 || !omega_reveal_source::matches(body,{core.definition,0x80809928U,0x4C8})
            || read<std::uint32_t>(body.data(),0x180)!=snapshot.generation
            || read<std::uint32_t>(body.data(),0x184)!=0 || read<std::uint8_t>(body.data(),0x188)!=0
            || read<std::uint8_t>(body.data(),0x189)!=0 || read<std::uint32_t>(body.data(),0x18C)!=0) continue;
        if(omega::mission::runtime::receipt([&](auto& s){return s.prepared(run,snapshot.generation,static_cast<std::uint8_t>(core.preparation),true);}))
            log("ev=omega_mission stage=transit_prepared run=%llu generation=%u index=%d core=%08X receipt=native_post_apply",
                run,snapshot.generation,core.preparation,self);
    }
    for(std::size_t i=0;i<omega::mission_devices::kCannons.size();++i) {
        const auto& cannon=omega::mission_devices::kCannons[i];
        if(!omega_reveal_source::matches(body,{cannon.coreAsset,0x80809928U,0x4C8})
            || read<std::uint32_t>(body.data(),0x180)!=snapshot.generation
            || read<std::uint32_t>(body.data(),0x184)!=0
            || read<std::uint8_t>(body.data(),0x188)!=0
            || read<std::uint8_t>(body.data(),0x189)!=0
            || read<std::uint32_t>(body.data(),0x18C)!=0) continue;
        if(omega::mission::runtime::receipt([&](auto& state){return state.prepared(run,snapshot.generation,static_cast<std::uint8_t>(i),false);}))
            log("ev=omega_mission stage=cannon_prepared run=%llu generation=%u index=%zu core=%08X receipt=native_post_apply",
                run,snapshot.generation,i,self);
    }
}

