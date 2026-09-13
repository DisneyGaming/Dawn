// Observe the native Ghost-link update after it handles interaction and playback.
// The server never substitutes proximity or a countdown for a confirmed scan.
namespace campaign_scan {
namespace coo=state::activity::coo;
coo::CampaignScanRequest request(unsigned mission) noexcept {
    return mission==0?state::activity::strike_pact::scan_request():state::activity::strike_bond::scan_request();
}
bool current(unsigned mission,const coo::CampaignScanRequest& expected) noexcept {
    const auto next=request(mission);
    return next.enabled() && next.owner==expected.owner && next.link==expected.link && next.generation==expected.generation;
}
bool capture(Read& read,std::uintptr_t sensor,const coo::CampaignScanRequest& wanted,
             std::uintptr_t& controller,Weak& identity) noexcept {
    Ref ref{},ownerRef{};std::uintptr_t definition{};std::array<std::uint32_t,2> scope{};
    std::uint32_t self{},parent{};
    return read.value(sensor,ref) && ref.handle==wanted.link.definition && ref.kind==0x80804D32U && ref.offset==0x258
        && read.resolve(ref.handle,definition) && read.value(definition+0x288,scope)
        && scope[0]==wanted.link.registry && scope[1]==65U
        && read.value(sensor+0x1D8,identity) && read.weak(identity) && read.resolve(identity.handle,controller)
        && read.value(controller,ownerRef) && ownerRef.kind==0x80804D3AU && ownerRef.offset>0 && ownerRef.offset<0x100000
        && read.value(controller+0x24,self) && self==identity.handle
        && read.value(controller+0x2C,parent) && parent!=UINT32_MAX;
}
void update(std::uintptr_t sensor) noexcept {
    for(unsigned mission=0;mission<2;++mission) {
        const auto wanted=request(mission);if(!wanted.enabled()) continue;
        Read read{image};std::uintptr_t controller{};Weak identity{};
        if(!capture(read,sensor,wanted,controller,identity)) continue;
        coo::ScanPlayback playback{};
        if(!read.value(controller+0x294,playback.revision) || !read.value(controller+0x298,playback.mode)
            || !read.value(controller+0x299,playback.active) || !read.value(controller+0x29C,playback.elapsed)
            || !read.value(controller+0x290,playback.duration) || !playback.valid(wanted.generation)) return;
        bool participant{};std::array<Weak,6> members{};
        if(read.value(controller+0x80,members)) for(const auto member:members) {
            std::uintptr_t actor{};Ref ref{};std::uint32_t self{};Weak target{};std::int64_t offset{};
            if(read.weak(member) && read.resolve(member.handle,actor) && read.value(actor,ref)
                && ref.kind==0x80803F45U && read.value(actor+0x24,self) && self==member.handle
                && read.value(actor+0x1C8,target) && target==identity && read.weak(target)
                && read.value(actor+0x1D0,offset) && offset==0x30) {participant=true;break;}
        }
        std::uintptr_t again{};Weak stable{};
        if(!capture(read,sensor,wanted,again,stable) || again!=controller || stable!=identity || !current(mission,wanted)) return;
        if(mission==0) state::activity::strike_pact::observe_scan(wanted.owner,identity.handle,identity.serial,playback,participant);
        else state::activity::strike_bond::observe_scan(wanted.owner,identity.handle,identity.serial,playback,participant);
        return;
    }
}
}
