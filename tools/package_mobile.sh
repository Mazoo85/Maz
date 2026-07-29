#!/usr/bin/env bash
# Maz Engine — mobile export, the Android/iOS counterpart to tools/package.sh.
#
# Where package.sh produces a desktop tarball, this stages a phone bundle LAYOUT: it drives the
# `mobilepack` CLI (which uses the single source of truth, io::planMobileBundle) to place the game's
# shared library / Mach-O, its shaders and assets, and the generated AndroidManifest.xml / Info.plist
# into dist/<app>-<version>-<os>[-<abi>]/, then verifies every planned file landed at its planned size.
#
# This is the toolchain-independent half of a mobile build — the part a cloud Linux box can do. Turning
# the staged tree into an installable .apk/.ipa still needs the owner's Gradle+NDK / Xcode toolchain and
# signing (see docs/MOBILE_BUILD.md); this makes "lay out the files + drop in the manifest" one command.
#
# Usage:
#   tools/package_mobile.sh <app> [version] [--os android|ios] [--abi ARCH]
#   tools/package_mobile.sh --list       # list built apps that can be staged
#
# Requires an existing build in build/bin (run: cmake -S . -B build && cmake --build build).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/bin"
DIST="$ROOT/dist"
MOBILEPACK="$BIN/mobilepack"

if [ "${1:-}" = "--list" ] || [ "${1:-}" = "" ]; then
    echo "Stageable apps (built executables in build/bin):"
    if [ -d "$BIN" ]; then
        for f in "$BIN"/*; do
            [ -f "$f" ] && [ -x "$f" ] && [ "$(basename "$f")" != "mobilepack" ] && echo "  $(basename "$f")"
        done
    else
        echo "  (none — run: cmake -S . -B build && cmake --build build)"
    fi
    exit 0
fi

APP="$1"; shift
VERSION="0.1.0"
OS="android"
ABI="arm64-v8a"

# First positional after the app that isn't a flag is the version; the rest are --os/--abi flags.
if [ "${1:-}" != "" ] && [ "${1#--}" = "${1:-}" ]; then
    VERSION="$1"; shift
fi
while [ "${1:-}" != "" ]; do
    case "$1" in
        --os)  OS="${2:?--os needs a value}"; shift 2 ;;
        --abi) ABI="${2:?--abi needs a value}"; shift 2 ;;
        *) echo "error: unknown option '$1'" >&2; exit 2 ;;
    esac
done

if [ ! -x "$MOBILEPACK" ]; then
    echo "==> Building mobilepack..."
    cmake --build "$ROOT/build" --target mobilepack >/dev/null
fi
if [ ! -x "$BIN/$APP" ]; then
    echo "error: '$APP' is not a built executable at $BIN/$APP" >&2
    echo "       run: cmake --build build --target $APP" >&2
    exit 1
fi

SUFFIX="$OS"
[ "$OS" = "android" ] && SUFFIX="$OS-$ABI"
OUT="$DIST/$APP-$VERSION-$SUFFIX"

echo "==> Staging mobile bundle: $APP v$VERSION ($OS${ABI:+, $ABI when android})"
"$MOBILEPACK" --app "$APP" --version "$VERSION" --os "$OS" --abi "$ABI" --from "$BIN" --out "$OUT"

echo "==> Bundle tree:"
if command -v find >/dev/null 2>&1; then
    ( cd "$OUT" && find . -type f | sort | sed 's/^/    /' )
fi
echo "==> Done. Next: assemble the .apk/.ipa with your Gradle/Xcode toolchain (docs/MOBILE_BUILD.md)."
