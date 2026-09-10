# Maz Forge

A nightly loop that reads the Maz repository's own state, decides what needs doing, runs one useful task, and records what happened.

Forge wakes daily, scans your code and issues, evaluates options, picks one actionable improvement, executes it on a branch, and logs the run — all hands-off. It makes steady progress on debt and improvements without your constant attention.

## Install

Requires Python 3.10+ and an Anthropic API key.

```bash
cd forge
pip install -e .
export ANTHROPIC_API_KEY=sk-ant-...
```

## Use

```bash
forge init        # scaffold a starter forge config
forge config      # show effective configuration
forge sense       # read repo state and list options
forge decide      # pick the best option for tonight's run
forge run         # execute the chosen task (branch, commit, log)
forge run --dry-run  # preview without executing
forge ledger      # show past runs
forge followup    # inspect the last run's output
```

Forge never pushes to main and never merges branches — all changes land on feature branches for your review.

## Development

```bash
cd forge
pip install -e '.[dev]'
pytest
```
