"""Read-only package geometry and captured-log evidence for Mercury's ambient probe."""
import argparse,collections,hashlib,json,math,struct,sys
from pathlib import Path

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--reader-dir',type=Path,required=True);ap.add_argument('--log',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
 args=ap.parse_args();sys.path.insert(0,str(args.reader_dir))
 from pkg import Reader
 from arrays import resolve_descriptor
 r=Reader();records=[]
 def read(tag):
  b,c=r.read_tag(tag);assert c==0x80809C36
  records.append({'tag':f'{tag:08X}','class':f'{c:08X}','bytes':len(b),'sha256':hashlib.sha256(b).hexdigest().upper()})
  return b
 def u32(b,o):return struct.unpack_from('<I',b,o)[0]
 def vec(b,o,n=3):return list(struct.unpack_from('<'+'f'*n,b,o))
 def text(b,o):
  start=o+struct.unpack_from('<q',b,o)[0];return b[start:b.index(0,start)].decode()
 monitor=read(0x80F5B788)
 assert u32(monitor,0x214)==0x80809530
 assert monitor[0x248:0x250]==struct.pack('<IHH',0xEB1E8934,30,3)
 assert monitor[0x270:0x278]==struct.pack('<IHH',0xEB1E8934,60,10)
 volume=read(0x80F5B778);gp=0xA70;pp=0xA40
 assert u32(volume,gp-8)==0x808099D0 and u32(volume,pp-8)==0x80809A6D
 assert volume[pp+8:pp+16]==volume[gp+12:gp+20]==struct.pack('<IHH',0xEB1E8934,60,10)
 assert u32(volume,pp+16)==u32(volume,gp+20)==15
 name=text(volume,gp);assert name==text(volume,pp)=='pf_lighthouse_vx_center_left_b._trigger_volume'
 va=resolve_descriptor(volume,gp+0xD0);ta=resolve_descriptor(volume,gp+0xE0)
 assert va[0]==4 and va[2]==0x80800094 and ta[0]==2 and ta[2]==0x80809B92
 verts=[vec(volume,va[1]+i*16,4) for i in range(va[0])]
 tris=[list(volume[ta[1]+i*3:ta[1]+i*3+3]) for i in range(ta[0])]
 low=vec(volume,gp+0xB0,4);high=vec(volume,gp+0xC0,4)
 assert all(v[3]==1 for v in verts) and all(i<4 for tri in tris for i in tri)
 def inside(p):
  for tri in tris:
   vs=[verts[i] for i in tri]
   cross=[(vs[(i+1)%3][0]-vs[i][0])*(p[1]-vs[i][1])-(vs[(i+1)%3][1]-vs[i][1])*(p[0]-vs[i][0]) for i in range(3)]
   if min(cross)>=-1e-6 or max(cross)<=1e-6:return True
  return False
 landmarks=[]
 for tag,label in [(0x80F5BF33,'Public-event flag'),(0x80F5B960,'The Up and Up'),(0x80F5B956,'Bug in the System'),(0x80F5B959,'The Runner')]:
  b=read(tag);pos=vec(b,0x5A0)
  landmarks.append({'name':label,'definition':f'{tag:08X}','position_offset':'5A0','xyz':pos,'inside_footprint':inside(pos)})
 flag=landmarks[0]['xyz'];distances=[]
 for i in range(4):
  a=verts[i];b=verts[(i+1)%4];dx=b[0]-a[0];dy=b[1]-a[1]
  t=max(0,min(1,((flag[0]-a[0])*dx+(flag[1]-a[1])*dy)/(dx*dx+dy*dy)))
  distances.append(math.hypot(flag[0]-(a[0]+t*dx),flag[1]-(a[1]+t*dy)))
 assert not inside(flag) and inside([210,310])
 import re
 lines=args.log.read_text(encoding='utf-8',errors='replace').splitlines();objects=collections.Counter();packets=collections.Counter();special=[]
 for n,line in enumerate(lines,1):
  if 'stage=sensor_sense_entry ' in line:
   m=re.search(r'key=(\S+) slot=(\S+)',line)
   if m:objects[m.groups()]+=1
  if 'stage=sensor_sense_update ' in line:
   m=re.search(r'result=(\S+)',line);packets[m.group(1)]+=1
  if 'ev=native_activity ' in line or 'ev=activity_ambient ' in line:special.append({'line':n,'text':line})
 result={'name':name,'monitor':{'definition':'80F5B788','registry':'EB1E8934','type':30,'slot':3,'target_type':60,'target_slot':10},
 'geometry':{'definition':'80F5B778','placement_offset':f'{pp:X}','record_offset':f'{gp:X}','bubble':15,'aabb_min':low,'aabb_max':high,'vertices_xyzw':verts,'triangles':tris,'extrusion':vec(volume,gp+0xF0,1)[0],'origin_xyz':vec(volume,gp+0x110),'interior_reference_xy':[210,310]},
 'landmarks':landmarks,'rally_flag_distance_to_footprint_xy':min(distances),'records':records,
 'log':{'path':str(args.log),'sha256':hashlib.sha256(args.log.read_bytes()).hexdigest().upper(),'packets_by_result':dict(packets),'sense_entries':sum(objects.values()),'probe_objects':{slot:count for (key,slot),count in objects.items() if key=='0xEB1E8934'},'monitor_entries':{f'{key}/{slot}':count for (key,slot),count in objects.items() if slot.startswith('30/')},'activity_messages':special},
 'limits':['No recorded player positions establish whether the trigger was entered.','The public flag location is outside the actual footprint.','No game process accessed or native/game memory modified.','Reference XY lies inside package geometry; traversability/current player height are not inferred.']}
 args.output.mkdir(parents=True,exist_ok=True);(args.output/'monitor-evidence.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 # Native-coordinate diagram with exact package landmarks; no map/compass inference.
 xmin,xmax,ymin,ymax=115,365,185,380
 def xy(p):return (70+(p[0]-xmin)*3.4,700-(p[1]-ymin)*3.0)
 svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="820" viewBox="0 0 1120 820">',
 '<rect width="1120" height="820" fill="#f8fafc"/>','<g font-family="Arial,sans-serif" fill="#172033">',
 '<text x="48" y="42" font-size="27" font-weight="bold">Mercury ambient probe: exact trigger footprint</text>',
 '<text x="48" y="72" font-size="16">Native world X/Y coordinates; labels are authored flag positions.</text>']
 for x in range(125,351,25):
  px,_=xy([x,ymin]);svg += [f'<line x1="{px}" y1="115" x2="{px}" y2="700" stroke="#dbe3ec"/>',f'<text x="{px}" y="724" text-anchor="middle" font-size="13">{x}</text>']
 for y in range(200,376,25):
  _,py=xy([xmin,y]);svg += [f'<line x1="70" y1="{py}" x2="920" y2="{py}" stroke="#dbe3ec"/>',f'<text x="59" y="{py+4}" text-anchor="end" font-size="13">{y}</text>']
 points=' '.join(f'{x:.2f},{y:.2f}' for x,y in map(xy,verts))
 svg += [f'<polygon points="{points}" fill="#14b8a6" fill-opacity="0.22" stroke="#0f766e" stroke-width="3"/>']
 tx,ty=xy([210,310]);svg += [f'<circle cx="{tx}" cy="{ty}" r="7" fill="#0f766e"/>',f'<text x="{tx}" y="{ty-44}" text-anchor="middle" font-size="18" font-weight="bold">Vex monitor 30/3</text>',f'<text x="{tx}" y="{ty-22}" text-anchor="middle" font-size="15">Interior reference X=210, Y=310</text>']
 colors=['#b45309','#475569','#475569','#475569']
 for n,row in enumerate(landmarks):
  x,y=xy(row['xyz']);dx,dy=[(15,21),(15,20),(15,-16),(15,20)][n]
  svg += [f'<circle cx="{x}" cy="{y}" r="6" fill="{colors[n]}"/>',f'<text x="{x+dx}" y="{y+dy}" font-size="15" font-weight="bold">{row["name"]}</text>',f'<text x="{x+dx}" y="{y+dy+18}" font-size="12">{row["xyz"][0]:.1f}, {row["xyz"][1]:.1f}, z {row["xyz"][2]:.1f}</text>']
 svg += ['<text x="70" y="764" font-size="16">Trigger Z range: 79.584 to 124.584. Public-event flag is outside, about 24.0 units from the edge.</text>',
 '<text x="70" y="791" font-size="14">Cross the shaded footprint on foot and confirm native selected-player reports; this drawing does not prove entry.</text>',
 '<text x="946" y="707" font-size="16">World X</text>','<text x="26" y="102" font-size="16">World Y</text>','</g></svg>']
 (args.output/'trigger-location.svg').write_text('\n'.join(svg),encoding='utf-8')
 print(json.dumps({'volume':name,'vertices':verts,'triangles':tris,'interior':[210,310],'z_range':[low[2],high[2]],'flag_distance_xy':min(distances),'log':result['log']},indent=2))
if __name__=='__main__':main()
