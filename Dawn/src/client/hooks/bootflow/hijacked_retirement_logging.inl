// Read-only diagnostics under the existing placement game-thread CallGate/lock.
struct RetirementActorRead {
    std::uint32_t actor{UINT32_MAX},expectedOwner{UINT32_MAX},actualOwner{UINT32_MAX},entity{UINT32_MAX},entityFlags{UINT32_MAX};
    std::int32_t member{INT32_MIN};std::uint8_t actorFlags{};
    // 0 unreadable, 1 salted actor reused, 2 matching actor/entity unreadable,
    // 3 entity reused, 4 matching actor and entity (entityFlags may prove removal).
    unsigned status{};bool objectAuthority{};
    friend bool operator==(const RetirementActorRead&,const RetirementActorRead&)=default;
};
struct RetirementSourceRead {
    std::uint32_t handle{UINT32_MAX},incoming{UINT32_MAX},applied{UINT32_MAX},requestRevision{UINT32_MAX};
    std::uint32_t ownedCount{UINT32_MAX},pending{UINT32_MAX},bubble{UINT32_MAX};
    std::int8_t policy{-127};std::uint8_t initialized{255};bool resolved{},readable{},authority{};
    std::array<RetirementActorRead,64> actors{};std::size_t actorsCount{};
    friend bool operator==(const RetirementSourceRead&,const RetirementSourceRead&)=default;
};
RetirementActorRead retirement_actor(std::uint32_t actorHandle,std::uint32_t expectedOwner) noexcept {
    RetirementActorRead out{};out.actor=actorHandle;out.expectedOwner=expectedOwner;Read read{image};
    std::uintptr_t table{},entities{};std::uint32_t stride{},entityStride{};
    if(actorHandle==UINT32_MAX || !read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0x68 || stride>0x100000) {return out;}
    const auto address=table+static_cast<std::uintptr_t>(actorHandle&0x1FFFU)*stride;
    std::array<std::byte,0x68> bytes{};if(!read.copy(address,bytes)) {return out;}
    if(at<std::uint32_t>(bytes.data()+0x48)!=actorHandle) {out.status=1;return out;}
    out.status=2;out.actorFlags=at<std::uint8_t>(bytes.data());
    out.actualOwner=at<std::uint32_t>(bytes.data()+0x38);out.entity=at<std::uint32_t>(bytes.data()+0x4C);
    out.member=at<std::int32_t>(bytes.data()+0x60);
    if(out.entity==UINT32_MAX || !read.value(image+0x1F93428,entities) || !read.value(image+0x1F93430,entityStride)
        || entityStride<0x10 || entityStride>0x100000) {return out;}
    std::array<std::byte,16> entity{};
    if(!read.copy(entities+static_cast<std::uintptr_t>(out.entity&0x1FFFU)*entityStride,entity)) {return out;}
    if(at<std::uint32_t>(entity.data()+12)!=out.entity) {out.status=3;return out;}
    std::uint32_t self{};
    if(!read.value(address+0x48,self) || self!=actorHandle) {out.status=1;return out;}
    std::uint32_t authorityWord{};
    if(read.value(image+0x26BE0E0+static_cast<std::uintptr_t>((out.entity&0x1FFFU)>>5)*4,authorityWord)) {
        out.objectAuthority=((authorityWord>>(out.entity&31U))&1U)!=0;
    }
    out.status=4;out.entityFlags=at<std::uint32_t>(entity.data()+4);return out;
}
__declspec(noinline) void retirement_logging(const mission::Request& request,std::uint64_t now) noexcept {
    static state::activity::coo::Generation loggedOwner{};
    static std::uint64_t next{};
    static std::array<RetirementSourceRead,7> previous{};
    static std::array<bool,7> seen{};
    static std::array<std::uint64_t,7> emitted{};
    if(loggedOwner!=request.owner) {loggedOwner=request.owner;next=0;previous={};seen={};emitted={};}
    if(now<next) {return;}next=now+250;
    bool needed{};
    for(std::uint16_t slot=1;slot<=7;++slot) {
        needed|=request.frame.native[mission::asset_index(mission::find(0x3E9B74F3U,1,slot)->asset)].retired;
    }
    if(!needed) {return;}
    const auto receipts=mission::retirement_enemies();if(receipts.owner!=request.owner) {return;}
    for(std::uint16_t slot=1;slot<=7;++slot) {
        const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
        const auto& state=request.frame.native[mission::asset_index(asset)];if(!state.retired) {continue;}
        RetirementSourceRead sample{};SourceIdentity native{};Read read{image};
        const auto ctx=context();if(ctx) {static_cast<void>(read.value(ctx+4,sample.bubble));}
        sample.authority=authority();
        sample.resolved=native_source(mission::spawn_index(asset),state.generation,native,true);
        if(sample.resolved) {
            sample.handle=native.handle;
            sample.readable=read.value(native.address+0x1FC,sample.incoming) && read.value(native.address+0x244,sample.applied)
                && read.value(native.address+0x238,sample.requestRevision) && read.value(native.address+0x23C,sample.policy)
                && read.value(native.address+0x260,sample.initialized) && read.value(native.address+0x314,sample.ownedCount)
                && read.value(native.address+0x310,sample.pending);
        }
        const auto add=[&](std::uint32_t handle,std::uint32_t expectedOwner) {
            if(handle==UINT32_MAX || sample.actorsCount==sample.actors.size()) {return;}
            for(std::size_t j=0;j<sample.actorsCount;++j) {if(sample.actors[j].actor==handle) {return;}}
            sample.actors[sample.actorsCount++]=retirement_actor(handle,expectedOwner);
        };
        // Retained authentic receipts keep disappeared/detached actors visible to diagnostics.
        for(std::size_t j=0;j<receipts.count;++j) {
            const auto& r=receipts.actors[j];if(r.registry==asset.registry && r.source==slot) {add(r.actor,r.owner);}
        }
        // Native source list also reveals late actors rejected by the mission ledger.
        if(sample.readable && sample.ownedCount<=64) {
            for(std::uint32_t j=0;j<sample.ownedCount;++j) {
                gateway_native::Weak weak{};
                if(!read.value(native.address+0x318+static_cast<std::uintptr_t>(j)*8,weak)) {break;}
                if(read.weak(weak)) {add(weak.handle,native.handle);}
            }
        }
        const auto live=mission::request();if(live.owner!=request.owner) {return;}
        if(sample.resolved) {
            SourceIdentity after{};
            if(!native_source(mission::spawn_index(asset),state.generation,after,true) || after!=native) {
                sample.resolved=false;sample.readable=false;
            }
        }
        const auto i=slot-1U;const bool changed=!seen[i] || sample!=previous[i];
        if(!changed && now-emitted[i]<5000) {continue;}
        std::array<char,768> line{};
        std::snprintf(line.data(),line.size(),
            "ev=hijacked_retirement stage=native_readback run=%llu registry=%08X source=%u desired_generation=%u desired_policy=0 resolved=%u readable=%u source_handle=%08X authority_generation=%u sense_generation=%u request_revision=%u policy=%d initialized=%u owned_count=%u pending=%08X bubble=%u native_authority=%u actor_samples=%zu changed=%u",
            static_cast<unsigned long long>(request.owner.run),asset.registry,slot,state.generation,sample.resolved?1U:0U,sample.readable?1U:0U,
            sample.handle,sample.incoming,sample.applied,sample.requestRevision,static_cast<int>(sample.policy),static_cast<unsigned>(sample.initialized),
            sample.ownedCount,sample.pending,sample.bubble,sample.authority?1U:0U,sample.actorsCount,changed?1U:0U);
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        if(changed) {for(std::size_t j=0;j<sample.actorsCount;++j) {
            const auto& a=sample.actors[j];
            std::snprintf(line.data(),line.size(),
                "ev=hijacked_retirement stage=actor_readback run=%llu registry=%08X source=%u actor=%08X status=%u expected_owner=%08X actual_owner=%08X owner_matches=%u member_index=%d filter1_eligible=%u actor_flags=%02X entity=%08X entity_flags=%08X entity_removed=%u object_authority=%u",
                static_cast<unsigned long long>(request.owner.run),asset.registry,slot,a.actor,a.status,a.expectedOwner,a.actualOwner,
                a.status>=2 && a.expectedOwner==a.actualOwner?1U:0U,a.member,a.status>=2 && a.member==-1?1U:0U,
                static_cast<unsigned>(a.actorFlags),a.entity,a.entityFlags,a.status==4 && (a.entityFlags&4U)?1U:0U,a.objectAuthority?1U:0U);
            core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        }}
        previous[i]=sample;seen[i]=true;emitted[i]=now;
    }
}
