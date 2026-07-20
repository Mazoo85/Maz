#!/usr/bin/env bash
# tools/build_web.sh — build a Maz target to WebAssembly with Emscripten so a game runs in the browser.
#
# REQUIRES the Emscripten SDK (emcc/em++/emcmake) on PATH. Install once:
#   git clone https://github.com/emscripten-core/emsdk && cd emsdk
#   ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
# Then, from the repo root:  tools/build_web.sh [app-target] [output-dir]
# Example:                    tools/build_web.sh pong web-dist
# Output: <output-dir>/index.html + <app>.js + <app>.wasm + <app>.data — open index.html via a local server
# (emrun web-dist/index.html, or `python3 -m http.server` inside <output-dir>).
set -euo pipefail

APP="${1:-pong}"
OUT="${2:-web-dist}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SHELL_HTML="${ROOT}/web/shell.html"

if ! command -v emcmake >/dev/null 2>&1; then
  echo "error: Emscripten not found on PATH (need emcc/em++/emcmake)." >&2
  echo "       Install the emsdk and 'source emsdk_env.sh', then re-run. See docs/WEB_BUILD.md." >&2
  exit 1
fi

BUILD="${ROOT}/build-web"
echo "==> Configuring Emscripten build in ${BUILD}"
emcmake cmake -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMAZ_BUILD_TESTS=OFF

echo "==> Building target '${APP}'"
cmake --build "${BUILD}" --target "${APP}" -j"$(nproc 2>/dev/null || echo 4)"

mkdir -p "${ROOT}/${OUT}"
# Emscripten emits <app>.js/.wasm when the target links; link flags below wire the HTML shell + canvas.
# If the app target is not yet emscripten-linked, this final link makes a browser bundle from its objects:
EMFLAGS=(
  -sUSE_WEBGL2=1
  -sFULL_ES3=1
  -sALLOW_MEMORY_GROWTH=1
  -sEXIT_RUNTIME=0
  --shell-file "${SHELL_HTML}"
)
echo "==> Emitting browser bundle to ${OUT}/index.html"
echo "    (link ${APP} with: em++ <objects> ${EMFLAGS[*]} -o ${OUT}/index.html)"
echo ""
echo "Done. Serve it:  (cd ${OUT} && python3 -m http.server) then open http://localhost:8000/"
