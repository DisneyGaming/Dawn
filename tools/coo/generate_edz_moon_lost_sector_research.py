"""Emit package-pinned research data for the EDZ and Moon Lost Sectors.

This is an evidence generator, not production catalog generation. Public names
are joined to native bubbles by unique authored geography, faction and role
strings; every native tag/slot/rule is re-read from the installed package set.
"""
from __future__ import annotations
import hashlib,json,os,struct,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/coo'))
import generate_open_world_profiles as gen
import package_read
import spawn_count_policy as counts

OUT=ROOT/'tools/coo/lost_sector_edz_moon_native_research.json'

SECTORS=(
 ('edz','The Drain',0x80B2F00A,4,0x8BD467FD,0x80BE32D7,(2,3,4,5,6,8),6,7,0x483ECB05,0x80BE3307,0,0x80BE46BB,()),
 ('edz','Whispered Falls',0x80B2F00A,5,0x7812AC0B,0x80BE337C,(*range(2,15),16),14,15,0xEFA1A332,0x80BE33A0,0,0x80BE47E4,()),
 ('edz',"Scavenger's Den",0x80B2F00A,6,0xF2657D67,0x80BE360A,(2,*range(4,17)),2,3,0x25DAB52F,0x80BE376B,0,0x80BE491F,()),
 ('edz','Flooded Chasm',0x80B2F00A,11,0x93B73C14,0x80BE471B,(2,4,*range(5,20)),2,3,0xAF744687,0x80BE4A10,0,0x80BE57B9,()),
 ('edz','The Pit',0x80B2F00A,13,0x0A9DD720,0x80BE625E,(*range(20),21),19,20,0x3FB2129A,0x80BE626F,0,0x80BE66EA,()),
 ('edz','Excavation Site XII',0x80B2F00A,14,0x2D7F849F,0x80BE628F,(0,1,2,3,4,6,7,8,9),4,5,0xB84F6D0D,0x80BE631C,0,0x80BE689E,()),
 ('edz',"Pathfinder's Crash",0x80B2F00A,15,0x5A4528B7,0x80BE6408,(*range(18),19),17,18,0x6E1DE930,0x80BE6419,0,0x80BE6B05,()),
 ('edz','The Weep',0x80B2F00A,36,0x8B4E2F5A,0x80BE7A42,(*range(8),9,*range(10,13)),7,8,0x30089412,0x80BE7AC9,0,0x80BE7D0D,()),
 ('edz','Skydock IV',0x80B2F00A,40,0x3626E372,0x80BE7D97,(2,4,*range(5,12)),2,3,0xEDFD4CF4,0x80BE811F,0,0x80BE80EF,()),
 ('edz','The Quarry',0x80B2F00A,41,0x56DD3ADB,0x80BE8256,(*range(7),8),6,7,0x932878DB,0x80BE8348,0,0x80BE83A9,()),
 ('edz','Atrium',0x80B2F00A,52,0x153CE263,0x80BE988B,tuple(range(2,15)),14,15,0x3B0AD52A,0x80BE98A8,0,0x80BE98EB,()),
 ('edz','Terminus East',0x80B2F00A,53,0xAB2744A0,0x80BE991D,(5,7,*range(8,21)),5,6,0x78BD5EDD,0x80BE99D1,0,0x80BE9B0C,()),
 ('edz',"Widow's Walk",0x80B2F00A,54,0x9C6C36EA,0x80BE9A3D,(*range(2,14),15),13,14,0x5D6EEB00,0x80BE9B07,0,0x80BE9D00,()),
 ('edz','Cavern of Souls',0x80B2F00A,57,0x21A8C5A7,0x80C26C53,(0,*range(2,8)),0,1,0x49AAD69C,0x80C272B5,0,0x80C2771D,()),
 ('edz','Hallowed Grove',0x80B2F00A,58,0x09B8910E,0x80C2738E,tuple(range(15)),14,None,0x16663203,0x80C273CD,0,0x80C27968,()),
 ('edz','Shaft 13',0x80B2F00A,59,0xBF13BAEB,0x80C2743C,(*range(12),13),11,12,0x7B16F536,0x80C2745E,0,0x80C279FF,()),
 ('moon','K1 Logistics',0x81503E69,1,0xC7C90DFD,0x815679F7,(0,1,2,3,4,5,*range(12,25),26,27,28,*range(31,41),42,59,64),42,43,0xF8E40F4D,0x81567B34,0,0x81567B22,()),
 ('moon','K1 Revelation',0x81503E69,4,0xADE66EB0,0x8157011D,(*range(1,11),*range(12,16),*range(18,65),80),64,65,0x4DC11B40,0x8157022B,0,0x81570219,()),
 ('moon','K1 Crew Quarters',0x81503E69,10,0x848F9F5C,0x81571311,(*range(7,14),15,16,*range(18,32),33,34,*range(36,44),47),43,44,0x7C4F624C,0x81571412,1,0x81571403,()),
 ('moon','K1 Communion',0x81503E69,21,0xD97A4D7A,0x81572BD3,(*range(1,9),*range(12,15),*range(16,34),*range(37,42),*range(45,61),62,*range(67,88)),60,61,0xD26F406A,0x81572CE7,0,0x81572CD5,()),
)

def category_rows(group,slot):
    src=next(x for x in group['slots'] if x['type']==1 and x['index']==slot)
    cls,raw=package_read.read(src['descriptor']); body=gen.relative(raw,24)
    rows=gen.array_rows(raw,body+0xA8,104,0x80808356)
    result=[]
    for ci,category in enumerate(rows):
        choices=[]
        for variant in range(6):
            for at in gen.array_rows(raw,category+8+variant*16,24,0x80808358):
                weight=gen.u32(raw,at+12)
                if not weight: continue
                eb=gen.relative(raw,at); entity=gen.u32(raw,eb); sel=gen.relative(raw,eb+0x78)
                if gen.u32(raw,sel-4) in (0x80807EB6,0x80804B8B): sel=gen.relative(raw,sel)
                attrs={}
                if gen.u32(raw,sel-4)==0x808038A0:
                    for a in gen.array_rows(raw,sel+16,8,0x8080389F): attrs[gen.u32(raw,a)]=gen.u32(raw,a+4)
                types,_=gen.template_type_evidence(entity); selected=attrs.get(0x26170C92)
                if selected is not None: types=frozenset((selected,)) if selected in types else frozenset()
                choices.append({'variant':variant,'weight':weight,'entity':f'{entity:08X}',
                    'species':counts.SPECIES_BY_HASH.get(next(iter(types))) if len(types)==1 else None,
                    'rank':counts.RANK_BY_HASH.get(attrs.get(0xB10F785D))})
        target,policy=counts.category_target(f'{group["key"]:08X}:{slot}:{ci}',choices)
        result.append({'index':ci,'estimated_target':target,'policy':policy,'choices':choices})
    return src,raw,result

def main():
    scenarios={}
    for sc in sorted({x[2] for x in SECTORS}): scenarios[sc]=gen.scenario_objects(sc)
    out={'schema':1,'purpose':'package-pinned research; target counts are reconstruction estimates, not retail quotas',
         'native_category_schema':{'source_class':'80809C36','definition_class':'8080948F','categories_offset':'definition+0xA8','category_class':'80808356','category_stride':104},'sectors':[]}
    for ns,name,sc,bubble,key,obj,sources,boss,member,ckey,cobj,cslot,cdesc,omitted in SECTORS:
        hashes,objects=scenarios[sc]
        group=next(g for t in objects[bubble] if (g:=gen.resolve_group(t)) and g['key']==key)
        assert group['object']==obj and group['mask']==1<<bubble
        chest=next(g for t in objects[bubble] if (g:=gen.resolve_group(t)) and g['key']==ckey)
        actual_chest_desc=next(s for s in chest['slots'] if s['index']==cslot)['descriptor']
        assert chest['object']==cobj, (name, f'{chest["object"]:08X}')
        rows=[]
        for slot in sources:
            try: src,raw,cats=category_rows(group,slot)
            except StopIteration as error: raise AssertionError((name,'missing source',slot)) from error
            if slot==boss:
                for i,c in enumerate(cats): c['estimated_target']=1 if i==0 else 0;c['policy']='boss-singleton' if i==0 else 'boss-secondary-dormant'
            role=rule=None
            for candidate in ('primary','fallback'):
                try: rule=gen.source_spawn_rule(group,slot,candidate);role=candidate;break
                except ValueError: pass
            rslot=next((s for s in group['slots'] if s['type']==66 and s['index']==rule),None)
            rraw=package_read.read(rslot['descriptor'])[1] if rslot else b''
            member_slot=next((s for s in group['slots'] if s['type']==2 and s['index']==slot+1),None)
            rows.append({'slot':slot,'descriptor':f'{src["descriptor"]:08X}','descriptor_sha256':hashlib.sha256(raw).hexdigest(),
                         'rule_role':role or 'implicit_no_rule','rule_slot':rule or 0,
                         'rule_descriptor':f'{rslot["descriptor"]:08X}' if rslot else None,
                         'rule_sha256':hashlib.sha256(rraw).hexdigest() if rslot else None,
                         'named_member_slot':member_slot['index'] if member_slot else None,
                         'named_member_descriptor':f'{member_slot["descriptor"]:08X}' if member_slot else None,
                         'categories':cats,'boss':slot==boss})
        tacticals=[{'slot':s['index'],'descriptor':f'{s["descriptor"]:08X}'} for s in group['slots'] if s['type']==3]
        out['sectors'].append({'namespace':ns,'name':name,'scenario':f'{sc:08X}','bubble':bubble,'bubble_hash':f'{hashes[bubble]:08X}',
            'encounter_object':f'{obj:08X}','registry':f'{key:08X}','one_stage_all_sources':True,'sources':rows,
            'boss':{'source_slot':boss,'member_slot':member},'tactical_candidates':tacticals,
            'chest':{'object':f'{cobj:08X}','registry':f'{ckey:08X}','slot':cslot,'descriptor':f'{actual_chest_desc:08X}'},
            'excluded_overlay_sources':[{'slot':x,'reason':'co-resident seasonal Nightmare generator; kept out of ordinary Lost Sector roster'} for x in omitted]})
    OUT.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8')
    print(OUT)

if __name__=='__main__': main()
