#!/usr/bin/env python3
"""Build War Room's map data from public sources.

    python build_map.py [--out ../data/map] [--common ../data/common]

Deterministic: the same inputs produce the same files. Takes a few minutes.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import ne                                   # noqa: E402
from raster import W, H                     # noqa: E402
from states import build_states             # noqa: E402
from provinces import build_land_provinces, build_sea_and_lakes, renumber  # noqa: E402


def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", file=sys.stderr, flush=True)


def write_id_png(prov: np.ndarray, path: str):
    rgb = np.empty((H, W, 3), dtype=np.uint8)
    rgb[..., 0] = prov & 255
    rgb[..., 1] = (prov >> 8) & 255
    rgb[..., 2] = (prov >> 16) & 255
    Image.fromarray(rgb, "RGB").save(path, optimize=True)


def dump(obj, path):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, separators=(",", ":"))
        f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "..", "data", "map"))
    ap.add_argument("--common", default=os.path.join(HERE, "..", "data", "common"))
    ap.add_argument("--stage", default="all", choices=["provinces", "all"])
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    os.makedirs(args.common, exist_ok=True)

    with open(os.path.join(HERE, "countries_1936.json"), encoding="utf-8") as f:
        countries = json.load(f)["countries"]
    with open(os.path.join(HERE, "owners_1936.json"), encoding="utf-8") as f:
        owners = json.load(f)

    log("loading populated places")
    places = ne.load_layer("places")

    log("building states")
    state_raster, states, _adm = build_states(owners, countries, places)
    log(f"  {len(states)} states")

    log("building land provinces")
    prov, provinces = build_land_provinces(state_raster, states)
    log(f"  {len(provinces)} land provinces")
    del state_raster

    log("building lakes and sea provinces")
    build_sea_and_lakes(prov, provinces, len(provinces) + 1)
    log(f"  {len(provinces)} provinces in all")
    renumber(prov, provinces, states)

    from naming import name_provinces, terrain_and_more  # noqa: E402
    log("naming provinces")
    name_provinces(provinces, states, places)

    if args.stage == "all":
        log("terrain, regions, adjacency, victory points")
        terrain_and_more(prov, provinces, states, countries, places, args)

    log("writing files")
    write_id_png(prov, os.path.join(args.out, "provinces.png"))
    dump(provinces, os.path.join(args.out, "provinces.json"))
    dump(states, os.path.join(args.out, "states.json"))
    log("done")


if __name__ == "__main__":
    main()
