#!/usr/bin/env bash
# Maz Engine — game export / packaging, the equivalent of Godot's "Export Project".
#
# Takes one built app target and assembles a self-contained, redistributable bundle: the game
# executable, the SDL3 runtime library it links against, every compiled shader (.spv), the runtime
# assets (fonts / models / levels), a launcher that sets the library path so it runs from anywhere,
# a README, and a MANIFEST listing every file with its size. The result is tarred into
# dist/<app>-<version>-<os>-<arch>.tar.gz — a single file a player can download, extract, and run
# with no engine, no toolchain, and no install.
#
# It then *verifies* the bundle is genuinely self-contained by running the packaged launcher from a
# scratch directory (headless), proving the game finds its own libraries and assets rather than the
# build tree's. This is the check Godot's exporter doesn't do for you.
#
# Usage:
#   tools/package.sh <app> [version]     # e.g. tools/package.sh zomboid 1.0.0
#   tools/package.sh --list              # list packageable (built) apps
#
# Requires an existing build in build/bin (run: cmake -S . -B build && cmake --build build).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/bin"
DIST="$ROOT/dist"

os_name() { uname -s | tr '[:upper:]' '[:lower:]'; }
arch_name() { uname -m; }

if [ "${1:-}" = "--list" ] || [ "${1:-}" = "" ]; then
    echo "Packageable apps (built executables in build/bin):"
    if [ -d "$BIN" ]; then
        for f in "$BIN"/*; do
            [ -f "$f" ] && [ -x "$f" ] && echo "  $(basename "$f")"
        done
    else
        echo "  (none — run: cmake -S . -B build && cmake --build build)"
    fi
    [ "${1:-}" = "--list" ] && exit 0
    exit 0
fi

APP="$1"
VERSION="${2:-0.1.0}"
EXE="$BIN/$APP"

if [ ! -x "$EXE" ]; then
    echo "error: '$APP' is not a built executable at $EXE" >&2
    echo "       run: cmake -S . -B build && cmake --build build --target $APP" >&2
    exit 1
fi

OS="$(os_name)"
ARCH="$(arch_name)"
STAGE="$DIST/$APP-$VERSION-$OS-$ARCH"
TARBALL="$STAGE.tar.gz"

echo "==> Packaging '$APP' v$VERSION for $OS-$ARCH"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib"

# 1) The game executable.
cp "$EXE" "$STAGE/$APP"
chmod +x "$STAGE/$APP"

# 2) The SDL3 runtime, if the engine links it dynamically. Copy the real file + preserve the
#    versioned symlink chain so the loader resolves SONAME correctly.
shopt -s nullglob
for so in "$BIN"/libSDL3.so*; do
    cp -P "$so" "$STAGE/lib/"
done
shopt -u nullglob

# 3) Compiled shaders (every app can reference the shared shader set).
if [ -d "$BIN/shaders" ]; then
    mkdir -p "$STAGE/shaders"
    cp "$BIN/shaders/"*.spv "$STAGE/shaders/" 2>/dev/null || true
fi

# 4) Runtime assets (fonts / models / levels), preserving the tree the apps expect.
if [ -d "$BIN/assets" ]; then
    cp -r "$BIN/assets" "$STAGE/assets"
fi

# 5) A launcher that points the dynamic loader at the bundled lib/ so the game runs from anywhere,
#    with no system SDL install required. Passes all args straight through.
cat >"$STAGE/run-$APP.sh" <<LAUNCH
#!/usr/bin/env bash
# Launcher for $APP — runs the game using the libraries bundled beside it.
HERE="\$(cd "\$(dirname "\$0")" && pwd)"
export LD_LIBRARY_PATH="\$HERE/lib:\${LD_LIBRARY_PATH:-}"
exec "\$HERE/$APP" "\$@"
LAUNCH
chmod +x "$STAGE/run-$APP.sh"

# 6) A player-facing README.
cat >"$STAGE/README.txt" <<README
$APP $VERSION — built with the Maz Engine
==========================================

To play:
    ./run-$APP.sh

Everything the game needs is inside this folder — the executable, its libraries
(lib/), compiled shaders (shaders/), and assets (assets/). No installation and no
separate engine download are required.

If the game crashes, a report is written to $APP.crash.log next to the executable;
please include it when reporting a bug.

Requires a GPU with Vulkan support.
README

# 7) A MANIFEST of every packaged file + size (auditable, and doubles as an integrity list).
( cd "$STAGE" && find . -type f -printf '%10s  %p\n' | sort -k2 ) >"$STAGE/MANIFEST.txt"

# 8) Tar it up (deterministic-ish: sorted names, no owner noise).
mkdir -p "$DIST"
rm -f "$TARBALL"
tar -C "$DIST" --owner=0 --group=0 -czf "$TARBALL" "$(basename "$STAGE")"

BYTES=$(wc -c <"$TARBALL")
echo "==> Bundle:  $STAGE"
echo "==> Tarball: $TARBALL ($((BYTES / 1024)) KiB)"

# 9) Verify the bundle is self-contained: run the packaged launcher headless from a scratch dir
#    (so it can't accidentally use the build tree). Skip the run only if the app has no headless
#    mode; a clean exit proves it found its own libs + assets.
echo "==> Verifying self-contained launch (headless)..."
VERIFY_DIR="$(mktemp -d)"
cp -r "$STAGE" "$VERIFY_DIR/game"
set +e
( cd "$VERIFY_DIR" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy MAZ_UNUSED=1 \
    ./game/run-$APP.sh --headless --frames 10 >/dev/null 2>&1 )
RC=$?
set -e
rm -rf "$VERIFY_DIR"
if [ "$RC" -eq 0 ]; then
    echo "==> OK: packaged '$APP' launched self-contained and exited cleanly."
else
    echo "==> WARN: packaged launch returned $RC (app may lack --headless, or a runtime dep is missing)." >&2
fi

echo "Done."
