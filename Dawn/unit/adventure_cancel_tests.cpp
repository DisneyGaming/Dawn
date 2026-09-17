#include "server/runtime/activity/adventure_mercury_openings.h"
#include "server/runtime/activity/adventure_native_bridge.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <vector>
namespace opening=dawn::server::runtime::activity::adventure;
namespace feedback=opening::cue_feedback;
namespace wire=feedback::wire;
namespace bridge=opening::native_bridge;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};void check(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"FAIL%u %s\n",checks,why);std::exit(1);}}
std::vector<std::byte> read(const std::filesystem::path& file){
 std::ifstream in(file,std::ios::binary|std::ios::ate);check(bool(in),"original fixture opens");const auto n=in.tellg();
 check(n>0 && n<1000,"bounded native fixture");std::vector<std::byte> bytes(static_cast<std::size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),n);check(bool(in),"full fixture read");return bytes;
}
int main(int argc,char** argv){
 if(argc!=2)return 2;const std::filesystem::path folder=argv[1];
 for(const auto& authored:opening::mercury::kOpenings){
  auto active=authored.cue;active.owner={42,{3}};active.boot=19;active.definitionRevision=73;active.selectionRevision=5;
  active.request.navigation={}; // Original pre-navigation activation fixture; clear semantics are independent.
  auto clear=active;clear.request.clear=true;
  std::array<char,32> stem{};std::snprintf(stem.data(),stem.size(),"%08X",active.definition);
  const auto before=read(folder/(std::string(stem.data())+".active.body"));
  const auto after=read(folder/(std::string(stem.data())+".clear.body"));
  const auto priorEntry=read(folder/(std::string(stem.data())+".active.entry"));
  const auto entry=read(folder/(std::string(stem.data())+".clear.entry"));
  check(wire::matches_fields(before,active.request) && wire::matches_fields(after,clear.request),"production codec equals original full apply images");
  std::array<std::byte,601> encoded{};bits::Writer writer(encoded);check(wire::write(writer,clear.request),"production clear body writes");
  std::ofstream out(folder/(std::string(stem.data())+".clear.wire"),std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());check(bool(out),"clear wire exported");
  feedback::Capture c{clear,{0xCCDD0001,0},4,feedback::kProducerRva,after,before,after,entry,1,1,true,true,true,priorEntry,0};
  feedback::Observation observation{};check(feedback::qualify(clear,c,observation)==feedback::Result::accepted,"original existing manager1->3 is an exact deactivation receipt");
  for(unsigned i=0;i<10;++i){auto bad=c;auto ticket=clear;std::vector<std::byte> altered=entry;
   switch(i){case 0:bad.prior=after;break;case 1:bad.managerCountAfter=2;break;case 2:bad.entryIndex=1;break;
    case 3:bad.priorEntry={};break;case 4:bad.originalForwarded=false;break;case 5:ticket.boot++;break;
    case 6:altered[0x70]=std::byte{1};bad.entry=altered;break;case 7:altered[0x2b]=std::byte{2};bad.entry=altered;break;
    case 8:altered[0x10]^=std::byte{1};bad.entry=altered;break;case 9:bad.managerReadyAfter=false;break;}
   check(feedback::qualify(ticket,bad,observation)!=feedback::Result::accepted,"repeat/wrong index/owner/body/unchanged mode fails");
  }
  bridge::Mailbox mailbox;check(mailbox.bind(active),"active ticket binds");
  check(!mailbox.clear(active,clear),"unconfirmed activation cannot fabricate manager deactivation");
  const auto armed=mailbox.lookup(active.definition);
  feedback::Observation accepted{active,{0xCCDD0001,0},3,0,wire::manager_id(active.request.event)};
  check(mailbox.submit({armed,accepted}),"qualified activation event queued");
  auto other=opening::mercury::kOpenings[(authored.activity==1076)?1:0].cue;
  other.owner=active.owner;other.boot=active.boot;other.definitionRevision=active.definitionRevision;other.selectionRevision=active.selectionRevision;
  check(mailbox.bind(other),"same owner independent feature ticket");const auto otherBinding=mailbox.lookup(other.definition);
  check(mailbox.clear(active,clear) && mailbox.lookup(active.definition).ticket==clear,"exact clear replaces consumed activation ticket");
  check(mailbox.lookup(other.definition).epoch==otherBinding.epoch,"other same-owner receipt is preserved");
  check(!mailbox.submit({armed,accepted}) && !mailbox.clear(active,clear),"stale old epoch and duplicate transition rejected");
  check(feedback::qualify(clear,c,observation)==feedback::Result::accepted,"fresh exact clear receipt");
  const auto clearBinding=mailbox.lookup(clear.definition);check(mailbox.submit({clearBinding,observation}),"native deactivation reaches mailbox");
  std::array<bridge::Event,1> events{};check(mailbox.drain(other,events)==0 && mailbox.drain(clear,events)==1,"exact-ticket drain prevents receipt theft");
  mailbox.release(active.owner);check(!mailbox.submit({clearBinding,observation}) && !mailbox.lookup(other.definition).epoch,"real world retirement clears all epochs");
 }
 std::printf("PASS %u production cue clear codec, original native receipt and exact-ticket isolation checks\n",checks);
}
