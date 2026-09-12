#!/usr/bin/env bash
#
# Git merge driver for the inventory's two generated files.
#
# docs/INVENTORY.md and docs/inventory.json are generated from the repository.
# Several Claude sessions work on several branches here at once, and any two of
# them that both regenerate the catalogue produce a textual conflict in a file
# nobody should ever hand-merge — the "resolution" is not a blend of two
# versions, it is whatever a fresh scan of the merged tree says.
#
# So that is what this does: ignore both sides, re-run the scan, write the
# answer. Git calls it with %A and %P. %A is a TEMPORARY file holding our side,
# and the file git reads the result back from — it is named something like
# .merge_file_dWVeUF and tells you nothing about which file is being merged.
# %P is the real pathname, and is the only way to know which of the two outputs
# to copy. Getting that wrong is not a silent failure but it is a confusing one,
# so: %A is where the answer goes, %P is what the answer is about.
#
# Registering it is `tools/inventory/install-merge-driver.sh`, which
# .claude/hooks/session-start.sh runs for every web session. A checkout where
# it is NOT registered falls back to a normal conflict, which is survivable:
# the fix is `node tools/inventory/main.mjs --write`, and the conflict markers
# are overwritten wholesale because the file is rewritten rather than patched.
set -uo pipefail

ours="${1:?merge driver needs %A — the temp file to write the result into}"
path="${2:?merge driver needs %P — the real pathname being merged}"
root="$(cd "$(dirname "$0")/../.." && pwd)"

# The tree is mid-merge: every other file has already been merged, so a scan
# now describes the merged repository, which is the correct answer for both
# outputs. If the scan cannot run at all, fail so git reports a conflict rather
# than silently committing whichever side happened to be in %A.
if ! node "$root/tools/inventory/main.mjs" --write >/dev/null 2>&1; then
    echo "inventory merge driver: the scan failed; resolve by hand with" >&2
    echo "  node tools/inventory/main.mjs --write" >&2
    exit 1
fi

# Copy the freshly generated file over whichever of the two git is asking about.
case "$path" in
    docs/INVENTORY.md)    src="$root/docs/INVENTORY.md" ;;
    docs/inventory.json)  src="$root/docs/inventory.json" ;;
    *)
        echo "inventory merge driver: asked about an unexpected path: $path" >&2
        echo "  it is registered for docs/INVENTORY.md and docs/inventory.json only" >&2
        exit 1
        ;;
esac

cp "$src" "$ours"
exit 0
