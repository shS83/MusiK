#!/usr/bin/env python3
"""One-time build of fog_shS's exact extruded text mesh.

The generated .npz is shipped with MusiK so runtime only needs PySide6,
NumPy and ModernGL. To regenerate: uv run --with matplotlib --with shapely
python tools/generate_fog_mesh.py
"""
from __future__ import annotations

from pathlib import Path
import numpy as np
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties
from shapely.geometry import Polygon
from shapely import constrained_delaunay_triangles
from shapely.geometry.polygon import orient

TEXT = "shS"
DEPTH = 0.22
HEIGHT = 0.82
# Resample edges to give the GPU enough vertices to oscillate visibly.
SPACING = 0.007


def normalize(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


def make_mesh():
    path = TextPath((0, 0), TEXT, size=1., prop=FontProperties(family="DejaVu Sans", weight="bold"))
    raw = [np.asarray(a, dtype=np.float64) for a in path.to_polygons()]
    raw = [a[:-1] if np.allclose(a[0],a[-1]) else a for a in raw]
    xys = np.concatenate(raw)
    lo = xys.min(axis=0); hi = xys.max(axis=0)
    center = (lo + hi) / 2
    scale = HEIGHT / (hi[1] - lo[1])
    # Each original filled glyph in source is one contour. Use its real paths,
    # rather than a texture or simplified font imitation.
    polys = []
    for ring in raw:
        p = Polygon((ring-center)*scale)
        if not p.is_valid: p = p.buffer(0)
        if p.geom_type == 'Polygon': polys.append(orient(p, 1.))
        elif p.geom_type == 'MultiPolygon': polys.extend(orient(a, 1.) for a in p.geoms)
    data = []
    def vertex(xy, z, normal, direction, t):
        # 10 floats: position 3, static normal 3, outward edge normal 2,
        # local perimeter coordinate 1, weight 1 (reserved for future).
        return [float(xy[0]), float(xy[1]), float(z), *[float(a) for a in normal],
                float(direction[0]), float(direction[1]), float(t), 1.]

    def append_tri(a,b,c): data.extend((a,b,c))

    for glyph_index, poly in enumerate(polys):
        rings = [np.asarray(poly.exterior.coords[:-1], dtype=np.float64)]
        rings.extend(np.asarray(r.coords[:-1], dtype=np.float64) for r in poly.interiors)
        resampled = []
        per_vertex = {}
        for ring_index, old in enumerate(rings):
            # A ring always wraps; interpolation at u=1 joins back to u=0.
            points=[]
            for i in range(len(old)):
                a,b=old[i],old[(i+1)%len(old)]
                n=max(1,int(np.ceil(np.linalg.norm(b-a)/SPACING)))
                for s in range(n): points.append(a+(b-a)*s/n)
            ring = np.asarray(points, dtype=np.float64)
            seg = np.linalg.norm(np.roll(ring,-1,axis=0)-ring,axis=1)
            u = np.concatenate(([0], np.cumsum(seg[:-1])))/seg.sum()
            dirs=[]
            for j in range(len(ring)):
                before=normalize(ring[j]-ring[j-1]); after=normalize(ring[(j+1)%len(ring)]-ring[j])
                tangent=normalize(before+after)
                if np.linalg.norm(tangent)<1e-9: tangent=after
                dirs.append(normalize(np.array([tangent[1], -tangent[0]])))
            dirs=np.array(dirs)
            resampled.append(ring)
            for j, point in enumerate(ring):
                # Key by rounded coordinates to counter Shapely float roundtrips.
                key=tuple(np.round(point, 8))
                per_vertex[key]=(dirs[j], float(u[j] + glyph_index*0.37 + ring_index*.13))
            # Extruded side surface, with the original face normals.
            for j in range(len(ring)):
                p0,p1=ring[j],ring[(j+1)%len(ring)]
                tangent=normalize(p1-p0)
                normal=np.array([tangent[1],-tangent[0],0.],dtype=float)
                k=(j+1)%len(ring)
                t0=u[j]+glyph_index*.37+ring_index*.13
                t1=(u[k] if k else 1.)+glyph_index*.37+ring_index*.13
                v0=vertex(p0,-DEPTH/2,normal,dirs[j],t0)
                v1=vertex(p1,-DEPTH/2,normal,dirs[k],t1)
                v2=vertex(p1,+DEPTH/2,normal,dirs[k],t1)
                v3=vertex(p0,+DEPTH/2,normal,dirs[j],t0)
                append_tri(v0,v1,v2)
                append_tri(v0,v2,v3)
        polygon=Polygon(resampled[0], holes=resampled[1:])
        geometry=constrained_delaunay_triangles(polygon)
        for tri in geometry.geoms:
            # Note all triangle points correspond to resampled contour vertices;
            # this ensures the OSC waveform deforms the actual letter boundaries.
            pts = np.asarray(tri.exterior.coords[:3])
            for z,forward in [(DEPTH/2,True),(-DEPTH/2,False)]:
                corners=[]
                for xy in pts:
                    key=tuple(np.round(xy,8))
                    direction,t=per_vertex[key]
                    corners.append(vertex(xy,z,(0,0,1 if forward else -1),direction,t))
                # Triangle order may be arbitrary; fix the winding by signed area.
                a,b,c=pts
                area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
                if bool(area>0)!=forward: corners=[corners[0],corners[2],corners[1]]
                append_tri(*corners)
    arr=np.asarray(data,dtype=np.float32)
    if arr.ndim !=2 or arr.shape[1] !=10 or not np.isfinite(arr).all():
        raise RuntimeError('Invalid fog geometry')
    return arr


if __name__=='__main__':
    result=make_mesh()
    output=Path(__file__).resolve().parent.parent/'musik'/'fog_mesh.npz'
    np.savez_compressed(output, mesh=result)
    print(f'{output}: {len(result):,} vertices, {result.nbytes/1024:.1f} KiB uncompressed')
