#pragma once
#include "client/hooks/bootflow/public_event_engagement_capture.h"

void capture_cases(const std::filesystem::path& fixtures,const f::Ticket& first) {
    namespace capture=dawn::client::hooks::bootflow::public_event_engagement_capture;
    namespace bridge=capture::bridge;
    auto ticket=first;ticket.request.participants={};ticket.request.participantCount=0;
    ticket.request.collection=wire::Collection::activePlayers;ticket.request.identifierEncoding=wire::IdentifierEncoding::unknown;
    for(const auto generations:std::array<std::array<std::int16_t,2>,3>{{{-1,0},{0,0},{7,6}}}) {
        ticket.request.generation=generations[1];
        const auto stem=std::string("engagement-active-generation-")+std::to_string(generations[0])+"-"+std::to_string(generations[1]);
        auto before=read(fixtures/(stem+".before")),after=read(fixtures/(stem+".after")),incoming=read(fixtures/(stem+".authority"));
        const auto definition=read(fixtures/"engagement-native-definition.bin");auto authored=definition;
        for(auto* bytes:{&before,&after}) {
            put(*bytes,0,ticket.definition);put(*bytes,4,0x808094EFU);put(*bytes,8,ticket.definitionOffset);
            put(*bytes,0x20,7U);put(*bytes,0x170,3U);
        }
        auto component=before;
        std::array<std::byte,0x70> authority{};put(authority,0,ticket.request.registry);authority[4]=std::byte{70};
        put(authority,6,ticket.request.slot);put(authority,0xC,0x808094F1U);put(authority,0x68,ticket.request.scope);
        std::array<std::byte,16> packet{};put(packet,8,std::uintptr_t{0x80000});
        std::uintptr_t deny{};bool resolveAllowed=true,selfAllowed=true;
        capture::Identity self{{1,0x1000},7};
        const auto reader=[&](std::uintptr_t address,std::span<std::byte> destination) {
            if(address==deny)return false;
            const auto region=[&](std::uintptr_t base,std::span<const std::byte> bytes) {
                if(address<base || address-base>bytes.size() || destination.size()>bytes.size()-(address-base))return false;
                std::copy_n(bytes.begin()+(address-base),destination.size(),destination.begin());return true;
            };
            return region(0x30000,component) || region(0x50000,authored) || region(0x60000,authority)
                || region(0x70000,packet) || region(0x80000,incoming);
        };
        const auto resolver=[&](std::uint32_t handle,std::int64_t offset,std::uintptr_t& output) {
            if(!resolveAllowed)return false;
            if(handle==ticket.definition && offset==ticket.definitionOffset){output=0x50000;return true;}
            if(handle==3 && offset==0){output=0x60000;return true;}
            return false;
        };
        const auto identity=[&](std::uintptr_t address,capture::Identity& output) {
            if(!selfAllowed || address!=0x30000)return false;output=self;return true;
        };
        bridge::Mailbox mailbox;check(mailbox.bind(ticket),"active engagement bound before publication");
        const auto binding=mailbox.lookup(ticket.definition);capture::Context context{};
        const auto begin=[&](){return capture::begin(binding,0x30000,0x70000,reader,resolver,identity,context);};
        check(begin()==capture::Result::accepted,"observer captures exact authored type70 authority");
        const auto original=context;
        for(auto address:{0x30000U,0x50000U,0x60000U,0x70000U,0x80000U}) {
            deny=address;check(begin()!=capture::Result::accepted && !context.binding.epoch,"inaccessible native region fails closed");deny=0;
        }
        selfAllowed=false;check(begin()==capture::Result::self,"missing common-pool self rejected");selfAllowed=true;
        self.componentLink++;check(begin()==capture::Result::self,"common-pool link differs from captured component");self.componentLink--;
        resolveAllowed=false;check(begin()==capture::Result::definition,"unresolvable definition rejected");resolveAllowed=true;
        authored[0x38]=std::byte{14};check(begin()==capture::Result::definition,"wrong authored bubble rejected");authored=definition;
        authored[0x48]^=std::byte{1};check(begin()==capture::Result::definition,"wrong authored authority schema rejected");authored=definition;
        authority[0x68]=std::byte{14};check(begin()==capture::Result::authority,"wrong authority scope rejected");authority[0x68]=std::byte{15};
        authority[0x18]=std::byte{1};check(begin()==capture::Result::authority,"authority mode changed rejected");authority[0x18]=std::byte{};
        component=after;context=original;f::Observation observation{};
        const auto finish=[&](){return capture::finish(context,0x30000,11,reader,resolver,identity,observation);};
        const bool expected=generations[0]<=generations[1];
        check(finish()==expected,"original active-player apply qualified with exact committed generation");
        if(!expected)continue;
        check(mailbox.submit({binding,observation}) && !mailbox.submit({binding,observation}),"qualified callback submits once");
        std::array<bridge::Event,1> events{};
        check(mailbox.drain(ticket,events)==1 && events[0].observation.source==self.source,"exact ticket receives native apply");
        self.source.member++;check(!finish(),"source reuse after original rejected");self.source.member--;
        self.componentLink++;check(!finish(),"common-pool link changed after original rejected");self.componentLink--;
        authority[0x20]^=std::byte{1};check(!finish(),"native authority lifetime changed rejected");authority[0x20]^=std::byte{1};
        authored[0x59]^=std::byte{1};check(!finish(),"definition changed during callback rejected");authored=definition;
        packet[0]^=std::byte{1};check(!finish(),"native packet identity changed rejected");packet[0]^=std::byte{1};
        incoming[1]^=std::byte{1};check(!finish(),"native incoming bytes changed rejected");incoming[1]^=std::byte{1};
        component[0x1A0]^=std::byte{1};check(!finish(),"partial native body copy rejected");component=after;
        for(auto address:{0x30000U,0x30170U,0x50000U,0x60000U,0x70000U,0x80000U}) {
            deny=address;check(!finish(),"unreadable post-original snapshot rejected");deny=0;
        }
        check(finish(),"unchanged original fixture still qualifies after rejected captures");
        mailbox.release(ticket.owner);check(!mailbox.submit({binding,observation}),"retired world cannot submit old capture");
    }
    ticket.request.generation=0;
    bridge::Mailbox mailbox;
    auto second=ticket;second.definition++;second.event++;
    check(mailbox.bind(ticket) && mailbox.bind(second),"same owner can retain independent engagement tickets");
    const auto a=mailbox.lookup(ticket.definition),b=mailbox.lookup(second.definition);
    check(mailbox.submit({b,{second,{4,0x1000},21}}) && mailbox.submit({a,{ticket,{3,0x1000},22}}),"interleaved independent receipts queued");
    std::array<bridge::Event,2> events{};
    check(mailbox.drain(ticket,events)==1 && events[0].observation.sequence==22,"ticket drain preserves other feature receipt");
    check(mailbox.drain(second,events)==1 && events[0].observation.sequence==21,"other ticket receipt remains ordered");
    auto replacement=ticket;replacement.event++;
    check(!mailbox.bind(replacement),"event renewal cannot overwrite retained native authority");
    mailbox.release(ticket.owner);
    check(!mailbox.lookup(ticket.definition).epoch && !mailbox.lookup(second.definition).epoch,"world retirement clears both bindings");
    check(mailbox.bind(ticket) && mailbox.lookup(ticket.definition).epoch!=a.epoch,"new world binding gets new epoch");
    check(!mailbox.submit({a,{ticket,{3,0x1000},22}}),"stale epoch rejected after rebind");
}
