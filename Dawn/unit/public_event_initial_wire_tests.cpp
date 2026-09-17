#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/bap/activity_message/sense_update.h"
#include "middleware/encoding/bit_reader.h"
#include <iostream>
#include <cstdlib>
namespace msg=dawn::middleware::bap::activity_message;
namespace wire=msg::native::engagement;
namespace auth=msg::sensor_auth_update;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};void check(bool b,const char* s){++checks;if(!b){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}}
int main(){
 wire::Request request{};request.registry=0xC8229B2B;request.slot=106;request.scope=15;request.generation=1;request.collection=wire::Collection::activePlayers;
 std::array<std::uint8_t,4> types{4,70,68,11},flags{3,3,3,3};std::array<std::uint16_t,4> slots{62,106,107,109};
 std::array<std::uint32_t,1> keys{request.registry};std::array<std::uint8_t,1> present{1};
 std::array<auth::BubbleSubBlock,2> blocks{{{15,keys,present},{15,keys,present}}};
 auth::Snapshot s{};s.region=120;s.hasRegion=true;s.lifetime=6;s.roster.groupCount=1;s.roster.groups[0]={request.registry,types,flags,slots};s.roster.bubbleSubBlocks=std::span(blocks).first(1);
 s.engagements.count=1;s.engagements.entries[0]=request;
 check(wire::valid(s.engagements,s.roster,120),"exact local registry/type70/bubble admitted");
 s.roster.topLevelGroupCount=1;check(!wire::valid(s.engagements,s.roster,120),"top-level admission cannot replace local lease");s.roster.topLevelGroupCount=0;
 s.roster.bubbleSubBlocks={};check(!wire::valid(s.engagements,s.roster,120),"missing bubble grant rejected");s.roster.bubbleSubBlocks=blocks;check(!wire::valid(s.engagements,s.roster,120),"duplicate bubble admission rejected");s.roster.bubbleSubBlocks=std::span(blocks).first(1);
 blocks[0].bubble=14;check(!wire::valid(s.engagements,s.roster,120),"wrong bubble admission rejected");blocks[0].bubble=15;
 present[0]=0;check(!wire::valid(s.engagements,s.roster,120),"retired registry rejected");present[0]=1;
 types[1]=69;check(!wire::valid(s.engagements,s.roster,120),"wrong source type rejected");types[1]=70;
 s.engagements.entries[1]=request;s.engagements.count=2;check(!wire::valid(s.engagements,s.roster,120),"duplicate authority rejected");s.engagements.count=1;
 std::array<std::byte,4096> a{},b{};bits::Writer direct(a),routed(b);
 check(wire::write(direct,request) && auth::legacy_write_auth_body(routed,s,request.registry,70,106,false) && a==b && direct.bit_count()==routed.bit_count(),"production other-missions type70 bytes match proven codec");
 check(auth::legacy_auth_body_bits(s,request.registry,70,106,false)==32 && auth::auth_body_bits(s,request.registry,70,106,false)==32,"both production body measurers expose exact32bits");
 bits::Writer mainBody(b);check(auth::write_auth_body(mainBody,s,request.registry,70,106,false) && a==b,"common production body matches type70 codec");
 s.placements.count=1;s.placements.entries[0]={request.registry,62,15};s.placements.entries[0].generation=1;
 s.cues.count=1;s.cues.entries[0]={request.registry,0x00D1C5B9,107,0,15};s.cues.entries[0].readiness={request.registry,70,106};
 check(msg::native::placement::valid(s.placements,s.roster,120) && msg::native::cue::valid(s.cues,s.roster),"gates/type70/cue share exact native admission");
 std::size_t written{};check(auth::legacy_encode_sensor_auth_update(s,a,written) && written>0,"assembled local gate/engagement/cue serializes through production encoder");
 s.music.count=1;s.music.entries[0].registry=request.registry;s.music.entries[0].slot=109;
 s.music.entries[0].scope=15;s.music.entries[0].candidateCount=4;
 check(msg::native::music::select(s.music.entries[0],0),"select authored music candidate");
 a={};b={};bits::Writer musicDirect(a),musicRouted(b);
 check(msg::native::music::write(musicDirect,s.music.entries[0])
  && auth::legacy_write_auth_body(musicRouted,s,request.registry,11,109,false)
  && a==b && musicDirect.bit_count()==musicRouted.bit_count(),"production music route matches proven native selector codec");
 check(auth::legacy_encode_sensor_auth_update(s,a,written) && written>0,"music and existing event authorities serialize together");
 blocks[0].bubble=14;check(!auth::legacy_encode_sensor_auth_update(s,b,written) && written==0,"production encoder rejects foreign-scope combined frame");blocks[0].bubble=15;
 // Empty optional batch preserves the existing unselected authority route.
 auth::Snapshot empty{};std::array<std::byte,128> old{},fresh{};bits::Writer oldWriter(old),newWriter(fresh);
 check(auth::legacy_write_auth_body(oldWriter,empty,0x85C38F77,4,0,false),"existing rally fallback body writes");empty.engagements.entries[0]=request;
 check(auth::legacy_write_auth_body(newWriter,empty,0x85C38F77,4,0,false) && old==fresh && oldWriter.bit_count()==newWriter.bit_count(),"unused optional storage leaves rally/default bytes unchanged");
 for(unsigned count:{0U,1U,16U}){
  std::array<std::byte,512> payload{};bits::Writer w(payload);
  const std::size_t engagementBits=54+64*count,groupBits=56+engagementBits+56+33+1;
  check(w.write(1,64)&&w.write(2,64)&&w.write(0,msg::sense_update::kLiteralZeroWidth)&&w.write(0,1)&&w.write(1,1)&&w.write(request.registry,32)&&w.write(groupBits,32),"native sense envelope fixture");
  check(w.write(1,1)&&w.write(request.registry,32)&&w.write(71,7)&&w.write(32768+106,16)&&w.write(1,1)&&w.write(count,5),"type70 variable root fixture");
  for(unsigned i=0;i<count;++i)check(w.write(0x123456789ABCDEF0ULL+i,64),"native opaque participant fixture");
  check(w.write(32769,16)&&w.write(99,32)&&w.write(1,1)&&w.write(request.registry,32)&&w.write(31,7)&&w.write(32768+120,16)&&w.write(0,1)&&w.write(100,32)&&w.write(0,1)&&w.write(0,1)&&w.write(0,1),"later type30 proves exact participant width");
  std::size_t used{};check(w.finish(used),"sense fixture finishes");msg::sense_update::SenseUpdate out{};std::size_t consumed{};
  check(msg::sense_update::parse_sense_update(std::span(payload).first(used),out,consumed) && out.objectCount==2,"full production parser accepts mixed variable type70 group");
  check(out.objects[0].hasNativeSchema && out.objects[0].nativeSchema==0x808094F0 && out.objects[0].engagement.participantCount==count && out.objects[0].engagement.generation==1 && out.objects[0].engagement.revision==99 && out.objects[1].nativeRevision==100,"decoded engagement generation/count retained without eating next object");
  for(std::size_t n=0;n<used;++n)check(!msg::sense_update::parse_sense_update(std::span(payload).first(n),out,consumed),"truncated mixed sense rejected atomically");
 }
 std::cout<<"PASS "<<checks<<" public initial wire checks\n";
}
