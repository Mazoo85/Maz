#!/usr/bin/env bash
# tools/make-3d-fixture.sh — write the frames film/tests/film-3d.test.js compares the browser against.
#
# The fixture is a handful of moments of one film, rendered by the NATIVE renderer. The test then
# renders the same moments through the WebAssembly module and demands they match. Re-run this and
# commit the result whenever the renderer's output legitimately changes; if it changes when you did not
# mean it to, that is the test doing its job.
#
#   tools/make-3d-fixture.sh <reel.json> [film3d binary]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reel="${1:?usage: make-3d-fixture.sh <reel.json> [film3d binary]}"
bin="${2:-$here/build/bin/film3d}"
out="$here/film/tests/fixtures-3d"

if [ ! -x "$bin" ]; then
  echo "make-3d-fixture: no film3d binary at $bin — build it, or pass one" >&2
  exit 1
fi

# The reel is read into memory BEFORE the output directory is wiped, because the obvious thing to do
# is to point this at the reel it wrote last time — film/tests/fixtures-3d/reel.json — and that lives
# inside the directory about to be deleted. Doing it in the other order deletes the reel and then
# fails to copy it, leaving no fixture at all.
reeltext="$(cat "$reel")"
rm -rf "$out"
mkdir -p "$out"
printf '%s' "$reeltext" > "$out/reel.json"
reel="$out/reel.json"

# 480 x 204 is the size the browser plays at, and supersample 1 with hard shadows is what it plays
# with; there is no point holding the module to a standard the page never asks for.
W=480
H=204
SS=1
SHADOWS=1
FPS=12
# Recorded in the manifest and passed explicitly, so the two sides cannot drift. They did: a new
# setting arrived with a non-zero default, the fixture picked it up and the test did not, and
# 226,416 pixels disagreed for a reason nobody had chosen.
CORNERS=55

python3 - "$reel" "$out" "$bin" "$W" "$H" "$SS" "$SHADOWS" "$FPS" "$CORNERS" <<'PY'
import json, subprocess, sys, os, shutil

reel_path, out, binary, W, H, SS, SHADOWS, FPS, CORNERS = sys.argv[1:10]
W, H, SS, SHADOWS, FPS, CORNERS = int(W), int(H), int(SS), int(SHADOWS), int(FPS), int(CORNERS)
reel = json.load(open(reel_path))
shots = reel['shots']

# Nine moments spread across the film, each landing inside a different shot, and each snapped to a
# frame boundary so the native renderer and the module are asked for exactly the same instant.
picks = []
step = max(1, len(shots) // 9)
for i in range(0, len(shots), step):
    s = shots[i]
    t = s['start'] + s['duration'] * 0.5
    t = round(t * FPS) / FPS
    picks.append((t, s['index']))
    if len(picks) == 9:
        break

# And the awkward moments, kept on purpose.
#
# Nine evenly spaced frames are a fair sample of the film and a poor sample of where the two builds
# disagree. Sweeping fifty-eight moments of this reel found the disagreement is not spread evenly at
# all: it lives on high-contrast edges seen at a slant, and one frame — the middle of shot 14, a
# doorway edge running diagonally across a lit wall — carried more of it than the other fifty-seven
# put together. The evenly spaced nine missed it entirely, which meant the test was passing by luck
# of sampling rather than by agreement. So the worst moment anyone has found goes in the fixture
# deliberately: a test that only looks where the answer is easy is not a test.
for extra in (65.33333333333333, 66.41666666666667):
    t = round(extra * FPS) / FPS
    for s in shots:
        if s['start'] <= t <= s['start'] + s['duration']:
            picks.append((t, s['index']))
            break

frames = []
tmp = os.path.join(out, '_tmp')
for n, (t, idx) in enumerate(picks):
    os.makedirs(tmp, exist_ok=True)
    subprocess.run([binary, reel_path, '--out', tmp, '--ppm', '--width', str(W),
                    '--ss', str(SS), '--shadows', str(SHADOWS),
                    '--corners', str(CORNERS), '--still', repr(t)],
                   check=True, stdout=subprocess.DEVNULL)
    # Gzipped: these are nine uncompressed frames and they live in the repository forever.
    name = 'frame-%02d.ppm.gz' % n
    import gzip
    with open(os.path.join(tmp, 'frame-00000.ppm'), 'rb') as src:
        with gzip.open(os.path.join(out, name), 'wb', compresslevel=9) as dst:
            shutil.copyfileobj(src, dst)
    os.remove(os.path.join(tmp, 'frame-00000.ppm'))
    frames.append({'file': name, 'time': t, 'shot': idx})
shutil.rmtree(tmp, ignore_errors=True)

manifest = {
    'reel': 'reel.json',
    'title': reel['title'],
    'duration': reel['duration'],
    'shots': len(shots),
    'people': len(reel['characters']),
    'width': W, 'height': H,
    'supersample': SS, 'shadows': SHADOWS, 'corners': CORNERS, 'fps': FPS,
    'frames': frames,
}
json.dump(manifest, open(os.path.join(out, 'manifest.json'), 'w'), indent=2)
print('make-3d-fixture: wrote %d frames of "%s" into %s' % (len(frames), reel['title'], out))
PY
