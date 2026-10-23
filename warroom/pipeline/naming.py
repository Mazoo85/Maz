"""Province names, and the hook that the terrain/adjacency stage plugs into."""
from __future__ import annotations

import numpy as np

from raster import W, H
import ne


def name_provinces(provinces, states, places):
    """Land provinces take the name of the biggest town inside them, else 'State n'."""
    by_id = {p["id"]: p for p in provinces}
    best = {}
    # Build a quick pixel → province lookup only where towns are: we need the id raster,
    # so the caller passes town positions through the province list positions instead.
    # Here we use each province's own pixel list is not kept; instead towns were mapped
    # during terrain_and_more. This first pass names everything by state and index.
    for s in states:
        for n, pid in enumerate(s["provinces"], start=1):
            by_id[pid]["name"] = f"{s['name']} {n}" if len(s["provinces"]) > 1 else s["name"]
    sea_n = 0
    for p in provinces:
        if p["type"] == "sea":
            sea_n += 1
            p["name"] = f"Sea {sea_n}"


def terrain_and_more(prov, provinces, states, countries, places, args):
    from terrain import apply_terrain
    from adjacency import apply_adjacency
    from regions import apply_regions
    from vp import apply_victory_points
    from raster import px_to_lonlat
    for s in states:
        lon, lat = px_to_lonlat(s["cx"], s["cy"])
        s["lat"], s["lon"] = lat, lon
    apply_terrain(prov, provinces, states, places)
    apply_adjacency(prov, provinces, args.out)
    apply_regions(prov, provinces, states, args.out)
    apply_victory_points(prov, provinces, states, countries, places, args)
    for s in states:
        for k in ("cx", "cy", "codes", "adm0", "pixels", "people"):
            s.pop(k, None)
    for p in provinces:
        p.pop("koppen", None)
