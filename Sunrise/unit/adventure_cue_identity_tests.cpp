#include "client/hooks/bootflow/adventure_cue_native_identity.h"
#include "client/hooks/bootflow/adventure_cue_observer.h"
#include "core/logging/log.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace sunrise::core::log {void write(Channel,Level,std::string_view) noexcept {}}
namespace identity=sunrise::client::hooks::bootflow::adventure_cue_native_identity;
namespace observer=sunrise::client::hooks::bootflow::adventure_cue_observer;
namespace f=identity::feedback;
unsigned checks{};
void check(bool value,const char* why) {++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,why);std::exit(1);}}
std::vector<std::byte> file(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"fixture opens");const auto n=in.tellg();
    check(n>0 && n<10000,"bounded fixture");std::vector<std::byte> result(static_cast<std::size_t>(n));
    in.seekg(0);in.read(reinterpret_cast<char*>(result.data()),n);check(bool(in),"fixture read");return result;
}
int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"usage: adventure_cue_identity_tests original-helper-proof-directory\n");return 2;}
    const std::filesystem::path directory(argv[1]);auto component=file(directory/"component.bin");
    check(f::field<std::uint32_t>(component,0)==0x80F46D67 && f::field<std::uint32_t>(component,4)==0x80804F54
        && f::field<std::int64_t>(component,8)==0xB88,"current native type68 identity");
    check(f::field<std::uint64_t>(component,0x160)==UINT64_MAX,"actual optional source is absent");
    identity::Identity first{};
    for(const auto name:{"captured","new-salt","embedded"}) {
        const auto input=file(directory/(std::string(name)+".input.bin"));auto row=file(directory/(std::string(name)+".row.bin"));
        const auto native=file(directory/(std::string(name)+".self.bin"));
        const auto address=f::field<std::uintptr_t>(input,0),pool=f::field<std::uintptr_t>(input,8),base=f::field<std::uintptr_t>(input,20);
        const auto stride=f::field<std::uint32_t>(input,16),link=f::field<std::uint32_t>(component,0x20);
        const auto member=f::field<std::uint32_t>(native,0);const auto rowAddress=pool+(link&0x1FFFU)*stride;
        bool readable=true,allowResolve=true;std::uint32_t resolvedMember=member;std::uintptr_t resolvedBase=base;unsigned reads{};
        const auto reader=[&](std::uintptr_t from,std::span<std::byte> to) noexcept {
            ++reads;if(!readable)return false;
            if(from==address+0x20 && to.size()==4){std::copy_n(component.begin()+0x20,4,to.begin());return true;}
            if(from==rowAddress+0x20 && to.size()==4){std::copy_n(row.begin()+0x20,4,to.begin());return true;}
            return false;
        };
        const auto resolver=[&](std::uint32_t handle,std::int64_t offset,std::uintptr_t& result) noexcept {
            if(!allowResolve || handle!=resolvedMember || offset!=0)return false;result=resolvedBase;return true;
        };
        identity::Identity actual{};
        check(identity::capture(address,pool,stride,reader,resolver,actual),"absent optional source admits native common self");
        check(reads==2,"only common link and pool member are read; no optional source read");
        check(actual.source.member==member && actual.source.offset==f::field<std::int64_t>(native,8)
            && actual.componentLink==link,"production identity equals original4E5640 complete reference");
        if(std::string_view(name)=="captured")first=actual;
        else check(actual!=first,"new full salt or embedded offset differs from prior identity");
        readable=false;check(!identity::capture(address,pool,stride,reader,resolver,actual),"unreadable source is rejected");readable=true;
        allowResolve=false;check(!identity::capture(address,pool,stride,reader,resolver,actual),"unresolved full source rejected");allowResolve=true;
        resolvedMember=member^0x2000;check(!identity::capture(address,pool,stride,reader,resolver,actual),"same slot different generation cannot resolve");resolvedMember=member;
        resolvedBase=address+1;check(!identity::capture(address,pool,stride,reader,resolver,actual),"negative source offset rejected");
        resolvedBase=base;row[0x20]^=std::byte{1};check(!identity::capture(address,pool,stride,reader,resolver,actual),"foreign common pool slot rejected");row[0x20]^=std::byte{1};
        check(!identity::capture(address,UINTPTR_MAX-4,stride,reader,resolver,actual),"pool address overflow rejected");
        check(!identity::capture(address,pool,0,reader,resolver,actual),"invalid pool stride rejected");
    }
    const auto cache=file(directory/"applied-decoded.bin"),entry=file(directory/"manager-entry.bin");
    const f::wire::Request request{0x08551BCF,0xC9E4C596,0,0};
    check(f::wire::matches_fields(cache,request),"complete captured native cache matches every decoder-written field");
    check(f::field<std::uint8_t>(entry,0)==2 && f::field<std::uint32_t>(entry,4)==f::wire::manager_id(request.event)
        && f::field<std::uint32_t>(entry,0x10)==request.registry && f::field<std::int16_t>(entry,0x124)==1076,
        "actual native manager converted the exact selected Adventure cue");
    f::Ticket ticket{};ticket.owner={0x9EAA300100200001,{1}};ticket.boot=1;ticket.definitionRevision=1;ticket.selectionRevision=3;
    ticket.activity=1076;ticket.definition=0x80F46D67;ticket.table=0x80F46D25;ticket.stringBank=0x80F56028;
    ticket.title=0x099C20CB;ticket.detail=0x9661E8EB;ticket.definitionOffset=0xB88;ticket.request=request;
    const f::Capture currentOnly{ticket,first.source,1,f::kProducerRva,cache,cache,cache,entry,0,1,true,true,true};
    f::Observation observation{};
    check(f::qualify(ticket,currentOnly,observation)==f::Result::unchanged,"current cache/manager cannot manufacture a past original-call receipt");
    check(!observer::begin(nullptr,nullptr).binding.epoch && !observer::finish(nullptr,{}),"compiled observer remains inert on missing input");
    std::printf("PASS %u Adventure native identity checks; original helper and current component fixtures; production observer compiled\n",checks);
}
