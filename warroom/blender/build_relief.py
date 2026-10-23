#!/usr/bin/env python3
"""Bake the globe's relief: a tangent-space normal map from the 10-arc-minute elevation
grid, so mountains catch the light on the globe. Runs under any Python with numpy,
netCDF4 and Pillow (the pipeline environment or Blender's).

    python build_relief.py [--out ../data/map/relief.png] [--size 4096]
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pipeline"))
import ne  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "..", "data", "map", "relief.png"))
    ap.add_argument("--size", type=int, default=4096)
    ap.add_argument("--strength", type=float, default=0.012)
    args = ap.parse_args()
    import netCDF4
    ds = netCDF4.Dataset(ne.topo_path())
    topo = np.array(ds.variables["topography"][:], dtype=np.float32)[::-1, :-1]   # y down, drop duplicate column
    topo = np.maximum(topo, 0.0)                                                  # the sea is flat
    W, H = args.size, args.size // 2
    ys = (np.arange(H) * topo.shape[0] // H)
    xs = (np.arange(W) * topo.shape[1] // W)
    e = topo[ys][:, xs]
    # soften so the relief reads from orbit rather than as noise
    k = np.array([1, 4, 6, 4, 1], dtype=np.float32); k = np.outer(k, k); k /= k.sum()
    pad = np.pad(e, 2, mode="edge")
    sm = sum(k[i, j] * pad[i:i + H, j:j + W] for i in range(5) for j in range(5))
    gy, gx = np.gradient(sm)
    nx, ny = -gx * args.strength, -gy * args.strength
    nz = np.ones_like(nx)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack([nx / n, ny / n, nz / n], axis=-1)
    img = ((rgb * 0.5 + 0.5) * 255).astype(np.uint8)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    Image.fromarray(img, "RGB").save(args.out, optimize=True)
    print(f"wrote {args.out} ({W}x{H}), max slope {float(np.hypot(gx, gy).max()):.0f} m/texel")


if __name__ == "__main__":
    main()
