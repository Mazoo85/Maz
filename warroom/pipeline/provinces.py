"""Provinces: land provinces by nearest-seed inside each state; sea provinces by a
lattice of seeds over the ocean; lakes from the lakes layer. All deterministic.
"""
from __future__ import annotations

import math

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from raster import W, H, px_to_lonlat, rasterize
from states import row_cos_lat, KM2_PER_PX_EQUATOR
import ne

MAX_SEEDS = 40
MIN_SEA_COMPONENT = 60      # smaller sea blobs enclosed by land become land
MIN_LAKE_PIXELS = 60


def seeds_for(area_km2: float, people: float) -> int:
    n = 1.0 + 1.6 * (area_km2 / 10000.0) ** 0.65 + people / 0.8e6
    return int(max(1, min(MAX_SEEDS, round(n))))


def farthest_points(pts: np.ndarray, k: int, rng: np.random.Generator) -> np.ndarray:
    if len(pts) <= k:
        return pts
    sub = pts if len(pts) <= 4000 else pts[rng.choice(len(pts), 4000, replace=False)]
    chosen = [sub[rng.integers(len(sub))]]
    d = np.full(len(sub), np.inf)
    for _ in range(1, k):
        d = np.minimum(d, ((sub - chosen[-1]) ** 2).sum(axis=1))
        chosen.append(sub[int(np.argmax(d))])
    return np.array(chosen)


def build_land_provinces(state_raster: np.ndarray, states: list[dict]):
    """Returns (prov_raster int32, provinces list). Province ids start at 1."""
    coslat = row_cos_lat()
    prov = np.zeros_like(state_raster)
    provinces = []
    objs = ndimage.find_objects(state_raster)
    next_id = 1
    for s in states:
        sl = objs[s["id"] - 1]
        local = state_raster[sl] == s["id"]
        ys, xs = np.nonzero(local)
        ys = ys + sl[0].start; xs = xs + sl[1].start
        k = seeds_for(s["area_km2"], s["people"])
        rng = np.random.default_rng(s["id"] * 7919 + 17)
        c = coslat[int(round(ys.mean()))]   # one scale per state: per-row scaling would shear the cells
        pts = np.stack([xs * c, ys.astype(np.float64)], axis=1)
        centers = farthest_points(pts, k, rng)
        # Lloyd relaxation for even sizes
        for _ in range(4):
            dist = ((pts[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2)
            labels = np.argmin(dist, axis=1)
            for i in range(len(centers)):
                m = labels == i
                if m.any():
                    centers[i] = pts[m].mean(axis=0)
        dist = ((pts[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2)
        labels = np.argmin(dist, axis=1)
        first = next_id
        for i in range(len(centers)):
            m = labels == i
            if not m.any():
                continue
            py, px = ys[m], xs[m]
            prov[py, px] = next_id
            cx, cy = float(px.mean()), float(py.mean())
            # centroid may fall outside a concave province: snap to the nearest own pixel
            j = int(np.argmin((px - cx) ** 2 + (py - cy) ** 2))
            lon, lat = px_to_lonlat(int(px[j]), int(py[j]))
            provinces.append({"id": next_id, "type": "land", "state": s["id"], "x": int(px[j]), "y": int(py[j]),
                              "lat": round(lat, 3), "lon": round(lon, 3), "pixels": int(m.sum())})
            next_id += 1
        s["provinces"] = list(range(first, next_id))
    return prov, provinces


def lonlat_to_xyz(lon, lat):
    la, lo = np.radians(lat), np.radians(lon)
    return np.stack([np.cos(la) * np.cos(lo), np.cos(la) * np.sin(lo), np.sin(la)], axis=-1)


def build_sea_and_lakes(prov: np.ndarray, provinces: list[dict], first_id: int):
    """Fill every non-land pixel: lakes from the lakes layer, then sea provinces.
    Mutates prov in place and appends to provinces. Returns the next free id."""
    land = prov > 0
    coslat = row_cos_lat()
    next_id = first_id

    # Lakes ------------------------------------------------------------------------
    lakes = ne.load_layer("lakes")
    items = [(i + 1, ne.polygons(f["geometry"])) for i, f in enumerate(lakes)]
    lake_r = rasterize(items)
    counts = np.bincount(lake_r.ravel(), minlength=len(lakes) + 1)
    objs = ndimage.find_objects(lake_r)
    for i, f in enumerate(lakes):
        if counts[i + 1] < MIN_LAKE_PIXELS or objs[i] is None:
            continue
        sl = objs[i]
        m = lake_r[sl] == i + 1
        ys, xs = np.nonzero(m)
        ys = ys + sl[0].start; xs = xs + sl[1].start
        taken = np.bincount(prov[ys, xs], minlength=1)
        prov[ys, xs] = next_id
        cx, cy = float(xs.mean()), float(ys.mean())
        j = int(np.argmin((xs - cx) ** 2 + (ys - cy) ** 2))
        lon, lat = px_to_lonlat(int(xs[j]), int(ys[j]))
        for pid in np.nonzero(taken)[0]:
            if pid > 0:
                provinces[pid - 1]["pixels"] -= int(taken[pid])
        provinces.append({"id": next_id, "type": "lake", "state": 0, "x": int(xs[j]), "y": int(ys[j]),
                          "lat": round(lat, 3), "lon": round(lon, 3), "pixels": int(len(ys)),
                          "name": ne.prop(f, "name", default=None) or f"Lake {next_id}"})
        next_id += 1
    del lake_r

    # Small enclosed water → land (join the nearest land province) ------------------
    water = prov == 0
    labels, nlab = ndimage.label(water)
    sizes = np.bincount(labels.ravel())
    small = np.nonzero((sizes < MIN_SEA_COMPONENT) & (np.arange(len(sizes)) > 0))[0]
    objs = ndimage.find_objects(labels)
    for lab in small.tolist():
        sl = objs[lab - 1]
        y0, y1 = max(0, sl[0].start - 3), min(H, sl[0].stop + 3)
        x0, x1 = max(0, sl[1].start - 3), min(W, sl[1].stop + 3)
        win = prov[y0:y1, x0:x1]
        m = labels[y0:y1, x0:x1] == lab
        if (win > 0).sum() == 0:
            continue
        _, (iy, ix) = ndimage.distance_transform_edt(win == 0, return_indices=True)
        win[m] = win[iy[m], ix[m]]
    del labels

    # Sea seeds: a lattice, denser near coasts ---------------------------------------
    sea = prov == 0
    coast = ndimage.binary_dilation(prov > 0, iterations=1) & sea
    coast_pts = np.argwhere(coast)
    coast_tree = cKDTree(lonlat_to_xyz(*[np.array(v) for v in zip(*[px_to_lonlat(int(x), int(y)) for y, x in coast_pts[::25]])]))

    seeds = []
    jit = np.random.default_rng(1936)
    lat = -87.5
    while lat < 90:
        step_lon = 5.0 / max(math.cos(math.radians(lat)), 0.25)
        lon = -180.0 + step_lon / 2
        while lon < 180:
            xyz = lonlat_to_xyz(lon, lat)
            d, _ = coast_tree.query(xyz)
            sub = 3 if d < (220.0 / 6371.0) else (2 if d < (900.0 / 6371.0) else 1)
            for a in range(sub):
                for b in range(sub):
                    slon = lon - step_lon / 2 + (a + 0.5 + jit.uniform(-0.3, 0.3)) * step_lon / sub
                    slat = lat - 2.5 + (b + 0.5 + jit.uniform(-0.3, 0.3)) * 5.0 / sub
                    if slat > 89.5 or slat < -89.5:
                        continue
                    x = int((slon + 180.0) / 360.0 * W) % W
                    y = min(H - 1, max(0, int((90.0 - slat) / 180.0 * H)))
                    if sea[y, x]:
                        seeds.append((x, y))
            lon += step_lon
        lat += 5.0
    seeds = np.array(seeds)
    seed_lonlat = np.array([px_to_lonlat(int(x), int(y)) for x, y in seeds])
    tree = cKDTree(lonlat_to_xyz(seed_lonlat[:, 0], seed_lonlat[:, 1]))

    # Assign sea pixels in row chunks
    sea_ids = np.zeros(len(seeds), dtype=np.int64)
    sea_ids[:] = np.arange(next_id, next_id + len(seeds))
    sums = np.zeros((len(seeds), 3))
    for y0 in range(0, H, 256):
        y1 = min(H, y0 + 256)
        block = sea[y0:y1]
        ys, xs = np.nonzero(block)
        if len(ys) == 0:
            continue
        ys = ys + y0
        lon = (xs + 0.5) / W * 360.0 - 180.0
        la = 90.0 - (ys + 0.5) / H * 180.0
        _, idx = tree.query(lonlat_to_xyz(lon, la), workers=-1)
        prov[ys, xs] = sea_ids[idx]
        np.add.at(sums, idx, np.stack([xs, ys, np.ones(len(xs))], axis=1))
    for i in range(len(seeds)):
        if sums[i, 2] == 0:
            continue
        cx, cy = sums[i, 0] / sums[i, 2], sums[i, 1] / sums[i, 2]
        lon, lat = px_to_lonlat(cx, cy)
        provinces.append({"id": int(sea_ids[i]), "type": "sea", "state": 0, "x": int(cx), "y": int(cy),
                          "lat": round(lat, 3), "lon": round(lon, 3), "pixels": int(sums[i, 2])})
    next_id += len(seeds)
    return next_id


def renumber(prov: np.ndarray, provinces: list[dict], states: list[dict]):
    """Drop provinces that lost every pixel (to a lake) and make ids contiguous 1..N."""
    alive = np.bincount(prov.ravel(), minlength=max(p["id"] for p in provinces) + 1)
    provinces[:] = [p for p in provinces if alive[p["id"]] > 0]
    for p in provinces:
        p["pixels"] = int(alive[p["id"]])
    old_ids = [p["id"] for p in provinces]
    remap = np.zeros(max(old_ids) + 1, dtype=np.int32)
    for new, old in enumerate(old_ids, start=1):
        remap[old] = new
    prov[:] = remap[prov]
    for new, p in enumerate(provinces, start=1):
        p["id"] = new
    # re-centre every province on one of its own pixels (lakes may have taken the old centre)
    objs = ndimage.find_objects(prov)
    for p in provinces:
        sl = objs[p["id"] - 1]
        if sl is None:
            continue
        if prov[p["y"], p["x"]] == p["id"]:
            continue
        ys, xs = np.nonzero(prov[sl] == p["id"])
        ys = ys + sl[0].start; xs = xs + sl[1].start
        cx, cy = xs.mean(), ys.mean()
        j = int(np.argmin((xs - cx) ** 2 + (ys - cy) ** 2))
        p["x"], p["y"] = int(xs[j]), int(ys[j])
        lon, lat = px_to_lonlat(p["x"], p["y"])
        p["lat"], p["lon"] = round(lat, 3), round(lon, 3)
    for s in states:
        s["provinces"] = [int(remap[i]) for i in s["provinces"] if remap[i] > 0]
    states[:] = [s for s in states if s["provinces"]]
    return prov
