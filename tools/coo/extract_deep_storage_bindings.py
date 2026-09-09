"""Recover Deep Storage evidence from installed packages, without changing runtime files."""
import hashlib
import json
import re
import struct
from pathlib import Path

import package_read as packages
from extract_gateway_bindings import array, strings, u32, i64, sources
from extract_deadly_trial_bindings import walk

OUT = packages.ROOT / 'build/coo/deep-storage-research'
SCENARIO = 0x80B5606D
BANK = 0x80F1EEB6


def recover_groups(result):
    # Cache 53 added one byte to InvestmentConstants in Header. Record layouts are unchanged.
    layout = json.loads((packages.ROOT / 'build/coo/gateway-research/cache-layout.json').read_text())['records']['RosterGroupRecord']
    section = json.loads((packages.ROOT / 'build/coo/gateway-research/cache-sections.json').read_text())['RosterGroupRecord']
    cache = (packages.ROOT / 'Sunrise/cache/build_data.bin').read_bytes()
    assert u32(cache, 8) == 53
    wanted = {o['tag'] for r in result['regions'] for o in r['objects'] if (o['array']==0 or r['bubble'] in (4,19))}
    # Exclude destination ambient roots: the mission root and universal game system own this profile.
    wanted -= {0x80C4042E, 0x80C40434}
    out=[]
    for index in range(section['count']):
        start=section['offset']+1+index*layout['size']; data=cache[start:start+layout['size']]
        registry,tag,count=struct.unpack_from('<IIH',data)
        if tag not in wanted: continue
        slots=[]
        for j in range(count):
            slot={}
            for name in ('slotTypes','slotFlags','slotIndices','descriptorTags','descriptorOffsets','componentClasses','senseSchemas','authSchemas'):
                offset,size=layout['fields'][name];stride=size//1280
                slot[name]=int.from_bytes(data[offset+j*stride:offset+(j+1)*stride],'little')
            if slot['descriptorTags']!=0xFFFFFFFF:
                _,blob=packages.read(slot['descriptorTags']);off=slot['descriptorOffsets']
                assert u32(blob,off+48)==registry
                assert struct.unpack_from('<H',blob,off+54)[0]==slot['slotIndices']
                at=off+0x50+i64(blob,off+0x50)
                slot['name']=blob[at:].split(b'\0',1)[0].decode(errors='replace') if 0<=at<len(blob) else ''
            slots.append(slot)
        out.append({'registry':registry,'objectTag':tag,'cacheIndex':index,'slots':slots})
    assert len(out)==11, len(out)
    return out

def discover_bank(result):
    sensor=next(s for g in result['groups'] if g['registry']==0xE6E910D2 for s in g['slots'] if s['slotTypes']==53)
    _,b=packages.read(sensor['descriptorTags']);matches=[]
    for o in range(sensor['descriptorOffsets'],len(b)-3,4):
        tag=u32(b,o)
        if not 0x80800000<=tag<0x82000000:continue
        try:cls,_=packages.read(tag)
        except (AssertionError,ValueError,FileNotFoundError):continue
        if cls==0x80808D54:matches.append(tag)
    assert len(set(matches))==1,matches
    return matches[0]


def recover():
    result = walk(SCENARIO)
    result['groups'] = recover_groups(result)
    result['sources'] = sources(result['groups'])
    result['dialogueBank'] = discover_bank(result)
    result['dialogue'] = []
    bank_tag = result['dialogueBank']
    _, bank = packages.read(bank_tag)
    roots = {u32(bank, o): o + 8 + i64(bank, o + 8) for o in array(bank, 24, 16)}
    starts = sorted(roots.values()) + [len(bank)]
    for row, o in enumerate(array(bank, 8, 8)):
        selector = u32(bank, o)
        start = roots[selector]
        end = starts[starts.index(start) + 1]
        texts = []
        for at in range(start, end - 7, 4):
            container, key = struct.unpack_from('<II', bank, at)
            if not 0x80F1E000 <= container < 0x80F20000:
                continue
            text = strings(container).get(key)
            if text and text not in texts:
                texts.append(text)
        result['dialogue'].append({'row': row, 'selector': selector,
            'durationMs': round(struct.unpack_from('<f', bank, o + 4)[0] * 1000), 'texts': texts})
    tags = set(range(0x80B56019, 0x80B56AB2))
    tags.add(result['dialogueBank'])
    tags.update(s['descriptorTags'] for g in result['groups'] for s in g['slots'] if s['descriptorTags'] != 0xFFFFFFFF)
    result['tags'] = []
    result['objectives'] = []
    result['volumes'] = []
    for tag in sorted(tags):
        cls, data = packages.read(tag)
        names = [m.group().decode() for m in re.finditer(rb'[a-z][a-z_0-9.\[\]]{8,}', data)]
        result['tags'].append({'tag': tag, 'class': cls, 'sha256': hashlib.sha256(data).hexdigest(), 'names': names})
        if cls == 0x80804F72:
            for index, at in enumerate(array(data, 8, 40)):
                result['objectives'].append({'tag': tag, 'row': index, 'event': u32(data, at),
                    'texts': [[strings(u32(data, a + j)).get(u32(data, a + j + 4), '')
                               for j in (0, 8, 16, 24)] for a in array(data, at + 16, 32)]})
        for at in range(12, len(data) - 0x114, 4):
            if struct.unpack_from('<H', data, at + 4)[0] != 60:
                continue
            base = at - 12
            name_at = base + i64(data, base)
            if not 0 <= name_at < len(data):
                continue
            name = data[name_at:].split(b'\0', 1)[0].decode(errors='replace')
            if not re.fullmatch(r'[a-z_0-9.\[\]]+', name):
                continue
            try:
                vertices = [struct.unpack_from('<3f', data, o) for o in array(data, base + 0xD0, 16, 0x80800094)]
            except (AssertionError, struct.error):
                continue
            result['volumes'].append({'tag': tag, 'offset': base, 'registry': u32(data, at),
                'slot': struct.unpack_from('<H', data, at + 6)[0], 'name': name,
                'min': struct.unpack_from('<3f', data, base + 0xB0),
                'max': struct.unpack_from('<3f', data, base + 0xC0), 'vertices': vertices})
    owned = {g['registry'] for g in result['groups']}
    result['volumes'] = [v for v in result['volumes'] if v['registry'] in owned]
    result['provenance'] = {'method': 'Installed package and version-53 cache extraction; no live observations',
        'scenario': f'{SCENARIO:08X}', 'dialogueBank': f"{result['dialogueBank']:08X}",
        'cacheSha256': hashlib.sha256((packages.ROOT / 'Sunrise/cache/build_data.bin').read_bytes()).hexdigest()}
    validate(result)
    return result


def validate(result):
    assert result['scenario'] == SCENARIO and result['packageHash'] == 0xE6E910D2
    assert result['objectives'][0]['texts'][0][0] == 'Steal a map of the Infinite Forest'


def tactics(data):
    import math
    slots={(g['registry'],s['slotTypes'],s['slotIndices']):s for g in data['groups'] for s in g['slots']}
    _,blob=packages.read(0x80B565ED)
    areas={struct.unpack_from('<Q',blob,p)[0]:{'slot':u32(blob,p+16),'min':struct.unpack_from('<3f',blob,p+32),'max':struct.unpack_from('<3f',blob,p+48)} for p in array(blob,0x200,128,0x80808354)}
    providers={u32(blob,p+12):[areas[struct.unpack_from('<Q',blob,q)[0]] for q in array(blob,p+16,16)] for p in array(blob,0x210,32,0x80808350)}
    points={}
    for tag in data['tags']:
        _,blob=packages.read(tag['tag'])
        for m in re.finditer(re.escape(struct.pack('<II',0x304,0)),blob):
            at=m.start()-0x68
            if at<0 or at%16 or at+0x78>len(blob):continue
            xyz=struct.unpack_from('<3f',blob,at+0x20)
            if u32(blob,at+0x2C)!=0x3F800000 or not all(math.isfinite(x) and abs(x)<10000 for x in xyz):continue
            guid=struct.unpack_from('<Q',blob,at+0x70)[0]
            points.setdefault(guid,[]).append({'tag':tag['tag'],'offset':at,'position':xyz})
    joins=[]
    for source in data['sources']:
        reg=source['registry'];ss=source['slot']
        task_index=max(slot for rr,kind,slot in slots if rr==reg and kind==3 and slot<ss)
        task=slots[reg,3,task_index];_,b=packages.read(task['descriptorTags']);rows=[]
        for index,p in enumerate(array(b,task['descriptorOffsets']+0x88,40)):
            ar=[]
            for q in array(b,p+16,40):
                rr,kind,slot=struct.unpack_from('<IHH',b,q+32);assert rr==reg and kind==45
                ar+=providers[slot]
            if ar:rows.append((index,ar))
        fallback=source['ruleRegistry']!=reg or source['ruleType']!=66
        rule=next(s for (rr,kind,_),s in slots.items() if rr==reg and kind==66 and s.get('name')==source['name']+'_spawnrule') if fallback else slots[reg,66,source['ruleSlot']]
        _,rb=packages.read(rule['descriptorTags'])
        headers=[m.start() for m in re.finditer(re.escape(struct.pack('<I',0x80809845)),rb)]
        assert len(headers)==1
        at=headers[0]; n=struct.unpack_from('<Q',rb,at-8)[0]
        guids=[struct.unpack_from('<Q',rb,at+8+j*8)[0] for j in range(n)]
        guid=guids[0]; placements=[p for guid in guids for p in points.get(guid,[])]
        assert placements,(source['name'],hex(guid))
        unique=set(tuple(p['position']) for p in placements)
        xyz=[sum(p[i] for p in unique)/len(unique) for i in range(3)]
        def rank(row):
            idx,areas=row
            return min((sum(max(lo-x,0,x-hi)**2 for x,lo,hi in zip(xyz,a['min'],a['max'])),sum((x-(lo+hi)/2)**2 for x,lo,hi in zip(xyz,a['min'],a['max'])),idx) for a in areas)
        assert rows,(source['name'],task_index)
        ranked=sorted((rank(row),row[0]) for row in rows)
        cohort=(next((i for i in range(1,7) if f'plat0{i}' in source['name']),1) if ss<24 else 7 if ss==25 else 8 if ss<29 else 9 if ss<35 else 10 if ss<44 else 11 if ss<51 else 12 if ss<72 else 13)
        joins.append({'registry':reg,'source':ss,'definition':source['tag'],'offset':source['offset'],'categories':len(source['categories']),'count':len(source['categories']),'rule':rule['slotIndices'],'ruleFallback':fallback,'group':task_index,'row':ranked[0][1],'cohort':cohort,'required':cohort in (8,9,10,12,13),'guid':guid,'placements':placements,'rankedRows':ranked})
    return {'policy':'Counts request one actor per native category; retail script counts are unconfirmed. Tactical rows use nearest native bounds, then center, then row. Missing direct rules use exact authored source-name spawnrule match.','joins':joins,'providers':providers}

def ai_text(report):
    h=lambda v:f'0x{v:08X}U'
    lines=['// Generated by extract_deep_storage_bindings.py; see tactical-joins.json for policy.','#pragma once','#include "catalog.h"','#include "../coo/native_combatant_authority.h"','namespace sunrise::state::activity::deep_storage {','struct Spawn { std::uint16_t source;std::uint32_t registry,definition,offset;std::uint16_t rule;std::uint8_t count,categories,cohort;coo::native_combatant::TacticalGroup tactical;bool required; };','inline constexpr std::array<Spawn,72> kSpawns{{']
    for s in report['joins']:
        lines.append(f'    {{{s["source"]},{h(s["registry"])},{h(s["definition"])},{s["offset"]},{s["rule"]},{s["count"]},{s["categories"]},{s["cohort"]},{{{h(s["registry"])},{s["group"]},{s["row"]}}},{str(s["required"]).lower()}}},')
    lines+=['}};','constexpr const Spawn* spawn(std::uint32_t registry,std::uint16_t slot) noexcept { for(const auto& s:kSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr; }', 'constexpr coo::native_combatant::TacticalGroup tactical_group(coo::Asset source) noexcept { const auto* s=source.type==1?spawn(source.registry,source.slot):nullptr;return s?s->tactical:coo::native_combatant::TacticalGroup{}; }','}\n']
    return '\n'.join(lines)


def catalog_text(data, digest):
    from generate_beyond_infinity_catalog import render
    result = render(data,digest).replace('generate_beyond_infinity_catalog.py','extract_deep_storage_bindings.py').replace('beyond_infinity','deep_storage').replace('0x80F46015U','0x80B5606DU').replace('0x03632571U','0xE6E910D2U').replace('0x80F1FDF7U','0x80F1EEB6U').replace('adventure_vod','adventure_whisk')
    return result.replace('std::array<Objective,11>','std::array<Objective,10>').replace('std::array<DialogueMetadata,49>','std::array<DialogueMetadata,15>')

def native_text(data):
    hx=lambda n:f'0x{n:08X}U'
    lines=['// Generated by extract_deep_storage_bindings.py; native package identities.', '#pragma once','#include "catalog.h"','namespace sunrise::state::activity::deep_storage {',
      'struct Slot { std::uint8_t type,flags; std::uint16_t index; std::uint32_t tag,offset,component,sense,auth; };',
      'struct Group { std::uint32_t key,tag; std::size_t hint; bool topLevel; std::uint8_t bubble; std::span<const Slot> slots; };']
    for i,g in enumerate(data['groups']):
        lines.append(f'inline constexpr Slot kSlots{i}[]{{')
        for s in g['slots']:lines.append('    {'+','.join(str(s[k])+'U' for k in ('slotTypes','slotFlags','slotIndices','descriptorTags','descriptorOffsets','componentClasses','senseSchemas','authSchemas'))+'},')
        lines.append('};')
    lines.append('inline constexpr Group kGroups[]{')
    for i,g in enumerate(data['groups']):
        memberships=[(r['bubble'],o['array']) for r in data['regions'] for o in r['objects'] if o['registry']==g['registry']]
        top=all(a==0 for _,a in memberships);assert top or len(memberships)==1
        lines.append(f'    {{{hx(g["registry"])},{hx(g["objectTag"])},{g["cacheIndex"]},{str(top).lower()},{memberships[0][0]},kSlots{i}}},')
    lines+=['};','struct SourceBinding { coo::Asset asset; std::uint16_t rule; std::uint8_t categories; bool sceneOwned,hasRule; };','inline constexpr SourceBinding kSources[]{']
    for s in data['sources']:
        has=s['ruleRegistry']==s['registry'] and s['ruleType']==66
        lines.append(f'    {{{{{hx(s["registry"])},{hx(s["tag"])},1,{s["slot"]}}},{s["ruleSlot"] if has else 0},{len(s["categories"])},false,{str(has).lower()}}},')
    lines+=['};','inline constexpr std::int16_t kActivity=295;','inline constexpr std::uint32_t kInvestment=0x550500EEU,kActivityTag=0x80B56019U,kLaunchTag=0x80F9F35EU;',
      'inline constexpr std::uint8_t kBubbleCount=22;','}\n']
    return '\n'.join(lines)


def recover_scan_durations():
    # E4BBB0: base 80804D5A+24, then nonnegative 80804D39+20 wins.
    out=[]
    for source,component,duration in ((0x80B5609E,0x8156EFA4,5.0),(0x80B5645C,0x8157E6B1,9.0)):
        _,base=packages.read(component);_,override=packages.read(source)
        assert u32(base,0x6AC)==0x80804D5A
        default=struct.unpack_from('<f',base,0x6D4)[0];assert default==3.0
        assert u32(override,0x614)==0x80804D39
        effective=struct.unpack_from('<f',override,0x638)[0];assert effective==duration
        out.append({'source':source,'component':component,'defaultRecordClass':0x80804D5A,
            'defaultDurationOffset':0x6D4,'defaultDuration':default,'overrideClass':0x80804D39,
            'overrideClassOffset':0x614,'overrideRecordOffset':0x618,'overrideDurationOffset':0x638,
            'sourceDuration':effective})
    return out


def mechanisms_text():
    # Exact final-room source -> entity -> health/graph join, shared native box ABI.
    _,lens_source=packages.read(0x80B568A6);assert u32(lens_source,0x580)==0x80F4803C
    _,lens_entity=packages.read(0x80F4803C)
    lens_resources=[u32(lens_entity,r) for r in array(lens_entity,0x10,12)]
    assert all(tag in lens_resources for tag in (0x80F48026,0x80F48031,0x80C7063B))
    _,lens_health=packages.read(0x80F48026)
    assert u32(lens_health,0xB04)==0x80804B8A and u32(lens_health,0xB08)==0x80F48026
    # Semantic labels are intentionally not guessed from anonymous graph predicates.
    device_rows=json.loads((OUT/'device-resources.json').read_text())
    ranges=json.loads((OUT/'device-position-ranges.json').read_text())
    h=lambda n:f'0x{n:08X}U'
    from generate_beyond_infinity_catalog import floating
    lines=['// Generated package mode ranges. Mode direction still needs native/live verification.', '#pragma once','#include "catalog.h"','namespace sunrise::state::activity::deep_storage {',
      'struct DeviceRange { std::uint32_t graph,offset,nameHash;float min,max; };']
    for i,item in enumerate(ranges):
        native=next(d for d in device_rows if d['name']==item['name'])
        _,placement=packages.read(native['tag']);assert struct.unpack_from('<Q',placement,native['offset'])[0]==native['guid']
        entity=u32(placement,native['offset']-0x70) if native['class']==0x808099D6 or u32(placement,native['offset']-8)==0x304 else u32(placement,0x580)
        assert entity==native['entity']
        _,entity_data=packages.read(entity)
        resource_tags=[u32(entity_data,r) for r in array(entity_data,0x10,12)]
        lines.append(f'inline constexpr std::array<DeviceRange,{len(item["ranges"])}> kDeviceRanges{i}{{{{')
        for v in item['ranges']:
            assert v['graph'] in resource_tags
            _,b=packages.read(v['graph']);assert struct.pack('<I',0x6D408B83) in b
            assert struct.unpack_from('<2f',b,v['offset'])==(v['min'],v['max'])
            assert u32(b,v['offset']-0x28)==0x10006
            lines.append('    {'+','.join([h(v['graph']),h(v['offset']),h(v['nameHash']),floating(v['min']),floating(v['max'])])+'},')
        lines.append('}};')
    lines+=['struct DeviceModes { std::string_view name;std::uint32_t entity;std::span<const DeviceRange> ranges; };','inline constexpr DeviceModes kDeviceModes[]{']
    for i,item in enumerate(ranges):lines.append(f'    {{{json.dumps(item["name"])},{h(item["entity"])},kDeviceRanges{i}}},')
    lines+=['};', '// All three authored plate objects select the same entity and native components.',
      'inline constexpr std::uint32_t kPlateEntity=0x80C6BE61U,kPlateTimer=0x815B8B3BU,kPlateDevice=0x80C7063BU;',
      'inline constexpr std::uint32_t kPlateTimerOffset=0x248U,kPlateDeviceOffset=0xA78U;',
      'inline constexpr std::uint32_t kLensEntity=0x80F4803CU,kLensHealth=0x80F48026U,kLensHealthKind=0x80804B8AU,kLensHealthOffset=0xB08U,kLensGraph=0x80F48031U;',
      'struct GhostScanBinding { coo::Asset source,sensor;std::uint32_t entity,component;std::uint32_t componentOffset,sensorOffset;float defaultDuration,sourceDuration; };',
      '// Source overrides are metadata; runtime receipts use live controller +0x290.',
      'inline constexpr std::uint32_t kScanOverrideClass=0x80804D39U,kScanOverrideClassOffset=0x614U,kScanOverrideDurationOffset=0x638U;',
      'inline constexpr GhostScanBinding kGhostScans[]{',
      '    {{0xA13D8A45U,0x80B5609EU,4,2},{0x4324A238U,0x80B56174U,65,0},0x80F4B127U,0x8156EFA4U,0x358U,0x258U,3.F,5.F},',
      '    {{0x59700FA7U,0x80B5645CU,4,95},{0xEA42F517U,0x80B5654EU,65,0},0x80F4B586U,0x8157E6B1U,0x358U,0x258U,3.F,9.F},',
      '};', '// Candidate values within recovered modes; these names do not assert visual direction.',
      'inline constexpr float kWarpMode1=.1F,kWarpMode2=.2F,kBarrierMode0=0.F,kBarrierModeHalf=.5F,kBarrierMode1=1.F;',
      '}\n']
    recover_scan_durations()
    return '\n'.join(lines)


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Compare fresh extraction to saved evidence without writing.')
    args = parser.parse_args()
    result = recover()
    result['scanDurations'] = recover_scan_durations()
    tactical=tactics(result)
    target = OUT / 'native-bindings.json'
    if args.check:
        assert json.loads(target.read_text(encoding='utf-8')) == json.loads(json.dumps(result)), 'Saved extraction differs from installed packages.'
        destination=packages.ROOT / 'Sunrise/src/state/activity/deep_storage'
        blob=target.read_bytes()
        assert (destination/'catalog.h').read_text(encoding='utf-8')==catalog_text(result,hashlib.sha256(blob).hexdigest())
        assert (destination/'native_catalog.h').read_text(encoding='utf-8')==native_text(result)
        assert (destination/'ai_bindings.h').read_text(encoding='utf-8')==ai_text(tactical)
        assert (destination/'mechanism_catalog.h').read_text(encoding='utf-8')==mechanisms_text()
        assert json.loads((OUT/'tactical-joins.json').read_text())==json.loads(json.dumps(tactical))
        print('PASS: catalog, native roster, source tactical joins and evidence match current packages/cache')
        return
    OUT.mkdir(parents=True, exist_ok=True)
    blob=json.dumps(result, indent=2) + '\n'
    target.write_bytes(blob.encode('utf-8'))
    destination=packages.ROOT / 'Sunrise/src/state/activity/deep_storage'
    destination.mkdir(parents=True,exist_ok=True)
    (destination/'catalog.h').write_text(catalog_text(result,hashlib.sha256(blob.encode()).hexdigest()),encoding='utf-8')
    (destination/'native_catalog.h').write_text(native_text(result),encoding='utf-8')
    (OUT/'tactical-joins.json').write_text(json.dumps(tactical,indent=2)+'\n',encoding='utf-8')
    (destination/'ai_bindings.h').write_text(ai_text(tactical),encoding='utf-8')
    (destination/'mechanism_catalog.h').write_text(mechanisms_text(),encoding='utf-8')
    for key in ('groups', 'sources', 'dialogue', 'objectives', 'volumes', 'tags'):
        print(key, len(result[key]))
    for row in result['objectives']:
        print('objective', f"{row['event']:08X}", row['texts'])


if __name__ == '__main__':
    main()


