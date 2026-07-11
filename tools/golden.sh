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
    "cube||2.5|0.03"
    "water||2.5|0.03"
    "model||2.5|0.08"
    "scene3d||2.5|0.13"
    "world|--demo|2.5|0.20"
    "village|--demo|2.5|0.22"
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
            rmse="$(compare -metric RMSE "$out" "$ref" null: 2>&1 | sed -E 's/.*\(([0-9.]+)\).*/\1/')"
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
