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
crew do "add retry with backoff to the http client"   # run a task end-to-end
crew resume                                            # continue the last task here
crew status                                            # show saved session state
crew agents                                            # list the crew and their tools
```

By default every checkpoint asks for your approval. To run unattended, pass
`--yes` (auto-approve every checkpoint) and optionally tune the repair loop:

```bash
crew do "bump the version and update the changelog" --yes --max-fix-rounds 2
```

`--yes` still never commits or pushes — it only auto-approves the plan/review
checkpoints. The commit remains yours to make.

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

Session state for the current directory is written to `.crew/session.json` (add
`.crew/` to your `.gitignore`).

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
