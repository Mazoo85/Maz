"""Neighbours, coasts, rivers and straits from the id raster."""
from __future__ import annotations

import json
import os

import numpy as np

from raster import W, H, rasterize_lines
import ne

HERE = os.path.dirname(os.path.abspath(__file__))


def _pairs(a, b):
    m = (a != b) & (a > 0) & (b > 0)
    lo = np.minimum(a[m], b[m]).astype(np.int64)
    hi = np.maximum(a[m], b[m]).astype(np.int64)
    return lo * (1 << 24) + hi, m


def apply_adjacency(prov, provinces, out_dir):
    n = len(provinces)
    types = np.zeros(n + 1, dtype=np.uint8)
    for p in provinces:
        types[p["id"]] = {"land": 1, "sea": 2, "lake": 3}[p["type"]]

    # rivers: scale rank <= 6 are the big ones
    lines = []
    for f in ne.load_layer("rivers"):
        if (ne.prop(f, "scalerank", default=9) or 9) > 6:
            continue
        g = f["geometry"]
        if g["type"] == "LineString":
            lines.append(g["coordinates"])
        elif g["type"] == "MultiLineString":
            lines.extend(g["coordinates"])
    river = rasterize_lines(lines, line_width=2)

    keys_all, river_all = [], []
    for a, b, ra, rb in (
        (prov[:, :-1], prov[:, 1:], river[:, :-1], river[:, 1:]),
        (prov[:-1, :], prov[1:, :], river[:-1, :], river[1:, :]),
        (prov[:, -1:], prov[:, :1], river[:, -1:], river[:, :1]),
    ):
        k, m = _pairs(a, b)
        keys_all.append(k)
        river_all.append((ra[m] | rb[m]))
    keys = np.concatenate(keys_all)
    riv = np.concatenate(river_all)
    uniq, inv, counts = np.unique(keys, return_inverse=True, return_counts=True)
    river_counts = np.bincount(inv, weights=riv.astype(np.float64), minlength=len(uniq))

    adjacency = [[] for _ in range(n + 1)]
    rivers = []
    for k, c, rc in zip(uniq.tolist(), counts.tolist(), river_counts.tolist()):
        a, b = k >> 24, k & ((1 << 24) - 1)
        adjacency[a].append(b)
        adjacency[b].append(a)
        if types[a] == 1 and types[b] == 1 and rc / c >= 0.4:
            rivers.append([a, b])

    # straits: declared crossings between land provinces over water
    straits = []
    with open(os.path.join(HERE, "straits.json"), encoding="utf-8") as f:
        for s in json.load(f)["straits"]:
            ids = []
            for lat, lon in (s["a"], s["b"]):
                x = min(W - 1, max(0, int((lon + 180.0) / 360.0 * W)))
                y = min(H - 1, max(0, int((90.0 - lat) / 180.0 * H)))
                pid = int(prov[y, x])
                if types[pid] != 1:
                    # look around for the nearest land pixel
                    best = None
                    for r in range(1, 12):
                        win = prov[max(0, y - r):y + r + 1, max(0, x - r):x + r + 1]
                        landpx = win[types[win] == 1]
                        if landpx.size:
                            best = int(np.bincount(landpx).argmax()); break
                    pid = best or 0
                ids.append(pid)
            a, b = ids
            if not a or not b:
                print(f"  WARNING: strait {s['name']} did not resolve to land ({ids})")
                continue
            if a == b:
                continue  # both shores already lie in one province
            if b not in adjacency[a]:
                adjacency[a].append(b); adjacency[b].append(a)
            straits.append({"name": s["name"], "a": a, "b": b})

    for lst in adjacency:
        lst.sort()
    for p in provinces:
        p["coastal"] = bool(p["type"] == "land" and any(types[q] == 2 for q in adjacency[p["id"]]))

    with open(os.path.join(out_dir, "adjacency.json"), "w", encoding="utf-8") as f:
        json.dump(adjacency, f, separators=(",", ":")); f.write("\n")
    with open(os.path.join(out_dir, "rivers.json"), "w", encoding="utf-8") as f:
        json.dump(rivers, f, separators=(",", ":")); f.write("\n")
    with open(os.path.join(out_dir, "straits.json"), "w", encoding="utf-8") as f:
        json.dump(straits, f, separators=(",", ":")); f.write("\n")
    print(f"  {sum(len(a) for a in adjacency)//2} edges, {len(rivers)} river crossings, {len(straits)} straits")
    return adjacency
