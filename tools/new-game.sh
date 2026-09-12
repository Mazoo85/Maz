#!/usr/bin/env sh
# new-game.sh — scaffold a new Maz game in one command.
#
#   tools/new-game.sh <name> ["Game Title"]
#
# It copies apps/_template (the canonical mobile-ready starting point) to apps/<name>, renames the
# build target to <name>, personalizes the header comment, and wires apps/<name> into the root
# CMakeLists.txt so it builds with the rest of the engine. After it runs:
#
#   cmake -S . -B build -G Ninja        # (re)configure so CMake picks up the new app
#   cmake --build build --target <name> # build just your game
#   ./build/bin/<name>                  # run it (add --headless for no window)
#
# <name> becomes both the directory (apps/<name>) and the CMake target / executable name, so it must
# be a valid identifier: lowercase letters, digits, hyphen and underscore, starting with a letter.
set -eu

# --- resolve repo root (this script lives in <root>/tools) -----------------------------------
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

usage() {
    echo "usage: tools/new-game.sh <name> [\"Game Title\"]" >&2
    echo "  <name>  lowercase letters/digits/-/_ , starting with a letter (e.g. spacegame)" >&2
    exit 2
}

[ "$#" -ge 1 ] || usage
NAME=$1
TITLE=${2:-$NAME}

# --- validate the name -----------------------------------------------------------------------
case "$NAME" in
    [a-z]*) : ;;
    *) echo "error: <name> must start with a lowercase letter" >&2; usage ;;
esac
# Only [a-z0-9_-] allowed. Strip the legal set and complain if anything remains.
if [ -n "$(printf '%s' "$NAME" | tr -d 'a-z0-9_-')" ]; then
    echo "error: <name> may only contain lowercase letters, digits, '-' and '_'" >&2
    usage
fi

TEMPLATE="$ROOT/apps/_template"
DEST="$ROOT/apps/$NAME"
CMAKE="$ROOT/CMakeLists.txt"

# --- safety checks ---------------------------------------------------------------------------
[ -d "$TEMPLATE" ] || { echo "error: template not found at $TEMPLATE" >&2; exit 1; }
[ -f "$CMAKE" ] || { echo "error: root CMakeLists.txt not found at $CMAKE" >&2; exit 1; }
if [ -e "$DEST" ]; then
    echo "error: apps/$NAME already exists — pick another name or remove it first" >&2
    exit 1
fi
if grep -q "add_subdirectory(apps/$NAME)" "$CMAKE"; then
    echo "error: apps/$NAME is already registered in CMakeLists.txt" >&2
    exit 1
fi

# --- copy the template ------------------------------------------------------------------------
cp -R "$TEMPLATE" "$DEST"

# --- rename the CMake target (app_template -> <name>) ----------------------------------------
# The template's CMakeLists.txt uses the target name `app_template`; swap it for the game name.
tmp="$DEST/CMakeLists.txt.tmp"
sed "s/app_template/$NAME/g" "$DEST/CMakeLists.txt" > "$tmp" && mv "$tmp" "$DEST/CMakeLists.txt"

# --- personalize the header comment of main.cpp ----------------------------------------------
# Replace only the first line's template label so the new file announces itself; leave all code
# untouched. (The template's first line begins: // Maz Engine — "_template" — ...)
if [ -f "$DEST/main.cpp" ]; then
    tmp="$DEST/main.cpp.tmp"
    sed "1s|Maz Engine — \"_template\" — the canonical starting point for a new (mobile-ready) Maz game\. It is|Maz Engine — \"$NAME\" ($TITLE) — a new Maz game, scaffolded from _template. It is|" \
        "$DEST/main.cpp" > "$tmp" && mv "$tmp" "$DEST/main.cpp"
fi

# --- wire it into the root CMakeLists.txt -----------------------------------------------------
# Insert `add_subdirectory(apps/<name>)` immediately after the LAST existing apps entry, so the new
# game sits with its peers regardless of which app is currently last.
awk -v line="add_subdirectory(apps/$NAME)" '
    /^add_subdirectory\(apps\// { last = NR }
    { rows[NR] = $0 }
    END {
        for (i = 1; i <= NR; i++) {
            print rows[i]
            if (i == last) print line
        }
    }
' "$CMAKE" > "$CMAKE.tmp" && mv "$CMAKE.tmp" "$CMAKE"

# --- done -------------------------------------------------------------------------------------
cat <<EOF
Created apps/$NAME  (title: "$TITLE", target: $NAME)
  - copied from apps/_template
  - registered in CMakeLists.txt

Next steps:
  cmake -S . -B build -G Ninja
  cmake --build build --target $NAME
  ./build/bin/$NAME            # add --headless --frames N to run without a window

Edit apps/$NAME/main.cpp to build your game. See docs/TUTORIAL_FIRST_GAME.md for a guided start.
EOF
