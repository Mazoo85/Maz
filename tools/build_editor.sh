#!/usr/bin/env bash
# build_editor.sh — one command to turn the Maz Engine source code into the Editor program.
#
# For macOS and Linux. (Windows users: run tools\build_editor.bat instead.)
#
# What it does, in plain terms: it checks that the tools it needs are installed, "cooks" the
# source code into a runnable program (this is called building), and then tells you exactly
# where the program is and how to open it. You do NOT need to know how to code to run this —
# just double-click it or run it from a terminal.
#
# If something needed is missing, it stops and tells you what to install (with links), instead
# of failing with a confusing error.

set -u

# ----- pretty output helpers -------------------------------------------------
bold=$(printf '\033[1m'); green=$(printf '\033[32m'); red=$(printf '\033[31m')
yellow=$(printf '\033[33m'); dim=$(printf '\033[2m'); reset=$(printf '\033[0m')
say()  { printf '%s\n' "$*"; }
ok()   { printf '%s✔%s %s\n' "$green" "$reset" "$*"; }
warn() { printf '%s!%s %s\n' "$yellow" "$reset" "$*"; }
die()  { printf '\n%s✖ %s%s\n' "$red" "$*" "$reset"; exit 1; }
step() { printf '\n%s== %s ==%s\n' "$bold" "$*" "$reset"; }

# Run from the repository root no matter where the script was launched from.
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here" || die "Could not find the project folder."

say "${bold}Maz Engine — Editor builder${reset}"
say "${dim}Project folder: $here${reset}"

# ----- 1. check the tools we need -------------------------------------------
step "Checking your computer has the tools it needs"

missing=0
need() {
  # need <command> <friendly name> <where to get it>
  if command -v "$1" >/dev/null 2>&1; then
    ok "$2 found"
  else
    warn "$2 is NOT installed"
    say  "    Get it here: $3"
    missing=1
  fi
}

need cmake "CMake (the build organizer)" "https://cmake.org/download/"

# A C++ compiler: any one of these is fine.
if command -v clang++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1; then
  ok "A C++ compiler found"
else
  warn "No C++ compiler found (need clang++ or g++)"
  if [ "$(uname)" = "Darwin" ]; then
    say "    On a Mac, run this once in Terminal:  xcode-select --install"
  else
    say "    On Ubuntu/Debian Linux, run:  sudo apt install build-essential"
  fi
  missing=1
fi

# The Vulkan SDK gives us the graphics library + the shader compiler (glslangValidator).
if command -v glslangValidator >/dev/null 2>&1; then
  ok "Vulkan shader compiler found"
else
  warn "Vulkan SDK is NOT installed (needed for graphics)"
  say  "    Get it here: https://vulkan.lunarg.com/sdk/home"
  if [ "$(uname)" = "Darwin" ]; then
    say  "    ${dim}(On a Mac, the Vulkan SDK includes MoltenVK, which lets Vulkan run on Apple hardware.)${reset}"
  fi
  missing=1
fi

if [ "$missing" -ne 0 ]; then
  die "Some tools are missing (see above). Install them, then run this script again."
fi

# ----- 2. build --------------------------------------------------------------
step "Building the Editor (this can take a few minutes the first time)"
say "${dim}First run downloads a couple of helper libraries automatically — that's normal.${reset}"

# Prefer Ninja if present (faster); otherwise let CMake pick the default.
gen_args=()
if command -v ninja >/dev/null 2>&1; then
  gen_args=(-G Ninja)
fi

cmake -S . -B build "${gen_args[@]}" -DCMAKE_BUILD_TYPE=Release \
  || die "The setup step failed. Scroll up for the reason; usually a missing tool."

cmake --build build --target editor \
  || die "The build failed. Scroll up for the first red error line."

# ----- 3. done ---------------------------------------------------------------
bin="build/bin/editor"
[ -f "$bin" ] || die "Build reported success but the program wasn't found at $bin."

step "Done!"
ok "Your Editor program is ready at:  ${bold}$here/$bin${reset}"
say ""
say "To open it, run this in a terminal:"
say "    ${bold}$bin${reset}"
say ""
say "${dim}(You need a real screen for the window to appear. If you're on a server with no"
say "display, you can smoke-test it with:  $bin --headless --frames 5 )${reset}"
