#pragma once
#include "state/activity/beyond_infinity/transit_rules.h"
#include "state/activity/beyond_infinity/transit_contacts.h"
#include <limits>
namespace beyond_transit_fixture {
template<class Check> void run(Check check) {
    namespace t=dawn::state::activity::beyond_infinity::transit;
    namespace bi=dawn::state::activity::beyond_infinity;
    bi::Frame frame{};frame.enabled=true;
    check(t::presentation_ready(frame,1,64),"remote Past source does not deadlock Forest exit");
    check(!t::presentation_ready(frame,1,144),"locally loaded Past entrance requires presentation");
    check(!t::presentation_ready(frame,2,144) && !t::presentation_ready(frame,4,32),"local return portals must prepare before travel");
    for(const auto key:{std::uint32_t{0xC7FB7155U},std::uint32_t{0x0FF26BCCU}}) {
        for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
            if(bi::kAssets[i].asset.registry==key) { frame.native[i]={7,true,true,true,true}; }
        }
    }
    check(t::presentation_ready(frame,1,144) && t::presentation_ready(frame,2,144)
        && t::presentation_ready(frame,4,32),"prepared native return source/device sets authorize travel");
    check(!t::presentation_ready(frame,2,64) && !t::presentation_ready(frame,4,144),"foreign source region cannot authorize a local portal");
    frame.native[bi::asset_index(bi::find(0x0FF26BCCU,23,2)->asset)].active=false;
    check(!t::presentation_ready(frame,4,32),"missing second native portal FX channel blocks dispatch");
    const t::Scope first{{21,7},{99,{1}},123,1};
    check(first.valid(),"valid mission-owned native transport scope");
    for(std::uint8_t route=1;route<=5;++route) { check(t::destination(route).valid(),"authored destination for each reconstructed route"); }
    check(!t::destination(0).valid() && !t::destination(6).valid(),"unknown transport routes rejected");
    t::Transaction transaction;t::Observation observation{};
    check(transaction.begin(first,observation),"idle native tuple bootstraps first command");
    auto host=transaction.authority(first);
    check(host.publish && !host.arrived && host.host.state==1 && host.host.token==1,"fresh request is not arrival");
    observation={host.host,144,true,true};observation.local.state=2;
    check(!transaction.observe(first,observation) && !transaction.authority(first).arrived,"loading is not arrival");
    observation.local.state=3;observation.local.token=0;
    check(!transaction.observe(first,observation),"stale token cannot complete transport");
    observation.local=host.host;observation.local.state=3;observation.currentRegion=32;
    check(!transaction.observe(first,observation),"foreign region cannot complete transport");
    observation.currentRegion=144;auto foreign=first;++foreign.owner.value;
    check(!transaction.observe(foreign,observation),"foreign mission owner cannot complete transport");
    check(transaction.observe(first,observation),"exact client state3 and region authenticates arrival");
    check(!transaction.observe(first,observation),"arrival receipt emitted once");
    host=transaction.authority(first);check(host.arrived && host.host.state==3 && !host.complete,"host releases after arrival");
    observation.local.state=0;transaction.observe(first,observation);host=transaction.authority(first);
    check(host.complete && host.host.state==0 && !transaction.pending(),"matching client idle completes handshake");
    observation.local.state=1;transaction.observe(first,observation);
    check(transaction.authority(first).host.state==0,"delayed local request cannot replay completed transit");
    t::Transaction wrapped;observation={};observation.local.token=255;observation.hasTeleport=true;
    check(wrapped.begin(first,observation) && wrapped.authority(first).host.token==1,"wire token wraps to nonzero");
    check(t::route_contact(1,-973.2185F,1072.8826F,-68.018F),"exact past entrance core contact");
    check(!t::route_contact(1,-944.F,1073.F,-65.F),"approach dialogue volume is not portal contact");
    check(t::route_contact(2,748.93F,709.90F,2.93F),"exact past return core contact");
    check(!t::route_contact(2,676.F,700.F,3.F),"vignette presence cannot trigger return");
    check(t::route_contact(3,-226.F,1072.F,-65.F),"far end of future corridor authenticates contact");
    check(!t::route_contact(3,-300.F,1072.F,-65.F),"forest end volume does not skip corridor");
    check(t::route_contact(4,262.09F,752.08F,312.57F) && t::route_contact(4,217.96F,749.95F,312.08F),"both authored future escape cores");
    check(!t::route_contact(4,230.F,750.F,312.F),"broad future platform cannot trigger escape");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    check(!t::route_contact(3,-226.F,1072.F,nan) && !t::route_contact(4,nan,752.F,312.F),"non-finite player position rejected");
    check(t::route_contact(5,40.F,1412.F,0.F),"authored return corridor directive contact");
}
}
