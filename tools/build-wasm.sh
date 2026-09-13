#!/usr/bin/env bash
# tools/build-wasm.sh — compile the 3D film renderer to WebAssembly for the browser app.
#
# The output is committed to the repository (film/wasm/film3d.js and film3d.wasm), because the film
# app is served as static files from GitHub Pages: there is no build step between the repo and the
# page. Re-run this and commit the result whenever the renderer changes.
#
#   tools/build-wasm.sh                 # needs emcc on PATH
#   EMSDK=/path/to/emsdk tools/build-wasm.sh
#
# Get the toolchain with:
#   git clone --depth 1 https://github.com/emscripten-core/emsdk
#   cd emsdk && ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$here/film/wasm"
glm="${GLM_DIR:-$here/.wasm-deps/glm}"

if [ -n "${EMSDK:-}" ] && [ -f "$EMSDK/emsdk_env.sh" ]; then
  # shellcheck disable=SC1091
  source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1
fi
if ! command -v em++ >/dev/null 2>&1; then
  echo "build-wasm: em++ not on PATH. See the header of this script for the toolchain." >&2
  exit 1
fi

# GLM is a header-only dependency that CMake normally fetches; fetch it the same way here.
if [ ! -d "$glm/glm" ]; then
  echo "build-wasm: fetching GLM 1.0.1 into $glm"
  mkdir -p "$(dirname "$glm")"
  git clone --depth 1 --branch 1.0.1 https://github.com/g-truc/glm.git "$glm" >/dev/null 2>&1
fi

mkdir -p "$out"

# -Oz over -O3: this file is downloaded over a phone connection before anything can be watched, and
# the renderer is not close to being the bottleneck at the sizes a phone plays at.
em++ -std=c++20 -Oz \
  -I"$here/engine/include" -I"$glm" \
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter -Werror \
  "$here/apps/film3d/wasm.cpp" "$here/engine/src/render/Shapes.cpp" \
  -o "$out/film3d.js" \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=0 \
  -s EXPORT_NAME=MazFilm3D \
  -s ENVIRONMENT=web,worker,node \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=33554432 \
  -s FILESYSTEM=0 \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8","UTF8ToString","stringToNewUTF8"]' \
  -s EXPORTED_FUNCTIONS='["_maz3d_load","_maz3d_error","_maz3d_title","_maz3d_genre","_maz3d_duration","_maz3d_shots","_maz3d_people","_maz3d_shot_at","_maz3d_render","_malloc","_free"]'

echo "build-wasm: wrote"
ls -l "$out" | awk 'NR>1 {printf "  %-16s %8.1f KB\n", $9, $5/1024}'
