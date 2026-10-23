"""Victory points, capitals, town names, manpower, infrastructure; writes the common
countries, terrain and scenario files."""
from __future__ import annotations

import json
import os

import numpy as np

from raster import W, H
import ne

HERE = os.path.dirname(os.path.abspath(__file__))
MAJORS = {"GER", "ENG", "FRA", "ITA", "SOV", "JAP", "USA"}

TERRAIN_TYPES = {
    "plains":    {"movement": 1.0, "attack": 0.0,   "defence": 0.0,  "width": 90, "density": 9.0},
    "forest":    {"movement": 0.8, "attack": -0.15, "defence": 0.0,  "width": 84, "density": 4.0},
    "hills":     {"movement": 0.75,"attack": -0.25, "defence": 0.0,  "width": 80, "density": 3.5},
    "mountain":  {"movement": 0.5, "attack": -0.5,  "defence": 0.2,  "width": 75, "density": 1.2},
    "urban":     {"movement": 0.9, "attack": -0.3,  "defence": 0.25, "width": 96, "density": 40.0},
    "jungle":    {"movement": 0.6, "attack": -0.3,  "defence": 0.1,  "width": 84, "density": 2.5},
    "marsh":     {"movement": 0.5, "attack": -0.4,  "defence": 0.0,  "width": 78, "density": 1.5},
    "desert":    {"movement": 0.9, "attack": 0.0,   "defence": -0.1, "width": 90, "density": 0.2},
    "wasteland": {"movement": 0.0, "attack": 0.0,   "defence": 0.0,  "width": 0,  "density": 0.0},
    "ocean":     {"movement": 1.0, "attack": 0.0,   "defence": 0.0,  "width": 0,  "density": 0.0},
    "lake":      {"movement": 0.0, "attack": 0.0,   "defence": 0.0,  "width": 0,  "density": 0.0},
}


def _px(lat, lon):
    return (min(W - 1, max(0, int((lon + 180.0) / 360.0 * W))), min(H - 1, max(0, int((90.0 - lat) / 180.0 * H))))


def _nearest_land(prov, by_id, x, y):
    pid = int(prov[y, x])
    if pid and by_id[pid]["type"] == "land":
        return pid
    for r in range(1, 40):
        win = prov[max(0, y - r):y + r + 1, max(0, x - r):x + r + 1]
        cand = [int(q) for q in np.unique(win) if q and by_id[int(q)]["type"] == "land"]
        if cand:
            return cand[0]
    return 0


def apply_victory_points(prov, provinces, states, countries, places, args):
    by_id = {p["id"]: p for p in provinces}
    state_by_id = {s["id"]: s for s in states}
    for p in provinces:
        p.setdefault("vp", 0)
        p["named"] = False

    # towns: biggest first, each names its province and may give victory points
    towns = []
    for f in places:
        pop = float(ne.prop(f, "pop_max", default=0) or 0)
        lon, lat = f["geometry"]["coordinates"][:2]
        towns.append((pop, ne.prop(f, "name") or ne.prop(f, "nameascii") or "", lat, lon))
    towns.sort(key=lambda t: -t[0])
    for pop, name, lat, lon in towns:
        if not name:
            continue
        x, y = _px(lat, lon)
        pid = int(prov[y, x])
        if not pid or by_id[pid]["type"] != "land":
            continue
        p = by_id[pid]
        if not p["named"]:
            p["name"] = name; p["named"] = True
            if pop >= 4000000:
                p["vp"] = max(p["vp"], 10)
            elif pop >= 1500000:
                p["vp"] = max(p["vp"], 5)
            elif pop >= 500000:
                p["vp"] = max(p["vp"], 2)
            elif pop >= 150000:
                p["vp"] = max(p["vp"], 1)

    # capitals
    out_countries = []
    for c in countries:
        lat, lon = c["capital"]
        x, y = _px(lat, lon)
        pid = _nearest_land(prov, by_id, x, y)
        if not pid:
            raise SystemExit(f"capital of {c['tag']} at {lat},{lon} is not on land")
        p = by_id[pid]
        p["name"] = c["capital_name"]; p["named"] = True
        p["vp"] = max(p["vp"], 25 if c["tag"] in MAJORS else 15)
        if p["terrain"] not in ("urban", "mountain") and c["tag"] in MAJORS:
            p["terrain"] = "urban"
        st = state_by_id[p["state"]]
        if st["owner"] != c["tag"]:
            print(f"  WARNING: capital of {c['tag']} ({c['capital_name']}) lies in {st['name']} owned by {st['owner']}; reassigning the state")
            st["owner"] = c["tag"]
            if c["tag"] not in st["cores"]:
                st["cores"].append(c["tag"])
        oc = {k: v for k, v in c.items() if k not in ("capital", "home")}
        oc["capital_state"] = st["id"]; oc["capital_province"] = pid
        out_countries.append(oc)

    # terrain overrides by name
    with open(os.path.join(HERE, "terrain_overrides.json"), encoding="utf-8") as f:
        for name, t in json.load(f).get("by_name", {}).items():
            for p in provinces:
                if p["name"] == name:
                    p["terrain"] = t

    # state capital, manpower, infrastructure, resources placeholder
    for s in states:
        provs = [by_id[pid] for pid in s["provinces"]]
        cap = max(provs, key=lambda p: (p["vp"], p["pixels"]))
        s["capital_province"] = cap["id"]
        area_per_px = s["area_km2"] / max(1, sum(p["pixels"] for p in provs))
        mp = sum(TERRAIN_TYPES[p["terrain"]]["density"] * p["pixels"] * area_per_px for p in provs)
        mp += s["people"] * 0.9
        s["manpower"] = max(1000, int(round(mp / 1000.0) * 1000))
        density = s["manpower"] / max(1.0, s["area_km2"])
        s["infrastructure"] = int(min(5, max(1, round(1 + np.log10(max(density, 0.1)) * 1.3))))
        s["resources"] = {}
        s["lat"] = round(s["lat"], 3); s["lon"] = round(s["lon"], 3)

    for p in provinces:
        p.pop("named", None)

    # scenario: 1936 armies at each capital state
    with open(os.path.join(HERE, "armies_1936.json"), encoding="utf-8") as f:
        armies = json.load(f)["divisions"]
    cap_state = {c["tag"]: c["capital_state"] for c in out_countries}
    scenario = {"start_hour": 0, "armies": {tag: [{"state": cap_state[tag], "divisions": n}] for tag, n in armies.items() if tag in cap_state}}

    common = args.common
    os.makedirs(os.path.join(common, "..", "scenarios"), exist_ok=True)
    with open(os.path.join(common, "countries.json"), "w", encoding="utf-8") as f:
        json.dump(out_countries, f, ensure_ascii=False, indent=1); f.write("\n")
    with open(os.path.join(common, "terrain.json"), "w", encoding="utf-8") as f:
        json.dump({"types": list(TERRAIN_TYPES), "modifiers": TERRAIN_TYPES}, f, indent=1); f.write("\n")
    with open(os.path.join(common, "..", "scenarios", "1936.json"), "w", encoding="utf-8") as f:
        json.dump(scenario, f, indent=1); f.write("\n")
