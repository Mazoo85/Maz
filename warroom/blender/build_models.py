"""Build every 3D piece of War Room 1936 in Blender and export it as glTF.

    python build_models.py --out ../models            # with `pip install bpy` (Blender as a module)
    blender -b -P build_models.py -- --out ../models  # with a full Blender

Every model is built from primitives in code: no hand-made .blend file to lose, and the
same bytes come out on every machine. Models are low-poly and flat-shaded, meant to be
read from orbit: counters (a plinth with a raised symbol), pieces (vehicles, ships,
planes) and markers (buildings and events). Each uses up to three materials named
Body, Accent and Base; the game tints Body with the owning country's colour.

All models are 1 unit tall at most and sit on the origin with +Z up (glTF export turns
that into +Y up); the pivot is the centre of the base.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys

import bpy
from mathutils import Vector

PALETTE = {
    "Body": (0.78, 0.78, 0.80, 1.0),
    "Accent": (0.12, 0.12, 0.13, 1.0),
    "Base": (0.20, 0.22, 0.26, 1.0),
    "Glass": (0.55, 0.75, 0.90, 1.0),
    "Warm": (0.85, 0.65, 0.30, 1.0),
    "Red": (0.75, 0.15, 0.12, 1.0),
}


# ----------------------------------------------------------------------------- helpers

def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for m in list(bpy.data.materials):
        bpy.data.materials.remove(m)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)


def material(name: str):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = PALETTE[name]
        bsdf.inputs["Roughness"].default_value = 0.7
        bsdf.inputs["Metallic"].default_value = 0.0
    return m


def _finish(obj, mat: str):
    obj.data.materials.append(material(mat))
    for poly in obj.data.polygons:
        poly.use_smooth = False
    return obj


def box(size, loc, mat="Body", rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    o = bpy.context.active_object
    o.scale = Vector(size)
    bpy.ops.object.transform_apply(scale=True)
    return _finish(o, mat)


def cyl(radius, depth, loc, mat="Body", verts=12, rot=(0, 0, 0), radius_top=None):
    if radius_top is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc, rotation=rot)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=radius, radius2=radius_top, depth=depth, location=loc, rotation=rot)
    return _finish(bpy.context.active_object, mat)


def cone(radius, depth, loc, mat="Body", verts=12, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=radius, radius2=0.0, depth=depth, location=loc, rotation=rot)
    return _finish(bpy.context.active_object, mat)


def sphere(radius, loc, mat="Body", segs=10, rings=6):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segs, ring_count=rings, radius=radius, location=loc)
    return _finish(bpy.context.active_object, mat)


def wedge(width, length, height, loc, mat="Body", rot=(0, 0, 0)):
    """A triangular prism lying along +Y (a hull bow, a roof)."""
    verts = [(-width / 2, 0, 0), (width / 2, 0, 0), (0, length, 0), (-width / 2, 0, height), (width / 2, 0, height), (0, length, height)]
    faces = [(0, 1, 2), (3, 5, 4), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)]
    mesh = bpy.data.meshes.new("wedge")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    o = bpy.data.objects.new("wedge", mesh)
    bpy.context.collection.objects.link(o)
    o.location = loc
    o.rotation_euler = rot
    return _finish(o, mat)


def join_all(name: str):
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.active_object
    o.name = name
    o.data.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # normalise: rest on z = 0, no taller than 1 unit
    zs = [(o.matrix_world @ v.co).z for v in o.data.vertices]
    zmin, zmax = min(zs), max(zs)
    scale = 1.0 / max(zmax - zmin, 1e-6) if (zmax - zmin) > 1.0 else 1.0
    for v in o.data.vertices:
        v.co.z -= zmin
        v.co *= scale
    o.data.update()
    return o


# ----------------------------------------------------------------------------- models

def counter(symbol: str):
    """A rounded plinth with a raised NATO-style symbol: the division/fleet/wing marker."""
    cyl(0.5, 0.12, (0, 0, 0.06), "Base", verts=24)
    cyl(0.44, 0.06, (0, 0, 0.15), "Body", verts=24)
    z = 0.21
    if symbol == "infantry":           # X
        box((0.05, 0.5, 0.04), (0, 0, z), "Accent", rot=(0, 0, math.radians(45)))
        box((0.05, 0.5, 0.04), (0, 0, z), "Accent", rot=(0, 0, math.radians(-45)))
    elif symbol == "armour":           # ellipse
        cyl(0.2, 0.04, (0, 0, z), "Accent", verts=16)
        cyl(0.14, 0.05, (0, 0, z), "Body", verts=16)
        bpy.context.active_object.scale = (1.0, 0.6, 1.0)
        bpy.ops.object.transform_apply(scale=True)
    elif symbol == "motorised":        # X with a dot
        box((0.05, 0.5, 0.04), (0, 0, z), "Accent", rot=(0, 0, math.radians(45)))
        box((0.05, 0.5, 0.04), (0, 0, z), "Accent", rot=(0, 0, math.radians(-45)))
        cyl(0.07, 0.06, (0, 0, z + 0.01), "Accent", verts=12)
    elif symbol == "special":          # chevron
        box((0.05, 0.3, 0.04), (-0.08, 0, z), "Accent", rot=(0, 0, math.radians(35)))
        box((0.05, 0.3, 0.04), (0.08, 0, z), "Accent", rot=(0, 0, math.radians(-35)))
    elif symbol == "fleet":            # anchor-ish: ring + bar
        cyl(0.16, 0.04, (0, 0.05, z), "Accent", verts=16)
        cyl(0.11, 0.05, (0, 0.05, z), "Body", verts=16)
        box((0.05, 0.3, 0.04), (0, -0.08, z), "Accent")
        box((0.22, 0.05, 0.04), (0, -0.2, z), "Accent")
    elif symbol == "air":              # propeller: two blades + hub
        box((0.5, 0.07, 0.04), (0, 0, z), "Accent")
        box((0.07, 0.5, 0.04), (0, 0, z), "Accent")
        cyl(0.07, 0.06, (0, 0, z + 0.01), "Accent", verts=12)


def infantry():
    box((0.32, 0.32, 0.05), (0, 0, 0.025), "Base")
    cyl(0.08, 0.3, (0, 0, 0.2), "Body", verts=8)         # torso
    sphere(0.07, (0, 0, 0.42), "Body")                  # head
    cyl(0.09, 0.03, (0, 0, 0.47), "Accent", verts=8)    # helmet brim
    sphere(0.075, (0, 0, 0.47), "Accent")               # helmet
    box((0.03, 0.03, 0.42), (0.11, 0.0, 0.26), "Accent", rot=(0, math.radians(10), 0))  # rifle


def tank(size: str):
    L, Wd, Hh = {"light": (0.45, 0.26, 0.12), "medium": (0.55, 0.3, 0.14), "heavy": (0.65, 0.36, 0.17)}[size]
    box((Wd, L, Hh), (0, 0, Hh / 2 + 0.04), "Body")
    box((Wd * 0.3, L * 1.02, 0.09), (-Wd * 0.42, 0, 0.055), "Accent")    # tracks
    box((Wd * 0.3, L * 1.02, 0.09), (Wd * 0.42, 0, 0.055), "Accent")
    cyl(Wd * 0.32, Hh * 0.7, (0, -L * 0.05, Hh + 0.04 + Hh * 0.35), "Body", verts=10)   # turret
    cyl(0.018, L * 0.7, (0, L * 0.3, Hh + 0.04 + Hh * 0.4), "Accent", verts=6, rot=(math.radians(90), 0, 0))  # gun


def truck():
    box((0.26, 0.28, 0.16), (0, -0.18, 0.16), "Body")               # cab
    box((0.28, 0.42, 0.2), (0, 0.17, 0.18), "Accent")              # bed cover
    for x in (-0.14, 0.14):
        for y in (-0.22, 0.1, 0.26):
            cyl(0.05, 0.05, (x, y, 0.05), "Accent", verts=8, rot=(0, math.radians(90), 0))


def artillery():
    box((0.1, 0.3, 0.05), (0, 0, 0.09), "Body")
    cyl(0.025, 0.55, (0, 0.2, 0.17), "Accent", verts=8, rot=(math.radians(-70), 0, 0))
    for x in (-0.14, 0.14):
        cyl(0.09, 0.03, (x, 0, 0.09), "Accent", verts=12, rot=(0, math.radians(90), 0))
    box((0.04, 0.3, 0.03), (-0.07, -0.2, 0.05), "Body", rot=(0, 0, math.radians(15)))
    box((0.04, 0.3, 0.03), (0.07, -0.2, 0.05), "Body", rot=(0, 0, math.radians(-15)))


def plane(kind: str):
    if kind == "fighter":
        fus_len, span = 0.5, 0.6
    else:
        fus_len, span = 0.7, 1.0
    cyl(0.05, fus_len, (0, 0, 0.2), "Body", verts=8, rot=(math.radians(90), 0, 0))
    cone(0.05, 0.12, (0, fus_len / 2 + 0.06, 0.2), "Accent", verts=8, rot=(math.radians(90), 0, 0))
    box((span, 0.14, 0.02), (0, 0.02, 0.2), "Body")
    box((0.3, 0.09, 0.015), (0, -fus_len / 2 + 0.03, 0.2), "Body")
    box((0.015, 0.09, 0.12), (0, -fus_len / 2 + 0.03, 0.26), "Accent")
    if kind == "bomber":
        for x in (-0.25, 0.25):
            cyl(0.04, 0.16, (x, 0.08, 0.2), "Accent", verts=8, rot=(math.radians(90), 0, 0))
    cyl(0.02, 0.35, (0, 0, 0.0), "Base", verts=6)   # a thin stand so it hovers above the counter


def ship(kind: str):
    L = {"destroyer": 0.6, "cruiser": 0.8, "battleship": 1.0, "carrier": 1.0, "submarine": 0.5, "convoy": 0.55}[kind]
    Wd = {"destroyer": 0.1, "cruiser": 0.14, "battleship": 0.2, "carrier": 0.22, "submarine": 0.08, "convoy": 0.14}[kind]
    box((Wd, L * 0.75, 0.06), (0, -L * 0.1, 0.03), "Body")
    wedge(Wd, L * 0.3, 0.06, (0, L * 0.27, 0.0), "Body")
    if kind == "carrier":
        box((Wd * 1.3, L * 0.95, 0.02), (0, 0, 0.07), "Accent")
        box((0.05, 0.1, 0.08), (Wd * 0.5, 0.05, 0.12), "Body")
    elif kind == "submarine":
        box((0.04, 0.1, 0.07), (0, 0, 0.09), "Accent")
    elif kind == "convoy":
        box((Wd * 0.8, L * 0.25, 0.08), (0, -L * 0.25, 0.1), "Accent")
        cyl(0.02, 0.08, (0, -L * 0.25, 0.18), "Accent", verts=6)
    else:
        box((Wd * 0.7, L * 0.3, 0.08), (0, -L * 0.05, 0.1), "Accent")
        cyl(0.02, 0.1, (0, -L * 0.05, 0.19), "Accent", verts=6)
        turrets = {"destroyer": 1, "cruiser": 2, "battleship": 3}[kind]
        for i in range(turrets):
            y = L * 0.15 + i * 0.1 if i < 2 else -L * 0.33
            cyl(Wd * 0.3, 0.04, (0, y, 0.08), "Accent", verts=8)
            cyl(0.01, 0.12, (0, y + 0.07, 0.09), "Accent", verts=6, rot=(math.radians(90), 0, 0))


def marker(kind: str):
    if kind == "factory":
        box((0.5, 0.35, 0.25), (0, 0, 0.125), "Body")
        for i in range(3):
            wedge(0.16, 0.35, 0.1, (-0.17 + i * 0.17, -0.175, 0.25), "Accent")
        cyl(0.04, 0.3, (0.18, 0.1, 0.4), "Accent", verts=8)
    elif kind == "dockyard":
        box((0.5, 0.3, 0.08), (0, 0, 0.04), "Base")
        box((0.06, 0.06, 0.5), (-0.15, 0, 0.3), "Body")
        box((0.5, 0.05, 0.05), (0.05, 0, 0.55), "Body")
    elif kind == "airbase":
        box((0.6, 0.25, 0.03), (0, 0, 0.015), "Base")
        for i in range(4):
            box((0.06, 0.02, 0.035), (-0.22 + i * 0.15, 0, 0.02), "Body")
        box((0.1, 0.1, 0.2), (0.22, 0.12, 0.1), "Body")
        box((0.14, 0.14, 0.06), (0.22, 0.12, 0.23), "Glass")
    elif kind == "naval_base":
        box((0.5, 0.1, 0.08), (0, -0.1, 0.04), "Base")
        box((0.1, 0.35, 0.08), (-0.2, 0.08, 0.04), "Base")
        cyl(0.05, 0.3, (0.18, 0.05, 0.15), "Body", verts=8)
        cone(0.06, 0.08, (0.18, 0.05, 0.34), "Red", verts=8)
    elif kind == "fort":
        cyl(0.26, 0.2, (0, 0, 0.1), "Body", verts=8)
        for i in range(8):
            a = i / 8 * math.tau
            box((0.08, 0.06, 0.08), (0.24 * math.cos(a), 0.24 * math.sin(a), 0.24), "Accent", rot=(0, 0, a))
    elif kind == "radar":
        cyl(0.03, 0.3, (0, 0, 0.15), "Accent", verts=6)
        bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.22, depth=0.03, location=(0, 0, 0.36), rotation=(math.radians(60), 0, 0))
        _finish(bpy.context.active_object, "Body")
    elif kind == "supply_hub":
        box((0.5, 0.3, 0.2), (0, 0, 0.1), "Body")
        wedge(0.5, 0.3, 0.12, (0, -0.15, 0.2), "Warm")
        box((0.2, 0.05, 0.05), (0.25, 0, 0.05), "Base")
    elif kind == "capital":
        cyl(0.26, 0.08, (0, 0, 0.04), "Base", verts=16)
        box((0.3, 0.3, 0.2), (0, 0, 0.18), "Body")
        cyl(0.12, 0.1, (0, 0, 0.33), "Body", verts=12)
        sphere(0.12, (0, 0, 0.4), "Warm")
        cone(0.03, 0.12, (0, 0, 0.56), "Warm", verts=6)
    elif kind == "victory_point":
        cyl(0.03, 0.5, (0, 0, 0.25), "Body", verts=6)
        box((0.22, 0.02, 0.14), (0.12, 0, 0.43), "Red")
    elif kind == "battle":
        box((0.05, 0.5, 0.04), (0, 0, 0.1), "Body", rot=(0, 0, math.radians(40)))
        box((0.05, 0.5, 0.04), (0, 0, 0.1), "Body", rot=(0, 0, math.radians(-40)))
        sphere(0.14, (0, 0, 0.2), "Red", segs=8, rings=5)
    elif kind == "nuke":
        cyl(0.04, 0.3, (0, 0, 0.15), "Body", verts=8)
        sphere(0.2, (0, 0, 0.45), "Warm", segs=10, rings=6)


MODELS = {
    "division": lambda: counter("infantry"),
    "division_armoured": lambda: counter("armour"),
    "division_motorised": lambda: counter("motorised"),
    "division_special": lambda: counter("special"),
    "fleet": lambda: counter("fleet"),
    "air_wing": lambda: counter("air"),
    "infantry": infantry,
    "tank_light": lambda: tank("light"),
    "tank_medium": lambda: tank("medium"),
    "tank_heavy": lambda: tank("heavy"),
    "truck": truck,
    "artillery": artillery,
    "fighter": lambda: plane("fighter"),
    "bomber": lambda: plane("bomber"),
    "destroyer": lambda: ship("destroyer"),
    "cruiser": lambda: ship("cruiser"),
    "battleship": lambda: ship("battleship"),
    "carrier": lambda: ship("carrier"),
    "submarine": lambda: ship("submarine"),
    "convoy": lambda: ship("convoy"),
    "factory": lambda: marker("factory"),
    "dockyard": lambda: marker("dockyard"),
    "airbase": lambda: marker("airbase"),
    "naval_base": lambda: marker("naval_base"),
    "fort": lambda: marker("fort"),
    "radar": lambda: marker("radar"),
    "supply_hub": lambda: marker("supply_hub"),
    "capital": lambda: marker("capital"),
    "victory_point": lambda: marker("victory_point"),
    "battle": lambda: marker("battle"),
    "nuke": lambda: marker("nuke"),
}


def export(name: str, out_dir: str) -> dict:
    reset_scene()
    MODELS[name]()
    obj = join_all(name)
    path = os.path.join(out_dir, f"{name}.glb")
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True,
                              export_apply=True, export_yup=True, export_animations=False,
                              export_skins=False, export_morph=False, export_cameras=False, export_lights=False)
    xs = [v.co.x for v in obj.data.vertices]; ys = [v.co.y for v in obj.data.vertices]; zs = [v.co.z for v in obj.data.vertices]
    return {"file": f"{name}.glb", "height": round(max(zs), 4), "radius": round(max(max(map(abs, xs)), max(map(abs, ys))), 4),
            "pivot": "base", "faces": len(obj.data.polygons), "bytes": os.path.getsize(path)}


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "models"))
    ap.add_argument("--only", nargs="*")
    args = ap.parse_args(argv)
    os.makedirs(args.out, exist_ok=True)
    manifest = {}
    for name in (args.only or MODELS):
        manifest[name] = export(name, args.out)
        print(f"  {name}: {manifest[name]['faces']} faces, {manifest[name]['bytes']} bytes")
    with open(os.path.join(args.out, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump({"blender": bpy.app.version_string, "models": manifest}, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    main(argv)
