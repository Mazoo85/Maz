#!/usr/bin/env python3
"""Generate assets/models/house.gltf — a small low-poly house used to demo the engine's glTF
loader. Emits a self-contained glTF 2.0 file (geometry AND a base-color texture base64-embedded in
one buffer, no external files) with POSITION / NORMAL / COLOR_0 / TEXCOORD_0 attributes, a 16-bit
index buffer, and a PBR material whose base-color texture is a 4-quadrant grayscale detail atlas
(brick / shingle / plank / glass). Each face's UVs map into the matching quadrant; the mesh shader
multiplies the grayscale detail by the vertex color, so walls read as beige brick, the roof as red
shingle, and so on.

Run from the repo root:  python3 tools/make_house_gltf.py
"""
import base64
import json
import os
import struct
import zlib

verts = []   # (px,py,pz, nx,ny,nz, r,g,b,a, u,v)
indices = []

# UV sub-rects (quadrants) of the 2x2 detail atlas, inset slightly to avoid bilinear bleed.
E = 0.02
WALL_UV = (0.0 + E, 0.0 + E, 0.5 - E, 0.5 - E)
ROOF_UV = (0.5 + E, 0.0 + E, 1.0 - E, 0.5 - E)
PLANK_UV = (0.0 + E, 0.5 + E, 0.5 - E, 1.0 - E)
GLASS_UV = (0.5 + E, 0.5 + E, 1.0 - E, 1.0 - E)


def uvmap(rect, s, t):
    return (rect[0] + (rect[2] - rect[0]) * s, rect[1] + (rect[3] - rect[1]) * t)


def quad(a, b, c, d, n, col, rect):
    base = len(verts)
    st = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for p, (s, t) in zip((a, b, c, d), st):
        u, v = uvmap(rect, s, t)
        verts.append((p[0], p[1], p[2], n[0], n[1], n[2], col[0], col[1], col[2], 1.0, u, v))
    indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])


def tri(a, b, c, n, col, rect):
    base = len(verts)
    st = [(0, 0), (1, 0), (0.5, 1)]
    for p, (s, t) in zip((a, b, c), st):
        u, v = uvmap(rect, s, t)
        verts.append((p[0], p[1], p[2], n[0], n[1], n[2], col[0], col[1], col[2], 1.0, u, v))
    indices.extend([base, base + 1, base + 2])


WALL = (0.86, 0.79, 0.64)
ROOF = (0.74, 0.24, 0.20)
DOOR = (0.42, 0.28, 0.16)
WIN = (0.60, 0.78, 0.95)

wt = 1.4     # wall top
apex = 2.1   # roof ridge height
ov = 1.15    # roof eave/overhang extent

# --- walls (a box, open top/bottom) -----------------------------------------
quad((-1, 0, -1), (1, 0, -1), (1, wt, -1), (-1, wt, -1), (0, 0, -1), WALL, WALL_UV)   # front
quad((1, 0, 1), (-1, 0, 1), (-1, wt, 1), (1, wt, 1), (0, 0, 1), WALL, WALL_UV)        # back
quad((-1, 0, 1), (-1, 0, -1), (-1, wt, -1), (-1, wt, 1), (-1, 0, 0), WALL, WALL_UV)   # left
quad((1, 0, -1), (1, 0, 1), (1, wt, 1), (1, wt, -1), (1, 0, 0), WALL, WALL_UV)        # right

# --- roof (gable) ------------------------------------------------------------
ln = (-0.52, 0.85, 0.0)
rn = (0.52, 0.85, 0.0)
quad((-ov, wt, ov), (-ov, wt, -ov), (0, apex, -ov), (0, apex, ov), ln, ROOF, ROOF_UV)
quad((0, apex, ov), (0, apex, -ov), (ov, wt, -ov), (ov, wt, ov), rn, ROOF, ROOF_UV)
tri((-1, wt, -1), (1, wt, -1), (0, apex, -1), (0, 0, -1), WALL, WALL_UV)   # front gable
tri((1, wt, 1), (-1, wt, 1), (0, apex, 1), (0, 0, 1), WALL, WALL_UV)       # back gable

# --- door + windows on the front wall (slightly proud to avoid z-fighting) --
z = -1.01
quad((-0.28, 0, z), (0.28, 0, z), (0.28, 0.9, z), (-0.28, 0.9, z), (0, 0, -1), DOOR, PLANK_UV)
quad((-0.72, 0.7, z), (-0.42, 0.7, z), (-0.42, 1.05, z), (-0.72, 1.05, z), (0, 0, -1), WIN, GLASS_UV)
quad((0.42, 0.7, z), (0.72, 0.7, z), (0.72, 1.05, z), (0.42, 1.05, z), (0, 0, -1), WIN, GLASS_UV)


# --- detail atlas: 64x64 grayscale RGBA, 4 quadrants (32x32 each) ------------
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
            if x < H and y < H:            # brick (staggered courses + mortar)
                row = qy // 8
                off = 0 if row % 2 == 0 else 8
                mortar = (qy % 8 == 0) or ((qx + off) % 16 == 0)
                put(x, y, 150 if mortar else 235)
            elif x >= H and y < H:         # shingles (overlapping scalloped rows)
                r = qy % 8
                shade = 205 + (r * 5)
                if r == 0:
                    shade = 150          # dark row seam
                if (qx + (qy // 8) * 4) % 16 < 1:
                    shade = 175          # vertical stagger gaps
                put(x, y, min(shade, 255))
            elif x < H and y >= H:         # planks (vertical boards + grain)
                board = qx % 8
                g = 210 if board != 0 else 150
                if (qx * 7 + qy * 3) % 11 == 0:
                    g -= 18              # faint grain speckle
                put(x, y, g)
            else:                          # glass (light panes + cross frame)
                frame = (qx in (0, 1, 15, 16, 31)) or (qy in (0, 1, 15, 16, 31))
                put(x, y, 130 if frame else 245)
    return bytes(px), S, S


def png_encode(rgba, w, h):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0 (none)
        raw.extend(rgba[y * w * 4:(y + 1) * w * 4])
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


# Tangent-space normal-map atlas: a height field per quadrant (mortar/seams/grooves are recessed),
# converted to normals via the height gradient, so the lights catch the surface relief.
def build_normal_atlas():
    S, H = 64, 32
    hgt = [[1.0] * S for _ in range(S)]
    for y in range(S):
        for x in range(S):
            qx, qy = x % H, y % H
            if x < H and y < H:            # brick: recessed mortar lines
                row = qy // 8
                off = 0 if row % 2 == 0 else 8
                mortar = (qy % 8 == 0) or ((qx + off) % 16 == 0)
                hgt[y][x] = 0.25 if mortar else 1.0
            elif x >= H and y < H:         # shingle: a step down at each row seam
                r = qy % 8
                hgt[y][x] = 0.2 if r == 0 else 0.5 + r * 0.06
            elif x < H and y >= H:         # plank: grooves between boards
                hgt[y][x] = 0.3 if (qx % 8 == 0) else 1.0
            else:                          # glass: raised frame
                frame = (qx in (0, 1, 15, 16, 31)) or (qy in (0, 1, 15, 16, 31))
                hgt[y][x] = 0.55 if frame else 1.0
    px = bytearray(S * S * 4)
    strength = 2.4
    for y in range(S):
        for x in range(S):
            hl, hr = hgt[y][(x - 1) % S], hgt[y][(x + 1) % S]
            hd, hu = hgt[(y - 1) % S][x], hgt[(y + 1) % S][x]
            nx, ny, nz = (hl - hr) * strength, (hd - hu) * strength, 1.0
            inv = 1.0 / ((nx * nx + ny * ny + nz * nz) ** 0.5)
            i = (y * S + x) * 4
            px[i] = int((nx * inv * 0.5 + 0.5) * 255)
            px[i + 1] = int((ny * inv * 0.5 + 0.5) * 255)
            px[i + 2] = int((nz * inv * 0.5 + 0.5) * 255)
            px[i + 3] = 255
    return bytes(px), S, S


atlas_rgba, aw, ah = build_atlas()
png = png_encode(atlas_rgba, aw, ah)
normal_rgba, _, _ = build_normal_atlas()
npng = png_encode(normal_rgba, aw, ah)

# --- pack one binary blob: POSITION, NORMAL, COLOR_0, TEXCOORD_0, INDICES, PNG
pos = b"".join(struct.pack("<3f", v[0], v[1], v[2]) for v in verts)
nrm = b"".join(struct.pack("<3f", v[3], v[4], v[5]) for v in verts)
col = b"".join(struct.pack("<4f", v[6], v[7], v[8], v[9]) for v in verts)
tex = b"".join(struct.pack("<2f", v[10], v[11]) for v in verts)
idx = b"".join(struct.pack("<H", i) for i in indices)


def pad4(b):
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


parts = [pad4(pos), pad4(nrm), pad4(col), pad4(tex), pad4(idx), pad4(png), pad4(npng)]
offs, cur = [], 0
for p in parts:
    offs.append(cur)
    cur += len(p)
blob = b"".join(parts)

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
        "indices": 4, "material": 0, "mode": 4}]}],
    "materials": [{
        "name": "HouseDetail",
        "pbrMetallicRoughness": {
            "baseColorTexture": {"index": 0},
            "metallicFactor": 0.0, "roughnessFactor": 1.0},
        "normalTexture": {"index": 1}}],
    "textures": [{"source": 0, "sampler": 0}, {"source": 1, "sampler": 0}],
    "images": [{"bufferView": 5, "mimeType": "image/png"},
               {"bufferView": 6, "mimeType": "image/png"}],
    "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 10497, "wrapT": 10497}],
    "buffers": [{
        "byteLength": len(blob),
        "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode()}],
    "bufferViews": [
        {"buffer": 0, "byteOffset": offs[0], "byteLength": len(pos), "target": 34962},
        {"buffer": 0, "byteOffset": offs[1], "byteLength": len(nrm), "target": 34962},
        {"buffer": 0, "byteOffset": offs[2], "byteLength": len(col), "target": 34962},
        {"buffer": 0, "byteOffset": offs[3], "byteLength": len(tex), "target": 34962},
        {"buffer": 0, "byteOffset": offs[4], "byteLength": len(idx), "target": 34963},
        {"buffer": 0, "byteOffset": offs[5], "byteLength": len(png)},
        {"buffer": 0, "byteOffset": offs[6], "byteLength": len(npng)},
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

out = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "assets", "models", "house.gltf"))
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w") as f:
    json.dump(gltf, f, indent=1)
print(f"wrote {out}: {n} verts, {len(indices)} indices, {aw}x{ah} atlas, buffer {len(blob)} bytes")
