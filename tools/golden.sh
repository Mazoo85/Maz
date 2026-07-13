#!/usr/bin/env bash
# Golden-image regression harness for the Maz Engine.
#
# Renders each listed app headlessly on the Mesa lavapipe software Vulkan ICD under Xvfb, captures a
# frame, and either records it as a reference (capture) or compares it against the committed
# reference with a tolerance (check). This catches gross visual regressions — a broken render pass,
# wrong colours, missing geometry — that the "exits 0" smoke tests and unit tests can't see.
#
# The comparison uses ImageMagick's normalized RMSE with a per-run threshold generous enough to
# absorb the software renderer's nondeterminism and mild animation-timing jitter, while still failing
# hard on structural breakage. Requires lavapipe + Xvfb + ImageMagick; if lavapipe is missing the
# script prints SKIP and exits 0 so it never breaks a GPU-less CI.
#
# Usage:
#   tools/golden.sh capture   # (re)write reference images into tests/golden/
#   tools/golden.sh check     # compare current renders against the references (default)
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/bin"
GOLDEN="$ROOT/tests/golden"
MODE="${1:-check}"
ICD="/usr/share/vulkan/icd.d/lvp_icd.json"

# app|args|settle-seconds|max-RMSE. The per-app threshold sits above each app's natural frame-to-
# frame jitter (deterministic scenes get tight thresholds that catch subtle changes; time-animated
# ones like the day/night village or the autopilot world get looser thresholds that still fail hard
# on a black screen or a broken pass). Tighten these once apps gain a deterministic hold-frame mode.
CASES=(
    "cube||2.5|0.06"
    "water||2.5|0.03"
    "model||2.5|0.08"
    "scene3d||2.5|0.13"
    "world|--demo|2.5|0.25"
    "village|--demo|2.5|0.22"
    "instances||2.5|0.15"
    "glass||2.5|0.16"
    "maze||2.5|0.14"
    "crowd||2.5|0.16"
    "tween||2.5|0.18"
    "menu||2.5|0.18"
    "persist||2.5|0.14"
    "guard||2.5|0.16"
    "sprites||2.5|0.16"
    "events||2.5|0.12"
    "jobs||2.5|0.05"
    "assetcache||2.5|0.05"
    "skeleton||2.5|0.16"
    "animclip||2.5|0.16"
    "physics||2.5|0.08"
    "animator||2.5|0.16"
    "behavior||2.5|0.16"
    "boxes||2.5|0.06"
    "scenes||2.5|0.12"
    "catcher||2.5|0.08"
    "data||2.5|0.12"
    "level||2.5|0.05"
    "config||2.5|0.06"
    "profiler||2.5|0.05"
    "ecsave||2.5|0.05"
    "actions||2.5|0.05"
    "solar||2.5|0.05"
    "camera||2.5|0.10"
    "fireworks||2.5|0.06"
    "scatter||2.5|0.05"
    "noise||2.5|0.05"
    "uilayout||2.5|0.05"
    "navmesh||2.5|0.05"
    "vectors||2.5|0.05"
    "lights2d||2.5|0.05"
    "tumble||4.5|0.06"
    "blendspace||2.0|0.05"
    "joints||4.5|0.06"
    "spatial2d||2.0|0.05"
    "form||2.0|0.05"
    "cave||2.0|0.05"
    "reach||2.0|0.05"
    "avoid||2.0|0.05"
    "bus||2.0|0.05"
    "timeline||2.0|0.05"
    "softshadow||2.0|0.05"
    "groove||3.0|0.06"
    "stylebox||2.0|0.05"
    "blackboard||2.0|0.05"
    "statemachine||2.0|0.05"
    "reverb||2.0|0.05"
    "tentacle||2.0|0.05"
    "goap||2.0|0.05"
    "theme||2.0|0.05"
    "stack||2.0|0.06"
    "normalmap||2.5|0.05"
    "flowfield||2.5|0.05"
    "sequencer||2.0|0.05"
    "envelope||2.0|0.05"
    "tree||2.0|0.05"
    "area2d||2.0|0.06"
    "blendtree||2.0|0.05"
    "layers||2.0|0.06"
    "tileset||2.0|0.05"
    "emitter||2.0|0.06"
    "spatial3d||2.0|0.06"
    "containers||2.0|0.06"
    "parallax||2.0|0.06"
    "choreo||2.0|0.06"
    "prefab||2.0|0.06"
    "line2d||2.0|0.06"
    "restext||2.0|0.06"
    "signals||2.0|0.06"
    "wav||2.0|0.06"
    "grid3d||2.5|0.10"
    "rayquery||2.0|0.06"
    "strtable||2.0|0.06"
    "locale||2.0|0.06"
    "textwrap||2.0|0.07"
    "addblend||2.0|0.06"
    "primitives||2.5|0.12"
    "slotmap||2.0|0.06"
    "polycollide||2.0|0.06"
    "sampler||2.0|0.06"
    "respack||2.0|0.07"
    "richtext||2.0|0.07"
    "curve||2.0|0.06"
    "multimesh||2.0|0.07"
    "billboard||2.5|0.12"
    "oneway||2.0|0.06"
    "modfx||2.0|0.06"
    "rootmotion||2.0|0.06"
    "groups||2.0|0.07"
    "rects||2.0|0.06"
    "progress||2.0|0.07"
)

if [ ! -f "$ICD" ]; then
    echo "SKIP: lavapipe ICD not found ($ICD); golden-image check needs software Vulkan."
    exit 0
fi
if ! command -v compare >/dev/null 2>&1 || ! command -v Xvfb >/dev/null 2>&1; then
    echo "SKIP: ImageMagick 'compare' or 'Xvfb' not available."
    exit 0
fi

mkdir -p "$GOLDEN"
export VK_ICD_FILENAMES="$ICD"
Xvfb :98 -screen 0 1280x720x24 >/dev/null 2>&1 &
XPID=$!
trap 'kill $XPID 2>/dev/null' EXIT
sleep 1.5

fails=0
for entry in "${CASES[@]}"; do
    IFS='|' read -r app args settle threshold <<<"$entry"
    if [ ! -x "$BIN/$app" ]; then
        echo "MISS: $app not built; skipping"
        continue
    fi
    out="$(mktemp --suffix=.png)"
    # shellcheck disable=SC2086
    DISPLAY=:98 "$BIN/$app" $args >/dev/null 2>&1 &
    gpid=$!
    sleep "$settle"
    DISPLAY=:98 import -window root "$out" 2>/dev/null || true
    kill $gpid 2>/dev/null
    wait $gpid 2>/dev/null

    ref="$GOLDEN/$app.png"
    if [ "$MODE" = "capture" ]; then
        cp "$out" "$ref"
        echo "captured $app -> $ref"
    else
        if [ ! -f "$ref" ]; then
            echo "FAIL: $app has no reference (run 'tools/golden.sh capture')"
            fails=$((fails + 1))
        else
            # compare prints "<abs> (<normalized>)"; grab the normalized value (may be scientific
            # notation like 7.8e-06, so allow e/E/+/- in the capture).
            rmse="$(compare -metric RMSE "$out" "$ref" null: 2>&1 | sed -E 's/.*\(([0-9.eE+-]+)\).*/\1/')"
            if [ -z "$rmse" ]; then rmse=1.0; fi
            if awk "BEGIN{exit !($rmse > $threshold)}"; then
                echo "FAIL: $app RMSE $rmse > $threshold"
                fails=$((fails + 1))
            else
                echo "ok:   $app RMSE $rmse (<= $threshold)"
            fi
        fi
    fi
    rm -f "$out"
done

if [ "$MODE" = "check" ] && [ "$fails" -gt 0 ]; then
    echo "$fails golden-image mismatch(es)."
    exit 1
fi
echo "golden-image $MODE complete."
exit 0
