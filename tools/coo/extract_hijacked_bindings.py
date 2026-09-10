"""Recover Hijacked evidence from installed packages, without changing runtime files."""
import hashlib
import json
import re
import struct
from pathlib import Path

import package_read as packages
from extract_gateway_bindings import array, strings, u32, i64, sources
from extract_deadly_trial_bindings import walk

OUT = packages.ROOT / 'build/coo/hijacked-research'
SCENARIO = 0x80B4206A
BANK = 0x80F5C3A2


def recover_groups(result):
    # Cache 53 added one byte to InvestmentConstants in Header. Record layouts are unchanged.
    layout = json.loads((packages.ROOT / 'build/coo/gateway-research/cache-layout.json').read_text())['records']['RosterGroupRecord']
    section = json.loads((packages.ROOT / 'build/coo/gateway-research/cache-sections.json').read_text())['RosterGroupRecord']
    cache = (packages.ROOT / 'Sunrise/cache/build_data.bin').read_bytes()
    assert u32(cache, 8) == 53
    wanted = {o['tag'] for r in result['regions'] for o in r['objects'] if (o['array'] in (0,2))}
    # Exclude destination ambient roots: the mission root and universal game system own this profile.
    wanted -= {0x80C4C0AE, 0x80C4C0BE}
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
    assert out
    return out

def discover_bank(result):
    sensor=next(s for g in result['groups'] if g['registry']==0x77852DB9 for s in g['slots'] if s['slotTypes']==53)
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
            if not 0x80F5C000 <= container < 0x80F5E000:
                continue
            text = strings(container).get(key)
            if text and text not in texts:
                texts.append(text)
        result['dialogue'].append({'row': row, 'selector': selector,
            'durationMs': round(struct.unpack_from('<f', bank, o + 4)[0] * 1000), 'texts': texts})
    tags = set(range(0x80B42000, 0x80B42ADC))
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
    assert result['scenario'] == SCENARIO and result['packageHash'] == 0x77852DB9
    assert any('Entangled' in str(d) for d in result['dialogue'])


def count_plan(data):
    """Validate explicit reconstruction counts against recovered native categories."""
    plan=json.loads(Path(__file__).with_name('hijacked_squad_counts.json').read_text())
    assert plan['version']==1
    rows={(int(r['registry'],16),r['source']):r for r in plan['sources']}
    assert len(rows)==len(plan['sources'])==len(data['sources'])
    for source in data['sources']:
        row=rows[source['registry'],source['slot']]
        assert int(row['definition'],16)==source['tag'] and row['name']==source['name']
        assert [int(c,16) for c in row['categories']]==[c['category'] for c in source['categories']]
        assert len(row['requested'])==len(source['categories']) in (1,2)
        assert all(isinstance(n,int) and 0<n<=63 for n in row['requested'])
        assert sum(row['requested'])<=16, 'Count exceeds per-source native receipt ledger capacity'
    for key in ((0x153E22CD,21),(0x3E9B74F3,18),(0xD997395E,35)):
        assert rows[key]['requested']==[1], 'An explicit individual member must remain single'
    return rows


def tactics(data):
    import math
    slots={(g['registry'],s['slotTypes'],s['slotIndices']):s for g in data['groups'] for s in g['slots']}
    providers={}
    counts=count_plan(data)
    for reg,tag in ((0x153E22CD,0x80B421A5),(0x3E9B74F3,0x80B4261B),(0xD997395E,0x80B42945)):
        _,blob=packages.read(tag)
        areas={struct.unpack_from('<Q',blob,p)[0]:{'slot':u32(blob,p+16),'min':struct.unpack_from('<3f',blob,p+32),'max':struct.unpack_from('<3f',blob,p+48)} for p in array(blob,0x200,128,0x80808354)}
        providers[reg]={u32(blob,p+12):[areas[struct.unpack_from('<Q',blob,q)[0]] for q in array(blob,p+16,16)] for p in array(blob,0x210,32,0x80808350)}
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
                ar+=providers[reg][slot]
            if ar:rows.append((index,ar))
        fallback=source['ruleRegistry']!=reg or source['ruleType']!=66
        rule=next(s for (rr,kind,_),s in slots.items() if rr==reg and kind==66 and s.get('name')==('sq_final_harpy_spawnrule' if source['name'].startswith('sq_final_harpy') else source['name']+'_spawnrule')) if fallback else slots[reg,66,source['ruleSlot']]
        _,rb=packages.read(rule['descriptorTags'])
        headers=[m.start() for m in re.finditer(re.escape(struct.pack('<I',0x80809845)),rb)]
        assert len(headers)==1
        at=headers[0]; n=struct.unpack_from('<Q',rb,at-8)[0]
        guids=[struct.unpack_from('<Q',rb,at+8+j*8)[0] for j in range(n)]
        guid=guids[0]; placements=[p for guid in guids for p in points.get(guid,[])]
        missing_placement=not placements
        unique=set(tuple(p['position']) for p in placements)
        xyz=[sum(p[i] for p in unique)/len(unique) for i in range(3)] if unique else [0.0,0.0,0.0]
        def rank(row):
            idx,areas=row
            return min((sum(max(lo-x,0,x-hi)**2 for x,lo,hi in zip(xyz,a['min'],a['max'])),sum((x-(lo+hi)/2)**2 for x,lo,hi in zip(xyz,a['min'],a['max'])),idx) for a in areas)
        assert rows,(source['name'],task_index)
        ranked=sorted((rank(row),row[0]) for row in rows) if not missing_placement else [(('unresolved placement; first native row policy',),rows[0][0])]
        cohort=(1 if ss<3 else 2 if ss<11 else 3 if ss<20 else 4) if reg==0x153E22CD else (5 if ss<8 else 6) if reg==0x3E9B74F3 else 7 if ss<4 else 8 if ss<20 else 9
        count=counts[reg,ss]
        joins.append({'registry':reg,'source':ss,'definition':source['tag'],'offset':source['offset'],'categories':len(source['categories']),'count':sum(count['requested']),'requested':count['requested'],'countBasis':count['basis'],'countReason':count['reason'],'rule':rule['slotIndices'],'ruleFallback':fallback,'group':task_index,'row':ranked[0][1],'cohort':cohort,'required':cohort in (4,9),'guid':guid,'placements':placements,'rankedRows':ranked})
    return {'policy':'Counts use the explicit user-directed plan in tools/coo/hijacked_squad_counts.json; retail script counts remain unconfirmed. Tactical rows use nearest native bounds, then center, then row. Missing direct rules use exact authored source-name spawnrule match.','joins':joins,'providers':providers}

def ai_text(report):
    h=lambda v:f'0x{v:08X}U'
    lines=['// Generated by extract_hijacked_bindings.py; see tactical-joins.json for policy.','#pragma once','#include "catalog.h"','#include "../coo/native_combatant_authority.h"','namespace sunrise::state::activity::hijacked {','struct Spawn { std::uint16_t source;std::uint32_t registry,definition,offset;std::uint16_t rule;std::uint8_t count,categories,cohort;coo::native_combatant::TacticalGroup tactical;bool required;std::array<std::uint8_t,2> requested; };','inline constexpr std::array<Spawn,'+str(len(report['joins']))+'> kSpawns{{']
    for s in report['joins']:
        requested=s["requested"]+[0]*(2-len(s["requested"]))
        lines.append(f'    {{{s["source"]},{h(s["registry"])},{h(s["definition"])},{s["offset"]},{s["rule"]},{s["count"]},{s["categories"]},{s["cohort"]},{{{h(s["registry"])},{s["group"]},{s["row"]}}},{str(s["required"]).lower()},{{{requested[0]},{requested[1]}}}}},')
    lines+=['}};','constexpr const Spawn* spawn(std::uint32_t registry,std::uint16_t slot) noexcept { for(const auto& s:kSpawns) { if(s.registry==registry && s.source==slot) { return &s; } } return nullptr; }', 'constexpr coo::native_combatant::TacticalGroup tactical_group(coo::Asset source) noexcept { const auto* s=source.type==1?spawn(source.registry,source.slot):nullptr;return s?s->tactical:coo::native_combatant::TacticalGroup{}; }','}\n']
    return '\n'.join(lines)


def catalog_text(data,digest):
    from generate_beyond_infinity_catalog import render
    return render(data,digest).replace('generate_beyond_infinity_catalog.py','extract_hijacked_bindings.py').replace('beyond_infinity','hijacked').replace('0x80F46015U','0x80B4206AU').replace('0x03632571U','0x77852DB9U').replace('0x80F1FDF7U','0x80F5C3A2U').replace('adventure_vod','adventure_rumba').replace('std::array<Objective,11>','std::array<Objective,8>').replace('std::array<DialogueMetadata,49>','std::array<DialogueMetadata,14>')


def native_text(data):
    hx=lambda n:f'0x{n:08X}U'
    lines=['// Generated by extract_hijacked_bindings.py; native package identities.', '#pragma once','#include "catalog.h"','namespace sunrise::state::activity::hijacked {',
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
    lines+=['};','inline constexpr std::int16_t kActivity=297;','inline constexpr std::uint32_t kInvestment=0x83211FEDU,kActivityTag=0x80B4200FU,kLaunchTag=0x80FB5018U;',
      'inline constexpr std::uint8_t kBubbleCount=45;','}\n']
    return '\n'.join(lines)


def boss_evidence():
    _,entity=packages.read(0x80F58FEF)
    resources=[u32(entity,o) for o in array(entity,0x10,12)]
    components=[(0x81578D59,0x80806832,0x7D8),(0x81578D5C,0x808036CF,0x2828),
        (0x8162C3A2,0x80803640,0x2008),(0x8162C3A3,0x808069EE,0xE40),
        (0x80F58FEE,0x80806751,0x188),(0x80F2FD41,0x80803A00,0xE78)]
    for tag,kind,offset in components:
        assert tag in resources
        _,blob=packages.read(tag)
        assert u32(blob,offset-4)==kind and u32(blob,offset)==tag
    _,health=packages.read(0x815B5AA2)
    body=array(health,0x1338+0x1B0,0x138)[0]
    assert body==0x1A30 and u32(health,body+0x10)==0x6DFE676D and u32(health,body+0xD0)==0
    _,lookup=packages.read(0x80F2FD3F)
    group=array(lookup,0x20,32)[0]
    assert u32(lookup,group+12)==0x1F992208
    sequences=[u32(lookup,a) for a in array(lookup,group+16,4)]
    assert sequences[6]==0xCBFDCA32
    _,points=packages.read(0x80B421AA)
    header=points.index(struct.pack('<I',0x8080916B))+8
    destinations=[]
    for i,slot in enumerate((54,57)):
        row=header+i*48
        assert u32(points,row+4)==slot
        destinations.append({'pointSlot':slot,'position':struct.unpack_from('<3f',points,row+32),
            'quaternion':struct.unpack_from('<4f',points,row+16)})
    # Join the activity definition to this package before looking up its public
    # identity. Row296 is Tree of Probabilities even though a destination override
    # can still load Nessus; checking a public hash alone missed that mismatch.
    _,definitions=packages.read(0x81327CD8)
    joined=[]
    for row in array(definitions,8,72,0x80807A35):
        for item in array(definitions,row+56,80,0x80807A3A):
            if u32(definitions,item+24)==0x77852DB9:
                joined.append(struct.unpack_from('<h',definitions,item+18)[0])
    assert joined==[297],f'Hijacked package maps to unexpected activity rows: {joined}'
    _,public=packages.read(0x81327CF0)
    assert u32(public,array(public,8,16)[joined[0]])==0x83211FED
    _,activity=packages.read(0x80B4200F)
    assert u32(activity,8)==0x77852DB9 and u32(activity,0x40)==SCENARIO
    cls,launch=packages.read(0x80FB5018)
    assert cls==0x80809BA3 and u32(launch,16)==0x80FB501A
    return {'entity':0x80F58FEF,'components':[{'tag':t,'kind':k,'offset':o} for t,k,o in components],
        'healthDefinition':0x815B5AA2,'healthDefinitionOffset':0x1338,'bodyRegionOffset':body,'bodyRegionHash':0x6DFE676D,'bodyRegionIndex':0,
        'sequenceTable':0x80F2FD3F,'teleportGroup':0,'teleportSequence':6,'sequenceHashes':sequences,
        'pointTable':0x80B421AA,'destinations':destinations,
        'activityOrdinal':297,'investment':0x83211FED,'launchTag':0x80FB5018,
        'activityDefinitionTable':0x81327CD8,'activityPackageHash':0x77852DB9,'packageJoinedActivityRows':joined,
        'fidelity':'Native identities and target points recovered. Health retreat thresholds and schedule are authored reconstruction; native arrival must be observed independently. No standalone core pickup source was recovered.'}


def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    data=recover()
    blob=json.dumps(data,indent=2)+'\n'
    tactical=tactics(data)
    tactical['policy']+=' Missing source placement for cave source 7 uses its first recovered native tactical row; this reconstruction is unverified. Cohorts and required flags are authored scheduling suggestions, not recovered retail script.'
    outputs={OUT/'native-bindings.json':blob,OUT/'tactical-joins.json':json.dumps(tactical,indent=2)+'\n',OUT/'boss-native.json':json.dumps(boss_evidence(),indent=2)+'\n'}
    directory=packages.ROOT/'Sunrise/src/state/activity/hijacked'
    outputs[directory/'catalog.h']=catalog_text(data,hashlib.sha256(blob.encode()).hexdigest())
    outputs[directory/'native_catalog.h']=native_text(data)
    outputs[directory/'ai_bindings.h']=ai_text(tactical)
    for path,content in outputs.items():
        if args.check:
            assert path.read_text(encoding='utf-8')==content,f'Extraction differs: {path}'
        else:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(content,encoding='utf-8')
    print('PASS: Hijacked native extraction and generated catalogs',len(data['groups']),'groups',len(data['sources']),'sources',len(data['volumes']),'volumes')

if __name__=='__main__':main()
