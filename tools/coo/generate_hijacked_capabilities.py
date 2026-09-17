"""Generate trusted Hijacked aliases from its recovered package catalog; no mission order."""
import argparse,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args()
d=json.loads((ROOT/'build/coo/hijacked-research/native-bindings.json').read_text(encoding='utf-8'))
r=ROOT/'Dawn/src/state/activity/hijacked'
prefix={0x77852DB9:'mission',0x555AA58B:'landing',0x153E22CD:'mists',0x3C7C8AE9:'exit_route',0x2D20FD66:'mind_route',0xD8FA09CA:'cave_route',0x6E65B834:'return_route',0x3E9B74F3:'surface',0xF5737F85:'tangle',0x701F9CE5:'well_route',0xD997395E:'well',0xA12CA9FA:'puzzle_route',0x40A009B5:'final_route'}
def asset(reg,kind,slot):
 for g in d['groups']:
  if g['registry']==reg:
   for s in g['slots']:
    if (s['slotTypes'],s['slotIndices'])==(kind,slot):return '{0x%08XU,0x%08XU,%d,%d}'%(reg,s['descriptorTags'],kind,slot)
 for v in d['volumes']:
  if (v['registry'],60,v['slot'])==(reg,kind,slot):return '{0x%08XU,0x%08XU,60,%d}'%(reg,v['tag'],slot)
 raise ValueError((reg,kind,slot))
M=asset(0x77852DB9,53,2);mod='{kRoot,kScenario,0,0}';boss=asset(0x153E22CD,1,21)
out=['// Generated native capability names. Story order belongs to hijacked.lua.','#pragma once','#include "native_catalog.h"','#include "ai_bindings.h"','#include "../coo/mission_script.h"','namespace dawn::state::activity::hijacked {',f'inline constexpr coo::Asset kModule{mod},kDialogueAsset{M},kBoss{boss};','inline constexpr coo::script::Capability kCapabilities[]{']
def cap(n,op,x,arg,wait='requested',domain='*'):out.append('    {"%s","%s",{coo::Operation::%s,%s,%sU,coo::Wait::%s}},'%(n,domain,op,x,arg,wait))
cap('mission.module','mechanic',mod,1,domain='composition');cap('mission.checked','observation','{}',0,'observed','composition');cap('mission.finish','complete',mod,6)
# Authored local route observation; no fabricated native volume identifier.
cap('tangle.cleanup_point','observation',mod,30,'observed')
cap('well.encounter.finish','mechanic',mod,12,domain='conflux');cap('respawn.restrict','mechanic',mod,11);cap('respawn.allow','mechanic',mod,10)
for i,o in enumerate(d['objectives']):cap('objective.%d'%i,'objective',asset(0x77852DB9,68,0),o['event'])
for row in d['dialogue']:
 i=row['row']
 if not row['texts']:continue
 cap(f'dialogue.{i}','dialogue',M,i,'nativeReady');cap(f'dialogue.{i}.queued','dialogue',M,i);cap(f'dialogue.finished.{i}','observation',M,i,'observed')
for g in d['groups']:
 if g['registry'] not in prefix:continue
 for s in g['slots']:
  t=s['slotTypes'];x=asset(g['registry'],t,s['slotIndices']);n=prefix[g['registry']]+'.'+s['name']
  if t==1:
   cap(n+'.spawn','population',x,1,'nativeReady');cap(n+'.request','population',x,1);cap(n+'.cleared','observation',x,2,'observed')
   if g['registry']==0x3E9B74F3 and s['slotIndices'] in range(1,8):cap(n+'.retire','population',x,0)
  if t in (4,23):cap(n+'.on','device',x,1,'nativeReady' if t==4 else 'requested');cap(n+'.off','device',x,0)
for v in d['volumes']:
 if v['registry'] not in prefix:continue
 x=asset(v['registry'],60,v['slot']);n=prefix[v['registry']]+'.'+v['name'];cap(n,'observation',x,0,'observed');cap(n+'.occupied','observation',x,1,'observed')
for name,stage in (('two_thirds',0),('one_third',1)):cap('mind.health.'+name,'observation',boss,100+stage,'observed')
for v in d['volumes']:
 if (v['registry'],v['name']) in ((0x153E22CD,'lrg_arena_trigger_volume'),(0x2D20FD66,'music_tv_boss_seen')):
  cap(prefix[v['registry']]+'.'+v['name']+'.after10s','eventAfter',asset(v['registry'],60,v['slot']),10000,'observed')
for stage in range(3):cap('mind.position.%d'%stage,'mechanic',boss,20+stage,'nativeReady')
cap('mind.position.0.request','mechanic',boss,20)
x=asset(0xD997395E,4,19)
cap('well.plate.arm','mechanic',x,10);cap('well.plate.occupied','observation',x,11,'observed');cap('well.plate.charged','observation',x,12,'observed')
x=asset(0xD997395E,4,23)
cap('conflux.scan.enable','mechanic',x,10);cap('conflux.scan.started','observation',x,11,'observed');cap('conflux.scan.finished','observation',x,12,'observed')
out+=['};','inline constexpr coo::script::ModuleCapability kModules[]{{"mission",{kModule,1}}};','inline constexpr coo::script::FactCapability kFacts[]{{"mission.checked",0}};','inline constexpr auto kPlaybackRows=[] {std::array<coo::DialogueRow,std::size(kDialogue)> out{};for(std::size_t i=0;i<out.size();++i) {out[i]={kDialogue[i].selector,kDialogue[i].durationMs,0,false};}return out;}();','inline constexpr coo::DialogueDefinition kPlayback{kBank,kPlaybackRows,{}};','inline constexpr auto kObjectiveEvents=[] {std::array<std::uint32_t,std::size(kObjectives)> out{};for(std::size_t i=0;i<out.size();++i) {out[i]=kObjectives[i].event;}return out;}();','inline constexpr coo::script::Profile kProfile{"hijacked.native.v1","otherMissions",coo::Schema::otherMissions,kCapabilities,kModules,kFacts,kPlayback,kObjectiveEvents,{},{},{}};','bool valid_document(const coo::script::Views&) noexcept;','}']
text='\n'.join(out)+'\n';target=r/'bindings.h'
if a.check:assert target.read_text(encoding='utf-8')==text,'Hijacked capabilities changed'
else:target.write_text(text,encoding='utf-8')
print('Verified' if a.check else 'Generated',sum('coo::Operation' in line for line in out),'Hijacked native aliases')
