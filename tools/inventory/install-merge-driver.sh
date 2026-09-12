#!/usr/bin/env bash
#
# Teach this checkout how to merge the inventory's generated files.
#
# A merge driver cannot live in .gitattributes alone: the attribute names a
# driver, and the driver's command has to be in the repository's own git config,
# which git deliberately does not take from tracked files (a repo that could
# configure commands would be a repo that could run them on clone).
#
# So every checkout registers it once. .claude/hooks/session-start.sh calls this
# for web sessions; run it by hand in a local clone. Safe to run repeatedly.
set -uo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root" || exit 0

git rev-parse --is-inside-work-tree >/dev/null 2>&1 || exit 0

git config merge.maz-inventory.name \
    "regenerate docs/INVENTORY.md and docs/inventory.json from the merged tree"
git config merge.maz-inventory.driver "tools/inventory/merge-driver.sh %A %P"

echo "inventory merge driver registered"
