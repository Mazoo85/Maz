# ZOMBOID — browser build

A self-contained, single-file HTML5 Canvas port of the ZOMBOID flagship game
(top-down twin-stick zombie survival). No external assets, no build step, no
network — all art is vector-drawn / procedurally generated at load, so the file
plays offline by double-clicking it.

## Files
- `zomboid.src.html` — canonical source (edited by hand; artifact-publish format,
  no `<!doctype>` wrapper). This is the file to modify.
- `ZOMBOID.html` — generated standalone (full `<!doctype>` wrapper) for download /
  double-click play. Regenerate after editing the source:

```sh
python3 - apps/zomboid/web/zomboid.src.html apps/zomboid/web/ZOMBOID.html <<'PY'
import sys,re
body=open(sys.argv[1]).read()
m=re.match(r'\s*<title>(.*?)</title>\s*', body); title=m.group(1) if m else "ZOMBOID"
if m: body=body[m.end():]
open(sys.argv[2],'w').write('<!doctype html><html lang="en"><head><meta charset="utf-8">'
 '<meta name="viewport" content="width=device-width,initial-scale=1">'
 f'<title>{title}</title></head><body>\n{body}\n</body></html>')
PY
```

## Controls
WASD move · mouse aim · click shoot · R reload · 1/2/3 weapons · Shift dash ·
Space melee kick · Q grenade · E eat ration · F ultimate · P/Esc pause.

## Visuals
- Cracked-asphalt ground (seamless tileable value-noise + gravel speckle + cracks).
- Persistent blood decals stamped where zombies die.
- Hand-drawn top-down commando hero and four zombie types (walker/runner/brute/boss).
- Glowing tracer rounds + muzzle-flash light pool, day/night cycle, low-health vignette.
