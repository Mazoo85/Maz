#!/bin/bash
#
# Make this repo's checks runnable in a Claude Code web session.
#
# The Forge (docs/FORGE.md) runs unattended every night and verifies its own
# work by running each project's real test command. A missing dependency does
# not produce a wrong answer — verification fails closed — but it fails for an
# environment reason rather than anything wrong with the work, and the Forge
# counts that as a strike against a legitimate task. Three strikes quarantine
# it. So the environment has to be able to run the checks before the loop is
# allowed to judge anything by them.
#
# What is installed here is the union of what the three Python CI jobs install
# (.github/workflows/{scraper,crew,forge}-ci.yml). Keep it in step with them.
#
#   pytest                      forge/, crew/ and scraper/ test suites
#   httpx selectolax pyyaml     scraper/ runtime imports, reached by its tests
#   typer rich                  the forge CLI, and crew's and scraper's CLIs
#
# Deliberately NOT installed: claude-agent-sdk. crew/pyproject.toml declares it,
# but crew's tests pass without it and the Forge never imports crew — it shells
# out. crew-ci.yml installs the same three packages this does, for the same
# reason.
#
# Node needs nothing: the checkers are dependency-free, and Playwright is only
# used by the site smoke test in CI, which the Forge does not run.
set -uo pipefail

# Local checkouts are the developer's own environment; do not install into it.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

# Minimum versions copied from the pyproject.toml files that declare them, so
# pip upgrades a package only when what is present is genuinely too old.
#
# There is deliberately no blanket --upgrade here, though the CI workflows use
# one. CI runs on a clean runner; this container ships a PyYAML installed by
# apt, which pip cannot uninstall ("RECORD file not found"), so --upgrade makes
# the whole install fail and takes every other package down with it. Constraints
# without --upgrade leave an already-good system package alone.
PACKAGES="pytest>=8 httpx>=0.27 selectolax>=0.3.21 PyYAML>=6.0 typer>=0.12.0 rich>=13.0.0"

python -m pip install --quiet --upgrade pip >/dev/null 2>&1 || true

if python -m pip install --quiet $PACKAGES; then
  echo "session-start: installed $PACKAGES"
else
  # Deliberately not fatal. A hard exit here would stop the session starting at
  # all, so a transient network failure would silently skip a night's run and
  # leave no ledger line. Failing soft means the run happens, the checks fail
  # closed, and this message is in the transcript to explain why.
  echo "session-start: WARNING — could not install $PACKAGES." >&2
  echo "session-start: the repo's Python checks will fail until this succeeds." >&2
fi
