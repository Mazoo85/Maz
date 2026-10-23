#!/usr/bin/env python3
"""Compare a fresh pipeline output with the committed data.

Hard checks (CI fails): the same number of provinces, states, regions and countries;
every country owns the same number of states; every capital resolves to the same
province name; the id map differs in under 0.5% of pixels. Everything else is reported
so a real change is visible, but library drift (k-means ties, float rounding) does not
break the build.
"""
import json
import os
import sys
from collections import Counter

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
fresh, committed = sys.argv[1], sys.argv[2]


def load(base, rel):
    with open(os.path.join(base, rel), encoding="utf-8") as f:
        return json.load(f)


bad, notes = [], []
for rel in ["map/provinces.json", "map/states.json", "map/regions.json", "map/adjacency.json", "map/rivers.json",
            "map/straits.json", "common/countries.json", "common/terrain.json", "scenarios/1936.json"]:
    a, b = load(fresh, rel), load(committed, rel)
    if a == b:
        continue
    notes.append(f"{rel} differs")
    if isinstance(a, list) and isinstance(b, list) and len(a) != len(b):
        bad.append(f"{rel}: {len(a)} entries fresh vs {len(b)} committed")

sa, sb = load(fresh, "map/states.json"), load(committed, "map/states.json")
oa, ob = Counter(s["owner"] for s in sa), Counter(s["owner"] for s in sb)
for tag in sorted(set(oa) | set(ob)):
    if oa[tag] != ob[tag]:
        bad.append(f"{tag} owns {oa[tag]} states fresh vs {ob[tag]} committed")

pa, pb = {p["id"]: p for p in load(fresh, "map/provinces.json")}, {p["id"]: p for p in load(committed, "map/provinces.json")}
ca, cb = load(fresh, "common/countries.json"), load(committed, "common/countries.json")
for x, y in zip(ca, cb):
    if x["tag"] != y["tag"] or pa[x["capital_province"]]["name"] != pb[y["capital_province"]]["name"]:
        bad.append(f"capital of {x['tag']} changed")

ia = np.array(Image.open(os.path.join(fresh, "map/provinces.png")))
ib = np.array(Image.open(os.path.join(committed, "map/provinces.png")))
if ia.shape != ib.shape:
    bad.append(f"provinces.png shape {ia.shape} vs {ib.shape}")
else:
    frac = float((ia != ib).any(axis=2).mean())
    print(f"provinces.png differs in {frac * 100:.4f}% of pixels")
    if frac > 0.005:
        bad.append(f"provinces.png differs in {frac * 100:.3f}% of pixels")

for n in notes:
    print("note:", n)
if bad:
    print("DIFFERENT:", *bad, sep="\n  ")
    sys.exit(1)
print("committed data matches the pipeline" + (" (with minor drift)" if notes else ""))
