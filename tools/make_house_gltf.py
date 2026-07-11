#!/usr/bin/env python3
"""Generate assets/models/house.gltf — a small low-poly house used to demo the engine's glTF
loader. Emits a self-contained glTF 2.0 file (geometry base64-embedded in a data: URI, no external
.bin) with POSITION / NORMAL / COLOR_0 / TEXCOORD_0 attributes and a 16-bit index buffer.

Run from the repo root:  python3 tools/make_house_gltf.py
"""
import base64
import json
import struct
import os

verts = []   # (px,py,pz, nx,ny,nz, r,g,b,a, u,v)
indices = []


def quad(a, b, c, d, n, col):
    base = len(verts)
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for p, uv in zip((a, b, c, d), uvs):
        verts.append((p[0], p[1], p[2], n[0], n[1], n[2],
                      col[0], col[1], col[2], 1.0, uv[0], uv[1]))
    indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])


def tri(a, b, c, n, col):
    base = len(verts)
    uvs = [(0, 0), (1, 0), (0.5, 1)]
    for p, uv in zip((a, b, c), uvs):
        verts.append((p[0], p[1], p[2], n[0], n[1], n[2],
                      col[0], col[1], col[2], 1.0, uv[0], uv[1]))
    indices.extend([base, base + 1, base + 2])


WALL = (0.86, 0.79, 0.64)
ROOF = (0.74, 0.24, 0.20)
DOOR = (0.36, 0.24, 0.14)
WIN = (0.55, 0.76, 0.92)

wt = 1.4     # wall top
apex = 2.1   # roof ridge height
ov = 1.15    # roof eave/overhang extent

# --- walls (a box, open top/bottom) -----------------------------------------
# Front (-z) and back (+z) faces are drawn as their own quads; the door/windows
# sit just in front of the front wall.
quad((-1, 0, -1), (1, 0, -1), (1, wt, -1), (-1, wt, -1), (0, 0, -1), WALL)   # front
quad((1, 0, 1), (-1, 0, 1), (-1, wt, 1), (1, wt, 1), (0, 0, 1), WALL)        # back
quad((-1, 0, 1), (-1, 0, -1), (-1, wt, -1), (-1, wt, 1), (-1, 0, 0), WALL)   # left
quad((1, 0, -1), (1, 0, 1), (1, wt, 1), (1, wt, -1), (1, 0, 0), WALL)        # right

# --- roof (gable) ------------------------------------------------------------
ln = (-0.52, 0.85, 0.0)
rn = (0.52, 0.85, 0.0)
quad((-ov, wt, ov), (-ov, wt, -ov), (0, apex, -ov), (0, apex, ov), ln, ROOF)  # left slope
quad((0, apex, ov), (0, apex, -ov), (ov, wt, -ov), (ov, wt, ov), rn, ROOF)    # right slope
tri((-1, wt, -1), (1, wt, -1), (0, apex, -1), (0, 0, -1), WALL)               # front gable
tri((1, wt, 1), (-1, wt, 1), (0, apex, 1), (0, 0, 1), WALL)                   # back gable

# --- door + windows on the front wall (slightly proud to avoid z-fighting) --
z = -1.01
quad((-0.28, 0, z), (0.28, 0, z), (0.28, 0.9, z), (-0.28, 0.9, z), (0, 0, -1), DOOR)
quad((-0.72, 0.7, z), (-0.42, 0.7, z), (-0.42, 1.05, z), (-0.72, 1.05, z), (0, 0, -1), WIN)
quad((0.42, 0.7, z), (0.72, 0.7, z), (0.72, 1.05, z), (0.42, 1.05, z), (0, 0, -1), WIN)

# --- pack a single binary blob: POSITION, NORMAL, COLOR_0, TEXCOORD_0, INDICES
pos = b"".join(struct.pack("<3f", v[0], v[1], v[2]) for v in verts)
nrm = b"".join(struct.pack("<3f", v[3], v[4], v[5]) for v in verts)
col = b"".join(struct.pack("<4f", v[6], v[7], v[8], v[9]) for v in verts)
tex = b"".join(struct.pack("<2f", v[10], v[11]) for v in verts)
idx = b"".join(struct.pack("<H", i) for i in indices)


def pad4(b):
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


blob = pad4(pos) + pad4(nrm) + pad4(col) + pad4(tex) + pad4(idx)
off_pos, off_nrm = 0, len(pad4(pos))
off_col = off_nrm + len(pad4(nrm))
off_tex = off_col + len(pad4(col))
off_idx = off_tex + len(pad4(tex))

pmin = [min(v[i] for v in verts) for i in range(3)]
pmax = [max(v[i] for v in verts) for i in range(3)]
n = len(verts)

gltf = {
    "asset": {"version": "2.0", "generator": "maz make_house_gltf.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0, "name": "House"}],
    "meshes": [{"name": "House", "primitives": [{
        "attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2, "TEXCOORD_0": 3},
        "indices": 4, "mode": 4}]}],
    "buffers": [{
        "byteLength": len(blob),
        "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode()}],
    "bufferViews": [
        {"buffer": 0, "byteOffset": off_pos, "byteLength": len(pos), "target": 34962},
        {"buffer": 0, "byteOffset": off_nrm, "byteLength": len(nrm), "target": 34962},
        {"buffer": 0, "byteOffset": off_col, "byteLength": len(col), "target": 34962},
        {"buffer": 0, "byteOffset": off_tex, "byteLength": len(tex), "target": 34962},
        {"buffer": 0, "byteOffset": off_idx, "byteLength": len(idx), "target": 34963},
    ],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": n, "type": "VEC3",
         "min": pmin, "max": pmax},
        {"bufferView": 1, "componentType": 5126, "count": n, "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": n, "type": "VEC4"},
        {"bufferView": 3, "componentType": 5126, "count": n, "type": "VEC2"},
        {"bufferView": 4, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
    ],
}

out = os.path.join(os.path.dirname(__file__), "..", "assets", "models", "house.gltf")
out = os.path.normpath(out)
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w") as f:
    json.dump(gltf, f, indent=1)
print(f"wrote {out}: {n} verts, {len(indices)} indices, buffer {len(blob)} bytes")
