"""
Bakes LA Royale's surface tiles in Blender: seamless 1024 × 1024 textures for
every kind of surface in Los Angeles, each a procedural Blender material baked
to a colour map and a height map (the normal map is made from the height).

    pip install bpy numpy pillow        # Blender as a Python module
    python3 la-royale/tools/blender_tiles.py   # → la-royale/textures/<name>_c.jpg, <name>_n.jpg

The materials, picked for LA:

    stucco      sand-finish stucco, the city's most common wall
    plaster     smooth-trowelled plaster (Spanish Colonial Revival)
    brick       common-bond red brick (Downtown's 1910s–20s blocks)
    siding      painted clapboard (Craftsman bungalows)
    block       concrete block, CMU (garden walls, warehouses, dingbats' sides)
    clay        Spanish barrel clay roof tiles
    shingle     asphalt composition shingles
    metal       corrugated metal (warehouses, sheds, awnings)
    sidewalk    concrete sidewalk scored in five-foot panels, broom finish
    asphalt     asphalt with LA's tar-snake crack sealing
    lawn        St. Augustine / Bermuda lawn
    dryGrass    summer hillside grass and chaparral soil
    dirt        decomposed granite and dirt

Everything tiles: noise is sampled on a 4-D torus (u and v each wrap round a
circle) and patterns repeat a whole number of times across the tile.

The colour maps are stored as detail: each divided by its own average, so the
game multiplies them onto a surface's own colour (a building's paint, the
ground's colour from the map) rather than replacing it.
"""

import math
import os
import sys

import bpy
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'textures')
RES = int(os.environ.get('TILE_RES', 1024))
os.makedirs(OUT, exist_ok=True)

# ------------------------------------------------------------------ scene

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 1
scene.cycles.bake_type = 'EMIT'
bpy.ops.mesh.primitive_plane_add(size=1)
plane = bpy.context.active_object


class Nodes:
    """A small builder for one material's node tree."""

    def __init__(self, name):
        self.mat = bpy.data.materials.new(name)
        self.mat.use_nodes = True
        self.t = self.mat.node_tree
        self.t.nodes.clear()
        self.uv = self.node('ShaderNodeTexCoord').outputs['UV']
        sep = self.node('ShaderNodeSeparateXYZ')
        self.link(self.uv, sep.inputs[0])
        self.u, self.v = sep.outputs['X'], sep.outputs['Y']

    def node(self, kind, **props):
        n = self.t.nodes.new(kind)
        for k, val in props.items():
            setattr(n, k, val)
        return n

    def link(self, a, b):
        self.t.links.new(a, b)

    def val(self, x):
        if isinstance(x, (int, float)):
            n = self.node('ShaderNodeValue')
            n.outputs[0].default_value = float(x)
            return n.outputs[0]
        return x

    def math(self, op, a, b=None, clamp=False):
        n = self.node('ShaderNodeMath', operation=op, use_clamp=clamp)
        self.link(self.val(a), n.inputs[0])
        if b is not None:
            self.link(self.val(b), n.inputs[1])
        return n.outputs[0]

    def add(self, *xs):
        out = xs[0]
        for x in xs[1:]:
            out = self.math('ADD', out, x)
        return out

    def mul(self, a, b): return self.math('MULTIPLY', a, b)
    def sub(self, a, b): return self.math('SUBTRACT', a, b)
    def fract(self, a): return self.math('FRACT', a)
    def floor(self, a): return self.math('FLOOR', a)
    def smooth(self, e0, e1, x):
        n = self.node('ShaderNodeMapRange', interpolation_type='SMOOTHSTEP')
        self.link(self.val(x), n.inputs['Value'])
        n.inputs['From Min'].default_value = e0
        n.inputs['From Max'].default_value = e1
        return n.outputs['Result']

    def torus(self, u, v, ru, rv):
        """(u, v) on a 4-D torus: a vector and a w that wrap when u or v does."""
        tau = 2 * math.pi
        cu, su = self.math('COSINE', self.mul(u, tau)), self.math('SINE', self.mul(u, tau))
        cv, sv = self.math('COSINE', self.mul(v, tau)), self.math('SINE', self.mul(v, tau))
        comb = self.node('ShaderNodeCombineXYZ')
        self.link(self.mul(cu, ru), comb.inputs[0])
        self.link(self.mul(su, ru), comb.inputs[1])
        self.link(self.mul(cv, rv), comb.inputs[2])
        return comb.outputs[0], self.mul(sv, rv)

    def noise(self, freq_u, freq_v=None, detail=4.0, rough=0.5, u=None, v=None, seed=0.0):
        """Periodic fBm noise: about freq features across the tile."""
        freq_v = freq_u if freq_v is None else freq_v
        vec, w = self.torus(u or self.u, v or self.v, freq_u / (2 * math.pi), freq_v / (2 * math.pi))
        n = self.node('ShaderNodeTexNoise', noise_dimensions='4D')
        self.link(vec, n.inputs['Vector'])
        self.link(self.add(w, seed * 17.13), n.inputs['W'])
        n.inputs['Scale'].default_value = 1.0
        n.inputs['Detail'].default_value = detail
        n.inputs['Roughness'].default_value = rough
        return n.outputs['Fac']

    def voronoi(self, freq, feature='F1', out='Distance', seed=0.0):
        vec, w = self.torus(self.u, self.v, freq / (2 * math.pi), freq / (2 * math.pi))
        n = self.node('ShaderNodeTexVoronoi', voronoi_dimensions='4D', feature=feature)
        self.link(vec, n.inputs['Vector'])
        self.link(self.add(w, seed * 11.7), n.inputs['W'])
        n.inputs['Scale'].default_value = 1.0
        return n.outputs[out]

    def cell_random(self, iu, iv, seed=0.0):
        """A random 0..1 per whole-number cell (iu, iv)."""
        comb = self.node('ShaderNodeCombineXYZ')
        self.link(self.add(iu, seed), comb.inputs[0])
        self.link(iv, comb.inputs[1])
        n = self.node('ShaderNodeTexWhiteNoise', noise_dimensions='3D')
        self.link(comb.outputs[0], n.inputs['Vector'])
        return n.outputs['Value']

    def rgb(self, r, g, b):
        comb = self.node('ShaderNodeCombineColor')
        for i, c in enumerate((r, g, b)):
            self.link(self.val(c), comb.inputs[i])
        return comb.outputs[0]

    def emit(self, socket):
        out = self.node('ShaderNodeOutputMaterial')
        em = self.node('ShaderNodeEmission')
        self.link(socket, em.inputs['Color'])
        self.link(em.outputs[0], out.inputs['Surface'])
        out.is_active_output = True
        return em, out


def grey(n, x): return n.rgb(x, x, x)


# ---------------------------------------------------------------- recipes
# Each returns (colour socket, height socket in 0..1, normal strength, tile size in metres).

def stucco(n):
    sand = n.noise(260, detail=2, rough=0.6)
    trowel = n.noise(9, detail=5, rough=0.55, seed=3)
    blotch = n.noise(3, detail=3, seed=7)
    h = n.add(n.mul(sand, 0.55), n.mul(trowel, 0.45))
    c = n.add(0.82, n.mul(n.sub(blotch, 0.5), 0.12), n.mul(n.sub(sand, 0.5), 0.10))
    return grey(n, c), h, 2.2, 2.0


def plaster(n):
    trowel = n.noise(5, detail=6, rough=0.5)
    fine = n.noise(120, detail=2, seed=2)
    h = n.add(n.mul(trowel, 0.8), n.mul(fine, 0.2))
    c = n.add(0.86, n.mul(n.sub(trowel, 0.5), 0.10))
    return grey(n, c), h, 0.8, 2.0


def brick(n):
    # common bond: 20 courses × 8 bricks a 1.6 m tile (≈ 7.5 cm courses, 20 cm bricks), mortar recessed
    rows, cols = 20, 8
    rv = n.mul(n.v, rows)
    row = n.floor(rv)
    shift = n.mul(n.math('MODULO', row, 2), 0.5)
    ru = n.add(n.mul(n.u, cols), shift)
    col = n.floor(ru)
    fu, fv = n.fract(ru), n.fract(rv)
    edge = n.math('MINIMUM', n.math('MINIMUM', fu, n.sub(1, fu)), n.mul(n.math('MINIMUM', fv, n.sub(1, fv)), 0.4))
    mortar = n.sub(1, n.smooth(0.03, 0.06, edge))
    tone = n.cell_random(n.math('MODULO', col, cols), row)
    grain = n.noise(140, detail=3, seed=5)
    face = n.add(n.mul(tone, 0.35), 0.75, n.mul(n.sub(grain, 0.5), 0.2))
    c = n.node('ShaderNodeMix', data_type='RGBA')
    n.link(mortar, c.inputs['Factor'])
    n.link(n.rgb(n.mul(face, 1.0), n.mul(face, 0.92), n.mul(face, 0.86)), c.inputs[6])
    n.link(n.rgb(1.25, 1.22, 1.15), c.inputs[7])
    h = n.add(n.mul(n.sub(1, mortar), 0.85), n.mul(grain, 0.15))
    return c.outputs[2], h, 3.0, 1.6


def siding(n):
    # clapboard: 10 boards a 2 m tile (≈ 20 cm reveal), each lapping the one below; faint grain under paint
    boards = 10
    bv = n.mul(n.v, boards)
    lap = n.fract(bv)
    h = n.add(n.mul(n.smooth(0.0, 0.92, lap), 0.85), n.mul(n.noise(4, 60, detail=3), 0.15))
    shade = n.add(0.8, n.mul(n.smooth(0.0, 0.25, lap), 0.2))
    c = n.mul(shade, n.add(0.95, n.mul(n.sub(n.noise(2, 30, seed=4), 0.5), 0.08)))
    return grey(n, c), h, 2.5, 2.0


def block(n):
    # concrete masonry units 40 × 20 cm, running bond: 5 × 10 a 2 m tile
    rows, cols = 10, 5
    rv = n.mul(n.v, rows)
    row = n.floor(rv)
    ru = n.add(n.mul(n.u, cols), n.mul(n.math('MODULO', row, 2), 0.5))
    fu, fv = n.fract(ru), n.fract(rv)
    edge = n.math('MINIMUM', n.math('MINIMUM', fu, n.sub(1, fu)), n.mul(n.math('MINIMUM', fv, n.sub(1, fv)), 0.5))
    joint = n.sub(1, n.smooth(0.012, 0.03, edge))
    pores = n.noise(220, detail=2, seed=6)
    tone = n.cell_random(n.math('MODULO', n.floor(ru), cols), row, 2)
    c = n.add(0.82, n.mul(n.sub(tone, 0.5), 0.12), n.mul(n.sub(pores, 0.5), 0.18), n.mul(joint, -0.08))
    h = n.add(n.mul(n.sub(1, joint), 0.8), n.mul(pores, 0.2))
    return grey(n, c), h, 2.6, 2.0


def clay(n):
    # Spanish barrel tiles: 8 across × 6 courses a 2 m tile; each barrel a half-cylinder, rows lapping,
    # every tile its own fired colour, a little lichen and dust in the hollows
    cols, rows = 8, 6
    cu = n.mul(n.u, cols)
    rv = n.mul(n.v, rows)
    barrel = n.math('ABSOLUTE', n.math('SINE', n.mul(n.fract(cu), math.pi)))
    lap = n.fract(rv)
    h = n.add(n.mul(barrel, 0.7), n.mul(n.smooth(0.0, 0.9, lap), 0.3))
    tone = n.cell_random(n.math('MODULO', n.floor(cu), cols), n.floor(rv), 9)
    dust = n.mul(n.sub(1, barrel), n.noise(30, detail=4, seed=8))
    shade = n.add(0.62, n.mul(barrel, 0.3), n.mul(tone, 0.16), n.mul(n.smooth(0.0, 0.3, lap), -0.18), n.mul(dust, -0.15))
    c = n.rgb(n.mul(shade, 1.0), n.mul(shade, n.add(0.9, n.mul(tone, 0.1))), n.mul(shade, 0.85))
    return c, h, 3.2, 2.0


def shingle(n):
    # three-tab asphalt shingles: 14 cm exposure; tabs 33 cm, courses offset half a tab; mineral granules
    rows, cols = 14, 6
    rv = n.mul(n.v, rows)
    row = n.floor(rv)
    ru = n.add(n.mul(n.u, cols), n.mul(n.math('MODULO', row, 2), 0.5))
    fu, fv = n.fract(ru), n.fract(rv)
    slot = n.mul(n.sub(1, n.smooth(0.0, 0.03, n.math('MINIMUM', fu, n.sub(1, fu)))), n.smooth(0.5, 0.6, fv))
    granules = n.noise(320, detail=1, seed=12)
    tone = n.cell_random(n.math('MODULO', n.floor(ru), cols), row, 4)
    h = n.add(n.mul(n.smooth(0.0, 0.95, fv), 0.7), n.mul(granules, 0.3), n.mul(slot, -0.5))
    c = n.add(0.8, n.mul(n.sub(tone, 0.5), 0.18), n.mul(n.sub(granules, 0.5), 0.35), n.mul(slot, -0.35))
    return grey(n, c), h, 2.4, 2.0


def metal(n):
    # corrugated sheet: 7.6 cm pitch — 26 waves a 2 m tile; streaks of rust and dirt running down
    waves = 26
    wave = n.math('SINE', n.mul(n.u, 2 * math.pi * waves))
    h = n.add(0.5, n.mul(wave, 0.5))
    streak = n.noise(40, 2, detail=4, seed=13)
    c = n.add(0.8, n.mul(wave, 0.08), n.mul(n.sub(streak, 0.5), 0.25))
    return grey(n, c), h, 2.2, 2.0


def sidewalk(n):
    # LA's sidewalks: five-foot panels scored into quarters (2 × 2 a 3 m tile), a broom finish across,
    # the odd dark stain (gum, drips)
    panels = 2
    pu, pv = n.mul(n.u, panels), n.mul(n.v, panels)
    fu, fv = n.fract(pu), n.fract(pv)
    joint = n.sub(1, n.smooth(0.004, 0.012, n.math('MINIMUM', n.math('MINIMUM', fu, n.sub(1, fu)), n.math('MINIMUM', fv, n.sub(1, fv)))))
    broom = n.noise(4, 400, detail=2, seed=14)
    stains = n.smooth(0.72, 0.78, n.noise(10, detail=3, seed=15))
    gum = n.smooth(0.08, 0.04, n.voronoi(70, seed=3))
    tone = n.cell_random(n.math('MODULO', n.floor(pu), panels), n.math('MODULO', n.floor(pv), panels), 6)
    c = n.add(0.86, n.mul(n.sub(tone, 0.5), 0.08), n.mul(n.sub(broom, 0.5), 0.04), n.mul(stains, -0.12), n.mul(gum, -0.3), n.mul(joint, -0.25))
    h = n.add(n.mul(n.sub(1, joint), 0.95), n.mul(broom, 0.05))
    return grey(n, c), h, 1.8, 3.0


def asphalt(n):
    # aged asphalt: aggregate showing through, and the tar snakes — black crack sealant in
    # wandering lines — that cover LA's streets
    agg = n.voronoi(240, seed=1)
    stone = n.smooth(0.35, 0.15, agg)
    fine = n.noise(400, detail=1, seed=16)
    crack = n.sub(n.voronoi(5, 'DISTANCE_TO_EDGE', 'Distance', seed=4), 0.0)
    wander = n.mul(n.sub(n.noise(18, detail=3, seed=17), 0.5), 0.05)
    tar = n.mul(n.smooth(0.012, 0.0, n.math('ABSOLUTE', n.add(crack, wander))), n.smooth(0.45, 0.6, n.noise(3, detail=2, seed=18)))
    wear = n.noise(2.5, detail=3, seed=19)
    c = n.add(0.86, n.mul(stone, 0.1), n.mul(n.sub(fine, 0.5), 0.08), n.mul(n.sub(wear, 0.5), 0.15), n.mul(tar, -0.55))
    h = n.add(n.mul(stone, 0.5), n.mul(fine, 0.3), n.mul(tar, 0.25))
    return grey(n, c), h, 0.7, 4.0


def lawn(n):
    blades = n.noise(260, 90, detail=3, seed=20)
    clumps = n.noise(14, detail=4, seed=21)
    dry = n.smooth(0.62, 0.75, n.noise(4, detail=3, seed=22))
    g = n.add(0.8, n.mul(n.sub(blades, 0.5), 0.5), n.mul(n.sub(clumps, 0.5), 0.2))
    c = n.rgb(n.mul(g, n.add(0.95, n.mul(dry, 0.35))), n.mul(g, 1.04), n.mul(g, n.add(0.9, n.mul(dry, 0.05))))
    h = n.add(n.mul(blades, 0.7), n.mul(clumps, 0.3))
    return c, h, 2.2, 2.0


def dryGrass(n):
    stalks = n.noise(180, 40, detail=3, seed=23)
    soil = n.smooth(0.55, 0.45, n.noise(8, detail=4, seed=24))
    g = n.add(0.8, n.mul(n.sub(stalks, 0.5), 0.45))
    c = n.rgb(n.mul(g, n.add(1.0, n.mul(soil, 0.05))), n.mul(g, n.add(0.95, n.mul(soil, -0.08))), n.mul(g, n.add(0.82, n.mul(soil, -0.1))))
    h = n.add(n.mul(stalks, 0.6), n.mul(n.sub(1, soil), 0.4))
    return c, h, 2.0, 3.0


def dirt(n):
    pebbles = n.smooth(0.3, 0.12, n.voronoi(60, seed=7))
    grains = n.noise(300, detail=2, seed=25)
    clods = n.noise(10, detail=5, seed=26)
    g = n.add(0.8, n.mul(pebbles, 0.25), n.mul(n.sub(grains, 0.5), 0.25), n.mul(n.sub(clods, 0.5), 0.2))
    c = n.rgb(g, n.mul(g, 0.94), n.mul(g, 0.86))
    h = n.add(n.mul(pebbles, 0.5), n.mul(clods, 0.3), n.mul(grains, 0.2))
    return c, h, 2.4, 3.0


RECIPES = [stucco, plaster, brick, siding, block, clay, shingle, metal, sidewalk, asphalt, lawn, dryGrass, dirt]

# ------------------------------------------------------------------- bake

def bake(mat, socket):
    em, out = mat_nodes.emit(socket)
    img = bpy.data.images.new('bake', RES, RES, float_buffer=True)
    tex = mat_nodes.node('ShaderNodeTexImage')
    tex.image = img
    mat_nodes.t.nodes.active = tex
    plane.data.materials.clear()
    plane.data.materials.append(mat)
    bpy.context.view_layer.objects.active = plane
    plane.select_set(True)
    bpy.ops.object.bake(type='EMIT')
    px = np.array(img.pixels[:], dtype=np.float32).reshape(RES, RES, 4)[::-1, :, :3]   # top row first
    bpy.data.images.remove(img)
    mat_nodes.t.nodes.remove(tex)
    mat_nodes.t.nodes.remove(em)
    mat_nodes.t.nodes.remove(out)
    return px


def normal_from_height(h, strength, metres):
    """Tangent-space normal map from a tiling height map (periodic differences).
    The height's 0..1 range is taken as `strength` centimetres of relief, so the
    slope is the same whatever resolution the tile is baked at."""
    k = 0.5 * strength * 0.01 * RES / metres
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * k
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * k
    nrm = np.stack([-dx, dy, np.ones_like(h)], axis=-1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    return nrm * 0.5 + 0.5


summary = []
for recipe in RECIPES:
    name = recipe.__name__
    mat_nodes = Nodes(name)
    col, height, strength, metres = recipe(mat_nodes)
    c = bake(mat_nodes.mat, col)
    hgt = bake(mat_nodes.mat, height)[:, :, 0]
    # colour as detail round 0.5: each channel divided by its own average
    detail = np.clip(c / np.maximum(c.reshape(-1, 3).mean(axis=0), 1e-4) * 0.5, 0, 1)
    print(f'    height {hgt.min():.3f}..{hgt.max():.3f}', flush=True)
    nrm = normal_from_height(hgt, strength, metres)
    Image.fromarray((detail * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, f'{name}_c.jpg'), quality=88)
    Image.fromarray((nrm * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, f'{name}_n.jpg'), quality=92)
    summary.append((name, metres))
    print(f'  {name:9s} {metres} m tile', flush=True)

with open(os.path.join(OUT, 'tiles.json'), 'w') as f:
    import json
    json.dump({'res': RES, 'tiles': [{'name': n, 'metres': m} for n, m in summary]}, f, indent=1)
print(f'{len(summary)} tiles → {os.path.relpath(OUT)}')
