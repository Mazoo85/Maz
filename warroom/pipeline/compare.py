#!/usr/bin/env python3
"""Compare a fresh pipeline output with the committed data: JSON must match exactly,
the id map may differ in a tiny fraction of pixels (library drift)."""
import json
import os
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
fresh, committed = sys.argv[1], sys.argv[2]
bad = []
for rel in ["map/provinces.json", "map/states.json", "map/regions.json", "map/adjacency.json", "map/rivers.json", "map/straits.json",
            "common/countries.json", "common/terrain.json", "scenarios/1936.json"]:
    a = json.load(open(os.path.join(fresh, rel), encoding="utf-8"))
    b = json.load(open(os.path.join(committed, rel), encoding="utf-8"))
    if a != b:
        bad.append(rel)
ia = np.array(Image.open(os.path.join(fresh, "map/provinces.png")))
ib = np.array(Image.open(os.path.join(committed, "map/provinces.png")))
if ia.shape != ib.shape:
    bad.append(f"provinces.png shape {ia.shape} vs {ib.shape}")
else:
    frac = float((ia != ib).any(axis=2).mean())
    print(f"provinces.png differs in {frac * 100:.4f}% of pixels")
    if frac > 0.0001:
        bad.append(f"provinces.png differs in {frac * 100:.4f}% of pixels")
if bad:
    print("DIFFERENT:", bad)
    sys.exit(1)
print("committed data matches the pipeline")
