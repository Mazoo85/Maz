<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

# Maz Crew

A personal command-line tool that hands any coding task to a small **team of AI
agents** and keeps you in control with checkpoints along the way.

It's a thin, opinionated layer on top of the [Claude Agent SDK](https://code.claude.com/docs/en/agent-sdk/overview):
the agent loop, tool use, permissions, and sub-agent machinery come from the SDK —
Crew just choreographs four specialised agents into a plan → code → review → test
workflow you can trust.

## The crew

| Agent    | Tools (scoped)                 | Role |
|----------|--------------------------------|------|
| planner  | Read, Glob, Grep               | Reads the codebase and writes an implementation plan. **Read-only.** |
| coder    | Read, Edit, Write, Bash, Grep  | Implements the approved plan. The **only** agent that can edit files. |
| reviewer | Read, Glob, Grep               | Reviews the diff for correctness & security. **Read-only.** |
| tester   | Bash, Read, Grep, Glob         | Runs the project's tests and linters and reports. **Read-only on code.** |

Tool scoping is a real guardrail: the planner and reviewer *physically cannot*
modify files, no matter what they decide.

## Workflow & checkpoints

```
PLAN ──▶ [you approve] ──▶ CODE ──▶ REVIEW ──▶ [apply fixes?] ──▶ TEST ⇄ FIX (bounded) ──▶ you commit
```

The crew pauses for your approval after the plan, after the review, and before
finishing. **It never commits or pushes** — that's always your call.

## Install

Requires Python 3.10+ and an Anthropic API key.

```bash
cd crew
pip install -e .
export ANTHROPIC_API_KEY=sk-ant-...
```

## Use

```bash
crew init                                              # scaffold a starter crew.json
crew do "add retry with backoff to the http client"   # run a task end-to-end
crew resume                                            # continue the last task here
crew status                                            # show saved session state
crew runs                                              # list saved run transcripts
crew show                                              # print a transcript (latest, or by #/id)
crew config                                            # show effective configuration
crew agents                                            # list the crew and their tools
```

To preview just the plan without writing any code, use `--dry-run`:

```bash
crew do "migrate the config loader to pydantic" --dry-run
```

By default every checkpoint asks for your approval. To run unattended, pass
`--yes` (auto-approve every checkpoint) and optionally tune the repair loop:

```bash
crew do "bump the version and update the changelog" --yes --max-fix-rounds 2
```

`--yes` still never commits or pushes — it only auto-approves the plan/review
checkpoints. The commit remains yours to make, unless you opt in with `--commit`:

```bash
crew do "add a healthcheck endpoint" --commit -m "feat: healthcheck endpoint"
```

`--commit` stages and commits the working tree after a completed run (with a
confirmation, or automatically under `--yes`). It **never pushes** — pushing stays
a deliberate step you take yourself.

When the SDK reports token cost, the crew tallies it per task, prints an approximate
total when it finishes, and shows it in `crew status`.

## Configuration

Everything tunable lives in `crew/config.py`, overridable via env vars:

| Env var                | Default  | Purpose |
|------------------------|----------|---------|
| `CREW_MODEL_OPUS`      | `opus`   | Model for the reviewer (pin an exact id like `claude-opus-4-8` for reproducibility). |
| `CREW_MODEL_SONNET`    | `sonnet` | Model for planner & coder. |
| `CREW_MODEL_HAIKU`     | `haiku`  | Model for the tester. |
| `CREW_MAX_FIX_ROUNDS`  | `3`      | Max coder↔tester repair rounds before stopping. |
| `CREW_MAX_TURNS`       | `40`     | Max agent turns per phase. |

For per-project settings you want to commit, drop a `crew.json` at the project root:

```json
{
  "reviewer_model": "claude-opus-4-8",
  "max_fix_rounds": 2,
  "max_turns": 60
}
```

Precedence is **defaults < `crew.json` < `CREW_MAX_*` env vars**, so the file sets
project defaults while env vars stay handy for one-off runs. Run `crew config` to see
the effective values and which file (if any) is in effect.

Session state for the current directory is written to `.crew/session.json`, and
every completed run saves a Markdown transcript (task + each phase's output) under
`.crew/runs/` so you can review what the crew did afterwards. Add `.crew/` to your
`.gitignore`.

## GitHub Actions CI (opt-in)

The tester normally runs your tests locally. With `github_ci` enabled, it can also
read your project's **GitHub Actions CI logs** and fold them into its verdict — a red
CI means `VERDICT: FAIL` even if local tests pass. Useful when CI runs jobs your dev
box can't (e.g. a Vulkan build).

It's off by default and needs a GitHub token in the environment:

```bash
export GITHUB_TOKEN=ghp_...
crew do "fix the flaky retry test" --github-ci
```

Or turn it on for the project in `crew.json` (`"github_ci": true`) or via
`CREW_GITHUB_CI=1`. If it's on but no token is found, the crew warns and simply runs
without CI logs. Under the hood this attaches a GitHub MCP server and grants the tester
read-only `mcp__github__*` tools.

## Development

```bash
cd crew
pip install -e '.[dev]'
pytest            # fast, offline — no API key needed
```

The tests run the whole workflow against a fake client (`tests/conftest.py`), so
the phase order, the plan checkpoint gate, the bounded repair loop, tool scoping,
and the test-verdict parser are all covered without hitting the API.

## How it fits together

- `cli.py` — the `crew` command (typer).
- `orchestrator.py` — the phased workflow and checkpoints (one `ClaudeSDKClient`
  session per task, so context carries across phases).
- `agents.py` — the four roles as data (`ROLES`) + `build_agents` (scoped tools).
- `verdict.py` — turns the tester's report into a pass/fail/unknown decision.
- `session.py` — save/resume the SDK `session_id` per project.
- `config.py` — models and loop bounds.

The same four agents are mirrored as Markdown under `.claude/agents/` so you can
also invoke them directly inside Claude Code.

---

← Back to the [**MAZ ARCADE hub**](../index.html) · [repository README](../README.md)
