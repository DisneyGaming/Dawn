"""Reproduce Eater's owned native catalog and spatial joins from installed packages.

This reads package data only. It never starts or attaches to the game. Slot holes
and server-only entries remain in their authored order in the generated roster.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'docs/raids/eater-of-worlds/tools'))
import extract_eater_of_worlds as eater

INVENTORY = ROOT/'docs/raids/eater-of-worlds/evidence/native-inventory.json'
EVIDENCE = ROOT/'docs/raids/eater-of-worlds/evidence/runtime-bindings.json'
OUTPUT = ROOT/'Dawn/src/state/activity/eater_of_worlds/catalog.h'
PROFILE = OUTPUT.with_name('profile.h')
number = lambda v: int(v, 16) if isinstance(v, str) else v
hx = lambda v: f'0x{number(v):08X}U'


def recover():
    raw = INVENTORY.read_bytes()
    data = json.loads(raw)
    groups = data['groups']
    blobs = {}
    def collect(tag):
        tag = number(tag)
        if tag in blobs: return
        assert len(blobs) < 5000
        cls, blob = eater.packages.read(tag)
        blobs[tag] = (cls, blob)
        refs = [eater.u32(blob, 12)] if cls == 0x80809B14 else (
            [eater.u32(blob, p) for p in eater.array(blob, 16, 4)] if cls == 0x80809468 else [])
        for ref in refs: collect(ref)
    identities = set()
    for g in groups:
        collect(g['objectTag'])
        for binding in g['bubbleBindings']:
            for tag in binding['handles']: collect(tag)
        for d in g['descriptors']: collect(d['sourceTag'])
        for s in g['declaredSlots']:
            identity = (number(g['registryKey']), s['type'], s['index'])
            assert identity not in identities
            identities.add(identity)
    volumes = []
    for tag, (cls, blob) in sorted(blobs.items()):
        if cls != 0x80809C36: continue
        for at in range(12, len(blob)-0xE0, 4):
            registry, typ, slot = struct.unpack_from('<IHH', blob, at)
            if typ != 60 or (registry, typ, slot) not in identities: continue
            base = at-12
            name_at = base+eater.i64(blob, base)
            if not 0 <= name_at < len(blob): continue
            name = blob[name_at:].split(b'\0', 1)[0].decode(errors='replace')
            if not re.fullmatch(r'[A-Za-z_0-9.\[\]]+', name): continue
            try:
                vertices = [struct.unpack_from('<3f', blob, p) for p in eater.array(blob, base+0xD0, 16, 0x80800094)]
                lower = struct.unpack_from('<3f', blob, base+0xB0)
                upper = struct.unpack_from('<3f', blob, base+0xC0)
            except (AssertionError, struct.error): continue
            assert len(vertices) >= 3 and all(math.isfinite(x) for p in vertices for x in p)
            assert all(math.isfinite(lo) and math.isfinite(hi) and lo <= hi for lo, hi in zip(lower, upper))
            volumes.append(dict(tag=tag, offset=base, registry=registry, slot=slot,
                                name=name, min=lower, max=upper, vertices=vertices))
    assert len({(v['registry'], v['slot']) for v in volumes}) == len(volumes)
    monitors = []
    entities = []
    for g in groups:
        key = number(g['registryKey'])
        for d in g['descriptors']:
            tag, off = number(d['sourceTag']), number(d['sourceOffset'])
            _, blob = blobs[tag]
            assert struct.unpack_from('<IHH', blob, off+48) == (key, d['type'], d['index'])
            if d['type'] in (30, 31):
                target = struct.unpack_from('<IHH', blob, off+0x58)
                if target in identities and target[1] == 60:
                    assert any(v['registry'] == target[0] and v['slot'] == target[2] for v in volumes)
                    monitors.append(dict(registry=key, type=d['type'], slot=d['index'],
                                         targetRegistry=target[0], targetSlot=target[2]))
            if d['type'] == 4:
                ent = eater.u32(blob, off+0xB8)
                if 0x80800000 <= ent < 0x82000000:
                    cls, entity = eater.packages.read(ent)
                    resources = [eater.u32(entity, p) for p in eater.array(entity, 0x10, 12)] if cls == 0x80809C0F else []
                    position = struct.unpack_from('<4f', blob, off+0xD8)
                    if not (position[3] == 1.0 and all(math.isfinite(p) for p in position)): position = ()
                    entities.append(dict(registry=key, slot=d['index'], source=tag, entity=ent,
                                         entityClass=cls, resources=resources,position=position[:3]))
    cache = eater.CACHE.read_bytes()
    counts, offsets = eater.cache_domains(cache)
    cached_matches = []
    for index in range(counts[eater.ROSTER_DOMAIN]):
        at = offsets[eater.ROSTER_DOMAIN]+index*30730
        key, tag, count = struct.unpack_from('<IIH', cache, at)
        g = next((g for g in groups if number(g['registryKey'])==key and number(g['objectTag'])==tag), None)
        if g is None: continue  # Registry keys alone do not establish package identity.
        expected = {(d['type'], d['index']): d for d in g['descriptors']}
        for j in range(count):
            identity = (cache[at+10+j], struct.unpack_from('<H', cache, at+2570+j*2)[0])
            assert identity in expected, (hex(key), identity)
            d = expected[identity]
            flags = (1 if number(d['senseSchema'])!=0xFFFFFFFF else 0) | (2 if number(d['authSchema'])!=0xFFFFFFFF else 0)
            assert cache[at+1290+j]==flags
            for field, offset in [('sourceTag',5130),('sourceOffset',10250),('componentClass',15370),('senseSchema',20490),('authSchema',25610)]:
                assert eater.u32(cache,at+offset+j*4)==number(d[field]), (hex(key),identity,field)
        cached_matches.append(dict(registry=key,objectTag=tag,matchedDescriptors=count,declaredSlots=len(g['declaredSlots'])))
    assert any(g['registry']==0x24C67333 and g['matchedDescriptors']==20 for g in cached_matches)
    return dict(schema='eater-runtime-bindings-v1', inventorySha256=hashlib.sha256(raw).hexdigest(),
                cacheSha256=hashlib.sha256(cache).hexdigest(),cacheMatches=cached_matches,
                groups=groups, bubbles=data['bubbles'], objectives=data['objectives'], dialogue=data['dialogue'],
                sources=data['squadSources'], volumes=volumes, monitors=monitors, entities=entities,
                tags=[dict(tag=t, cls=c, bytes=len(b), sha256=hashlib.sha256(b).hexdigest()) for t, (c,b) in sorted(blobs.items())])


def render(data):
    q = lambda x: json.dumps(x, ensure_ascii=True)
    def point(p):
        return '{'+','.join((format(v,'.9g')+('.0' if float(v).is_integer() and 'e' not in format(v,'.9g') else '')+'F') for v in p)+'}'
    lines = ['// Generated by tools/coo/extract_eater_bindings.py. Do not edit by hand.',
             '#pragma once', '#include <array>', '#include <span>', '#include <string_view>',
             '#include "../coo/executor.h"', '#include "../coo/dialogue_service.h"',
             'namespace dawn::state::activity::eater_of_worlds {',
             'inline constexpr std::uint32_t kScenario=0x80B49E7AU,kRoot=0xD6E30062U,kBank=0x80F1F9A5U;',
             'inline constexpr std::string_view kPackage="raid_envy_v310";',
             'struct AssetBinding {std::string_view name;coo::Asset asset;std::uint32_t offset,component,sense,authority;};',
             'inline constexpr AssetBinding kAssets[]{']
    def slots(g):
        by = {(d['type'], d['index']):d for d in g['descriptors']}
        for s in g['declaredSlots']:
            d=by.get((s['type'],s['index']),{})
            yield s,d
    for g in data['groups']:
        for s,d in slots(g):
            values=','.join(hx(d.get(k,0xFFFFFFFF)) for k in ('sourceOffset','componentClass','senseSchema','authSchema'))
            lines.append(f'    {{{q(d.get("name",""))},{{{hx(g["registryKey"])},{hx(d.get("sourceTag",0xFFFFFFFF))},{s["type"]},{s["index"]}}},{values}}},')
    lines+=['};','constexpr std::size_t asset_index(coo::Asset a) noexcept {for(std::size_t i=0;i<std::size(kAssets);++i) if(kAssets[i].asset==a) return i;return std::size(kAssets);}',
            'constexpr const AssetBinding* find(std::uint32_t key,std::uint16_t type,std::uint16_t slot) noexcept {for(const auto& a:kAssets) if(a.asset.registry==key && a.asset.type==type && a.asset.slot==slot) return &a;return nullptr;}',
            'struct Slot {std::uint8_t type,flags;std::uint16_t index;std::uint32_t tag,offset,component,sense,auth;};',
            'struct Group {std::uint32_t key,tag,bubblesMask;std::span<const Slot> slots;};']
    masks={}
    for g in data['groups']:
        key=number(g['registryKey'])
        memberships=[b['ordinal'] for b in data['bubbles'] if any(number(o['registryKey'])==key for st in b['states'] for o in st['objects'])]
        masks[key]=sum(1<<b for b in set(memberships))
    for i,g in enumerate(data['groups']):
        lines.append(f'inline constexpr std::array<Slot,{len(g["declaredSlots"])}> kSlots{i}{{{{')
        for s,d in slots(g):
            flags=(1 if number(d.get('senseSchema',0xFFFFFFFF)) != 0xFFFFFFFF else 0) | (2 if number(d.get('authSchema',0xFFFFFFFF)) != 0xFFFFFFFF else 0)
            lines.append('    {'+f'{s["type"]},{flags},{s["index"]},'+','.join(hx(d.get(k,0xFFFFFFFF)) for k in ('sourceTag','sourceOffset','componentClass','senseSchema','authSchema'))+'},')
        lines.append('}};')
    lines+=['inline constexpr Group kGroups[]{']
    for i,g in enumerate(data['groups']):lines.append(f'    {{{hx(g["registryKey"])},{hx(g["objectTag"])},{masks[number(g["registryKey"])]}U,kSlots{i}}},')
    lines+=['};','struct Point {float x,y,z;};','struct Volume {coo::Asset asset;Point min,max;std::span<const Point> vertices;};']
    for i,v in enumerate(data['volumes']):lines.append(f'inline constexpr Point kVertices{i}[]{{'+','.join(point(p) for p in v['vertices'])+'};')
    lines+=['inline constexpr Volume kVolumes[]{']
    for i,v in enumerate(data['volumes']):lines.append(f'    {{{{{hx(v["registry"])},{hx(v["tag"])},60,{v["slot"]}}},{point(v["min"])},{point(v["max"])},kVertices{i}}},')
    lines+=['};','struct Trigger {std::uint32_t registry;std::uint16_t type,slot;std::uint32_t volumeRegistry;std::uint16_t volumeSlot;};','inline constexpr Trigger kTriggers[]{']
    for m in data['monitors']:lines.append(f'    {{{hx(m["registry"])},{m["type"]},{m["slot"]},{hx(m["targetRegistry"])},{m["targetSlot"]}}},')
    lines+=['};','struct Spawn {coo::Asset asset;std::uint16_t rule,objective;std::uint8_t categories,count,taskCount;bool sceneOwned;};','inline constexpr Spawn kSpawns[]{']
    for s in data['sources']:
        # Solo combat assignment policy, using only exact authored objective IDs.
        # Task rows are selected exclusively from the native reachability reports.
        n=len(s['categories']);assert 1<=n<=16
        key=number(s['registryKey']);slot=s['slot']
        finale=key==0x686321C8 and slot in (18,19,20)
        # The final ship drops use holdout_door_objective (five native task rows).
        # holdout_objective's only area is the previous island, not the landing deck.
        if key==0x686321C8: objective=22 if finale else 3 if slot<=21 else 26 if slot<=25 else 33 if slot<=32 else 37
        elif key==0x91264981: objective=98 if slot<=103 else 104 if slot<=109 else 110
        elif key==0xE8D290A0: objective=0 if slot==3 else 17 if slot<=21 else 33 if slot<=37 else 49 if slot<=53 else 122 if slot<=127 else 133 if slot<=132 else 139
        else: raise AssertionError('No exact native combat objective for source')
        assert any(number(g['registryKey'])==key and any(d['type']==3 and d['index']==objective for d in g['descriptors']) for g in data['groups'])
        task_count=5 if finale else 24
        lines.append(f'    {{{{{hx(s["registryKey"])},{hx(s["sourceTag"])},1,{slot}}},{s["ruleSlot"]},{objective},{n},{n},{task_count},{str(s["name"].endswith("sq_boss")).lower()}}},')
    lines+=['};','constexpr std::size_t spawn_index(std::uint32_t key,std::uint16_t slot) noexcept {for(std::size_t i=0;i<std::size(kSpawns);++i) if(kSpawns[i].asset.registry==key && kSpawns[i].asset.slot==slot) return i;return std::size(kSpawns);}',
            'inline constexpr std::uint32_t kObjectives[]{'+','.join(hx(o['eventHash']) for o in data['objectives'])+'};',
            'inline constexpr coo::DialogueRow kDialogueRows[]{']
    # DialogueRow's fields are selector,durationMs (verified by shared service declaration).
    for d in data['dialogue']:lines.append(f'    {{{hx(d["selector"])},{d["durationMs"]}U,0,false}},')
    lines+=['};','} // namespace dawn::state::activity::eater_of_worlds','']
    return '\n'.join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--cached',action='store_true',help='Render saved extraction without reading packages.')
    parser.add_argument('--catalog-only',action='store_true',help='Regenerate base evidence/catalog before deriving health bindings and the capability profile.')
    args=parser.parse_args()
    data=json.loads(EVIDENCE.read_text()) if args.cached else recover()
    evidence=json.dumps(data,indent=2)+'\n'
    catalog=render(data)
    outputs=[(EVIDENCE,evidence),(OUTPUT,catalog)]
    if not args.catalog_only: outputs.append((PROFILE,render_profile(data)))
    for path,content in outputs:
        if args.check: assert path.read_text(encoding='utf-8')==content,str(path)
        else:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(content,encoding='utf-8',newline='\n')
    print(json.dumps(dict(groups=len(data['groups']),volumes=len(data['volumes']),triggers=len(data['monitors']),sources=len(data['sources']))))


def render_profile(data):
    health = json.loads((EVIDENCE.parent/'health-bindings.json').read_text(encoding='utf-8'))
    assert json.dumps(data,indent=2)+'\n' == EVIDENCE.read_text(encoding='utf-8'), 'Regenerate the runtime catalog before its health-derived capabilities.'
    assert health['runtimeBindingsSha256'] == hashlib.sha256(EVIDENCE.read_bytes()).hexdigest(), 'Regenerate Eater health bindings against the current runtime catalog.'
    health_sources = {(number(b['registry']),number(b['source']),b['slot']) for b in health['bindings']}
    assert len(health_sources)==201
    carry = json.loads((EVIDENCE.parent/'carry-bindings.json').read_text(encoding='utf-8'))
    assert carry['runtimeBindingsSha256'] == hashlib.sha256(EVIDENCE.read_bytes()).hexdigest(), 'Regenerate Eater carry bindings against the current runtime catalog.'
    carry_sources = {(number(b['registry']),number(b['source']),b['slot']) for b in carry['bindings']}
    assert len(carry_sources)==120
    stations = json.loads((EVIDENCE.parent/'station-bindings.json').read_text(encoding='utf-8'))
    assert stations['runtimeBindingsSha256'] == hashlib.sha256(EVIDENCE.read_bytes()).hexdigest(), 'Regenerate Eater station bindings against the current runtime catalog.'
    station_sources = {(number(b['registry']),number(b['source']),b['slot']) for b in stations['bindings']}
    assert len(station_sources)==18
    prefixes={0xA9E6185F:'entrance',0xB270AC62:'berth',0x686321C8:'reactor',0x5654D7FD:'reactor_entry',
              0x93BF5E9D:'traversal',0x91264981:'barrier',0xE8D290A0:'argos'}
    lines=['// Generated capability identities. Native handlers still validate current ownership.',
           '#pragma once','#include "catalog.h"','#include "../coo/mission_script.h"',
           'namespace dawn::state::activity::eater_of_worlds {',
           'inline constexpr coo::Asset kModule{kRoot,kScenario,57,0};',
           'inline constexpr coo::Asset kRegion{kRoot,kScenario,57,1};',
           'inline constexpr coo::DialogueDefinition kDialogue{kBank,kDialogueRows,{}};',
           'inline constexpr coo::script::Capability kCapabilities[]{',
           '    {"mission.module","composition",{coo::Operation::mechanic,kModule,1,coo::Wait::requested}},',
           '    {"mission.checked","composition",{coo::Operation::observation,kModule,0,coo::Wait::observed}},',
           '    {"mission.finish","*",{coo::Operation::complete,kModule,0,coo::Wait::requested}},',
           '    {"respawn.allow","*",{coo::Operation::mechanic,kModule,2,coo::Wait::requested}},',
           '    {"respawn.restrict","*",{coo::Operation::mechanic,kModule,3,coo::Wait::requested}},',
           '    {"reactor.exit.open","*",{coo::Operation::mechanic,kModule,113,coo::Wait::requested}},',
           '    {"reactor.exit.opened","*",{coo::Operation::observation,kModule,114,coo::Wait::observed}},',
           '    {"argos.arrival.begin","*",{coo::Operation::mechanic,kModule,115,coo::Wait::requested}},',
           '    {"argos.arrival.landed","*",{coo::Operation::observation,kModule,116,coo::Wait::observed}},',
           '    {"traversal.exit.opened","*",{coo::Operation::observation,{0x93BF5E9DU,0x80B49F18U,23,2},1,coo::Wait::observed}},',
           '    {"traversal.ejection.opened","*",{coo::Operation::observation,{0x93BF5E9DU,0x80B49EF7U,23,0},1,coo::Wait::observed}},']
    def cap(name,operation,asset,argument=0,wait='requested'):
        lines.append(f'    {{{json.dumps(name)},"*",{{coo::Operation::{operation},{asset},{argument}U,coo::Wait::{wait}}}}},')
    for i,n in enumerate(('arrival','cavern','entrance','belly_approach','descent','thunderwall','belly','mouth')):
        cap('region.'+n,'observation','kRegion',i*8,'observed')
    for i,o in enumerate(data['objectives']):cap(f'objective.{i}','objective','{kRoot,0x8155C30FU,68,0}',number(o['eventHash']))
    for d in data['dialogue']:
        cap(f'dialogue.{d["row"]}','dialogue','{kRoot,0x8155C312U,53,2}',d['row'])
        cap(f'dialogue.finished.{d["row"]}','observation','{kRoot,0x8155C312U,53,2}',d['row'],'observed')
    for name,arg in [('reactor.path1',101),('reactor.path2',102),('reactor.path3',103),('reactor.path4',104),('barrier.cycle',110),('argos.cycle',111)]:
        cap(name+'.begin','mechanic','kModule',arg)
        cap(name+'.finished','observation','kModule',arg,'observed')
    for g in data['groups']:
        key=number(g['registryKey']);prefix=prefixes.get(key,f'group_{key:08X}')
        for d in g['descriptors']:
            name=prefix+'.'+d['name']
            asset=f'{{{hx(key)},{hx(d["sourceTag"])},{d["type"]},{d["index"]}}}'
            if d['type']==1:
                cap(name+'.request','population',asset,1)
                cap(name+'.ready','observation',asset,1,'observed')
                cap(name+'.cleared','observation',asset,2,'observed')
            elif d['type'] in (4,23,43):
                for n in (0,1):cap(name+('.on' if n else '.off'),'device',asset,n)
                if d['type']==4:
                    cap(name+'.ready','observation',asset,1,'observed')
                    if (key,number(d['sourceTag']),d['index']) in health_sources:
                        cap(name+'.destroyed','observation',asset,2,'observed')
                    if (key,number(d['sourceTag']),d['index']) in carry_sources:
                        cap(name+'.carried','observation',asset,3,'observed')
                        cap(name+'.dropped','observation',asset,4,'observed')
                    if (key,number(d['sourceTag']),d['index']) in station_sources:
                        cap(name+'.used','observation',asset,5,'observed')
            elif d['type'] in (30,31):cap(name,'observation',asset,0,'observed')
    for v in data['volumes']:
        prefix=prefixes.get(v['registry'],f'group_{v["registry"]:08X}')
        cap(prefix+'.'+v['name'],'observation',f'{{{hx(v["registry"])},0xFFFFFFFFU,60,{v["slot"]}}}',0,'observed')
    lines+=['};','inline constexpr coo::script::ModuleCapability kModules[]{{"mission",{kModule,1}}};',
            'inline constexpr coo::script::FactCapability kFacts[]{{"mission.checked",0}};',
            'inline constexpr coo::script::ParameterCapability kParameters[]{',
            '    {"platform_hold_ms",250,3000,500,false},',
            '    {"convergence_window_ms",15000,180000,90000,false},',
            '    {"damage_window_ms",15000,90000,45000,false},',
            '};',
            'inline constexpr coo::script::Profile kProfile{"eater_of_worlds.native.v1","otherMissions",coo::Schema::otherMissions,kCapabilities,kModules,kFacts,kDialogue,kObjectives,{},{},{},kParameters};',
            'bool valid_document(const coo::script::Views&) noexcept;',
            '} // namespace dawn::state::activity::eater_of_worlds','']
    return '\n'.join(lines)
if __name__=='__main__':main()
