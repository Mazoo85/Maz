"""
The rooms behind LA Royale's windows, modelled and lit in Blender.

Every window in the game looks into a room: an apartment living room, a
bedroom, a kitchen, an office floor, a shop. This script builds eight such
rooms out of simple furniture, renders each one twice with Cycles — by day,
lit by the sky through the window, and by night, lit by its own lamps — and
packs them into two atlases the facade shader samples with parallax
("interior mapping"), so the rooms have real depth as the camera moves.

How a room is rendered: the room is a box W wide, H high and D deep with its
front wall missing. The camera sits C in front of that missing wall, its field
of view just wide enough that the opening exactly fills the frame. A point in the
room at depth z then lands on the image at 0.5 + (p - 0.5) * C / (C + z),
which is what the shader inverts (render.js, ROOM_*).

    .../blenv/bin/python la-royale/tools/blender_rooms.py

writes la-royale/textures/rooms_day.jpg, rooms_night.jpg and rooms.json.
"""

import json
import math
import os
import sys

import bpy
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'textures')
TW, TH = 320, 240                        # one room's image
COLS, ROWS = 4, 2                        # the atlas
W, H, D = 4.0, 3.0, 4.5                  # the room, in metres
C = W                                    # the camera's distance in front of the opening (53° wide)
SAMPLES = int(os.environ.get('ROOM_SAMPLES', '96'))

# ------------------------------------------------------------------ scene

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = SAMPLES
scene.cycles.use_denoising = True
try:
    scene.cycles.denoiser = 'OPENIMAGEDENOISE'
except TypeError:
    pass
scene.cycles.max_bounces = 6
scene.render.resolution_x, scene.render.resolution_y = TW, TH
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'

world = bpy.data.worlds.new('world')
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes['Background']

cam_data = bpy.data.cameras.new('cam')
cam_data.sensor_fit = 'HORIZONTAL'
cam_data.angle = 2 * math.atan(W / 2 / C)
cam_data.clip_start = 0.05
cam = bpy.data.objects.new('cam', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
# Blender: x right, y into the room, z up; the opening is the plane y = 0
cam.location = (W / 2, -C, H / 2)
cam.rotation_euler = (math.radians(90), 0, 0)

_mats = {}


def mat(name, rgb, rough=0.7, emit=None, strength=0.0, metal=0.0):
    key = (name, rgb, rough, emit, strength, metal)
    if key in _mats:
        return _mats[key]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*srgb(rgb), 1)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    if emit is not None:
        p.inputs['Emission Color'].default_value = (*srgb(emit), 1)
        p.inputs['Emission Strength'].default_value = strength
    _mats[key] = m
    return m


def srgb(rgb):
    """an sRGB 0..255 colour to Blender's linear"""
    return tuple(((c / 255) / 12.92) if c / 255 <= 0.04045 else (((c / 255) + 0.055) / 1.055) ** 2.4 for c in rgb)


class Room:
    """One room: its objects live in their own collection, so rooms render one at a time."""

    def __init__(self, name):
        self.col = bpy.data.collections.new(name)
        scene.collection.children.link(self.col)
        self.lights = []          # (light object, on by day?)
        self.glows = []           # (material, strength at night, strength by day)

    def box(self, x0, y0, z0, x1, y1, z1, m):
        me = bpy.data.meshes.new('b')
        vs = [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
        fs = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        me.from_pydata(vs, [], fs)
        me.materials.append(m)
        ob = bpy.data.objects.new('b', me)
        self.col.objects.link(ob)
        return ob

    def cyl(self, x, y, z0, z1, r, m, seg=16):
        me = bpy.data.meshes.new('c')
        vs, fs = [], []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            vs += [(x + r * math.cos(a), y + r * math.sin(a), z0), (x + r * math.cos(a), y + r * math.sin(a), z1)]
        for i in range(seg):
            j = (i + 1) % seg
            fs.append((2 * i, 2 * j, 2 * j + 1, 2 * i + 1))
        fs.append(tuple(2 * i for i in range(seg))[::-1])
        fs.append(tuple(2 * i + 1 for i in range(seg)))
        me.from_pydata(vs, [], fs)
        me.materials.append(m)
        ob = bpy.data.objects.new('c', me)
        self.col.objects.link(ob)
        return ob

    def ball(self, x, y, z, r, m):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=(x, y, z), segments=16, ring_count=10)
        ob = bpy.context.active_object
        ob.data.materials.append(m)
        for c in ob.users_collection:
            c.objects.unlink(ob)
        self.col.objects.link(ob)
        return ob

    def shell(self, wall, floor, ceiling, back=None):
        """the walls, floor and ceiling (no front wall: that's the window)"""
        t = 0.1
        self.box(-t, 0, -t, W + t, D + t, 0, floor)
        self.box(-t, 0, H, W + t, D + t, H + t, ceiling)
        self.box(-t, 0, 0, 0, D, H, wall)
        self.box(W, 0, 0, W + t, D, H, wall)
        self.box(0, D, 0, W, D + t, H, back or wall)
        # a skirting board and the window's own reveal at the opening
        trim = mat('trim', (235, 232, 224), 0.5)
        for x0, x1 in ((0, 0.02), (W - 0.02, W)):
            self.box(x0, 0, 0, x1, D, 0.1, trim)
        self.box(0, D - 0.02, 0, W, D, 0.1, trim)

    def light(self, kind, loc, energy, rgb, size=0.3, day=False, rot=(0, 0, 0)):
        ld = bpy.data.lights.new('l', kind)
        ld.energy = energy
        ld.color = srgb(rgb)
        if kind == 'AREA':
            ld.size = size
        elif kind in ('POINT', 'SPOT'):
            ld.shadow_soft_size = size
        ob = bpy.data.objects.new('l', ld)
        ob.location = loc
        ob.rotation_euler = rot
        ob['energy'] = energy
        self.col.objects.link(ob)
        self.lights.append((ob, day))
        return ob

    def glow(self, name, rgb, night, day=0.0):
        m = mat(name, rgb, 0.4, emit=rgb, strength=night)
        self.glows.append((m, night, day))
        return m


def frame(room, x, z, w, h, picture):
    """a framed picture hung on the back wall"""
    room.box(x - 0.03, D - 0.05, z - 0.03, x + w + 0.03, D - 0.01, z + h + 0.03, mat('frame', (40, 32, 26), 0.5))
    room.box(x, D - 0.06, z, x + w, D - 0.05, z + h, mat('pic' + str(picture), picture, 0.6))


def plant(room, x, y, s=1.0):
    room.cyl(x, y, 0, 0.35 * s, 0.17 * s, mat('pot', (180, 90, 60), 0.8))
    leaf = mat('leaf', (60, 105, 50), 0.7)
    for dx, dy, dz, r in ((0, 0, 0.75, 0.3), (0.12, -0.08, 0.95, 0.22), (-0.12, 0.06, 1.05, 0.2)):
        room.ball(x + dx * s, y + dy * s, dz * s, r * s, leaf)


# ------------------------------------------------------------------ rooms

def living():
    r = Room('living')
    r.shell(mat('wall', (226, 214, 192), 0.85), mat('wood', (126, 88, 58), 0.45), mat('ceil', (238, 236, 230), 0.9))
    sofa = mat('sofa', (92, 108, 120), 0.9)
    r.box(0.7, D - 1.0, 0, 3.3, D - 0.15, 0.45, sofa)
    r.box(0.7, D - 0.35, 0.45, 3.3, D - 0.15, 0.85, sofa)
    r.box(0.55, D - 1.0, 0, 0.75, D - 0.15, 0.62, sofa)
    r.box(3.25, D - 1.0, 0, 3.45, D - 0.15, 0.62, sofa)
    for i, c in enumerate(((200, 170, 90), (170, 70, 60))):
        r.box(0.95 + i * 1.7, D - 0.45, 0.45, 1.4 + i * 1.7, D - 0.3, 0.8, mat('cush' + str(i), c, 0.9))
    r.box(1.2, D - 2.1, 0, 2.8, D - 1.4, 0.42, mat('table', (70, 52, 40), 0.4))
    r.box(0.4, D - 2.8, 0.001, 3.6, D - 0.9, 0.012, mat('rug', (168, 120, 96), 1.0))
    frame(r, 1.35, 1.35, 1.3, 0.85, (60, 120, 160))
    # a bookcase on the left wall, a floor lamp in the corner
    r.box(0.02, 1.4, 0, 0.42, 2.6, 2.0, mat('shelf', (230, 225, 215), 0.6))
    for k in range(4):
        for j in range(5):
            c = [(150, 40, 40), (40, 60, 120), (200, 180, 120), (60, 90, 60), (30, 30, 30)][(k + j) % 5]
            r.box(0.06, 1.48 + j * 0.22, 0.12 + k * 0.48, 0.38, 1.62 + j * 0.22, 0.42 + k * 0.48, mat('book', c, 0.8))
    r.cyl(3.7, D - 0.4, 0, 1.5, 0.02, mat('metal', (40, 40, 40), 0.4, metal=1))
    r.cyl(3.7, D - 0.4, 1.4, 1.75, 0.2, r.glow('shade', (255, 226, 180), 4.0))
    r.light('POINT', (3.7, D - 0.4, 1.6), 35, (255, 200, 140), 0.15)
    r.light('AREA', (W / 2, D / 2, H - 0.05), 45, (255, 220, 180), 1.0, rot=(0, 0, 0))
    plant(r, 0.45, D - 0.45)
    return r


def living_tv():
    r = Room('tv')
    r.shell(mat('wall2', (196, 204, 200), 0.85), mat('carpet', (150, 140, 128), 1.0), mat('ceil', (238, 236, 230), 0.9))
    # the TV on the left wall, the couch facing it, side-on to the window
    r.box(0.03, 1.3, 0, 0.5, 3.2, 0.5, mat('console', (50, 40, 36), 0.5))
    r.box(0.04, 1.6, 0.85, 0.1, 2.9, 1.6, mat('tvframe', (15, 15, 15), 0.3))
    r.box(0.1, 1.65, 0.9, 0.11, 2.85, 1.55, r.glow('screen', (110, 150, 255), 8.0, 0.0))
    sofa = mat('sofa2', (120, 92, 70), 0.9)
    r.box(2.6, 1.2, 0, 3.4, 3.4, 0.45, sofa)
    r.box(3.2, 1.2, 0.45, 3.4, 3.4, 0.85, sofa)
    frame(r, 0.7, 1.4, 0.7, 0.9, (190, 150, 90))
    frame(r, 1.6, 1.6, 0.5, 0.5, (90, 60, 80))
    r.light('AREA', (0.4, 2.2, 1.2), 14, (130, 160, 255), 1.0, rot=(0, math.radians(-90), 0))
    r.light('POINT', (3.6, D - 0.3, 1.0), 25, (255, 190, 130), 0.2)
    plant(r, 3.6, D - 0.35, 1.2)
    return r


def bedroom():
    r = Room('bed')
    r.shell(mat('wall3', (210, 196, 205), 0.85), mat('wood2', (150, 110, 78), 0.5), mat('ceil', (238, 236, 230), 0.9))
    r.box(1.0, D - 2.2, 0, 3.0, D - 0.05, 0.5, mat('bedframe', (90, 70, 56), 0.6))
    r.box(1.05, D - 2.15, 0.5, 2.95, D - 0.1, 0.62, mat('sheet', (240, 240, 236), 0.9))
    r.box(1.05, D - 1.6, 0.6, 2.95, D - 0.1, 0.66, mat('duvet', (70, 110, 150), 0.95))
    for x in (1.2, 2.15):
        r.box(x, D - 0.45, 0.62, x + 0.65, D - 0.12, 0.78, mat('pillow', (245, 245, 240), 0.95))
    r.box(1.0, D - 0.12, 0, 3.0, D - 0.04, 1.2, mat('headboard', (90, 70, 56), 0.6))
    for x in (0.45, 3.1):
        r.box(x, D - 0.5, 0, x + 0.45, D - 0.08, 0.55, mat('stand', (200, 190, 175), 0.5))
        r.cyl(x + 0.22, D - 0.3, 0.55, 0.9, 0.13, r.glow('bedlamp', (255, 214, 170), 5.0))
        r.light('POINT', (x + 0.22, D - 0.3, 0.8), 25, (255, 190, 130), 0.1)
    r.box(0.02, 0.6, 0, 0.62, 1.8, 2.1, mat('closet', (236, 230, 220), 0.6))
    frame(r, 1.6, 1.55, 0.8, 0.55, (210, 120, 80))
    return r


def kitchen():
    r = Room('kitchen')
    r.shell(mat('wall4', (238, 232, 214), 0.8), mat('tilefloor', (200, 196, 186), 0.4), mat('ceil', (238, 236, 230), 0.9),
            back=mat('backsplash', (180, 200, 196), 0.3))
    cab = mat('cab', (246, 244, 238), 0.5)
    r.box(0.3, D - 0.62, 0, 3.0, D - 0.02, 0.9, cab)
    r.box(0.3, D - 0.64, 0.9, 3.0, D - 0.02, 0.94, mat('counter', (60, 60, 64), 0.25))
    r.box(0.3, D - 0.38, 1.45, 3.0, D - 0.02, 2.25, cab)
    r.box(3.05, D - 0.75, 0, 3.85, D - 0.05, 1.9, mat('fridge', (200, 202, 206), 0.25, metal=0.8))
    r.box(1.1, D - 0.6, 0.95, 1.6, D - 0.3, 1.05, mat('sink', (180, 180, 185), 0.2, metal=1))
    r.box(1.3, 1.4, 0.72, 2.7, 2.3, 0.77, mat('table2', (180, 140, 100), 0.4))
    for x, y in ((1.35, 1.45), (2.6, 1.45), (1.35, 2.2), (2.6, 2.2)):
        r.cyl(x, y, 0, 0.72, 0.03, mat('metal', (40, 40, 40), 0.4, metal=1))
    for x in (1.3, 2.7):
        r.box(x - 0.2, 1.1 if x < 2 else 2.4, 0, x + 0.2, 1.5 if x < 2 else 2.8, 0.45, mat('chair', (160, 60, 40), 0.6))
    r.cyl(2.0, 1.85, 2.35, 3.0, 0.01, mat('metal', (40, 40, 40), 0.4, metal=1))
    r.cyl(2.0, 1.85, 2.2, 2.38, 0.22, r.glow('pendant', (255, 230, 190), 8.0))
    r.light('POINT', (2.0, 1.85, 2.1), 90, (255, 215, 170), 0.2)
    r.light('AREA', (1.6, D - 0.5, 1.4), 30, (255, 240, 220), 0.6, rot=(0, math.radians(180), 0))
    return r


def office(variant=0):
    r = Room('office' + str(variant))
    r.shell(mat('owall', (214, 214, 210), 0.8), mat('ocarpet', (88, 92, 98), 1.0), mat('tiles', (232, 232, 228), 0.9))
    # the dropped ceiling's grid and its fluorescent panels — on by day as well as by night
    grid = mat('grid', (190, 190, 186), 0.6)
    for i in range(1, 7):
        r.box(i * W / 6.5 - 0.01, 0, H - 0.02, i * W / 6.5 + 0.01, D, H, grid)
    for j in range(1, 7):
        r.box(0, j * D / 6.5 - 0.01, H - 0.02, W, j * D / 6.5 + 0.01, H, grid)
    panel = r.glow('fluor', (235, 242, 255), 9.0, 4.0)
    for x in (0.8, 2.6):
        for y in (1.0, 2.6, 4.0):
            r.box(x, y, H - 0.03, x + 0.6, y + 0.6, H - 0.01, panel)
            r.light('AREA', (x + 0.3, y + 0.3, H - 0.06), 70, (235, 242, 255), 0.6, day=True)
    if variant == 0:
        # cubicles: partitions, desks, monitors, chairs
        part = mat('partition', (120, 130, 140), 0.9)
        for y in (D - 1.7,):
            r.box(0.2, y, 0, 3.8, y + 0.06, 1.15, part)
            for x in (0.3, 1.6, 2.9):
                r.box(x, y + 0.06, 0.72, x + 1.0, y + 0.8, 0.76, mat('desk', (196, 186, 170), 0.5))
                r.box(x + 0.35, y + 0.12, 0.76, x + 0.75, y + 0.16, 1.1, mat('mon', (25, 25, 28), 0.3))
                r.box(x + 0.37, y + 0.165, 0.79, x + 0.73, y + 0.17, 1.07, r.glow('monscreen', (150, 190, 255), 3.0, 1.0))
                r.box(x + 0.3, y + 0.9, 0, x + 0.7, y + 1.3, 0.5, mat('ochair', (30, 30, 34), 0.6))
        r.box(1.2, D - 0.03, 0.9, 2.8, D - 0.01, 1.8, mat('whiteboard', (245, 245, 245), 0.2))
    else:
        # a meeting room: a long table and chairs, a screen, glass to the corridor
        r.box(0.8, 1.2, 0.72, 3.2, 3.6, 0.78, mat('mtable', (90, 70, 56), 0.3))
        for y in (1.5, 2.3, 3.1):
            for x in (0.45, 3.25):
                r.box(x, y, 0, x + 0.35, y + 0.4, 0.5, mat('ochair', (30, 30, 34), 0.6))
                r.box(x + (0.3 if x < 2 else 0), y, 0.5, x + (0.35 if x < 2 else 0.05), y + 0.4, 1.0, mat('ochair', (30, 30, 34), 0.6))
        r.box(1.2, D - 0.04, 1.0, 2.8, D - 0.02, 1.9, r.glow('confscreen', (120, 170, 230), 4.0, 2.0))
        plant(r, 0.4, D - 0.4, 1.3)
        plant(r, 3.6, D - 0.4, 1.3)
    return r


def shop():
    r = Room('shop')
    r.shell(mat('swall', (244, 244, 240), 0.7), mat('sfloor', (215, 210, 200), 0.3), mat('sceil', (250, 250, 250), 0.9))
    shelf = mat('sshelf', (250, 250, 250), 0.4)
    colours = [(220, 60, 50), (240, 200, 60), (60, 140, 210), (90, 180, 90), (240, 140, 50), (200, 80, 160), (250, 250, 250)]
    for x0, x1, y0, y1 in ((0.02, 0.45, 0.6, D - 0.1), (W - 0.45, W - 0.02, 0.6, D - 0.1), (0.5, W - 0.5, D - 0.45, D - 0.02)):
        # the shelving's back panel and its shelves, stocked
        if x1 - x0 < 1:
            r.box(x0 if x0 < 1 else x1 - 0.04, y0, 0, x0 + 0.04 if x0 < 1 else x1, y1, 2.0, shelf)
        else:
            r.box(x0, y1 - 0.04, 0, x1, y1, 2.0, shelf)
        for k in range(5):
            r.box(x0, y0, 0.2 + k * 0.45, x1, y1, 0.23 + k * 0.45, shelf)
        for k in range(4):
            z = 0.23 + k * 0.45
            n = 10
            for i in range(n):
                c = colours[(i * 3 + k) % len(colours)]
                if x1 - x0 < 1:
                    a = y0 + (y1 - y0) * i / n
                    r.box(x0 + 0.05, a + 0.03, z, x1 - 0.05, a + (y1 - y0) / n - 0.03, z + 0.3, mat('prod', c, 0.5))
                else:
                    a = x0 + (x1 - x0) * i / n
                    r.box(a + 0.03, y0 + 0.05, z, a + (x1 - x0) / n - 0.03, y1 - 0.05, z + 0.3, mat('prod', c, 0.5))
    r.box(1.2, 1.4, 0, 2.8, 2.0, 0.95, mat('counter2', (60, 60, 64), 0.4))
    track = r.glow('track', (255, 246, 230), 12.0, 8.0)
    for x in (1.0, 2.0, 3.0):
        for y in (1.0, 2.5, 3.8):
            r.box(x - 0.1, y - 0.1, H - 0.04, x + 0.1, y + 0.1, H - 0.01, track)
            r.light('SPOT', (x, y, H - 0.06), 120, (255, 240, 220), 0.1, day=True)
    return r


def vacant():
    r = Room('vacant')
    r.shell(mat('vwall', (200, 196, 186), 0.95), mat('concrete', (150, 146, 140), 0.9), mat('vceil', (210, 208, 200), 0.95))
    box = mat('carton', (176, 140, 96), 0.9)
    for x, y, s in ((0.5, D - 0.8, 0.6), (1.1, D - 0.7, 0.5), (0.7, D - 0.75, 0.45), (3.0, D - 1.4, 0.55)):
        r.box(x, y, 0, x + s, y + s, s, box)
    r.box(0.8, D - 0.8, 0.6, 1.25, D - 0.35, 1.0, box)
    r.box(2.0, D - 0.06, 0.6, 3.5, D - 0.03, 2.4, mat('door', (120, 110, 100), 0.6))
    r.cyl(W / 2, D / 2, H - 0.25, H, 0.01, mat('metal', (40, 40, 40), 0.4, metal=1))
    r.ball(W / 2, D / 2, H - 0.3, 0.06, r.glow('bulb', (255, 220, 160), 20.0))
    r.light('POINT', (W / 2, D / 2, H - 0.4), 40, (255, 210, 150), 0.05)
    return r


ROOMS = [living(), living_tv(), bedroom(), kitchen(), office(0), office(1), shop(), vacant()]

# daylight: the sky's light pouring in through the opening, and a patch of sun on the floor
sun_d = bpy.data.lights.new('sun', 'SUN')
sun_d.energy = 3.0
sun_d.angle = math.radians(2)
sun_d.color = srgb((255, 244, 226))
sun = bpy.data.objects.new('sun', sun_d)
sun.rotation_euler = (math.radians(50), 0, math.radians(25))
scene.collection.objects.link(sun)


def render(room, night):
    for r in ROOMS:
        r.col.hide_render = r is not room
    for ob, day in room.lights:
        ob.hide_render = not (night or day)
        ob.data.energy = ob['energy'] * (1.0 if night else 0.35)    # (by day the room's own lights count for less)
    for m, n, d in room.glows:
        m.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value = n if night else d
    sun.hide_render = night
    bg.inputs['Color'].default_value = (0.02, 0.025, 0.04, 1) if night else (0.62, 0.72, 0.85, 1)
    bg.inputs['Strength'].default_value = 0.3 if night else 1.6
    path = os.path.join(bpy.app.tempdir or '/tmp', f'room_{room.col.name}_{int(night)}.png')
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return np.asarray(Image.open(path).convert('RGB'))


os.makedirs(OUT, exist_ok=True)
for night in (False, True):
    atlas = np.zeros((ROWS * TH, COLS * TW, 3), np.uint8)
    for i, room in enumerate(ROOMS):
        img = render(room, night)
        r, c = divmod(i, COLS)
        atlas[r * TH:(r + 1) * TH, c * TW:(c + 1) * TW] = img
        print(f'  {room.col.name:9s} {"night" if night else "day"}', flush=True)
    Image.fromarray(atlas).save(os.path.join(OUT, 'rooms_night.jpg' if night else 'rooms_day.jpg'), quality=90)

with open(os.path.join(OUT, 'rooms.json'), 'w') as f:
    json.dump({'cols': COLS, 'rows': ROWS, 'tile': [TW, TH], 'room': [W, H, D], 'camera': C,
               'rooms': [r.col.name for r in ROOMS]}, f, indent=1)
print(f'{len(ROOMS)} rooms → {os.path.relpath(OUT)}')
