"""
The leaves, palm fronds, flowers and bark of LA Royale's trees, modelled
and rendered in Blender.

The game builds its trees from cards — flat cut-outs of foliage — and from
trunks wrapped in bark, all cut from one 4 × 4 atlas (js/flora.js, CELL).
This script makes each cell out of real geometry: hundreds of leaves with a
midrib and a fold, the pleated fan of a Mexican fan palm, a date palm's
feather frond, jacaranda blossom, bougainvillea bracts, bird of paradise; and
bark as a relief lit from the side. Each is rendered from straight above with
Cycles (self-shadowed, so a card has depth) onto a transparent background,
laid out exactly as flora.js draws its own fallback, and saved as one image.
Grey cells are tinted in the game; coloured cells keep their colour.

    .../blenv/bin/python la-royale/tools/blender_flora.py

writes la-royale/textures/flora.webp.
"""

import math
import os
import random

import bpy
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'textures')
S = int(os.environ.get('FLORA_CELL', '512'))
SAMPLES = int(os.environ.get('FLORA_SAMPLES', '48'))

CELL = dict(leaf=0, jacaranda=1, fan=2, feather=3, skirt=4, grass=5, needles=6, bark=7,
            coral=8, bougain=9, paradise=10, magnolia=11, sycBark=12, palmBark=13, canaryBark=14, shrub=15)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = SAMPLES
scene.cycles.use_denoising = True
scene.render.resolution_x = scene.render.resolution_y = S
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.view_settings.view_transform = 'Standard'

world = bpy.data.worlds.new('w')
scene.world = world
world.use_nodes = True
world.node_tree.nodes['Background'].inputs['Color'].default_value = (1, 1, 1, 1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.55

cam_d = bpy.data.cameras.new('cam')
cam_d.type = 'ORTHO'
cam_d.ortho_scale = 1.0
cam = bpy.data.objects.new('cam', cam_d)
cam.location = (0, 0, 5)
scene.collection.objects.link(cam)
scene.camera = cam

sun_d = bpy.data.lights.new('sun', 'SUN')
sun_d.energy = 2.6
sun_d.angle = math.radians(12)
sun = bpy.data.objects.new('sun', sun_d)
sun.rotation_euler = (math.radians(30), math.radians(-18), 0)
scene.collection.objects.link(sun)


def lin(c):
    c = c / 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name, rough=0.6, translucent=0.25, sheen=0.0):
    """a leaf material coloured per face by the mesh's colour attribute"""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes['Principled BSDF']
    attr = nt.nodes.new('ShaderNodeVertexColor')
    attr.layer_name = 'Col'
    nt.links.new(attr.outputs['Color'], p.inputs['Base Color'])
    p.inputs['Roughness'].default_value = rough
    if translucent:
        # leaves let light through: mix in a translucent shader of the same colour
        tr = nt.nodes.new('ShaderNodeBsdfTranslucent')
        nt.links.new(attr.outputs['Color'], tr.inputs['Color'])
        mix = nt.nodes.new('ShaderNodeMixShader')
        mix.inputs['Fac'].default_value = translucent
        out = nt.nodes['Material Output']
        nt.links.new(p.outputs[0], mix.inputs[1])
        nt.links.new(tr.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs['Surface'])
    return m


LEAF_MAT = material('leaf')
DRY_MAT = material('dry', 0.85, 0.1)
BARK_MAT = material('bark', 0.9, 0.0)


class Builder:
    """collects triangles with a colour each, then makes one mesh"""

    def __init__(self):
        self.v, self.f, self.c = [], [], []

    def tri(self, a, b, c, col):
        n = len(self.v)
        self.v += [a, b, c]
        self.f.append((n, n + 1, n + 2))
        self.c.append(col)

    def quad(self, a, b, c, d, col):
        self.tri(a, b, c, col)
        self.tri(a, c, d, col)

    def mesh(self, mat, name='m'):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.v, [], self.f)
        attr = me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
        for i, col in enumerate(self.c):
            lc = (*[lin(x) for x in col], 1.0)
            for k in range(3):
                attr.data[i * 3 + k].color = lc
        me.materials.append(mat)
        me.shade_flat()
        ob = bpy.data.objects.new(name, me)
        scene.collection.objects.link(ob)
        return ob


def rot2(x, y, a):
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def leaf(B, cx, cy, z, length, width, angle, col, tilt=0.25, fold=0.25, seg=6):
    """one leaf: pointed at both ends, folded along its midrib, its tip dipping"""
    pts_l, pts_r, mid = [], [], []
    for i in range(seg + 1):
        t = i / seg
        w = width * math.sin(math.pi * t) ** 0.8 * (1 - 0.25 * t)
        x = t * length
        dz = -tilt * length * t * t
        mid.append((x, 0, dz + fold * width))
        pts_l.append((x, w, dz))
        pts_r.append((x, -w, dz))

    def place(p):
        x, y = rot2(p[0], p[1], angle)
        return (cx + x, cy + y, z + p[2])

    shade = [max(0, min(255, c * (0.92 + 0.12 * random.random()))) for c in col]
    lighter = [min(255, c * 1.06) for c in shade]
    for i in range(seg):
        B.quad(place(mid[i]), place(pts_l[i]), place(pts_l[i + 1]), place(mid[i + 1]), lighter)
        B.quad(place(mid[i]), place(mid[i + 1]), place(pts_r[i + 1]), place(pts_r[i]), shade)


def blob_points(n, radius, lobes=6, rnd=random):
    """points bunched into overlapping lobes, thinning to a ragged edge (as flora.js's canvas does)"""
    L = []
    for i in range(lobes):
        a = i / lobes * 2 * math.pi + rnd.random() * 0.8
        d = 0 if i == 0 else radius * (0.35 + rnd.random() * 0.35)
        L.append((math.cos(a) * d, math.sin(a) * d, radius * (0.32 + rnd.random() * 0.2)))
    out = []
    for k in range(n):
        lx, ly, lr = L[k % lobes]
        a = rnd.random() * 2 * math.pi
        f = rnd.random() ** 0.75
        if f > 0.7 and rnd.random() < (f - 0.7) * 2.2:
            continue
        out.append((lx + math.cos(a) * f * lr, ly + math.sin(a) * f * lr * 0.9, f))
    return out


def grey(l):
    return (l * 255, l * 255, l * 255)


# --------------------------------------------------------------- the cells

def foliage(n, size, colour, inner=0.42, seed=1, extra=None):
    random.seed(seed)
    B = Builder()
    for x, y, f in blob_points(n, inner):
        z = 0.12 * (1 - f) + random.random() * 0.02
        leaf(B, x, y, z, size * (0.6 + random.random() * 0.6), size * 0.32, random.random() * 2 * math.pi, colour(random.random(), f))
    if extra:
        extra(B)
    return [B.mesh(LEAF_MAT)]


def c_leaf():
    return foliage(1500, 0.06, lambda q, f: grey(0.5 + q * 0.4 + (1 - f) * 0.06), seed=2)


def c_shrub():
    return foliage(2200, 0.038, lambda q, f: grey(0.55 + q * 0.38), seed=3)


def c_magnolia():
    def flowers(B):
        for k in range(10):
            x, y = (random.random() - 0.5) * 0.6, (random.random() - 0.5) * 0.6
            for p in range(6):
                leaf(B, x, y, 0.2, 0.05, 0.03, p / 6 * 2 * math.pi + random.random() * 0.3, (244, 241, 230), tilt=-0.4, fold=0.4)
    return foliage(700, 0.11, lambda q, f: (45 + q * 35, 78 + q * 45, 34 + q * 22), seed=4, extra=flowers)


def c_jacaranda():
    def blossom(B):
        for x, y, f in blob_points(2600, 0.42):
            # small bell flowers, in trusses
            col = (150 + random.random() * 50, 120 + random.random() * 40, 205 + random.random() * 40)
            leaf(B, x, y, 0.16 + random.random() * 0.03, 0.022, 0.012, random.random() * 6.3, col, tilt=0, fold=0.6, seg=3)
    return foliage(450, 0.05, lambda q, f: (70 + q * 30, 100 + q * 30, 55 + q * 20), seed=5, extra=blossom)


def c_coral():
    def spikes(B):
        for k in range(26):
            x, y, _ = blob_points(40, 0.38)[k]
            a = random.random() * 6.3
            for j in range(7):
                leaf(B, x + math.cos(a) * j * 0.012, y + math.sin(a) * j * 0.012, 0.18, 0.035, 0.01, a + 0.6 * (j % 2 - 0.5),
                     (225 + random.random() * 25, 60 + random.random() * 50, 30), tilt=-0.2, seg=3)
    return foliage(1100, 0.065, lambda q, f: (70 + q * 30, 110 + q * 40, 50 + q * 20), seed=6, extra=spikes)


def c_bougain():
    def bracts(B):
        for x, y, f in blob_points(1700, 0.42):
            a = random.random() * 6.3
            col = (205 + random.random() * 45, 30 + random.random() * 40, 120 + random.random() * 50)
            for p in range(3):           # three papery bracts round a tiny flower
                leaf(B, x, y, 0.17, 0.028, 0.02, a + p * 2.09, col, tilt=-0.3, fold=0.3, seg=3)
    return foliage(700, 0.05, lambda q, f: (60 + q * 30, 100 + q * 40, 45), seed=7, extra=bracts)


def c_needles():
    random.seed(8)
    B = Builder()
    for k in range(260):
        a = random.random() * 6.3
        r = math.sqrt(random.random()) * 0.42
        x0, y0 = math.cos(a) * r, math.sin(a) * r
        d = a + (random.random() - 0.5)
        for j in range(14):
            px, py = x0 + math.cos(d) * j * 0.008, y0 + math.sin(d) * j * 0.008
            for s in (-1, 1):
                leaf(B, px, py, 0.1 - r * 0.15, 0.03, 0.003, d + s * 0.7, grey(0.45 + random.random() * 0.45), tilt=0.1, fold=0, seg=2)
    return [B.mesh(LEAF_MAT)]


def c_fan():
    """the Mexican fan palm's leaf: forty-odd pleated segments radiating from the petiole's tip,
    their tips split and drooping (the petiole at the bottom, as flora.js maps it)"""
    random.seed(9)
    B = Builder()
    ox, oy = 0, 0.5 - 0.92            # canvas (0.5, 0.92) → the bottom middle
    n = 64
    for k in range(n):
        a0 = math.pi * 0.06 + k / n * math.pi * 0.88
        a1 = math.pi * 0.06 + (k + 1) / n * math.pi * 0.88
        length = 0.62 + random.random() * 0.22
        l = 0.55 + random.random() * 0.4
        up = 0.02 if k % 2 else -0.02           # the pleats: ridges and valleys
        steps = 8
        for i in range(steps):
            t0, t1 = i / steps, (i + 1) / steps
            r0, r1 = 0.06 + length * t0, 0.06 + length * t1
            droop0, droop1 = -0.25 * t0 ** 2, -0.25 * t1 ** 2
            # beyond two thirds out, the segment splits in two
            split = t0 > 0.66
            am = (a0 + a1) / 2
            # each strip narrows to a point at its tip
            taper = lambda t: 1.0 if t < 0.45 else max(0.08, 1 - (t - 0.45) / 0.55 * 0.92)
            for aa, ab in ((a0, am), (am, a1)) if split else ((a0, a1),):
                g = l * (0.9 if aa == a0 else 1.0)
                c, h = (aa + ab) / 2, (ab - aa) / 2
                if split:                  # (the split halves taper from where they part)
                    k0, k1 = taper(t0 + 0.2), taper(t1 + 0.2)
                else:
                    k0, k1 = taper(t0), taper(t1)
                b0, e0, b1, e1 = c - h * k0, c + h * k0, c - h * k1, c + h * k1
                p00 = (ox + math.cos(b0) * r0, oy + math.sin(b0) * r0, droop0)
                p01 = (ox + math.cos(e0) * r0, oy + math.sin(e0) * r0, droop0 + up)
                p11 = (ox + math.cos(e1) * r1, oy + math.sin(e1) * r1, droop1 + up)
                p10 = (ox + math.cos(b1) * r1, oy + math.sin(b1) * r1, droop1)
                B.quad(p00, p10, p11, p01, grey(g))
        # a thread or two hanging from the split tips
        for s in range(2):
            aa = a0 + (a1 - a0) * (0.3 + 0.4 * s)
            r = 0.06 + length
            leaf(B, ox + math.cos(aa) * r, oy + math.sin(aa) * r, -0.26, 0.05, 0.002, aa, grey(l * 0.8), tilt=0.5, fold=0, seg=2)
    # the hastula where the segments meet
    for k in range(12):
        a = k / 12 * 2 * math.pi
        b = (k + 1) / 12 * 2 * math.pi
        B.tri((ox, oy, 0.01), (ox + math.cos(a) * 0.035, oy + math.sin(a) * 0.035, 0.01), (ox + math.cos(b) * 0.035, oy + math.sin(b) * 0.035, 0.01), grey(0.5))
    return [B.mesh(LEAF_MAT)]


def c_feather():
    """a feather frond: the rachis down the middle, stiff leaflets in a V either side, angled down"""
    random.seed(10)
    B = Builder()
    for k in range(int(S / 5 * 1.0)):
        cy = 0.5 - (6 + k * 5) / S                 # (canvas y → Blender y)
        if cy < -0.49:
            break
        t = (0.5 - cy)
        length = 0.46 * math.sin(math.pi * min(1, t * 1.05 + 0.05))
        for s in (-1, 1):
            a = math.atan2(-(0.035 + length * 0.25), s * length) if length > 0.01 else 0
            leaf(B, 0, cy, 0.03, math.hypot(length, 0.035 + length * 0.25), 0.012, a, grey(0.55 + random.random() * 0.4), tilt=0.05, fold=0.8, seg=4)
    for k in range(40):
        y0, y1 = 0.5 - k / 40, 0.5 - (k + 1) / 40
        B.quad((-0.007, y0, 0.04), (0.007, y0, 0.04), (0.007, y1, 0.04), (-0.007, y1, 0.04), grey(0.75))
    return [B.mesh(LEAF_MAT)]


def c_skirt():
    """the skirt of dead fronds hanging down a fan palm's trunk: straw-brown, shaggy"""
    random.seed(11)
    B = Builder()
    for k in range(700):
        x = (random.random() - 0.5)
        y0 = 0.5 - random.random() * 0.2
        length = 0.6 + random.random() * 0.4
        col = (120 + random.random() * 70, 95 + random.random() * 55, 60 + random.random() * 35)
        w = 0.004 + random.random() * 0.006
        sway = (random.random() - 0.5) * 0.06
        z = random.random() * 0.05
        prev = None
        for i in range(7):
            t = i / 6
            p = (x + sway * t * t, y0 - length * t, z + 0.02 * math.sin(t * 9 + k))
            if prev:
                B.quad((prev[0] - w, prev[1], prev[2]), (prev[0] + w, prev[1], prev[2]), (p[0] + w, p[1], p[2]), (p[0] - w, p[1], p[2]), col)
            prev = p
    return [B.mesh(DRY_MAT)]


def c_grass():
    random.seed(12)
    B = Builder()
    for k in range(420):
        x = 0.15 + random.random() * 0.7 - 0.5
        length = 0.4 + random.random() * 0.5
        bend = (random.random() - 0.5) * 0.24
        col = (170 + random.random() * 60, 145 + random.random() * 50, 80 + random.random() * 40)
        w0 = 0.004 + random.random() * 0.003
        prev = None
        for i in range(8):
            t = i / 7
            p = (x + bend * t * t, -0.5 + length * t, random.random() * 0.01)
            w = w0 * (1 - t * 0.9)
            if prev:
                B.quad((prev[0] - pw, prev[1], prev[2]), (prev[0] + pw, prev[1], prev[2]), (p[0] + w, p[1], p[2]), (p[0] - w, p[1], p[2]), col)
            prev, pw = p, w
    return [B.mesh(DRY_MAT)]


def c_paradise():
    """bird of paradise: a paddle leaf on its stalk, and the orange-and-blue flower"""
    random.seed(13)
    B = Builder()
    # the paddle: an ellipse, split here and there between its veins
    cx, cy = 0, 0.5 - 0.4
    n = 24
    for i in range(n):
        for side in (-1, 1):
            t0, t1 = i / n, (i + 1) / n
            y0, y1 = cy + 0.36 * math.cos(math.pi * t0), cy + 0.36 * math.cos(math.pi * t1)
            w0, w1 = 0.2 * math.sin(math.pi * t0), 0.2 * math.sin(math.pi * t1)
            col = (63 + random.random() * 10, 110 + random.random() * 12, 58)
            if random.random() < 0.15:
                w1 *= 0.85
            B.quad((0, y0, 0.03), (side * w0, y0 - 0.02, 0.0), (side * w1, y1 - 0.02, 0.0), (0, y1, 0.03), col)
    B.quad((-0.008, 0.45, 0.04), (0.008, 0.45, 0.04), (0.008, -0.5, 0.04), (-0.008, -0.5, 0.04), (92, 138, 76))
    # the flower: an orange crest over a blue tongue, on a beaked bract
    fx, fy = 0.62 - 0.5, 0.5 - 0.12
    for k in range(4):
        leaf(B, fx, fy, 0.08, 0.2 - k * 0.02, 0.018, 0.4 + k * 0.12, (255, 122, 26), tilt=-0.1, fold=0.4)
    leaf(B, fx + 0.02, fy - 0.03, 0.09, 0.14, 0.015, 0.25, (43, 76, 194), fold=0.5)
    leaf(B, fx, fy - 0.04, 0.06, 0.24, 0.02, 0.1, (70, 90, 70), fold=0.6)
    return [B.mesh(LEAF_MAT)]


# barks: a relief over three by three copies of a periodic height field, lit from the side,
# rendered opaque (the middle copy fills the frame, so the cell tiles both ways)

def bark(height, colour, n=160):
    B = Builder()
    xs = np.linspace(0, 1, n + 1)
    H = np.array([[height(x, y) for x in xs] for y in xs])
    C = [[colour(x, y, H[j][i]) for i, x in enumerate(xs)] for j, y in enumerate(xs)]
    for ty in (-1, 0, 1):
        for tx in (-1, 0, 1):
            for j in range(n):
                for i in range(n):
                    p = lambda ii, jj: (xs[ii] - 0.5 + tx, xs[jj] - 0.5 + ty, H[jj][ii] * 0.03)
                    B.quad(p(i, j), p(i + 1, j), p(i + 1, j + 1), p(i, j + 1), C[j][i])
    return [B.mesh(BARK_MAT)]


def per(x, y, terms):
    """a periodic field: a sum of sines with whole-number frequencies"""
    return sum(a * math.sin(2 * math.pi * (fx * x + fy * y) + ph) for a, fx, fy, ph in terms)


random.seed(20)
FIBRE = [(random.random() * 0.3, random.randint(8, 40), random.randint(0, 3), random.random() * 6.3) for _ in range(14)]
MOTTLE = [(random.random(), random.randint(3, 11), random.randint(2, 9), random.random() * 6.3) for _ in range(10)]


def c_bark():
    h = lambda x, y: abs(per(x, y, FIBRE)) * 1.4
    return bark(h, lambda x, y, z: grey(0.38 + 0.4 * min(1, z)))


def c_sycBark():
    h = lambda x, y: 0.3 * per(x, y, MOTTLE)
    def col(x, y, z):
        m = per(x * 1.0, y, MOTTLE)
        return (230, 225, 212) if m > 0.5 else (196, 186, 160) if m > -0.2 else (157, 154, 134) if m > -0.9 else (194, 180, 142)
    return bark(h, col)


def c_palmBark():
    # Washingtonia: tight horizontal rings of old leaf scars, fine vertical fibres between
    def h(x, y):
        ring = abs(math.sin(math.pi * y * 56)) ** 0.4
        return 0.6 * ring + 0.25 * per(x, y, FIBRE) + 0.1 * math.sin(2 * math.pi * (y * 7 + 0.3 * math.sin(2 * math.pi * x)))
    return bark(h, lambda x, y, z: (130 + 70 * z, 120 + 60 * z, 100 + 50 * z))


def c_canaryBark():
    # Canary Island date palm: a diamond lattice of cut leaf bases
    def h(x, y):
        a = math.sin(2 * math.pi * (x + y) * 5)
        b = math.sin(2 * math.pi * (x - y) * 5)
        return 0.8 * min(abs(a), abs(b)) ** 0.5 + 0.1 * per(x, y, FIBRE)
    return bark(h, lambda x, y, z: (110 + 70 * z, 92 + 55 * z, 62 + 40 * z))


MAKERS = dict(leaf=c_leaf, jacaranda=c_jacaranda, fan=c_fan, feather=c_feather, skirt=c_skirt, grass=c_grass,
              needles=c_needles, bark=c_bark, coral=c_coral, bougain=c_bougain, paradise=c_paradise, magnolia=c_magnolia,
              sycBark=c_sycBark, palmBark=c_palmBark, canaryBark=c_canaryBark, shrub=c_shrub)
OPAQUE = {'bark', 'sycBark', 'palmBark', 'canaryBark'}

atlas = np.zeros((4 * S, 4 * S, 4), np.uint8)
for name, cell in CELL.items():
    obs = MAKERS[name]()
    scene.render.film_transparent = name not in OPAQUE
    # (the barks are lit low from the side, so the relief shows; the foliage from high up)
    sun.rotation_euler = (math.radians(62), math.radians(-25), 0) if name in OPAQUE else (math.radians(30), math.radians(-18), 0)
    path = os.path.join(bpy.app.tempdir or '/tmp', f'flora_{name}.png')
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = np.asarray(Image.open(path).convert('RGBA'))
    if name in OPAQUE:
        img = img.copy()
        img[..., 3] = 255
    ox, oy = (cell % 4) * S, (cell // 4) * S
    atlas[oy:oy + S, ox:ox + S] = img
    for ob in obs:
        bpy.data.objects.remove(ob)
    print(f'  {name}', flush=True)

# (fill the transparent pixels with the colour of their neighbours, so filtering doesn't fringe the cut-outs dark)
rgb, a = atlas[..., :3].astype(np.float32), atlas[..., 3:4].astype(np.float32) / 255
acc, wt = rgb * a, a.copy()
for _ in range(6):
    acc = (acc + np.roll(acc, 1, 0) + np.roll(acc, -1, 0) + np.roll(acc, 1, 1) + np.roll(acc, -1, 1))
    wt = (wt + np.roll(wt, 1, 0) + np.roll(wt, -1, 0) + np.roll(wt, 1, 1) + np.roll(wt, -1, 1))
fill = acc / np.maximum(wt, 1e-6)
out = atlas.copy()
out[..., :3] = np.where(a > 0.5, rgb, fill).astype(np.uint8)
os.makedirs(OUT, exist_ok=True)
Image.fromarray(out, 'RGBA').save(os.path.join(OUT, 'flora.webp'), quality=88, method=6)
print(f'flora atlas {4 * S}² → {os.path.relpath(OUT)}/flora.webp')
