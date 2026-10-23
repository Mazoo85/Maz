"""Strategic regions: land air-regions by clustering states, naval regions by named seas."""
from __future__ import annotations

import json
import os
from collections import Counter

import numpy as np
from scipy.spatial import cKDTree

HERE = os.path.dirname(os.path.abspath(__file__))


def xyz(lat, lon):
    la, lo = np.radians(lat), np.radians(lon)
    return np.stack([np.cos(la) * np.cos(lo), np.cos(la) * np.sin(lo), np.sin(la)], axis=-1)


def apply_regions(prov, provinces, states, out_dir):
    by_id = {p["id"]: p for p in provinces}
    regions = []

    # --- land regions: k-means over state centroids ---------------------------------
    pts = np.array([xyz(s["lat"], s["lon"]) for s in states])
    k = max(1, round(len(states) / 9))
    rng = np.random.default_rng(7)
    centers = [pts[rng.integers(len(pts))]]
    d = np.full(len(pts), np.inf)
    for _ in range(1, k):
        d = np.minimum(d, ((pts - centers[-1]) ** 2).sum(axis=1))
        centers.append(pts[int(np.argmax(d))])
    centers = np.array(centers)
    for _ in range(20):
        lab = np.argmin(((pts[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2), axis=1)
        for i in range(k):
            m = lab == i
            if m.any():
                c = pts[m].mean(axis=0); centers[i] = c / np.linalg.norm(c)
    lab = np.argmin(((pts[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2), axis=1)
    next_id = 1
    for i in range(k):
        members = [s for s, l in zip(states, lab) if l == i]
        if not members:
            continue
        pids = [pid for s in members for pid in s["provinces"]]
        top = max((by_id[pid] for pid in pids), key=lambda p: (p.get("vp", 0), p["pixels"]))
        big = {"name": top["name"] if top.get("vp", 0) > 0 else max(members, key=lambda s: s["area_km2"])["name"]}
        climate = Counter(by_id[pid]["koppen"] for pid in pids).most_common(1)[0][0]
        if climate == "ocean":
            climate = "temperate"
        regions.append({"id": next_id, "name": big["name"], "type": "land", "climate": climate, "provinces": pids})
        for pid in pids:
            by_id[pid]["region"] = next_id
        next_id += 1

    # --- sea regions: nearest named sea --------------------------------------------
    with open(os.path.join(HERE, "seas.json"), encoding="utf-8") as f:
        seas = json.load(f)["seas"]
    tree = cKDTree(np.array([xyz(lat, lon) for _, lat, lon in seas]))
    water = [p for p in provinces if p["type"] in ("sea", "lake")]
    _, idx = tree.query(np.array([xyz(p["lat"], p["lon"]) for p in water]))
    groups = {}
    for p, i in zip(water, idx):
        groups.setdefault(int(i), []).append(p["id"])
    sea_region_of = {}
    for i, pids in sorted(groups.items()):
        regions.append({"id": next_id, "name": seas[i][0], "type": "sea", "climate": "ocean", "provinces": pids})
        for pid in pids:
            by_id[pid]["region"] = next_id
        sea_region_of[i] = next_id
        next_id += 1
    # name sea provinces after their region
    counter = Counter()
    for p in water:
        r = regions[p["region"] - 1]
        counter[r["name"]] += 1
        if p["type"] == "sea":
            p["name"] = f"{r['name']} {counter[r['name']]}"

    # every region name unique
    seen = Counter()
    for r in regions:
        seen[r["name"]] += 1
        if seen[r["name"]] > 1:
            r["name"] = f"{r['name']} ({seen[r['name']]})"

    with open(os.path.join(out_dir, "regions.json"), "w", encoding="utf-8") as f:
        json.dump(regions, f, ensure_ascii=False, separators=(",", ":")); f.write("\n")
    print(f"  {sum(1 for r in regions if r['type']=='land')} land regions, {sum(1 for r in regions if r['type']=='sea')} sea regions")
    return regions
