"""Rasterise polygons onto the equirectangular id grid with Pillow.

The grid is W×H pixels covering lon −180..180 (x) and lat 90..−90 (y). Features are
drawn largest-area first so an enclave drawn later overwrites its host; holes are drawn
back as 0 right after their exterior ring, before any later feature.
"""
from __future__ import annotations

import math

import numpy as np
from PIL import Image, ImageDraw

W, H = 8192, 4096


def to_px(lon: float, lat: float) -> tuple[float, float]:
    return ((lon + 180.0) / 360.0 * W, (90.0 - lat) / 180.0 * H)


def px_to_lonlat(x: float, y: float) -> tuple[float, float]:
    return ((x + 0.5) / W * 360.0 - 180.0, 90.0 - (y + 0.5) / H * 180.0)


def ring_area_km2(ring) -> float:
    """Approximate spherical-ish area of a lon/lat ring (shoelace with cos(lat) scaling)."""
    if len(ring) < 3:
        return 0.0
    lat0 = sum(p[1] for p in ring) / len(ring)
    k = math.cos(math.radians(lat0)) * 111.32
    a = 0.0
    for i in range(len(ring)):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % len(ring)]
        a += (x1 * k) * (y2 * 111.32) - (x2 * k) * (y1 * 111.32)
    return abs(a) / 2.0


def rasterize(items, width: int = W, height: int = H) -> np.ndarray:
    """items: iterable of (id, polygons) where polygons come from ne.polygons().

    Returns an int32 array (height, width) with 0 where nothing was drawn.
    """
    img = Image.new("I", (width, height), 0)
    draw = ImageDraw.Draw(img)
    ordered = []
    for fid, polys in items:
        for poly in polys:
            ordered.append((ring_area_km2(poly[0]), fid, poly))
    ordered.sort(key=lambda t: -t[0])
    sx, sy = width / W, height / H
    for _, fid, poly in ordered:
        ext = [(x * sx, y * sy) for x, y in (to_px(lon, lat) for lon, lat in poly[0])]
        if len(ext) < 3:
            continue
        draw.polygon(ext, fill=int(fid))
        for hole in poly[1:]:
            pts = [(x * sx, y * sy) for x, y in (to_px(lon, lat) for lon, lat in hole)]
            if len(pts) >= 3:
                draw.polygon(pts, fill=0)
    return np.array(img, dtype=np.int32)


def rasterize_lines(lines, width: int = W, height: int = H, line_width: int = 1) -> np.ndarray:
    """lines: iterable of coordinate lists (lon, lat). Returns a bool mask."""
    img = Image.new("1", (width, height), 0)
    draw = ImageDraw.Draw(img)
    sx, sy = width / W, height / H
    for coords in lines:
        pts = [(x * sx, y * sy) for x, y in (to_px(lon, lat) for lon, lat in coords)]
        if len(pts) >= 2:
            draw.line(pts, fill=1, width=line_width)
    return np.array(img, dtype=bool)
