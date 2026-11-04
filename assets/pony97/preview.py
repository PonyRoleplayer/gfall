#!/usr/bin/env python3
"""Render the authored mesh for visual inspection, not a Glide benchmark.
Optional workstation dependencies: numpy and Pillow. No game code is built.
"""
import json, math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
p=Path(__file__).resolve().parent; data=json.loads((p/'model.json').read_text())
v=np.array(data['vertices']);v[:,1]-=1.65
W,H=760,720
for angle,name in [(38,'preview'),(-38,'preview-other'),(100,'preview-side')]:
    yaw=math.radians(angle); pitch=.10
    eye=np.array([math.sin(yaw)*6,2,math.cos(yaw)*6]); target=np.array([0,0,0]);forward=(target-eye);forward/=np.linalg.norm(forward)
    right=np.cross(forward,[0,1,0]);right/=np.linalg.norm(right);up=np.cross(right,forward)
    delta=v-eye; vv=np.stack([delta@right,delta@up,delta@forward],axis=1)
    screen=np.stack([W/2+vv[:,0]/vv[:,2]*850,H/2-vv[:,1]/vv[:,2]*850],axis=1)
    rgb=np.full((H,W,3),(27,32,47),dtype=np.uint8);depth=np.full((H,W),np.inf)
    for a,b,c,col in data['triangles']:
        ids=[a,b,c]
        normal=np.cross(v[b]-v[a],v[c]-v[a])
        if normal@(eye-v[a])<=0:continue
        pts=screen[ids];zs=vv[ids,2]
        lo=np.maximum(np.floor(pts.min(0)).astype(int),[0,0]);hi=np.minimum(np.ceil(pts.max(0)).astype(int),[W-1,H-1])
        if np.any(lo>hi):continue
        x,y=np.meshgrid(np.arange(lo[0],hi[0]+1)+.5,np.arange(lo[1],hi[1]+1)+.5)
        den=(pts[1,1]-pts[2,1])*(pts[0,0]-pts[2,0])+(pts[2,0]-pts[1,0])*(pts[0,1]-pts[2,1])
        if abs(den)<1e-9:continue
        wa=((pts[1,1]-pts[2,1])*(x-pts[2,0])+(pts[2,0]-pts[1,0])*(y-pts[2,1]))/den
        wb=((pts[2,1]-pts[0,1])*(x-pts[2,0])+(pts[0,0]-pts[2,0])*(y-pts[2,1]))/den;wc=1-wa-wb
        zz=1/(wa/zs[0]+wb/zs[1]+wc/zs[2]); region=depth[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
        mask=(wa>=0)&(wb>=0)&(wc>=0)&(zz<region);region[mask]=zz[mask];rgb[lo[1]:hi[1]+1,lo[0]:hi[0]+1][mask]=data['colors'][col]
    img=Image.fromarray(rgb);d=ImageDraw.Draw(img);d.text((24,24),'STARLING / PONY97',fill=(235,242,249));d.text((24,H-35),f"{len(data['triangles'])} triangles  /  16 x 16 indexed atlas  /  static mesh",fill=(170,192,205));img.save(p/(name+'.png'))
