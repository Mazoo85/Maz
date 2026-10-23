"""States: cut from Natural Earth first-level admin regions, merged where tiny and split
where huge, so every state is a sensible unit of territory for a 1936 war map.

A region's *weight* is (area / 10,000 km²)^0.6 + (people / 1,000,000). Regions lighter
than W_MIN merge into their heaviest neighbour of the same modern country and 1936
owner; groups heavier than W_MAX are split by k-means on their pixels.
"""
from __future__ import annotations

import math

import numpy as np
from scipy import ndimage

from raster import W, H, px_to_lonlat, rasterize
import ne

W_MIN = 3.0
W_MAX = 10.0
MAX_SPLIT = 5
ISLAND_MERGE_KM = 900.0
KM2_PER_PX_EQUATOR = (40075.0 / W) * (20004.0 / H)


def row_cos_lat(height: int = H) -> np.ndarray:
    lats = 90.0 - (np.arange(height) + 0.5) / height * 180.0
    return np.cos(np.radians(lats)).astype(np.float64)


def weight(area_km2: float, people: float) -> float:
    return (max(area_km2, 1.0) / 10000.0) ** 0.5 + people / 0.5e6


def raster_neighbours(grid: np.ndarray) -> dict[int, dict[int, int]]:
    """Count shared border pixels between distinct ids (4-neighbourhood, wrapping in x)."""
    pairs = {}

    def add(a: np.ndarray, b: np.ndarray):
        m = (a != b) & (a > 0) & (b > 0)
        if not m.any():
            return
        lo = np.minimum(a[m], b[m]).astype(np.int64)
        hi = np.maximum(a[m], b[m]).astype(np.int64)
        keys, counts = np.unique(lo * (1 << 24) + hi, return_counts=True)
        for k, c in zip(keys.tolist(), counts.tolist()):
            x, y = k >> 24, k & ((1 << 24) - 1)
            pairs.setdefault(x, {}).setdefault(y, 0)
            pairs[x][y] += c
            pairs.setdefault(y, {}).setdefault(x, 0)
            pairs[y][x] += c

    add(grid[:, :-1], grid[:, 1:])
    add(grid[:-1, :], grid[1:, :])
    add(grid[:, -1:], grid[:, :1])
    return pairs


def kmeans_split(ys: np.ndarray, xs: np.ndarray, coslat: np.ndarray, k: int, seed: int) -> np.ndarray:
    """Split pixels into k compact clusters; returns a label array 0..k-1."""
    rng = np.random.default_rng(seed)
    c = coslat[int(round(ys.mean()))]
    pts = np.stack([xs * c, ys.astype(np.float64)], axis=1)
    # farthest-point initialisation on a subsample
    sub = pts if len(pts) <= 20000 else pts[rng.choice(len(pts), 20000, replace=False)]
    centers = [sub[rng.integers(len(sub))]]
    d = np.full(len(sub), np.inf)
    for _ in range(1, k):
        d = np.minimum(d, ((sub - centers[-1]) ** 2).sum(axis=1))
        centers.append(sub[int(np.argmax(d))])
    centers = np.array(centers)
    labels = None
    for _ in range(12):
        dist = ((pts[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2)
        labels = np.argmin(dist, axis=1)
        for i in range(k):
            m = labels == i
            if m.any():
                centers[i] = pts[m].mean(axis=0)
    return labels


def split_names(base: str, ys, xs, labels, k: int) -> list[str]:
    """'Northern X', 'Southern X', ... for the parts of a split region, unique."""
    if k <= 1:
        return [base]
    gx, gy = xs.mean(), ys.mean()
    names, used = [], {}
    for part in range(k):
        m = labels == part
        if not m.any():
            names.append(base); continue
        dx, dy = xs[m].mean() - gx, ys[m].mean() - gy
        if abs(dx) < 1e-9 and abs(dy) < 1e-9:
            word = "Central"
        elif abs(dy) >= abs(dx):
            word = "Northern" if dy < 0 else "Southern"
        else:
            word = "Eastern" if dx > 0 else "Western"
        name = f"{word} {base}"
        used[name] = used.get(name, 0) + 1
        if used[name] > 1:
            name = f"{name} {used[name]}"
        names.append(name)
    return names


def build_states(owners: dict, countries: list[dict], places: list[dict]):
    """Returns (state_raster int32 H×W with ids 1..N, states list, adm_raster)."""
    feats = ne.load_layer("admin1")
    drop = set(owners.get("drop", []))
    by_adm0 = owners["by_adm0"]
    by_adm1 = owners["by_adm1"]
    home = {c["tag"]: set(c["home"]) for c in countries}
    tags = {c["tag"] for c in countries}

    regions = []  # index i -> record; raster id = i + 1
    items = []
    missing_adm0 = set()
    for f in feats:
        adm0 = ne.prop(f, "adm0_a3")
        if adm0 in drop:
            continue
        code = ne.prop(f, "adm1_code") or ""
        owner = by_adm1.get(code) or by_adm0.get(adm0)
        if owner is None:
            missing_adm0.add(adm0)
            continue
        if owner not in tags:
            raise SystemExit(f"owners_1936.json names unknown tag {owner} for {code} ({adm0})")
        name = ne.prop(f, "name") or ne.prop(f, "name_en") or code
        rec = {"code": code, "name": name, "adm0": adm0, "owner": owner,
               "core": (adm0 in home.get(owner, ())) or (code in by_adm1)}
        regions.append(rec)
        items.append((len(regions), ne.polygons(f["geometry"])))
    if missing_adm0:
        print(f"  WARNING: no 1936 owner for modern countries {sorted(missing_adm0)}; dropped")

    adm = rasterize(items)
    n = len(regions)
    coslat = row_cos_lat()
    counts = np.bincount(adm.ravel(), minlength=n + 1).astype(np.float64)
    area = np.bincount(adm.ravel(), weights=np.repeat(coslat, W), minlength=n + 1) * KM2_PER_PX_EQUATOR

    people = np.zeros(n + 1)
    for p in places:
        lon, lat = p["geometry"]["coordinates"][:2]
        x = min(W - 1, max(0, int((lon + 180.0) / 360.0 * W)))
        y = min(H - 1, max(0, int((90.0 - lat) / 180.0 * H)))
        i = adm[y, x]
        if i > 0:
            people[i] += float(ne.prop(p, "pop_max", default=0) or 0)

    # --- merge tiny regions --------------------------------------------------------
    nb = raster_neighbours(adm)
    parent = list(range(n + 1))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    gw = {i: weight(area[i], people[i]) for i in range(1, n + 1) if counts[i] > 0}
    changed = True
    while changed:
        changed = False
        for i in sorted(gw, key=lambda j: gw[j]):
            ri = find(i)
            if ri != i or gw[ri] >= W_MIN:
                continue
            best, best_w = None, -1.0
            for j, shared in nb.get(i, {}).items():
                rj = find(j)
                if rj == ri or rj not in gw:
                    continue
                if regions[rj - 1]["owner"] != regions[ri - 1]["owner"] or regions[rj - 1]["adm0"] != regions[ri - 1]["adm0"]:
                    continue
                if gw[rj] > best_w:
                    best, best_w = rj, gw[rj]
            if best is None:
                # look one step further through already-merged members
                for member in [m for m in range(1, n + 1) if find(m) == ri]:
                    for j in nb.get(member, {}):
                        rj = find(j)
                        if rj == ri or rj not in gw:
                            continue
                        if regions[rj - 1]["owner"] != regions[ri - 1]["owner"] or regions[rj - 1]["adm0"] != regions[ri - 1]["adm0"]:
                            continue
                        if gw[rj] > best_w:
                            best, best_w = rj, gw[rj]
            if best is None:
                continue
            # the name follows the larger piece of land
            keep, gone = (best, ri) if area[best] >= area[ri] else (ri, best)
            parent[gone] = keep
            gw[keep] = weight(area[keep] + area[gone], people[keep] + people[gone])
            area[keep] += area[gone]; people[keep] += people[gone]; counts[keep] += counts[gone]
            del gw[gone]
            changed = True


    # --- islands: light groups with no neighbour join the nearest same-owner group ----
    cy_sum = np.bincount(adm.ravel(), weights=np.repeat(np.arange(H, dtype=np.float64), W), minlength=n + 1)
    cx_sum = np.bincount(adm.ravel(), weights=np.tile(np.arange(W, dtype=np.float64), H), minlength=n + 1)
    def centroid(g):
        members = [m for m in range(1, n + 1) if find(m) == g]
        c = sum(counts0[m] for m in members)
        return (cx_sum[members].sum() / c, cy_sum[members].sum() / c)
    counts0 = np.bincount(adm.ravel(), minlength=n + 1).astype(np.float64)
    cents = {g: centroid(g) for g in gw}
    for g in sorted(gw, key=lambda j: gw[j]):
        if find(g) != g or gw[g] >= W_MIN:
            continue
        best, best_d = None, ISLAND_MERGE_KM
        gx, gy = cents[g]
        for h in gw:
            if h == g or find(h) != h:
                continue
            if regions[h - 1]["owner"] != regions[g - 1]["owner"] or regions[h - 1]["adm0"] != regions[g - 1]["adm0"]:
                continue
            hx, hy = cents[h]
            lat = 90.0 - (gy + hy) / 2 / H * 180.0
            dx = (hx - gx) * math.cos(math.radians(lat)) * (40075.0 / W)
            dy = (hy - gy) * (20004.0 / H)
            d = math.hypot(dx, dy)
            if d < best_d:
                best, best_d = h, d
        if best is None:
            continue
        keep, gone = (best, g) if area[best] >= area[g] else (g, best)
        parent[gone] = keep
        gw[keep] = weight(area[keep] + area[gone], people[keep] + people[gone])
        area[keep] += area[gone]; people[keep] += people[gone]; counts[keep] += counts[gone]
        cents[keep] = centroid(keep)
        del gw[gone]

    root = np.array([find(i) for i in range(n + 1)], dtype=np.int32)
    merged = root[adm]

    # --- split huge groups ---------------------------------------------------------
    objs = ndimage.find_objects(merged)
    state_raster = np.zeros_like(merged)
    states = []
    next_id = 1
    for gid in sorted(gw):
        sl = objs[gid - 1]
        if sl is None:
            continue
        local = merged[sl] == gid
        ys, xs = np.nonzero(local)
        ys = ys + sl[0].start; xs = xs + sl[1].start
        k = min(MAX_SPLIT, int(math.ceil(gw[gid] / W_MAX)))
        members = [regions[m - 1] for m in range(1, n + 1) if root[m] == gid]
        base = regions[gid - 1]
        if k <= 1 or len(ys) < 2 * k:
            labels = np.zeros(len(ys), dtype=np.int64); k = 1
        else:
            labels = kmeans_split(ys, xs, coslat, k, seed=gid)
        part_names = split_names(base["name"], ys, xs, labels, k)
        for part in range(k):
            m = labels == part
            if not m.any():
                continue
            py, px = ys[m], xs[m]
            state_raster[py, px] = next_id
            a = float((coslat[py]).sum() * KM2_PER_PX_EQUATOR)
            name = part_names[part]
            states.append({
                "id": next_id, "name": name, "adm0": base["adm0"], "owner": base["owner"],
                "cores": [base["owner"]] if base["core"] else [],
                "codes": sorted({r["code"] for r in members}),
                "pixels": int(m.sum()), "area_km2": round(a),
                "people": int(people[gid] * (m.sum() / len(ys))),
                "cx": float(px.mean()), "cy": float(py.mean()),
            })
            next_id += 1
    return state_raster, states, adm
