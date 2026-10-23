"""Terrain per province from elevation (slope and height), the Köppen climate raster,
Natural Earth's physical regions and glaciers, with a hand override table last.
"""
from __future__ import annotations

import csv
import json
import os

import numpy as np
from PIL import Image

from raster import W, H, rasterize
import ne

HERE = os.path.dirname(os.path.abspath(__file__))
Image.MAX_IMAGE_PIXELS = None

KOPPEN_GROUP = {}  # zone number -> 'tropical' | 'arid' | 'temperate' | 'continental' | 'polar'
KOPPEN_CODE = {}


def _load_koppen():
    import kgcpy
    d = os.path.dirname(kgcpy.__file__)
    with open(os.path.join(d, "kg_zoneNum.csv")) as f:
        for row in csv.DictReader(f):
            code, num = row["kg_zone"], int(row["zoneNum"])
            KOPPEN_CODE[num] = code
            KOPPEN_GROUP[num] = {"A": "tropical", "B": "arid", "C": "temperate", "D": "continental", "E": "polar"}.get(code[0], "ocean")
    img = Image.open(os.path.join(d, "kmz_int_reshape.png"))
    arr = np.array(img)
    ys = (np.arange(H) * arr.shape[0] // H)
    xs = (np.arange(W) * arr.shape[1] // W)
    return arr[ys][:, xs].astype(np.uint8)


def _load_elevation():
    import netCDF4
    ds = netCDF4.Dataset(ne.topo_path())
    topo = np.array(ds.variables["topography"][:], dtype=np.float32)   # (1081, 2161), lat -90..90
    topo = topo[::-1, :]                                                # y down
    topo = topo[:, :-1]                                                 # drop the duplicated 180° column
    gy, gx = np.gradient(topo)
    slope = np.hypot(gx, gy)                                            # metres per 10' cell
    ys = (np.arange(H) * topo.shape[0] // H)
    xs = (np.arange(W) * topo.shape[1] // W)
    return topo[ys][:, xs], slope[ys][:, xs]


def _per_province_mean(prov, values, n):
    s = np.bincount(prov.ravel(), weights=values.ravel().astype(np.float64), minlength=n + 1)
    c = np.bincount(prov.ravel(), minlength=n + 1).astype(np.float64)
    return np.divide(s, c, out=np.zeros_like(s), where=c > 0), c


def _per_province_mode(prov, classes, n, nclasses):
    best = np.zeros(n + 1, dtype=np.int64)
    best_count = np.zeros(n + 1, dtype=np.int64)
    for k in range(nclasses + 1):
        m = classes == k
        if not m.any():
            continue
        cnt = np.bincount(prov[m], minlength=n + 1)
        better = cnt > best_count
        best[better] = k
        best_count[better] = cnt[better]
    return best


def apply_terrain(prov, provinces, states, places):
    n = len(provinces)
    kop = _load_koppen()
    elev, slope = _load_elevation()
    mean_elev, counts = _per_province_mean(prov, elev, n)
    mean_slope, _ = _per_province_mean(prov, slope, n)
    mode_kop = _per_province_mode(prov, kop, n, 32)
    del kop, elev, slope

    # physical regions: 1 wetland/delta, 2 desert, 3 mountain range, 4 plateau
    code = {"Wetlands": 1, "Delta": 1, "Desert": 2, "Range/mtn": 3, "Plateau": 4}
    items = []
    for f in ne.load_layer("regions"):
        c = code.get(ne.prop(f, "featurecla"))
        if c:
            items.append((c, ne.polygons(f["geometry"])))
    phys = rasterize(items)
    frac = {}
    for c in (1, 2, 3, 4):
        frac[c], _ = _per_province_mean(prov, (phys == c).astype(np.float32), n)
    del phys
    ice = rasterize([(1, ne.polygons(f["geometry"])) for f in ne.load_layer("ice")])
    ice_frac, _ = _per_province_mean(prov, (ice == 1).astype(np.float32), n)
    del ice

    # big cities make urban provinces
    urban = set()
    for p in places:
        pop = float(ne.prop(p, "pop_max", default=0) or 0)
        if pop < 2500000:
            continue
        lon, lat = p["geometry"]["coordinates"][:2]
        x = min(W - 1, max(0, int((lon + 180.0) / 360.0 * W)))
        y = min(H - 1, max(0, int((90.0 - lat) / 180.0 * H)))
        if prov[y, x] > 0:
            urban.add(int(prov[y, x]))

    with open(os.path.join(HERE, "terrain_overrides.json"), encoding="utf-8") as f:
        overrides = json.load(f).get("by_name", {})

    for p in provinces:
        i = p["id"]
        p["koppen"] = KOPPEN_GROUP.get(int(mode_kop[i]), "ocean")
        p["elevation"] = int(round(float(mean_elev[i])))
        if p["type"] == "sea":
            p["terrain"] = "ocean"; p["koppen"] = "ocean"; continue
        if p["type"] == "lake":
            p["terrain"] = "lake"; continue
        kcode = KOPPEN_CODE.get(int(mode_kop[i]), "")
        sl, el = float(mean_slope[i]), float(mean_elev[i])
        if ice_frac[i] >= 0.6 or kcode == "EF":
            t = "wasteland"
        elif i in urban:
            t = "urban"
        elif sl >= 330 or el >= 3200 or (frac[3][i] >= 0.6 and sl >= 200):
            t = "mountain"
        elif sl >= 160 or el >= 1900 or (frac[3][i] >= 0.6 and sl >= 80):
            t = "hills"
        elif frac[1][i] >= 0.35:
            t = "marsh"
        elif kcode in ("BWh", "BWk") or frac[2][i] >= 0.5:
            t = "desert"
        elif kcode in ("Af", "Am"):
            t = "jungle"
        elif kcode in ("Dfb", "Dfc", "Dfd", "Dwb", "Dwc", "Dwd", "Dsb", "Dsc", "Dsd", "Cfc"):
            t = "forest"
        else:
            t = "plains"
        p["terrain"] = t
    # overrides by province name are applied after naming, in vp.apply_victory_points
    return overrides
