#!/usr/bin/env python3
"""Generate assets/models/village.gltf — a small scene used to demo the engine's glTF *scene*
loader (maz::render::loadGltfScene). Unlike house.gltf (one merged model), this file defines three
meshes — a textured house, a ground plane, and a tree — and places many nodes referencing them at
different positions/rotations, so the loader returns one SceneNode per placed object. The house
mesh carries the same brick/shingle/plank/glass detail atlas as house.gltf; ground and trees use
vertex colors only (no texture).

Self-contained: all geometry and the atlas PNG are base64-embedded in one buffer.

Run from the repo root:  python3 tools/make_village_gltf.py
"""
import base64
import json
import math
import os
import struct
import zlib

# ---------------------------------------------------------------------------
# Geometry builders: each returns (verts, indices), verts as
# (px,py,pz, nx,ny,nz, r,g,b,a, u,v).
# ---------------------------------------------------------------------------
E = 0.02
WALL_UV = (0.0 + E, 0.0 + E, 0.5 - E, 0.5 - E)
ROOF_UV = (0.5 + E, 0.0 + E, 1.0 - E, 0.5 - E)
PLANK_UV = (0.0 + E, 0.5 + E, 0.5 - E, 1.0 - E)
GLASS_UV = (0.5 + E, 0.5 + E, 1.0 - E, 1.0 - E)


def _uv(rect, s, t):
    return (rect[0] + (rect[2] - rect[0]) * s, rect[1] + (rect[3] - rect[1]) * t)


class MeshB:
    def __init__(self):
        self.v = []
        self.i = []

    def quad(self, a, b, c, d, n, col, rect=(0, 0, 1, 1)):
        base = len(self.v)
        for p, (s, t) in zip((a, b, c, d), [(0, 0), (1, 0), (1, 1), (0, 1)]):
            u, w = _uv(rect, s, t)
            self.v.append((*p, *n, *col, 1.0, u, w))
        self.i += [base, base + 1, base + 2, base, base + 2, base + 3]

    def tri(self, a, b, c, n, col, rect=(0, 0, 1, 1)):
        base = len(self.v)
        for p, (s, t) in zip((a, b, c), [(0, 0), (1, 0), (0.5, 1)]):
            u, w = _uv(rect, s, t)
            self.v.append((*p, *n, *col, 1.0, u, w))
        self.i += [base, base + 1, base + 2]


def build_house():
    m = MeshB()
    WALL, ROOF, DOOR, WIN = (0.86, 0.79, 0.64), (0.74, 0.24, 0.20), (0.42, 0.28, 0.16), (0.60, 0.78, 0.95)
    wt, apex, ov = 1.4, 2.1, 1.15
    m.quad((-1, 0, -1), (1, 0, -1), (1, wt, -1), (-1, wt, -1), (0, 0, -1), WALL, WALL_UV)
    m.quad((1, 0, 1), (-1, 0, 1), (-1, wt, 1), (1, wt, 1), (0, 0, 1), WALL, WALL_UV)
    m.quad((-1, 0, 1), (-1, 0, -1), (-1, wt, -1), (-1, wt, 1), (-1, 0, 0), WALL, WALL_UV)
    m.quad((1, 0, -1), (1, 0, 1), (1, wt, 1), (1, wt, -1), (1, 0, 0), WALL, WALL_UV)
    ln, rn = (-0.52, 0.85, 0.0), (0.52, 0.85, 0.0)
    m.quad((-ov, wt, ov), (-ov, wt, -ov), (0, apex, -ov), (0, apex, ov), ln, ROOF, ROOF_UV)
    m.quad((0, apex, ov), (0, apex, -ov), (ov, wt, -ov), (ov, wt, ov), rn, ROOF, ROOF_UV)
    m.tri((-1, wt, -1), (1, wt, -1), (0, apex, -1), (0, 0, -1), WALL, WALL_UV)
    m.tri((1, wt, 1), (-1, wt, 1), (0, apex, 1), (0, 0, 1), WALL, WALL_UV)
    z = -1.01
    m.quad((-0.28, 0, z), (0.28, 0, z), (0.28, 0.9, z), (-0.28, 0.9, z), (0, 0, -1), DOOR, PLANK_UV)
    m.quad((-0.72, 0.7, z), (-0.42, 0.7, z), (-0.42, 1.05, z), (-0.72, 1.05, z), (0, 0, -1), WIN, GLASS_UV)
    m.quad((0.42, 0.7, z), (0.72, 0.7, z), (0.72, 1.05, z), (0.42, 1.05, z), (0, 0, -1), WIN, GLASS_UV)
    return m


def build_ground(half=40.0):
    m = MeshB()
    g = (0.40, 0.50, 0.32)
    m.quad((-half, 0, half), (half, 0, half), (half, 0, -half), (-half, 0, -half), (0, 1, 0), g)
    return m


def build_tree():
    m = MeshB()
    trunk, leaf = (0.40, 0.28, 0.16), (0.24, 0.52, 0.26)
    # trunk: a thin box from y=0..0.8
    tw = 0.14
    c = [(-tw, 0, -tw), (tw, 0, -tw), (tw, 0, tw), (-tw, 0, tw)]
    top = 0.8
    for k in range(4):
        a, b = c[k], c[(k + 1) % 4]
        nrm = (a[0] + b[0], 0, a[2] + b[2])
        m.quad((a[0], 0, a[2]), (b[0], 0, b[2]), (b[0], top, b[2]), (a[0], top, a[2]), nrm, trunk)
    # foliage: a cone (base ring at y=top-0.1, apex above)
    seg, rad, base_y, apex_y = 8, 0.9, top - 0.1, top + 1.9
    for k in range(seg):
        a0 = 2 * math.pi * k / seg
        a1 = 2 * math.pi * (k + 1) / seg
        p0 = (math.cos(a0) * rad, base_y, math.sin(a0) * rad)
        p1 = (math.cos(a1) * rad, base_y, math.sin(a1) * rad)
        apex = (0, apex_y, 0)
        nx, nz = math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2)
        m.tri(p0, p1, apex, (nx, 0.4, nz), leaf)
    return m


# ---------------------------------------------------------------------------
# Detail atlas (same as house.gltf) + minimal PNG encoder.
# ---------------------------------------------------------------------------
def build_atlas():
    S, H = 64, 32
    px = bytearray(S * S * 4)

    def put(x, y, g):
        i = (y * S + x) * 4
        px[i] = px[i + 1] = px[i + 2] = g
        px[i + 3] = 255

    for y in range(S):
        for x in range(S):
            qx, qy = x % H, y % H
            if x < H and y < H:
                row = qy // 8
                off = 0 if row % 2 == 0 else 8
                mortar = (qy % 8 == 0) or ((qx + off) % 16 == 0)
                put(x, y, 150 if mortar else 235)
            elif x >= H and y < H:
                r = qy % 8
                shade = 205 + r * 5
                if r == 0:
                    shade = 150
                if (qx + (qy // 8) * 4) % 16 < 1:
                    shade = 175
                put(x, y, min(shade, 255))
            elif x < H and y >= H:
                board = qx % 8
                g = 210 if board != 0 else 150
                if (qx * 7 + qy * 3) % 11 == 0:
                    g -= 18
                put(x, y, g)
            else:
                frame = (qx in (0, 1, 15, 16, 31)) or (qy in (0, 1, 15, 16, 31))
                put(x, y, 130 if frame else 245)
    return bytes(px), S, S


def png_encode(rgba, w, h):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw.extend(rgba[y * w * 4:(y + 1) * w * 4])
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


# ---------------------------------------------------------------------------
# Assemble glTF: 3 meshes, an atlas image/texture/material, and many nodes.
# ---------------------------------------------------------------------------
def yrot_node(mesh_index, x, z, deg, name):
    a = math.radians(deg)
    # column-major TRS as a matrix (rotation about Y + translation).
    c, s = math.cos(a), math.sin(a)
    return {"mesh": mesh_index, "name": name,
            "matrix": [c, 0, -s, 0,  0, 1, 0, 0,  s, 0, c, 0,  x, 0, z, 1]}


house, ground, tree = build_house(), build_ground(), build_tree()
meshes = [house, ground, tree]

atlas_rgba, aw, ah = build_atlas()
png = png_encode(atlas_rgba, aw, ah)

# Pack all mesh arrays + the PNG into one buffer, recording bufferViews/accessors per mesh.
blob = bytearray()
bufferViews = []
accessors = []
mesh_prims = []  # accessor indices per mesh: (pos,nrm,col,tex,idx)


def pad4():
    while len(blob) % 4:
        blob.append(0)


def add_view(data, target=None):
    pad4()
    off = len(blob)
    blob.extend(data)
    bv = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
    if target:
        bv["target"] = target
    bufferViews.append(bv)
    return len(bufferViews) - 1


for m in meshes:
    pos = b"".join(struct.pack("<3f", v[0], v[1], v[2]) for v in m.v)
    nrm = b"".join(struct.pack("<3f", v[3], v[4], v[5]) for v in m.v)
    col = b"".join(struct.pack("<4f", v[6], v[7], v[8], v[9]) for v in m.v)
    tex = b"".join(struct.pack("<2f", v[10], v[11]) for v in m.v)
    idx = b"".join(struct.pack("<H", i) for i in m.i)
    n = len(m.v)
    pmin = [min(v[k] for v in m.v) for k in range(3)]
    pmax = [max(v[k] for v in m.v) for k in range(3)]
    vp = add_view(pos, 34962)
    vn = add_view(nrm, 34962)
    vc = add_view(col, 34962)
    vt = add_view(tex, 34962)
    vi = add_view(idx, 34963)
    a_pos = len(accessors); accessors.append({"bufferView": vp, "componentType": 5126, "count": n, "type": "VEC3", "min": pmin, "max": pmax})
    a_nrm = len(accessors); accessors.append({"bufferView": vn, "componentType": 5126, "count": n, "type": "VEC3"})
    a_col = len(accessors); accessors.append({"bufferView": vc, "componentType": 5126, "count": n, "type": "VEC4"})
    a_tex = len(accessors); accessors.append({"bufferView": vt, "componentType": 5126, "count": n, "type": "VEC2"})
    a_idx = len(accessors); accessors.append({"bufferView": vi, "componentType": 5123, "count": len(m.i), "type": "SCALAR"})
    mesh_prims.append((a_pos, a_nrm, a_col, a_tex, a_idx))

png_view = add_view(png)

gltf_meshes = []
for k, (a_pos, a_nrm, a_col, a_tex, a_idx) in enumerate(mesh_prims):
    prim = {"attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "COLOR_0": a_col, "TEXCOORD_0": a_tex},
            "indices": a_idx, "mode": 4}
    if k == 0:  # the house mesh uses the detail atlas material
        prim["material"] = 0
    gltf_meshes.append({"primitives": [prim]})

# Place nodes: ground, a ring/rows of houses, and scattered trees.
nodes = [yrot_node(1, 0, 0, 0, "Ground")]
house_spots = [(-8, -6, 20), (0, -8, 0), (8, -6, -20), (-9, 6, -25), (2, 8, 200), (10, 7, 160)]
for i, (x, z, d) in enumerate(house_spots):
    nodes.append(yrot_node(0, x, z, d, f"House{i}"))
tree_spots = [(-4, -2), (4, -3), (-3, 3), (6, 3), (-8, 1), (9, -1), (0, 5), (-6, -6), (7, 6)]
for i, (x, z) in enumerate(tree_spots):
    nodes.append(yrot_node(2, x, z, 0, f"Tree{i}"))

gltf = {
    "asset": {"version": "2.0", "generator": "maz make_village_gltf.py"},
    "scene": 0,
    "scenes": [{"nodes": list(range(len(nodes)))}],
    "nodes": nodes,
    "meshes": gltf_meshes,
    "materials": [{"name": "HouseDetail", "pbrMetallicRoughness": {
        "baseColorTexture": {"index": 0}, "metallicFactor": 0.0, "roughnessFactor": 1.0}}],
    "textures": [{"source": 0, "sampler": 0}],
    "images": [{"bufferView": png_view, "mimeType": "image/png"}],
    "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 10497, "wrapT": 10497}],
    "buffers": [{"byteLength": len(blob),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
    "bufferViews": bufferViews,
    "accessors": accessors,
}

out = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "assets", "models", "village.gltf"))
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w") as f:
    json.dump(gltf, f, indent=1)
print(f"wrote {out}: {len(nodes)} nodes, {len(meshes)} meshes, buffer {len(blob)} bytes")
