#!/usr/bin/env python3
"""Author the original 'Starling' pony as a compact, static 1997-style asset.

Only Python's standard library is needed. This is asset authoring, not an
executable build. Coordinates are right-handed, Y up, muzzle toward +Z.
Disconnected intersecting low-poly shells deliberately mimic period props;
this is a display mesh, not a watertight printable or animation-ready mesh.
One flat-colour indexed atlas and baked directional ramps avoid normals,
lighting shaders, alpha sorting, or multiple texture binds on the target.
"""
import json, math
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
V, F = [], []
BASE = [(91,191,184),(28,47,91),(83,103,173),(218,247,234),(130,93,193),(13,22,42),(255,255,244),(235,174,193),(66,133,142),(255,211,112)]
names = ['coat','mane','mane_highlight','eye_white','iris','ink','sparkle','inner_ear','hoof','star']
def add(p): V.append(tuple(p)); return len(V)-1
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def sub(a,b): return tuple(a[i]-b[i] for i in range(3))
def tri(a,b,c,col,center=None):
    n=cross(sub(V[b],V[a]),sub(V[c],V[a]))
    if center and sum(n[i]*(sum(V[j][i] for j in (a,b,c))/3-center[i]) for i in range(3))<0: b,c=c,b
    F.append((a,b,c,col))
def ell(center,scale,col,seg=10,rings=5):
    top=add((center[0],center[1]+scale[1],center[2])); rows=[]
    for j in range(1,rings):
        p=math.pi*j/rings
        rows.append([add((center[0]+scale[0]*math.sin(p)*math.cos(2*math.pi*k/seg),center[1]+scale[1]*math.cos(p),center[2]+scale[2]*math.sin(p)*math.sin(2*math.pi*k/seg))) for k in range(seg)])
    bot=add((center[0],center[1]-scale[1],center[2]))
    for k in range(seg):
        q=(k+1)%seg; tri(top,rows[0][k],rows[0][q],col,center); tri(bot,rows[-1][q],rows[-1][k],col,center)
    for a,b in zip(rows,rows[1:]):
        for k in range(seg):
            q=(k+1)%seg; tri(a[k],b[k],a[q],col,center); tri(a[q],b[k],b[q],col,center)
def vertical_tube(rings,col,seg=6):
    rows=[]
    for x,y,z,rx,rz in rings:
        rows.append([add((x+rx*math.cos(k*2*math.pi/seg),y,z+rz*math.sin(k*2*math.pi/seg))) for k in range(seg)])
    for j,(a,b) in enumerate(zip(rows,rows[1:])):
        center=tuple((rings[j][i]+rings[j+1][i])/2 for i in range(3))
        for k in range(seg):
            q=(k+1)%seg;tri(a[k],a[q],b[k],col,center);tri(a[q],b[q],b[k],col,center)
    for row,r,sign in ((rows[0],rings[0],-1),(rows[-1],rings[-1],1)):
        c=add(r[:3]);center=(r[0],r[1]-sign*.1,r[2])
        for k in range(seg): tri(c,row[k],row[(k+1)%seg],col,center)
def disk(center,u,v,rx,ry,col,seg=12):
    c=add(center); ring=[add(tuple(center[i]+u[i]*rx*math.cos(k*2*math.pi/seg)+v[i]*ry*math.sin(k*2*math.pi/seg) for i in range(3))) for k in range(seg)]
    for k in range(seg):tri(c,ring[k],ring[(k+1)%seg],col)
# Rounded large head and compact barrel; legs widen into stylized hooves.
ell((0,1.27,-.20),(.53,.57,.95),0,12,5)
ell((0,1.83,.53),(.40,.66,.42),0,10,4)
ell((0,2.45,.75),(.64,.66,.62),0,12,6)
ell((0,2.17,1.22),(.43,.28,.40),0,10,4)
for x in (-.33,.33):
    for z in (-.78,.47):
        vertical_tube([(x,.07,z+.05,.25,.27),(x,.24,z+.02,.24,.25),(x,.75,z,.16,.18),(x,1.25,z,.22,.24)],0)
        vertical_tube([(x,.065,z+.05,.252,.272),(x,.16,z+.04,.249,.269)],8)
# Pointed pony ears: a solid wedge with a small pink inner triangle.
for s in (-1,1):
    pts=[(s*.34,2.89,.57),(s*.63,2.89,.70),(s*.49,3.42,.62),(s*.43,2.90,.39)]
    ids=[add(p) for p in pts];center=tuple(sum(p[i] for p in pts)/4 for i in range(3))
    for a,b,c in [(0,1,2),(0,3,1),(0,2,3),(1,3,2)]:tri(ids[a],ids[b],ids[c],0,center)
    a,b,c=[add(p) for p in [(s*.39,2.98,.641),(s*.55,2.98,.714),(s*.48,3.27,.663)]]
    tri(a,b,c,7,center)
# Chunky swoop forelock, back mane and curved tapering tail.
ell((-.08,2.98,.73),(.55,.22,.46),1,10,4)
ell((-.39,2.80,1.03),(.25,.35,.22),1,8,4)
vertical_tube([(-.32,1.24,.00,.16,.22),(-.39,1.65,.02,.22,.28),(-.38,2.11,.14,.24,.29),(-.30,2.63,.35,.25,.31),(-.12,3.01,.57,.32,.27)],1,7)
vertical_tube([(-.22,.42,-1.48,.10,.12),(-.27,.64,-1.65,.24,.23),(-.20,1.02,-1.67,.30,.25),(-.08,1.40,-1.42,.29,.28),(0,1.51,-1.02,.18,.24)],1,7)
vertical_tube([(-.41,1.46,.14,.05,.13),(-.56,1.85,.22,.045,.14),(-.56,2.25,.38,.045,.16),(-.37,2.86,.78,.06,.18)],2,5)
# Eye layers sit outside the head, with outward winding on both sides.
for s in (-1,1):
    n=(s*.88,.08,.47); u=(s*.47,0,-.88);v=(0,1,0)
    # u cross v points outward for + side; flip ring on negative side.
    if s<0: u=tuple(-t for t in u)
    base=(s*.605,2.51,1.115)
    for depth,rx,ry,col in [(0,.292,.375,5),(.009,.264,.343,3),(.019,.144,.267,4),(.027,.074,.224,5)]:
        c=tuple(base[i]+n[i]*depth for i in range(3));disk(c,u,v,rx,ry,col)
    c=tuple(base[i]+n[i]*.040+v[i]*.105+u[i]*.035 for i in range(3));disk(c,u,v,.046,.068,6,8)
    # Small nostril on each side of the muzzle.
    disk((s*.33,2.24,1.48),u,v,.035,.022,5,7)
    # Five-point golden star cutie mark on each flank, drawn as geometry.
    center=(s*.508,1.40,-.59); axis=(0,0,-s); up=(0,1,0); c=add(center);rim=[]
    for k in range(10):
        a=math.pi/2+k*math.pi/5;r=.17 if k%2==0 else .075
        rim.append(add(tuple(center[i]+axis[i]*r*math.cos(a)+up[i]*r*math.sin(a) for i in range(3))))
    for k in range(10):tri(c,rim[k],rim[(k+1)%10],9)
# One 16x16 swatch atlas; bake modest face lighting into palette ramps.
COLORS=[]
for color in BASE:
    for factor in (.64,.76,.88,1.,1.08):COLORS.append(tuple(min(255,round(t*factor)) for t in color))
faces=[]
for a,b,c,col in F:
    n=cross(sub(V[b],V[a]),sub(V[c],V[a]));l=math.sqrt(sum(t*t for t in n))
    assert l>1e-8
    d=sum(n[i]*(-.35,.8,.48)[i] for i in range(3))/l
    shade=max(0,min(4,round((d+1)*2))) if col not in (3,4,5,6,9) else 3
    faces.append((a,b,c,col*5+shade))
assert len(faces)<1600 and len(COLORS)<=256
# Text interchange assets remain editable with ordinary modelling tools.
with (HERE/'starling.obj').open('w') as f:
    f.write('# Original Starling pony. Y up; +Z front; CCW outside.\nmtllib starling.mtl\no Starling\n')
    for p in V:f.write('v '+' '.join(f'{x:.6f}' for x in p)+'\n')
    for k in range(len(COLORS)):f.write(f'vt {(k%16+.5)/16:.6f} {1-(k//16+.5)/16:.6f}\n')
    f.write('s off\n');last=None
    for a,b,c,k in faces:
        if last!=k:f.write(f'usemtl swatch{k}\n');last=k
        f.write('f '+' '.join(f'{j+1}/{k+1}' for j in (a,b,c))+'\n')
with (HERE/'starling.mtl').open('w') as f:
    for k,rgb in enumerate(COLORS):f.write(f'newmtl swatch{k}\nKd '+' '.join(f'{c/255:.6f}' for c in rgb)+'\nKa 0 0 0\nKs 0 0 0\nillum 1\n\n')
# Uncompressed 24-bit TGA, top-left origin; universally importable small atlas.
import struct
pixels=bytes(t for k in range(256) for t in (COLORS[k] if k<len(COLORS) else (0,0,0))[::-1])
(HERE/'palette.tga').write_bytes(struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,16,16,24,32)+pixels)
header=['/* Generated by assets/pony97/generate.py; original Starling asset. Y up, +Z front. */','#ifndef PONY_MODEL_H','#define PONY_MODEL_H',f'#define PONY_VERTEX_COUNT {len(V)}',f'#define PONY_TRIANGLE_COUNT {len(faces)}',f'#define PONY_COLOR_COUNT {len(COLORS)}','static const float pony_vertices[][3] = {']
header += ['    {'+', '.join(f'{x:.6f}f' for x in p)+'},' for p in V]
header += ['};','static const unsigned short pony_triangles[][4] = {']+['    {'+', '.join(map(str,p))+'},' for p in faces]+['};','static const unsigned char pony_colors[][3] = {']+['    {'+', '.join(map(str,p))+'},' for p in COLORS]+['};','#endif']
(ROOT/'apps/pony97/pony_model.h').write_text('\n'.join(header)+'\n')
(HERE/'model.json').write_text(json.dumps({'vertices':V,'triangles':faces,'colors':COLORS},separators=(',',':')))
print(f'{len(V)} vertices, {len(faces)} triangles, {len(COLORS)} palette entries; 16x16 texture')

# Keep the browser's embedded copy synchronized with the native/header export.
template = HERE / 'viewer-template.html'
if template.exists():
    (HERE / 'viewer.html').write_text(template.read_text().replace('__MODEL__', (HERE / 'model.json').read_text()))
