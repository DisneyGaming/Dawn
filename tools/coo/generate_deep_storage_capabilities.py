"""Generate or check the trusted Deep Storage Lua capabilities from recovered evidence."""
import argparse
import json
from extract_deep_storage_bindings import recover_scan_durations
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check',action='store_true')
parser.add_argument('--evidence',type=Path,default=ROOT/'build/coo/deep-storage-research/native-bindings.json')
args=parser.parse_args()
r=ROOT/'Dawn/src/state/activity/deep_storage'
d=json.loads(args.evidence.read_text(encoding='utf-8'))
assert d['scanDurations']==recover_scan_durations(), 'Scan duration metadata differs from installed source overrides.'
def emit(path,text):
 if args.check:
  if not path.exists() or path.read_text(encoding='utf-8')!=text:raise SystemExit('Generated file differs: '+str(path))
 else:path.write_text(text,encoding='utf-8')
prefix={0xE6E910D2:'mission',0xA13D8A45:'entry',0x4324A238:'entry_route',0x59700FA7:'pyramidion',0xE6402111:'descent_exit',0xEA42F517:'final',0x559E8DE2:'final_route',0xE86A5BFD:'gate_route',0x54DA5E1B:'corridor_route',0xABF05147:'descent_route'}
def asset(reg,t,slot):
 for g in d['groups']:
  if g['registry']==reg:
   for s in g['slots']:
    if s['slotTypes']==t and s['slotIndices']==slot:return '{0x%08XU,0x%08XU,%d,%d}'%(reg,s['descriptorTags'],t,slot)
 for v in d['volumes']:
  if v['registry']==reg and t==60 and v['slot']==slot:return '{0x%08XU,0x%08XU,60,%d}'%(reg,v['tag'],slot)
 raise ValueError((reg,t,slot))
M=asset(0xE6E910D2,53,2);mod='{kRoot,kScenario,0,0}'
out=['// Native capabilities. Story order and encounter activation belong to deep_storage.lua.','#pragma once','#include "native_catalog.h"','#include "ai_bindings.h"','#include "../coo/mission_script.h"','namespace dawn::state::activity::deep_storage {',f'inline constexpr coo::Asset kModule{mod},kDialogueAsset{M};','inline constexpr coo::script::Capability kCapabilities[]{']
def cap(name,op,a,n,wait='requested',domain='*'):out.append('    {"%s","%s",{coo::Operation::%s,%s,%sU,coo::Wait::%s}},'%(name,domain,op,a,n,wait))
cap('map.encounter.finish','mechanic',mod,12,domain='map_room')
cap('mission.module','mechanic',mod,1,domain='composition');cap('mission.checked','observation','{}',0,'observed','composition');cap('mission.finish','complete',mod,6);cap('respawn.restrict','mechanic',mod,11);cap('respawn.allow','mechanic',mod,10)
for i,o in enumerate(d['objectives']):cap(f'objective.{i}','objective',asset(0xE6E910D2,68,0),o['event'])
for i in range(len(d['dialogue'])):
 cap(f'dialogue.{i}','dialogue',M,i,'nativeReady');cap(f'dialogue.{i}.queued','dialogue',M,i);cap(f'dialogue.finished.{i}','observation',M,i,'observed')
for g in d['groups']:
 if g['registry'] not in prefix:continue
 for s in g['slots']:
  t=s['slotTypes'];a=asset(g['registry'],t,s['slotIndices']);name=prefix[g['registry']]+'.'+s['name']
  if t==1:
   cap(name+'.spawn','population',a,1,'nativeReady');cap(name+'.cleared','observation',a,2,'observed')
   if g['registry']==0x59700FA7 and 1<=s['slotIndices']<=17:cap(name+'.request','population',a,1)
  if g['registry']==0x59700FA7 and t==4 and s['slotIndices']==79:cap('map.lens.destroyed','observation',a,12,'observed')
  if t in (4,23):
   cap(name+'.on','device',a,1,'nativeReady' if t==4 else 'requested');cap(name+'.off','device',a,0)
for v in d['volumes']:
 if v['registry'] not in prefix:continue
 name=prefix[v['registry']]+'.'+v['name'];a=asset(v['registry'],60,v['slot'])
 cap(name,'observation',a,0,'observed');cap(name+'.occupied','observation',a,1,'observed')
for name,reg,slot in [('entry.plate',0xA13D8A45,4),('map.left',0x59700FA7,72),('map.right',0x59700FA7,74)]:
 a=asset(reg,4,slot);cap(name+'.arm','mechanic',a,10);cap(name+'.occupied','observation',a,11,'observed');cap(name+'.charged','observation',a,12,'observed')
for name,reg,slot in [('entry.scan',0xA13D8A45,2),('map.scan',0x59700FA7,95)]:
 a=asset(reg,4,slot);cap(name+'.enable','mechanic',a,10);cap(name+'.started','observation',a,11,'observed');cap(name+'.finished','observation',a,12,'observed')
out+=['};','inline constexpr coo::script::ModuleCapability kModules[]{{"mission",{kModule,1}}};','inline constexpr coo::script::FactCapability kFacts[]{{"mission.checked",0}};', 'inline constexpr auto kPlaybackRows=[] {std::array<coo::DialogueRow,std::size(kDialogue)> out{};for(std::size_t i=0;i<out.size();++i) {out[i]={kDialogue[i].selector,kDialogue[i].durationMs,0,false};}return out;}();','inline constexpr coo::DialogueDefinition kPlayback{kBank,kPlaybackRows,{}};','inline constexpr auto kObjectiveEvents=[] {std::array<std::uint32_t,std::size(kObjectives)> out{};for(std::size_t i=0;i<out.size();++i) {out[i]=kObjectives[i].event;}return out;}();','inline constexpr coo::script::Profile kProfile{"deep_storage.native.v1","otherMissions",coo::Schema::otherMissions,kCapabilities,kModules,kFacts,kPlayback,kObjectiveEvents,{},{},{}};','bool valid_document(const coo::script::Views&) noexcept;','}']
emit(r/'bindings.h','\n'.join(out)+'\n')
emit(r/'mechanism_bindings.h','''#pragma once
#include "native_catalog.h"
#include "mechanism_catalog.h"
#include "../coo/objective_service.h"
namespace dawn::state::activity::deep_storage {
struct PlateBinding { coo::Asset source,volume;float chargeSeconds; };
// VIDEO estimates; native 815B8B3B owns progress and the completion latch.
inline constexpr PlateBinding kPlates[]{
    {%s,%s,5.F},
    {%s,%s,10.F},
    {%s,%s,10.F},
};
struct ScanBinding { coo::Asset source,link;std::uint32_t controllerDefinition;std::uint64_t guid;float defaultSeconds,sourceSeconds; };
inline constexpr ScanBinding kScans[]{
    {%s,%s,0x8156EFA4U,0xCDFC784B0C4A9CF5ULL,kGhostScans[0].defaultDuration,kGhostScans[0].sourceDuration},
    {%s,%s,0x8157E6B1U,0x844920F35A95B55BULL,kGhostScans[1].defaultDuration,kGhostScans[1].sourceDuration},
};
// The opening uses the package-local ap_pyramidion_altar ActivityPoint.
// Its locator resolves the entrance plate across native destination contexts.
// Interaction markers use their scoped native sources.
inline constexpr coo::MarkerTarget marker(std::uint32_t event) noexcept {
    if(event==kObjectives[0].event) {return {{0x4324A238U,0x80B5616BU,47,4},{0x2D7B770FU,0x22723FADU,0x4324A238U,0x22BCA6B6U}};}
    if(event==kObjectives[1].event || event==kObjectives[2].event) {return {kScans[0].source,{}};}
    if(event==kObjectives[8].event) {return {kScans[1].source,{}};}
    return {};
}
float device_position(coo::Asset,bool) noexcept;
}
'''%(asset(0xA13D8A45,4,4),asset(0xA13D8A45,60,12),asset(0x59700FA7,4,72),asset(0x59700FA7,60,273),asset(0x59700FA7,4,74),asset(0x59700FA7,60,280),asset(0xA13D8A45,4,2),asset(0x4324A238,65,0),asset(0x59700FA7,4,95),asset(0xEA42F517,65,0)))
print('Generated %d named native capabilities; three plates and two scans bound.'%sum('coo::Operation' in x for x in out))
