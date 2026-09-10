"""Read-only native-template and tactical-task evidence for a bounded Vex probe.

This proves available authored choices, not the original host's request count or
source-to-tactical-row schedule. No live process is opened.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def u32(b, p):
    return struct.unpack_from('<I', b, p)[0]


def u64(b, p):
    return struct.unpack_from('<Q', b, p)[0]


def rel(b, p):
    return p + struct.unpack_from('<q', b, p)[0]


def array(b, cls):
    hits=[p for p in range(8,len(b)-4,4) if u32(b,p)==cls]
    assert len(hits)==1,(hex(cls),hits)
    mark=hits[0]
    return u64(b,mark-8),mark+8


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();sys.path.insert(0,str(args.reader_dir))
    from pkg import Reader
    reader=Reader();records={}
    for tag in (0x80F5B797,0x80F5B77F,0x80F5B78E,0x80F5B785):
        b,cls=reader.read_tag(tag)
        records[tag]=(b,cls)
    source=records[0x80F5B77F][0]
    assert records[0x80F5B77F][1]==0x80809C36
    assert u32(source,0x724)==0x8080948F
    assert source[0x758:0x760]==struct.pack('<IHH',0xEB1E8934,1,0)
    assert source[0x7C0:0x7C8]==struct.pack('<IHH',0xEB1E8934,66,5)
    assert source[0x7C8:0x7D0]==struct.pack('<IHH',0xEB1E8934,66,6)
    categories,member=array(source,0x80808356)
    assert categories==1 and u32(source,member)==0x19C57E65
    variants=[]
    for variant in range(6):
        descriptor=member+8+variant*16
        count=u64(source,descriptor)
        assert count==1
        # 4EAE40: entry = array + relativeOffset + 18; weight at entry+C.
        entry=rel(source,descriptor+8)+16
        assert u32(source,entry-8)==0x80808358
        weight=u32(source,entry+12);entity=rel(source,entry)
        assert weight==1 and u32(source,entity-4)==0x808099D8
        assert u32(source,entity)==0x80C0D08A
        variants.append({'variant':variant,'choices':count,'weight':weight,
                         'template_entity':'80C0D08A','entry_offset':entry})
    objective=records[0x80F5B78E][0]
    groups,group_start=array(objective,0x80807D8F)
    assert groups==2
    rows=[]
    for row in range(groups):
        group=group_start+row*40
        task_count=u64(objective,group+16)
        assert task_count==1
        task=rel(objective,group+24)+16
        assert u32(objective,task-8)==0x80807D95
        key,kind,index=struct.unpack_from('<IBxH',objective,task+32)
        assert key==0xEB1E8934 and kind==45
        rows.append({'row':row,'tasks':task_count,'task_offset':task,
                     'firing_area_set':{'registry':f'{key:08X}','type':kind,'slot':index}})
    assert [r['firing_area_set']['slot'] for r in rows]==[4,12]
    firing_area=records[0x80F5B785][0]
    assert firing_area[0x1E0:0x1E8]==struct.pack('<IHH',0xEB1E8934,44,7)
    assert firing_area[0x290:0x298]==struct.pack('<IHH',0xEB1E8934,45,4)
    assert firing_area[0x2A8:0x2B0]==struct.pack('<IHH',0xEB1E8934,45,12)
    result={'probe':'native Vex center-left-b source 0; explicit policy, not retail scheduling',
            'registry':'EB1E8934','object':'80F5B797','source':'80F5B77F',
            'source_primary_rule_slot':5,'source_fallback_rule_slot':6,
            'category_count':categories,'variants':variants,'objective':'80F5B78E',
            'related_firing_area':{'definition':'80F5B785','type':44,'slot':7,'firing_area_sets':[4,12]},
            'objective_rows':rows,'retail_initial_request_count':None,
            'retail_source_tactical_row':None,
            'native_consumers':['4EAE40','4E2A90','4EC710','AB5810','A9AD70','4E2C40'],
            'records':[{'tag':f'{tag:08X}','class':f'{cls:08X}','size':len(b),
                        'sha256':hashlib.sha256(b).hexdigest().upper()}
                       for tag,(b,cls) in records.items()]}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('Vex probe evidence: one category, six one-choice weight-1 templates, two scoped tactical task rows; retail count/assignment unresolved')


if __name__=='__main__':
    main()
