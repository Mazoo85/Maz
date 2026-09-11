# Maz Forge

A nightly loop that reads the Maz repository's own state, decides what needs doing, runs one useful task, and records what happened.

Forge wakes daily, scans your code and issues, evaluates options, picks one actionable improvement, executes it on a branch, and logs the run — all hands-off. It makes steady progress on debt and improvements without your constant attention.

The primary way to run Forge is a Claude Code Routine (a scheduled trigger) that wakes a fresh
session nightly and runs `forge run`. No API key to manage, no CI minutes, and it inherits the
repo's Superpowers skills. A `.github/workflows/forge.yml` cron is a later, optional path for a
fully self-hosted setup — that path needs an `ANTHROPIC_API_KEY` repo secret, but the Routine
path does not.

See `docs/FORGE.md` for the full operator's guide (lands in a later task).

## Install

Requires Python 3.10+.

```bash
cd forge
pip install -e .
```

## Use

```bash
forge init        # scaffold a starter forge config
forge config      # show effective configuration
forge sense       # read repo state and list options
forge decide      # pick the best option for tonight's run
forge run         # dry run by default: sense, decide, write a ledger line, change nothing
forge run --live  # execute the chosen task for real (branch, commit, log)
forge ledger      # show past runs
forge followup    # backfill merged / human_edits for past PRs, by asking GitHub
```

Forge never pushes to main and never merges branches — all changes land on feature branches for your review.

Talking to GitHub at all — reading CI status, opening a PR, and `forge followup` — needs a `GITHUB_TOKEN` (or `GH_TOKEN`) environment variable set wherever Forge runs. The Routine path needs no API key for Crew, but it still needs this. See `docs/FORGE.md` for what a missing token looks like.

## Development

```bash
cd forge
pip install -e '.[dev]'
pytest
```
