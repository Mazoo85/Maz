"""The crew: four specialised agents with scoped tool access.

Each role is described as plain data in ``ROLES`` (name, tools, model, prompt) so
tool-scoping — the safety story — can be asserted in tests without importing the
SDK. ``build_agents`` maps that data onto Claude Agent SDK ``AgentDefinition``
objects, importing the SDK lazily so ``crew --help`` works before it's installed.

The important design choice is *tool scoping*: the planner and reviewer are given
read-only tools, so they physically cannot modify files — a real guardrail, not just
a prompt instruction. Only the coder can edit.
"""

from __future__ import annotations

from dataclasses import dataclass
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

End your report with a single final line, exactly one of:
  REVIEW: CLEAN    (nothing needs changing)
  REVIEW: ISSUES   (you listed problems above that should be fixed)
"""

TESTER_PROMPT = """\
You are the TESTER on a small software team. Detect and run the project's tests
and linters (look for pytest, npm test, cargo test, a Makefile, CI config, etc.).
Run them and report:
- The exact command(s) you ran.
- Pass/fail, with the key failing output quoted (not the whole log).
- A one-line diagnosis of each failure so the coder can fix it.
Do not edit code. If you cannot find any tests, say so and suggest what to add.

End your report with a single final line, exactly one of:
  VERDICT: PASS
  VERDICT: FAIL
Use PASS only if every test and linter you ran succeeded (or there were genuinely
none to run). Use FAIL if anything failed.
"""


@dataclass(frozen=True)
class Role:
    """Plain description of a crew member — no SDK dependency."""

    name: str
    description: str
    prompt: str
    tools: list[str]
    model_attr: str  # attribute on CrewConfig holding this role's model id

    def model(self, config: CrewConfig) -> str:
        return getattr(config, self.model_attr)


# The crew, as data. Tool scoping here is the guardrail tests assert on.
ROLES: list[Role] = [
    Role(
        name="planner",
        description="Reads the codebase and produces an implementation plan. Read-only.",
        prompt=PLANNER_PROMPT,
        tools=list(READ_ONLY),
        model_attr="planner_model",
    ),
    Role(
        name="coder",
        description="Implements the approved plan. The only agent that can edit files.",
        prompt=CODER_PROMPT,
        tools=["Read", "Edit", "Write", "Bash", "Glob", "Grep"],
        model_attr="coder_model",
    ),
    Role(
        name="reviewer",
        description="Reviews the diff for correctness and security. Read-only.",
        prompt=REVIEWER_PROMPT,
        tools=list(READ_ONLY),
        model_attr="reviewer_model",
    ),
    Role(
        name="tester",
        description="Runs the project's tests and linters and reports results. Read-only on code.",
        prompt=TESTER_PROMPT,
        tools=["Bash", "Read", "Grep", "Glob"],
        model_attr="tester_model",
    ),
]

ROLES_BY_NAME: dict[str, Role] = {r.name: r for r in ROLES}


def build_agents(config: CrewConfig) -> dict[str, Any]:
    """Construct the ``AgentDefinition`` map keyed by role name."""
    from claude_agent_sdk import AgentDefinition  # lazy import

    from .integrations import augment_tester_prompt, augment_tester_tools

    def tools_for(role: Role) -> list[str]:
        return augment_tester_tools(role.tools, config) if role.name == "tester" else role.tools

    def prompt_for(role: Role) -> str:
        return augment_tester_prompt(role.prompt, config) if role.name == "tester" else role.prompt

    return {
        role.name: AgentDefinition(
            description=role.description,
            prompt=prompt_for(role),
            tools=tools_for(role),
            model=role.model(config),
            # Run synchronously: the orchestrator drives phases in order and must
            # not advance until a phase's agent has actually finished its work.
            background=False,
        )
        for role in ROLES
    }
