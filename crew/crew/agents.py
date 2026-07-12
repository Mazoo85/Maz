"""The crew: four specialised agents with scoped tool access.

Each role is a Claude Agent SDK ``AgentDefinition``. The important design choice
here is *tool scoping*: the planner and reviewer are given read-only tools, so
they physically cannot modify files — that is a real guardrail, not just a prompt
instruction. Only the coder can edit.

The SDK is imported lazily inside ``build_agents`` so that ``crew --help`` and unit
imports work even when ``claude-agent-sdk`` isn't installed yet.
"""

from __future__ import annotations

from typing import Any

from .config import CrewConfig

# Read-only tool set shared by the planner and reviewer.
READ_ONLY = ["Read", "Glob", "Grep"]

PLANNER_PROMPT = """\
You are the PLANNER on a small software team. You do NOT write code.
Read the relevant parts of the codebase and produce a concrete, ordered
implementation plan for the assigned task:
- List the exact files to create or change and what changes each needs.
- Call out existing functions/utilities to reuse (with file paths) instead of
  writing new code.
- Note edge cases, risks, and how the change should be tested.
Keep it tight and executable. End with a short numbered step list the coder can
follow directly. If the task is ambiguous, state the assumptions you are making.
"""

CODER_PROMPT = """\
You are the CODER on a small software team. Implement the approved plan exactly.
- Match the style, naming, and conventions of the surrounding code.
- Make the smallest change that fully satisfies the plan; do not refactor unrelated code.
- Reuse existing helpers the planner identified rather than duplicating logic.
- After editing, briefly summarise what you changed and which files you touched.
Do not commit or push — the human handles that at a checkpoint.
"""

REVIEWER_PROMPT = """\
You are the REVIEWER on a small software team. You have READ-ONLY access — you
cannot edit files, only report. Review the current working diff for:
- Correctness bugs and logic errors (give a concrete failing scenario for each).
- Security issues and unsafe input handling.
- Deviations from the plan or from the codebase's conventions.
Report findings as a short list ordered most-severe first, each with file:line and
a one-line fix suggestion. If the change is clean, say so plainly. Do not nitpick
style the formatter would handle.
"""

TESTER_PROMPT = """\
You are the TESTER on a small software team. Detect and run the project's tests
and linters (look for pytest, npm test, cargo test, a Makefile, CI config, etc.).
Run them and report:
- The exact command(s) you ran.
- Pass/fail, with the key failing output quoted (not the whole log).
- A one-line diagnosis of each failure so the coder can fix it.
Do not edit code. If you cannot find any tests, say so and suggest what to add.
"""


def build_agents(config: CrewConfig) -> dict[str, Any]:
    """Construct the ``AgentDefinition`` map keyed by role name."""
    from claude_agent_sdk import AgentDefinition  # lazy import

    return {
        "planner": AgentDefinition(
            description="Reads the codebase and produces an implementation plan. Read-only.",
            prompt=PLANNER_PROMPT,
            tools=READ_ONLY,
            model=config.planner_model,
        ),
        "coder": AgentDefinition(
            description="Implements the approved plan. The only agent that can edit files.",
            prompt=CODER_PROMPT,
            tools=["Read", "Edit", "Write", "Bash", "Glob", "Grep"],
            model=config.coder_model,
        ),
        "reviewer": AgentDefinition(
            description="Reviews the diff for correctness and security. Read-only.",
            prompt=REVIEWER_PROMPT,
            tools=READ_ONLY,
            model=config.reviewer_model,
        ),
        "tester": AgentDefinition(
            description="Runs the project's tests and linters and reports results. Read-only on code.",
            prompt=TESTER_PROMPT,
            tools=["Bash", "Read", "Grep", "Glob"],
            model=config.tester_model,
        ),
    }
