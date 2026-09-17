"""Generate the trusted native capability names. Story decisions stay in Lua."""
import argparse
import json
from generate_beyond_infinity_runtime import playback,cues
from package_read import read
from extract_gateway_bindings import array,u32,i64
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Dawn/src/state/activity/beyond_infinity/bindings.h'
PREFIX={0x233E7149:'well',0x1194F70F:'reflections',0x338D8E1D:'reveal',
        0x8E70632B:'forest',0x15FFBE16:'future',0x0FF26BCC:'ambush',
        0xC7FB7155:'past',0xDA02FEF1:'lighthouse',0xA908C5F7:'well_objectives',
        0xCFB5D3B3:'well_upper',0x45AFDE9B:'escape',0x91AF0A4E:'future_objectives',
        0x2F94AB34:'future_vignette',0xBB66F71D:'past_objectives',
        0xC05EAA69:'past_exit',0x199D7650:'forest_future',0x6A921301:'forest_past'}

def render():
    j=json.loads((ROOT/'build/coo/beyond-infinity-research/native-bindings.json').read_text())
    caps=[]
    def add(name,op,asset,arg=0,wait='requested',domain='*'):
        assert name not in [c[0] for c in caps],name
        caps.append((name,domain,op,asset,arg,wait))
    module=(0x03632571,0x80F46015,0,0)
    dialogue=(0x03632571,0x80F4622B,53,2)
    add('mission.module','mechanic',module,1,domain='composition')
    add('mission.checked','observation',(0,0,0,0),wait='observed',domain='composition')
    add('mission.finish','complete',module,6)
    add('travel.reset','mechanic',module,10)
    add('forest.past_route','mechanic',module,11)
    add('forest.future_route','mechanic',module,12)
    for route in range(1,6):
        add(f'transit.{route}.contact','observation',module,30+route,'observed')
        add(f'transit.{route}.request','mechanic',module,20+route)
        add(f'transit.{route}.arrived','observation',module,20+route,'observed')
    add('well.lens_destroyed','observation',(0x233E7149,0x80F462B3,4,36),2,'observed')
    for row in j['objectives']:
        add(f'objective.{row["row"]}','objective',(0x03632571,0x80F46225,68,0),row['event'])
    for row in j['dialogue']:
        if not row['durationMs']: continue
        add(f'dialogue.{row["row"]}','dialogue',dialogue,row['row'],'nativeReady')
        if row['row'] in (6,9,10,12,13,14,15,17,19):
            add(f'dialogue.{row["row"]}.queued','dialogue',dialogue,row['row'],'requested')
        add(f'dialogue.finished.{row["row"]}','observation',dialogue,row['row'],'observed')
    for v in j['volumes']:
        if v['registry'] not in PREFIX: continue
        name=PREFIX[v['registry']]+'.'+v['name']
        asset=(v['registry'],v['tag'],60,v['slot'])
        add(name,'observation',asset,0,'observed')
        add(name+'.occupied','observation',asset,1,'observed')
        if v['registry']==0x233E7149 and v['slot']==119:
            add('well.plate_occupied','observation',asset,1,'observed')
    for g in j['groups']:
        if g['registry'] not in PREFIX: continue
        for s in g['slots']:
            kind=s['slotTypes']
            if kind not in (1,4,23,26,34,43): continue
            if kind in (26,34) and (g['registry'],kind,s['slotIndices']) not in (
                    (0x0FF26BCC,26,15),(0x0FF26BCC,26,16),(0x0FF26BCC,34,19),(0x0FF26BCC,34,20)): continue
            # Only these reflected layouts are supported by this adapter.
            expected={1:0x80807EC9,4:0x8080992F,23:0x80804F48,26:0x8080954B,34:0x8080956A,43:0x8080626B}
            # Object schemas are checked again by the authority writer.
            if s['authSchemas']!=expected[kind]: continue
            asset=(g['registry'],s['descriptorTags'],kind,s['slotIndices'])
            name=PREFIX[g['registry']]+'.'+s['name']
            if kind==43:
                add(name,'scene',asset,1,'nativeReady')
                add(name+'.off','device',asset,0)
                add(name+'.finished','observation',asset,2,'observed')
                if g['registry']==0x1194F70F and s['name'] in ('scene_echo_first','scene_split_2char',
                        'scene_echo_intro_two','scene_echo_intro_four_3char','scene_echo_intro_six','scene_echo_intro_six_right'):
                    add(name+'.silent','scene',asset,3,'nativeReady')
                if s['descriptorTags']==0x80F4608D: add(name+'.escape_speech.finished','observation',asset,0x417,'observed')
                _,definition=read(s['descriptorTags']);_,entity=read(u32(definition,s['descriptorOffsets']+0x60))
                graphs=[]
                for resource in array(entity,0x10,12):
                    tag=u32(entity,resource);kind,graph=read(tag)
                    if kind==0x80809C36 and len(graph)>=0xA0 and u32(graph,0x94)==0x80806384: graphs.append(tag)
                assert len(graphs)==1
                events,speeches=playback(graphs[0],j)
                for event in events: add(name+f'.input.{event:08X}','mechanic',asset,event)
                if s['descriptorTags'] in (0x80F461D1,0x80F4608D):
                    for idx,_,_,_,_,_,_,emitted in cues(graphs[0]):
                        add(name+f'.cue.{idx}.started','observation',asset,0x200+idx,'observed')
                        add(name+f'.cue.{idx}.finished','observation',asset,0x300+idx,'observed')
                        for event in emitted: add(name+f'.event.{event:08X}.emitted','observation',asset,0x300+idx,'observed')
                for row in sorted(set(speech[0] for speech in speeches)):
                    add(name+f'.dialogue.{row}.finished','observation',asset,0x100+row,'observed')
            else:
                add(name+'.on','population' if kind==1 else 'device',asset,1)
                add(name+'.off','population' if kind==1 else 'device',asset,0)
    def asset(a):return '{'+','.join(f'0x{x:X}U' for x in a)+'}'
    lines=['// Generated native capabilities; Lua owns progression.', '#pragma once',
           '#include "native_catalog.h"','#include "../coo/mission_script.h"',
           'namespace dawn::state::activity::beyond_infinity {',
           f'inline constexpr coo::Asset kModule={asset(module)},kDialogueAsset={asset(dialogue)};',
           'inline constexpr coo::script::Capability kCapabilities[]{']
    for name,domain,op,a,arg,wait in caps:
        lines.append(f'    {{"{name}","{domain}",{{coo::Operation::{op},{asset(a)},{arg}U,coo::Wait::{wait}}}}},')
    lines+=['};','inline constexpr coo::script::ModuleCapability kModules[]{{"mission",{kModule,1}}};',
            'inline constexpr coo::script::FactCapability kFacts[]{{"mission.checked",0}};',
            'inline constexpr auto kPlaybackRows=[] { std::array<coo::DialogueRow,49> rows{};',
            '    for(std::size_t i=0;i<rows.size();++i) { rows[i]={kDialogue[i].selector,kDialogue[i].durationMs,0,false}; } return rows; }();',
            'inline constexpr coo::DialogueDefinition kPlayback{kBank,kPlaybackRows,{}};',
            'inline constexpr auto kObjectiveEvents=[] { std::array<std::uint32_t,11> events{};',
            '    for(std::size_t i=0;i<events.size();++i) { events[i]=kObjectives[i].event; } return events; }();',
            'inline constexpr coo::script::Profile kProfile{"beyond_infinity.native.v1","otherMissions",coo::Schema::otherMissions,kCapabilities,kModules,kFacts,kPlayback,kObjectiveEvents,{},{},{}};',
            'bool valid_document(const coo::script::Views&) noexcept;', '}', '']
    return '\n'.join(lines)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
    content=render()
    if args.check: assert OUT.read_text()==content,'Profile capabilities differ from source evidence'
    else: OUT.write_text(content,encoding='utf-8')
    print(f'PASS: {OUT.name}')
