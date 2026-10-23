"""Natural Earth and other public-domain inputs: download once, cache under pipeline/cache/.

Everything here is public domain (Natural Earth) or CC-BY (fatiando-data's repackaged
ETOPO topography; Beck et al.'s Köppen map via kgcpy). Sources:
  https://github.com/nvkelso/natural-earth-vector
  https://github.com/fatiando-data/earth-topography-10arcmin
  https://pypi.org/project/kgcpy/
"""
from __future__ import annotations

import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, "cache")

NE_BASE = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/"
TOPO_URL = ("https://github.com/fatiando-data/earth-topography-10arcmin/releases/download/v1/"
            "earth-topography-10arcmin.nc")

LAYERS = {
    "admin1": "ne_10m_admin_1_states_provinces.geojson",
    "admin0": "ne_10m_admin_0_countries.geojson",
    "lakes": "ne_10m_lakes.geojson",
    "rivers": "ne_10m_rivers_lake_centerlines.geojson",
    "places": "ne_10m_populated_places.geojson",
    "regions": "ne_10m_geography_regions_polys.geojson",
    "ice": "ne_10m_glaciated_areas.geojson",
}


def _fetch(url: str, dest: str) -> str:
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return dest
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    print(f"  downloading {os.path.basename(dest)} …", file=sys.stderr)
    tmp = dest + ".part"
    with urllib.request.urlopen(url, timeout=300) as r, open(tmp, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    os.replace(tmp, dest)
    return dest


def layer_path(name: str) -> str:
    return _fetch(NE_BASE + LAYERS[name], os.path.join(CACHE, LAYERS[name]))


def load_layer(name: str) -> list[dict]:
    with open(layer_path(name), encoding="utf-8") as f:
        return json.load(f)["features"]


def topo_path() -> str:
    return _fetch(TOPO_URL, os.path.join(CACHE, "earth-topography-10arcmin.nc"))


def polygons(geometry: dict) -> list[list[list[tuple[float, float]]]]:
    """Return a list of polygons; each polygon is [exterior_ring, hole_ring, ...]."""
    if geometry is None:
        return []
    t = geometry["type"]
    if t == "Polygon":
        return [[[(x, y) for x, y in ring] for ring in geometry["coordinates"]]]
    if t == "MultiPolygon":
        return [[[(x, y) for x, y in ring] for ring in poly] for poly in geometry["coordinates"]]
    return []


def prop(feature: dict, *names: str, default=None):
    """Natural Earth property names vary in case between layers; try each spelling."""
    p = feature["properties"]
    for n in names:
        for k in (n, n.upper(), n.lower()):
            if k in p and p[k] is not None:
                return p[k]
    return default
