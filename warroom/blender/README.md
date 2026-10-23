# Blender pieces for War Room 1936

Every 3D piece in the game is built here, in code, and exported as glTF:

- `build_models.py` — the counters (division, armoured, motorised, special forces, fleet,
  air wing), the pieces (infantry, three tanks, truck, artillery, fighter, bomber, six
  ship types) and the markers (factory, dockyard, air base, naval base, fort, radar,
  supply hub, capital, victory point, battle, nuke). Low-poly and flat-shaded: they are
  read from orbit. Materials are named `Body` (tinted by the owning country in the game),
  `Accent`, `Base` and a few fixed colours.
- `build_relief.py` — the globe's relief normal map from the elevation grid.

Run them with Blender as a Python module or with a full Blender:

```sh
python -m venv .venv && . .venv/bin/activate      # Python 3.11
pip install "bpy==5.0.*" numpy netCDF4 pillow
python build_models.py --out ../models
python build_relief.py --out ../data/map/relief.png

blender -b -P build_models.py -- --out ../models  # or with the Blender app
```

Outputs are committed (`models/*.glb`, `models/manifest.json`, `data/map/relief.png`);
CI rebuilds the models and fails if any is missing, so the committed files can always be
regenerated. There are no hand-made `.blend` files to lose: change the script, rerun it.
