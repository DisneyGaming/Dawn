// Native callback observations. No runtime pointer is retained across callbacks.
struct MissionCharacter {
    std::uint32_t actor{UINT32_MAX},character{UINT32_MAX},entity{UINT32_MAX},asset{},offset{};
};
std::mutex missionReceiptMutex;
std::uint64_t missionReceiptRun{};
std::array<MissionCharacter,256> missionCharacters{};
std::array<omega::mission::ActorReceipt,256> missionActors{};
std::array<bool,256> missionAdmitted{};
void reset_mission_receipts(std::uint64_t run) noexcept {
    if(missionReceiptRun==run) return;
    missionReceiptRun=run; missionCharacters={}; missionActors={}; missionAdmitted={};
}
void observe_mission_character(std::byte* character,const std::uint32_t* actor) noexcept {
    const auto run=state::activity::mission_run_generation();
    if(!omega::mission::runtime::snapshot(run).generation) return;
    std::array<std::byte,0xC4> body{}; std::uint32_t fullActor{};
    if(!copy_native(actor,&fullActor,sizeof fullActor) || !copy_native(character,body.data(),body.size())
        || read<std::uint32_t>(body.data(),0xC0)!=fullActor) return;
    const auto asset=read<std::uint32_t>(body.data(),0),offset=read<std::uint32_t>(body.data(),8);
    bool known=false;
    for(const auto& row:omega_mission_characters::kCharacters)
        known|=omega_reveal_source::matches(body,{row.asset,0x80806832U,row.offset});
    if(!known) return;
    const auto self=read<std::uint32_t>(body.data(),0x24),entity=read<std::uint32_t>(body.data(),0x2C);
    if(fullActor==UINT32_MAX || self==UINT32_MAX || entity==UINT32_MAX || resolve_handle(self)!=character) return;
    const std::lock_guard lock(missionReceiptMutex); reset_mission_receipts(run);
    for(auto& row:missionCharacters) {
        if(row.actor==fullActor) return;
        if(row.actor==UINT32_MAX) {row={fullActor,self,entity,asset,offset};return;}
    }
}
bool mission_health(const MissionCharacter& owner,std::uint32_t& health,bool& dead) noexcept {
    std::array<std::byte,0x2F8> character{};
    if(!copy_native(resolve_handle(owner.character),character.data(),character.size())
        || !omega_reveal_source::matches(character,{owner.asset,0x80806832U,owner.offset})
        || read<std::uint32_t>(character.data(),0x24)!=owner.character
        || read<std::uint32_t>(character.data(),0x2C)!=owner.entity
        || read<std::uint32_t>(character.data(),0xC0)!=owner.actor
        || read<std::uint32_t>(character.data(),0x2EC)!=0x80804BEEU
        || read<std::uint32_t>(character.data(),0x2F0)!=0) return false;
    health=read<std::uint32_t>(character.data(),0x2E8);
    std::array<std::byte,0x33C> body{};
    if(health==UINT32_MAX || !copy_native(resolve_handle(health),body.data(),body.size())
        || read<std::uint32_t>(body.data(),4)!=0x80804B8AU
        || read<std::uint32_t>(body.data(),0x24)!=health
        || read<std::uint32_t>(body.data(),0x2C)!=owner.entity) return false;
    dead=(read<std::uint8_t>(body.data(),0x338)&1)!=0; return true;
}
void observe_mission_source(std::byte* component) noexcept {
    namespace mission=omega::mission;
    const auto run=state::activity::mission_run_generation();
    std::uint32_t tag{};
    if(!copy_value(component,tag)) return;
    bool known=false;
    for(const auto& source:mission::kSources) known|=source.definition==tag;
    if(!known) return;
    const auto snapshot=mission::runtime::snapshot(run);
    if(!snapshot.generation) return;
    std::array<std::byte,0x690> body{};
    if(!copy_native(component,body.data(),body.size())) return;
    const auto registry=read<std::uint32_t>(body.data(),0x5D8);
    const auto slot=read<std::uint16_t>(body.data(),0x5DE);
    const auto index=mission::source_index(registry,slot);
    if(index==mission::kSources.size() || read<std::uint8_t>(body.data(),0x5DC)!=1) return;
    const auto& source=mission::kSources[index];
    if(!omega_reveal_source::matches(body,{source.definition,0x8080948FU,0x728})
        || read<std::uint32_t>(body.data(),0x1FC)!=snapshot.generation
        || read<std::uint32_t>(body.data(),0x244)!=snapshot.generation
        || (snapshot.requested[index][0]+snapshot.requested[index][1])==0) return;
    const auto fullSource=read<std::uint32_t>(body.data(),0x170);
    std::array<std::byte,0x10> sync{};
    if(!copy_native(resolve_handle(fullSource),sync.data(),sync.size())
        || read<std::uint32_t>(sync.data(),0)!=registry || read<std::uint8_t>(sync.data(),4)!=1
        || read<std::uint16_t>(sync.data(),6)!=slot || read<std::uint32_t>(sync.data(),0xC)!=0x80807EC9U) return;
    using Sense=const std::byte*(__fastcall*)(std::byte*) noexcept;
    std::uint32_t senseGeneration{};
    if(!copy_value(native<Sense>(0x4E2630)(component),senseGeneration) || senseGeneration!=snapshot.generation) return;
    const auto count=read<std::int32_t>(body.data(),0x314);
    if(count<0 || count>64) return;
    for(int i=0;i<count;++i) {
        std::array<std::byte,8> weak{}; std::uint32_t actor=UINT32_MAX;
        if(!copy_native(component+0x318+8*i,weak.data(),weak.size())) continue;
        native<void(__fastcall*)(const void*,std::uint32_t*) noexcept>(0x352310)(weak.data(),&actor);
        if(actor==UINT32_MAX) continue;
        const std::lock_guard lock(missionReceiptMutex); reset_mission_receipts(run);
        for(std::size_t c=0;c<missionCharacters.size();++c) {
            const auto& owner=missionCharacters[c];
            if(owner.actor!=actor || missionAdmitted[c]) continue;
            std::uint32_t health{}; bool dead{};
            if(!mission_health(owner,health,dead) || dead) continue;
            unsigned category=source.categories;
            for(unsigned candidate=0;candidate<source.categories;++candidate)
                for(const auto& asset:omega_mission_characters::kCharacters)
                    if(asset.entity==source.entities[candidate] && asset.asset==owner.asset && asset.offset==owner.offset)
                        category=candidate;
            if(category==source.categories) continue;
            const mission::ActorReceipt receipt{run,snapshot.generation,registry,fullSource,actor,owner.entity,
                owner.character,health,slot,static_cast<std::uint8_t>(category)};
            if(!mission::runtime::receipt([&](auto& state){return state.admit(receipt);})) continue;
            missionActors[c]=receipt; missionAdmitted[c]=true;
            log("ev=omega_mission stage=admitted run=%llu registry=%08X slot=%u source=%08X actor=%08X character=%08X health=%08X category=%u",
                run,registry,slot,fullSource,actor,owner.character,health,category);
        }
    }
}
bool __fastcall mission_character_death(std::byte* character,std::uint32_t event) noexcept {
    const hooking::CallGate::Scope call(callGate);
    if(call.accepts_side_effects() && active()) observe_mission_boss_death(character,event);
    if(call.accepts_side_effects() && active()) {
        const auto run=state::activity::mission_run_generation();
        std::uint32_t self{};
        if(copy_value(character+0x24,self)) {
            const std::lock_guard lock(missionReceiptMutex);
            if(missionReceiptRun==run) for(std::size_t i=0;i<missionCharacters.size();++i) {
                if(!missionAdmitted[i] || missionCharacters[i].character!=self || resolve_handle(self)!=character) continue;
                std::uint32_t health{}; bool dead{};
                if(!mission_health(missionCharacters[i],health,dead) || !dead || health!=missionActors[i].health) continue;
                // C72390 is the native character death handler. Its typed health
                // bit must already be set before original teardown; no event enum is guessed.
                if(omega::mission::runtime::receipt([&](auto& state){return state.death(missionActors[i]);}))
                    log("ev=omega_mission stage=death run=%llu actor=%08X character=%08X health=%08X event=%08X receipt=native_death_and_typed_health",
                        run,missionActors[i].actor,self,health,event);
            }
        }
    }
    return hooking::await_original(deathOriginal)(character,event);
}
