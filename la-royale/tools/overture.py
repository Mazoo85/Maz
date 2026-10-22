#!/usr/bin/env python3
"""
Downloads real Los Angeles from Overture Maps, for the true-scale map:

    pip install pyarrow
    python3 la-royale/tools/overture.py          # → la-royale/data/raw/osm_overture_*.json
    node la-royale/tools/fetch.mjs dem           # the ground (if not there yet)
    node la-royale/tools/build-tiles.mjs         # → la-royale/data/real/

Overture Maps (overturemaps.org) is free open map data on AWS: OpenStreetMap's
buildings, streets, parks and water, plus extra building outlines and
heights, and named businesses. This reads only the parts of its files that
cover the area (REAL in js/osm.js), straight from the public bucket, and
writes them out in OpenStreetMap's own form (Overpass JSON: ways with tags
and geometry), so build-tiles.mjs reads them exactly as it reads a direct
OpenStreetMap download (tools/fetch.mjs osm).

Licences: buildings, streets and base layers are ODbL (© OpenStreetMap
contributors and others); places are CDLA-Permissive-2.0.
"""

import json
import os
import re
import struct
import sys
from concurrent.futures import ThreadPoolExecutor

import pyarrow.compute as pc
import pyarrow.fs as pafs
import pyarrow.parquet as pq

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RAW = os.environ.get('LA_RAW') or os.path.join(ROOT, 'data', 'raw')
BUCKET = 'overturemaps-us-west-2'


def area_from_osm_js():
    """The area to fetch, read from REAL in js/osm.js so there's one source."""
    src = open(os.path.join(ROOT, 'js', 'osm.js'), encoding='utf8').read()
    block = re.search(r'export const REAL = \{(.*?)\};', src, re.S).group(1)
    get = lambda k: float(re.search(k + r':\s*(-?[\d.]+)', block).group(1))
    return get('west'), get('east'), get('south'), get('north')


W, E, S, N = area_from_osm_js()
s3 = pafs.S3FileSystem(anonymous=True, region='us-west-2')


def latest_release():
    rel = [i.path.rsplit('/', 1)[-1] for i in s3.get_file_info(pafs.FileSelector(f'{BUCKET}/release/'))]
    return sorted(r for r in rel if re.match(r'\d{4}-\d\d-\d\d', r))[-1]


RELEASE = os.environ.get('OVERTURE_RELEASE') or latest_release()


def read(path, cols):
    """Every row of a theme/type whose bounding box touches the area. The
    files are sorted in space, so most row groups are skipped from their
    footer statistics without downloading them."""
    files = [i.path for i in s3.get_file_info(pafs.FileSelector(f'{BUCKET}/release/{RELEASE}/{path}/'))]

    def groups(p):
        pf = pq.ParquetFile(s3.open_input_file(p))
        md = pf.metadata
        names = [md.schema.column(i).path for i in range(md.num_columns)]
        ix = {n: names.index(n) for n in ('bbox.xmin', 'bbox.xmax', 'bbox.ymin', 'bbox.ymax')}
        out = []
        for g in range(md.num_row_groups):
            st = lambda n: md.row_group(g).column(ix[n]).statistics
            if st('bbox.xmin').min > E or st('bbox.xmax').max < W or st('bbox.ymin').min > N or st('bbox.ymax').max < S:
                continue
            out.append((p, g))
        return out

    with ThreadPoolExecutor(32) as ex:
        todo = [g for gs in ex.map(groups, files) for g in gs]

    def load(pg):
        p, g = pg
        t = pq.ParquetFile(s3.open_input_file(p)).read_row_group(g, columns=cols + ['bbox', 'geometry'])
        b = t.column('bbox')
        f = lambda k: pc.struct_field(b, k)
        m = pc.and_(pc.and_(pc.less_equal(f('xmin'), E), pc.greater_equal(f('xmax'), W)),
                    pc.and_(pc.less_equal(f('ymin'), N), pc.greater_equal(f('ymax'), S)))
        return t.filter(m).to_pylist()

    with ThreadPoolExecutor(16) as ex:
        rows = [r for part in ex.map(load, todo) for r in part]
    print(f'  {path}: {len(rows):,} features from {len(todo)} row groups', flush=True)
    return rows


# ------------------------------------------------------------ geometry (WKB)

def wkb(buf):
    """Points, lines and polygons from well-known binary, as lists of
    (lon, lat): returns (kind, parts) with kind 'point' | 'line' | 'poly'."""
    pos = 0

    def u8():
        nonlocal pos
        pos += 1
        return buf[pos - 1]

    def one(end):
        nonlocal pos
        fmt = '<' if end else '>'
        (typ,) = struct.unpack_from(fmt + 'I', buf, pos); pos += 4
        dims = 3 if typ // 1000 in (1, 2) or typ & 0x80000000 else 2
        typ = typ % 1000 & 0xff
        def pts():
            nonlocal pos
            (n,) = struct.unpack_from(fmt + 'I', buf, pos); pos += 4
            out = []
            for _ in range(n):
                x, y = struct.unpack_from(fmt + 'dd', buf, pos); pos += 8 * dims
                out.append((x, y))
            return out
        if typ == 1:
            x, y = struct.unpack_from(fmt + 'dd', buf, pos); pos += 8 * dims
            return 'point', [[(x, y)]]
        if typ == 2:
            return 'line', [pts()]
        if typ == 3:
            (n,) = struct.unpack_from(fmt + 'I', buf, pos); pos += 4
            rings = [pts() for _ in range(n)]
            return 'poly', rings[:1]                 # the outer ring; holes are left out
        if typ in (4, 5, 6, 7):
            (n,) = struct.unpack_from(fmt + 'I', buf, pos); pos += 4
            kind, parts = None, []
            for _ in range(n):
                k, p = one(u8())
                kind = kind or k
                parts += p
            return kind, parts
        return None, []
    return one(u8())


# ------------------------------------------------------- Overture → OSM tags

elements = []
next_id = [1]


def emit(tags, kind, parts):
    for p in parts:
        if kind == 'point':
            elements.append({'type': 'node', 'id': next_id[0], 'lat': p[0][1], 'lon': p[0][0], 'tags': tags})
        elif len(p) >= 2:
            elements.append({'type': 'way', 'id': next_id[0], 'tags': tags, 'geometry': [{'lat': y, 'lon': x} for x, y in p]})
        next_id[0] += 1


def name_of(r):
    n = r.get('names')
    return n.get('primary') if n else None


def num(v):
    return None if v is None else (str(round(v, 1)) if isinstance(v, float) else str(v))


SUBTYPE_BUILDING = {'residential': 'residential', 'commercial': 'commercial', 'industrial': 'industrial', 'civic': 'civic',
                    'religious': 'religious', 'education': 'school', 'medical': 'hospital', 'transportation': 'transportation',
                    'agricultural': 'yes', 'military': 'yes', 'outbuilding': 'shed', 'entertainment': 'yes', 'service': 'yes'}


def building_tags(r, part=False):
    t = {'building:part': 'yes'} if part else {'building': r.get('class') or SUBTYPE_BUILDING.get(r.get('subtype') or '', 'yes')}
    if part and r.get('class'):
        t['building'] = r['class']
    for k, v in (('height', r.get('height')), ('min_height', r.get('min_height')), ('building:levels', r.get('num_floors')),
                 ('building:min_level', r.get('min_floor')), ('roof:shape', r.get('roof_shape')), ('roof:height', r.get('roof_height')),
                 ('building:colour', r.get('facade_color')), ('roof:colour', r.get('roof_color')),
                 ('building:material', r.get('facade_material')), ('name', name_of(r))):
        if v is not None:
            t[k] = num(v) if not isinstance(v, str) else v
    return t


def buildings():
    cols = ['names', 'subtype', 'class', 'height', 'min_height', 'num_floors', 'min_floor', 'roof_shape', 'roof_height',
            'facade_color', 'facade_material', 'roof_color', 'is_underground']
    for r in read('theme=buildings/type=building', cols):
        if r.get('is_underground'):
            continue
        emit(building_tags(r), *wkb(r['geometry']))
    for r in read('theme=buildings/type=building_part', [c for c in cols if c not in ('subtype', 'class')]):
        if r.get('is_underground'):
            continue
        emit(building_tags(r, part=True), *wkb(r['geometry']))


RAIL = {'standard_gauge': 'rail', 'light_rail': 'light_rail', 'tram': 'tram', 'funicular': 'rail', 'narrow_gauge': 'rail'}
ROAD = {'motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'residential', 'living_street', 'unclassified', 'service',
        'pedestrian', 'footway', 'steps', 'path', 'cycleway'}


def whole(rule):
    return rule.get('between') is None


def streets():
    for r in read('theme=transportation/type=segment',
                  ['names', 'subtype', 'class', 'subclass', 'road_flags', 'width_rules', 'access_restrictions', 'level_rules']):
        cls = r.get('class')
        if r.get('subtype') == 'rail':
            if cls not in RAIL:
                continue
            t = {'railway': RAIL[cls]}
        elif cls in ROAD:
            t = {'highway': cls + ('_link' if r.get('subclass') == 'link' and cls in ('motorway', 'trunk', 'primary', 'secondary', 'tertiary') else '')}
        else:
            continue
        flags = [v for f in (r.get('road_flags') or []) if whole(f) for v in (f.get('values') or [])]
        if 'is_tunnel' in flags or 'is_covered' in flags or any(whole(l) and (l.get('value') or 0) < 0 for l in (r.get('level_rules') or [])):
            continue
        if 'is_bridge' in flags:
            t['bridge'] = 'yes'
        for a in r.get('access_restrictions') or []:
            w = a.get('when') or {}
            if a.get('access_type') == 'denied' and w.get('heading') == 'backward' and whole(a) \
                    and not any(w.get(k) for k in ('mode', 'using', 'recognized', 'vehicle', 'during')):
                t['oneway'] = 'yes'
        for wr in r.get('width_rules') or []:
            if whole(wr) and wr.get('value'):
                t['width'] = num(wr['value'])
        if name_of(r):
            t['name'] = name_of(r)
        emit(t, *wkb(r['geometry']))


def osm_tags(r):
    st = r.get('source_tags')
    return dict(st) if st else {}


BASE_FALLBACK = {
    ('water', 'swimming_pool'): {'leisure': 'swimming_pool'}, ('water', None): {'natural': 'water'},
    ('land_use', 'park'): {'leisure': 'park'}, ('land_use', 'cemetery'): {'landuse': 'cemetery'},
    ('land_use', 'golf_course'): {'leisure': 'golf_course'}, ('land_use', 'grass'): {'landuse': 'grass'},
    ('land_use', 'pitch'): {'leisure': 'pitch'}, ('land_use', 'playground'): {'leisure': 'playground'},
    ('land', 'forest'): {'natural': 'wood'}, ('land', 'wood'): {'natural': 'wood'}, ('land', 'scrub'): {'natural': 'scrub'},
    ('land', 'grass'): {'landuse': 'grass'}, ('land', 'sand'): {'natural': 'sand'},
}


def base():
    for typ in ('land_use', 'water', 'land', 'infrastructure'):
        for r in read(f'theme=base/type={typ}', ['subtype', 'class', 'source_tags', 'names']):
            kind, parts = wkb(r['geometry'])
            t = osm_tags(r)
            if typ == 'infrastructure':
                # just the trees, and car parks
                if not (t.get('natural') == 'tree' or t.get('amenity') == 'parking' and kind == 'poly'):
                    continue
            elif not t:
                t = BASE_FALLBACK.get((typ, r.get('class'))) or BASE_FALLBACK.get((typ, None)) or {}
            if not t or kind == 'line':
                continue
            if kind == 'poly':
                parts = [p if p[0] == p[-1] else p + [p[0]] for p in parts]
            emit(t, kind, parts)


# Overture's categories → the OSM shop/amenity tags osm.js knows
AMENITY = [(r'coffee|cafe|tea_room|juice', 'cafe'), (r'\bbar\b|^bar|pub|brewery|wine_bar|cocktail|lounge|night_?club', 'bar'),
           (r'fast_food|burger|hot_dog|sandwich|chicken_wings|pizza|taco|food_truck|casual_eatery', 'fast_food'),
           (r'restaurant|eatery|diner|bistro|steakhouse|sushi|ramen|bbq|barbecue|deli', 'restaurant'),
           (r'ice_cream|frozen_yogurt|dessert', 'ice_cream'), (r'bank|credit_union', 'bank'), (r'pharmacy|drugstore', 'pharmacy'),
           (r'movie_theat|cinema', 'cinema'), (r'theat(er|re)|performing_arts|concert_hall|music_venue', 'theatre'),
           (r'gas_station|fuel', 'fuel')]
SHOP = r'store|shop|shopping|market|grocery|supermarket|bakery|boutique|florist|salon|barber|beauty|tattoo|laundr|dry_clean|liquor|pawn|thrift|optician|jewel|book|record|tailor|mall'


def places():
    for r in read('theme=places/type=place', ['names', 'basic_category', 'taxonomy', 'confidence', 'operating_status']):
        name = name_of(r)
        if not name or (r.get('confidence') or 0) < 0.6 or r.get('operating_status') in ('permanently_closed', 'temporarily_closed'):
            continue
        tax = r.get('taxonomy') or {}
        cat = ' '.join([r.get('basic_category') or '', tax.get('primary') or ''] + list(tax.get('hierarchy') or []))
        t = None
        for pat, am in AMENITY:
            if re.search(pat, cat):
                t = {'amenity': am}
                break
        if not t and re.search(r'hotel|motel|lodging|hostel', cat):
            t = {'tourism': 'hotel'}
        if not t and re.search(SHOP, cat):
            t = {'shop': (r.get('basic_category') or 'yes').replace('_store', '')}
        if not t:
            continue
        t['name'] = name
        emit(t, *wkb(r['geometry']))


def write(kind):
    global elements
    out = os.path.join(RAW, f'osm_overture_{kind}.json')
    with open(out, 'w', encoding='utf8') as f:
        json.dump({'source': f'Overture Maps {RELEASE}', 'elements': elements}, f, separators=(',', ':'))
    print(f'  → {os.path.relpath(out, ROOT)} ({len(elements):,} elements)', flush=True)
    elements = []


if __name__ == '__main__':
    os.makedirs(RAW, exist_ok=True)
    print(f'Overture Maps {RELEASE}, {W},{S} to {E},{N}', flush=True)
    which = sys.argv[1:] or ['buildings', 'streets', 'base', 'places']
    for k in which:
        globals()[k]()
        write(k)
